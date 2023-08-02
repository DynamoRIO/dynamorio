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
#include "dr_ir_utils.h"
#include "dr_defines.h"

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

static byte *
test_instr_encoding(void *dc, uint opcode, instr_t *instr)
{
    instr_t *decin;
    byte *pc, *next_pc;

    ASSERT(instr_get_opcode(instr) == opcode);
    instr_disassemble(dc, instr, STDERR);
    print("\n");
    ASSERT(instr_is_encoding_possible(instr));
    pc = instr_encode(dc, instr, buf);
    ASSERT(pc != NULL);
    decin = instr_create(dc);
    next_pc = decode(dc, buf, decin);
    ASSERT(next_pc != NULL);
    if (!instr_same(instr, decin)) {
        print("Disassembled as:\n");
        instr_disassemble(dc, decin, STDERR);
        print("\n");
        ASSERT(instr_same(instr, decin));
    }

    instr_destroy(dc, instr);
    instr_destroy(dc, decin);
    return pc;
}

static void
test_instr_encoding_jal_or_branch(void *dc, uint opcode, instr_t *instr)
{
    /* XXX i#3544: For jal and branch instructions, current disassembler will print
     * the complete jump address, that is, an address relative to `buf`. But the
     * value of `buf` is indeterminate at runtime, so we skip checking the disassembled
     * format for these instructions. Same for test_instr_encoding_auipc().
     *
     * FIXME i#3544: For branch instructions, we should use relative offsets instead.
     */
    instr_t *decin;
    byte *pc, *next_pc;

    ASSERT(instr_get_opcode(instr) == opcode);
    ASSERT(instr_is_encoding_possible(instr));
    pc = instr_encode(dc, instr, buf);
    ASSERT(pc != NULL);
    decin = instr_create(dc);
    next_pc = decode(dc, buf, decin);
    ASSERT(next_pc != NULL);
    ASSERT(instr_same(instr, decin));
    instr_destroy(dc, instr);
    instr_destroy(dc, decin);
}

static void
test_instr_encoding_auipc(void *dc, uint opcode, app_pc instr_pc, instr_t *instr)
{
    instr_t *decin;
    byte *pc, *next_pc;

    ASSERT(instr_get_opcode(instr) == opcode);
    ASSERT(instr_is_encoding_possible(instr));
    pc = instr_encode_to_copy(dc, instr, buf, instr_pc);
    ASSERT(pc != NULL);
    decin = instr_create(dc);
    next_pc = decode_from_copy(dc, buf, instr_pc, decin);
    ASSERT(next_pc != NULL);
    ASSERT(instr_same(instr, decin));
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
        opnd_add_flags(opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_1),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_lb, instr);
    instr = INSTR_CREATE_lbu(
        dc, opnd_create_reg(DR_REG_X0),
        opnd_add_flags(opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, -1, OPSZ_1),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_lbu, instr);
    instr =
        INSTR_CREATE_lh(dc, opnd_create_reg(DR_REG_A0),
                        opnd_add_flags(opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0,
                                                             (1 << 11) - 1, OPSZ_2),
                                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_lh, instr);
    instr = INSTR_CREATE_lhu(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_add_flags(opnd_create_base_disp(DR_REG_X0, DR_REG_NULL, 0, 0, OPSZ_2),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_lhu, instr);
    instr = INSTR_CREATE_lw(
        dc, opnd_create_reg(DR_REG_X31),
        opnd_add_flags(opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, -1, OPSZ_4),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_lw, instr);
    instr = INSTR_CREATE_lwu(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_add_flags(opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, 0, OPSZ_4),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_lwu, instr);
    instr = INSTR_CREATE_ld(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_add_flags(opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 42, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_ld, instr);

    /* Store */
    instr = INSTR_CREATE_sb(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_1),
                       DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_sb, instr);
    instr = INSTR_CREATE_sh(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_X0, DR_REG_NULL, 0, -1, OPSZ_2),
                       DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_reg(DR_REG_X31));
    test_instr_encoding(dc, OP_sh, instr);
    instr =
        INSTR_CREATE_sw(dc,
                        opnd_add_flags(opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0,
                                                             (1 << 11) - 1, OPSZ_4),
                                       DR_OPND_IMM_PRINT_DECIMAL),
                        opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_sw, instr);
    instr = INSTR_CREATE_sd(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 42, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_sd, instr);

    /* Compressed Load */
    instr = INSTR_CREATE_c_ldsp(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_ldsp, instr);
    instr = INSTR_CREATE_c_ld(
        dc, opnd_create_reg(DR_REG_X8),
        opnd_add_flags(opnd_create_base_disp(DR_REG_X15, DR_REG_NULL, 0,
                                             ((1 << 5) - 1) << 3, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_ld, instr);
    instr = INSTR_CREATE_c_lwsp(
        dc, opnd_create_reg(DR_REG_X0),
        opnd_add_flags(
            opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, ((1 << 5) - 1) << 2, OPSZ_4),
            DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_lwsp, instr);
    instr = INSTR_CREATE_c_lw(
        dc, opnd_create_reg(DR_REG_X8),
        opnd_add_flags(opnd_create_base_disp(DR_REG_X15, DR_REG_NULL, 0,
                                             ((1 << 5) - 1) << 2, OPSZ_4),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_lw, instr);

    /* Compressed Store */
    instr = INSTR_CREATE_c_sdsp(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_reg(DR_REG_A0));
    test_instr_encoding(dc, OP_c_sdsp, instr);
    instr = INSTR_CREATE_c_sd(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_X15, DR_REG_NULL, 0,
                                             ((1 << 5) - 1) << 3, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_reg(DR_REG_X8));
    test_instr_encoding(dc, OP_c_sd, instr);
    instr = INSTR_CREATE_c_swsp(
        dc,
        opnd_add_flags(
            opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, ((1 << 5) - 1) << 2, OPSZ_4),
            DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_c_swsp, instr);
    instr = INSTR_CREATE_c_sw(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_X15, DR_REG_NULL, 0,
                                             ((1 << 5) - 1) << 2, OPSZ_4),
                       DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_reg(DR_REG_X8));
    test_instr_encoding(dc, OP_c_sw, instr);
}

static void
test_float_load_store(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_flw(
        dc, opnd_create_reg(DR_REG_F0),
        opnd_add_flags(opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_4),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_flw, instr);
    instr = INSTR_CREATE_fld(
        dc, opnd_create_reg(DR_REG_F31),
        opnd_add_flags(opnd_create_base_disp(DR_REG_X0, DR_REG_NULL, 0, -1, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_fld, instr);
    instr = INSTR_CREATE_flq(
        dc, opnd_create_reg(DR_REG_F31),
        opnd_add_flags(opnd_create_base_disp(DR_REG_X0, DR_REG_NULL, 0, -1, OPSZ_16),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_flq, instr);
    instr =
        INSTR_CREATE_fsw(dc,
                         opnd_add_flags(opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0,
                                                              (1 << 11) - 1, OPSZ_4),
                                        DR_OPND_IMM_PRINT_DECIMAL),
                         opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fsw, instr);
    instr =
        INSTR_CREATE_fsd(dc,
                         opnd_add_flags(opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0,
                                                              (1 << 11) - 1, OPSZ_8),
                                        DR_OPND_IMM_PRINT_DECIMAL),
                         opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fsd, instr);
    instr =
        INSTR_CREATE_fsq(dc,
                         opnd_add_flags(opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0,
                                                              (1 << 11) - 1, OPSZ_16),
                                        DR_OPND_IMM_PRINT_DECIMAL),
                         opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fsq, instr);

    /* Compressed */
    instr = INSTR_CREATE_c_fldsp(
        dc, opnd_create_reg(DR_REG_F0),
        opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_fldsp, instr);
    instr = INSTR_CREATE_c_fld(
        dc, opnd_create_reg(DR_REG_F8),
        opnd_add_flags(opnd_create_base_disp(DR_REG_X15, DR_REG_NULL, 0,
                                             ((1 << 5) - 1) << 3, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_fld, instr);
    /* There is no c.flw* instructions in RV64. */
    instr = INSTR_CREATE_c_fsdsp(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_c_fsdsp, instr);
    instr = INSTR_CREATE_c_fsd(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_X15, DR_REG_NULL, 0,
                                             ((1 << 5) - 1) << 3, OPSZ_8),
                       DR_OPND_IMM_PRINT_DECIMAL),
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

    /* FIXME i#3544: Use rounding mode string instead of hex number when disassembling. */

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

static void
test_float_arith(void *dc)
{
    instr_t *instr;

    /* FIXME i#3544: Use rounding mode string instead of hex number when disassembling. */

    instr = INSTR_CREATE_fmadd_d(dc, opnd_create_reg(DR_REG_F31),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F2),
                                 opnd_create_reg(DR_REG_F3));
    test_instr_encoding(dc, OP_fmadd_d, instr);
    instr = INSTR_CREATE_fmsub_d(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_immed_int(0b001, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F31), opnd_create_reg(DR_REG_F0),
                                 opnd_create_reg(DR_REG_F3));
    test_instr_encoding(dc, OP_fmsub_d, instr);
    instr = INSTR_CREATE_fnmadd_d(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b010, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F31),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fnmadd_d, instr);
    instr = INSTR_CREATE_fnmsub_d(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b011, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2),
                                  opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fnmsub_d, instr);

    instr = INSTR_CREATE_fmadd_q(dc, opnd_create_reg(DR_REG_F31),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F2),
                                 opnd_create_reg(DR_REG_F3));
    test_instr_encoding(dc, OP_fmadd_q, instr);
    instr = INSTR_CREATE_fmsub_q(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_immed_int(0b001, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F31), opnd_create_reg(DR_REG_F0),
                                 opnd_create_reg(DR_REG_F3));
    test_instr_encoding(dc, OP_fmsub_q, instr);
    instr = INSTR_CREATE_fnmadd_q(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b010, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F31),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fnmadd_q, instr);
    instr = INSTR_CREATE_fnmsub_q(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b011, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2),
                                  opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fnmsub_q, instr);

    instr = INSTR_CREATE_fmadd_s(dc, opnd_create_reg(DR_REG_F31),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F2),
                                 opnd_create_reg(DR_REG_F3));
    test_instr_encoding(dc, OP_fmadd_s, instr);
    instr = INSTR_CREATE_fmsub_s(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_immed_int(0b001, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F31), opnd_create_reg(DR_REG_F0),
                                 opnd_create_reg(DR_REG_F3));
    test_instr_encoding(dc, OP_fmsub_s, instr);
    instr = INSTR_CREATE_fnmadd_s(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b010, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F31),
                                  opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fnmadd_s, instr);
    instr = INSTR_CREATE_fnmsub_s(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_immed_int(0b011, OPSZ_3b),
                                  opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2),
                                  opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fnmsub_s, instr);

    instr = INSTR_CREATE_fadd_d(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_immed_int(0b100, OPSZ_3b),
                                opnd_create_reg(DR_REG_F31), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fadd_d, instr);
    instr = INSTR_CREATE_fsub_d(dc, opnd_create_reg(DR_REG_F31),
                                opnd_create_immed_int(0b111, OPSZ_3b),
                                opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fsub_d, instr);
    instr = INSTR_CREATE_fmul_d(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_immed_int(0b000, OPSZ_3b),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fmul_d, instr);
    instr = INSTR_CREATE_fdiv_d(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_immed_int(0b000, OPSZ_3b),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fdiv_d, instr);

    instr = INSTR_CREATE_fadd_q(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_immed_int(0b100, OPSZ_3b),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fadd_q, instr);
    instr = INSTR_CREATE_fsub_q(dc, opnd_create_reg(DR_REG_F31),
                                opnd_create_immed_int(0b111, OPSZ_3b),
                                opnd_create_reg(DR_REG_F31), opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fsub_q, instr);
    instr = INSTR_CREATE_fmul_q(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_immed_int(0b000, OPSZ_3b),
                                opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fmul_q, instr);
    instr = INSTR_CREATE_fdiv_q(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_immed_int(0b000, OPSZ_3b),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fdiv_q, instr);

    instr = INSTR_CREATE_fadd_s(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_immed_int(0b100, OPSZ_3b),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fadd_s, instr);
    instr = INSTR_CREATE_fsub_s(dc, opnd_create_reg(DR_REG_F31),
                                opnd_create_immed_int(0b111, OPSZ_3b),
                                opnd_create_reg(DR_REG_F31), opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fsub_s, instr);
    instr = INSTR_CREATE_fmul_s(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_immed_int(0b000, OPSZ_3b),
                                opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fmul_s, instr);
    instr = INSTR_CREATE_fdiv_s(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_immed_int(0b000, OPSZ_3b),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fdiv_s, instr);

    instr = INSTR_CREATE_fsqrt_d(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fsqrt_d, instr);
    instr = INSTR_CREATE_fsqrt_q(dc, opnd_create_reg(DR_REG_F31),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fsqrt_q, instr);
    instr = INSTR_CREATE_fsqrt_s(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fsqrt_s, instr);

    instr = INSTR_CREATE_fsqrt_d(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fsqrt_d, instr);
    instr = INSTR_CREATE_fsqrt_q(dc, opnd_create_reg(DR_REG_F31),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fsqrt_q, instr);
    instr = INSTR_CREATE_fsqrt_s(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fsqrt_s, instr);

    instr = INSTR_CREATE_fsgnj_d(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fsgnj_d, instr);
    instr = INSTR_CREATE_fsgnjn_d(dc, opnd_create_reg(DR_REG_F31),
                                  opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fsgnjn_d, instr);
    instr = INSTR_CREATE_fsgnjx_d(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fsgnjx_d, instr);

    instr = INSTR_CREATE_fsgnj_q(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fsgnj_q, instr);
    instr = INSTR_CREATE_fsgnjn_q(dc, opnd_create_reg(DR_REG_F31),
                                  opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fsgnjn_q, instr);
    instr = INSTR_CREATE_fsgnjx_q(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fsgnjx_q, instr);

    instr = INSTR_CREATE_fsgnj_s(dc, opnd_create_reg(DR_REG_F0),
                                 opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fsgnj_s, instr);
    instr = INSTR_CREATE_fsgnjn_s(dc, opnd_create_reg(DR_REG_F31),
                                  opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fsgnjn_s, instr);
    instr = INSTR_CREATE_fsgnjx_s(dc, opnd_create_reg(DR_REG_F0),
                                  opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fsgnjx_s, instr);

    instr = INSTR_CREATE_fmax_d(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_reg(DR_REG_F31), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fmax_d, instr);
    instr = INSTR_CREATE_fmin_d(dc, opnd_create_reg(DR_REG_F31),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_fmin_d, instr);
    instr = INSTR_CREATE_fmax_q(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_fmax_q, instr);
    instr = INSTR_CREATE_fmin_q(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fmin_q, instr);
    instr = INSTR_CREATE_fmax_s(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fmax_s, instr);
    instr = INSTR_CREATE_fmin_s(dc, opnd_create_reg(DR_REG_F0),
                                opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F2));
    test_instr_encoding(dc, OP_fmin_s, instr);

    instr =
        INSTR_CREATE_fclass_d(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fclass_d, instr);
    instr =
        INSTR_CREATE_fclass_q(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fclass_q, instr);
    instr =
        INSTR_CREATE_fclass_s(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_fclass_s, instr);
}

static void
test_float_compare(void *dc)
{
    instr_t *instr;
    instr = INSTR_CREATE_feq_d(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F0),
                               opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_feq_d, instr);
    instr = INSTR_CREATE_flt_d(dc, opnd_create_reg(DR_REG_X0),
                               opnd_create_reg(DR_REG_F31), opnd_create_reg(DR_REG_F31));
    test_instr_encoding(dc, OP_flt_d, instr);
    instr = INSTR_CREATE_flt_d(dc, opnd_create_reg(DR_REG_X31),
                               opnd_create_reg(DR_REG_F1), opnd_create_reg(DR_REG_F0));
    test_instr_encoding(dc, OP_flt_d, instr);

    instr = INSTR_CREATE_feq_q(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F1),
                               opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_feq_q, instr);
    instr = INSTR_CREATE_flt_q(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F1),
                               opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_flt_q, instr);
    instr = INSTR_CREATE_flt_q(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F1),
                               opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_flt_q, instr);

    instr = INSTR_CREATE_feq_s(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F1),
                               opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_feq_s, instr);
    instr = INSTR_CREATE_flt_s(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F1),
                               opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_flt_s, instr);
    instr = INSTR_CREATE_flt_s(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_F1),
                               opnd_create_reg(DR_REG_F1));
    test_instr_encoding(dc, OP_flt_s, instr);
}

static void
test_hypervisor(void *dc)
{
    instr_t *instr;

    instr =
        INSTR_CREATE_hlv_b(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hlv_b, instr);
    instr =
        INSTR_CREATE_hlv_bu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hlv_bu, instr);
    instr =
        INSTR_CREATE_hlv_h(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hlv_h, instr);
    instr =
        INSTR_CREATE_hlv_hu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hlv_hu, instr);
    instr =
        INSTR_CREATE_hlvx_hu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hlvx_hu, instr);
    instr =
        INSTR_CREATE_hlv_w(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hlv_w, instr);
    instr =
        INSTR_CREATE_hlv_wu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hlv_wu, instr);
    instr =
        INSTR_CREATE_hlvx_wu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hlvx_wu, instr);
    instr =
        INSTR_CREATE_hlv_d(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hlv_d, instr);
    instr =
        INSTR_CREATE_hsv_b(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hsv_b, instr);
    instr =
        INSTR_CREATE_hsv_h(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hsv_h, instr);
    instr =
        INSTR_CREATE_hsv_w(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hsv_w, instr);
    instr =
        INSTR_CREATE_hsv_d(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hsv_d, instr);
    instr = INSTR_CREATE_hinval_vvma(dc, opnd_create_reg(DR_REG_A0),
                                     opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hinval_vvma, instr);
    instr = INSTR_CREATE_hinval_gvma(dc, opnd_create_reg(DR_REG_A0),
                                     opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hinval_gvma, instr);
    instr = INSTR_CREATE_hfence_vvma(dc, opnd_create_reg(DR_REG_A0),
                                     opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hfence_vvma, instr);
    instr = INSTR_CREATE_hfence_gvma(dc, opnd_create_reg(DR_REG_A0),
                                     opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_hfence_gvma, instr);
}

static void
test_integer(void *dc)
{
    instr_t *instr;
    instr = INSTR_CREATE_addi(
        dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
        opnd_add_flags(opnd_create_immed_int(0, OPSZ_12b), DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_addi, instr);
    instr = INSTR_CREATE_addiw(
        dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
        opnd_add_flags(opnd_create_immed_int(0, OPSZ_12b), DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_addiw, instr);
    instr = INSTR_CREATE_slti(
        dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
        opnd_add_flags(opnd_create_immed_int(-1, OPSZ_12b), DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_slti, instr);
    instr =
        INSTR_CREATE_sltiu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                           opnd_add_flags(opnd_create_immed_int((1 << 11) - 1, OPSZ_12b),
                                          DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_sltiu, instr);
    instr =
        INSTR_CREATE_xori(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                          opnd_add_flags(opnd_create_immed_int((1 << 11) - 1, OPSZ_12b),
                                         DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_xori, instr);
    instr =
        INSTR_CREATE_ori(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                         opnd_add_flags(opnd_create_immed_int((1 << 11) - 1, OPSZ_12b),
                                        DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_ori, instr);
    instr =
        INSTR_CREATE_andi(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                          opnd_add_flags(opnd_create_immed_int((1 << 11) - 1, OPSZ_12b),
                                         DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_andi, instr);
    instr = INSTR_CREATE_slli(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                             DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_slli, instr);
    instr =
        INSTR_CREATE_slliw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                           opnd_add_flags(opnd_create_immed_int((1 << 5) - 1, OPSZ_5b),
                                          DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_slliw, instr);
    instr = INSTR_CREATE_srli(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                             DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_srli, instr);
    instr =
        INSTR_CREATE_srliw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                           opnd_add_flags(opnd_create_immed_int((1 << 5) - 1, OPSZ_5b),
                                          DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_srliw, instr);
    instr = INSTR_CREATE_srai(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                             DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_srai, instr);
    instr =
        INSTR_CREATE_sraiw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                           opnd_add_flags(opnd_create_immed_int((1 << 5) - 1, OPSZ_5b),
                                          DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_sraiw, instr);

    instr = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_add, instr);
    instr = INSTR_CREATE_addw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_addw, instr);
    instr = INSTR_CREATE_sub(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sub, instr);
    instr = INSTR_CREATE_subw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_subw, instr);
    instr = INSTR_CREATE_sll(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sll, instr);
    instr = INSTR_CREATE_sllw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sllw, instr);
    instr = INSTR_CREATE_slt(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_slt, instr);
    instr = INSTR_CREATE_sltu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sltu, instr);
    instr = INSTR_CREATE_xor(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_xor, instr);
    instr = INSTR_CREATE_srl(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_srl, instr);
    instr = INSTR_CREATE_srlw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_srlw, instr);
    instr = INSTR_CREATE_sra(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sra, instr);
    instr = INSTR_CREATE_sraw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sraw, instr);
    instr = INSTR_CREATE_or(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                            opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_or, instr);
    instr = INSTR_CREATE_and(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_mul(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_mul, instr);
    instr = INSTR_CREATE_mulw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_mulw, instr);
    instr = INSTR_CREATE_mulh(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_mulh, instr);
    instr = INSTR_CREATE_mulhsu(dc, opnd_create_reg(DR_REG_A0),
                                opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_mulhsu, instr);
    instr = INSTR_CREATE_mulhu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_mulhu, instr);
    instr = INSTR_CREATE_div(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_div, instr);
    instr = INSTR_CREATE_divw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_divw, instr);
    instr = INSTR_CREATE_divu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_divu, instr);
    instr = INSTR_CREATE_divuw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_divuw, instr);
    instr = INSTR_CREATE_rem(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_rem, instr);
    instr = INSTR_CREATE_remw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_remw, instr);
    instr = INSTR_CREATE_remu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_remu, instr);
    instr = INSTR_CREATE_remuw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_remuw, instr);

    /* Compressed */
    instr =
        INSTR_CREATE_c_addiw(dc, opnd_create_reg(DR_REG_A0),
                             opnd_add_flags(opnd_create_immed_int((1 << 5) - 1, OPSZ_5b),
                                            DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_addiw, instr);
    instr =
        INSTR_CREATE_c_addw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_c_addw, instr);
    instr =
        INSTR_CREATE_c_subw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_c_subw, instr);

    instr =
        INSTR_CREATE_c_slli(dc, opnd_create_reg(DR_REG_A1),
                            opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                           DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_slli, instr);
    instr =
        INSTR_CREATE_c_srli(dc, opnd_create_reg(DR_REG_A1),
                            opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                           DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_srli, instr);
    instr =
        INSTR_CREATE_c_srai(dc, opnd_create_reg(DR_REG_A1),
                            opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                           DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_srai, instr);
    instr = INSTR_CREATE_c_andi(
        dc, opnd_create_reg(DR_REG_A1),
        opnd_add_flags(opnd_create_immed_int(-1, OPSZ_6b), DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_andi, instr);

    instr = INSTR_CREATE_c_mv(dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_c_mv, instr);
    instr =
        INSTR_CREATE_c_add(dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_c_add, instr);
    instr =
        INSTR_CREATE_c_and(dc, opnd_create_reg(DR_REG_X8), opnd_create_reg(DR_REG_X15));
    test_instr_encoding(dc, OP_c_and, instr);
    instr =
        INSTR_CREATE_c_or(dc, opnd_create_reg(DR_REG_X8), opnd_create_reg(DR_REG_X15));
    test_instr_encoding(dc, OP_c_or, instr);
    instr =
        INSTR_CREATE_c_xor(dc, opnd_create_reg(DR_REG_X8), opnd_create_reg(DR_REG_X15));
    test_instr_encoding(dc, OP_c_xor, instr);
    instr =
        INSTR_CREATE_c_sub(dc, opnd_create_reg(DR_REG_X8), opnd_create_reg(DR_REG_X15));
    test_instr_encoding(dc, OP_c_sub, instr);
}

static void
test_jump_and_branch(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* FIXME i#3544: Need to be better formatted. */
    instr = INSTR_CREATE_lui(dc, opnd_create_reg(DR_REG_A0),
                             opnd_create_immed_int(42, OPSZ_20b));
    pc = test_instr_encoding(dc, OP_lui, instr);
    instr = INSTR_CREATE_auipc(dc, opnd_create_reg(DR_REG_A0),
                               opnd_create_pc(pc + (3 << 12)));
    test_instr_encoding_auipc(dc, OP_auipc, pc, instr);
    instr = INSTR_CREATE_jal(dc, opnd_create_reg(DR_REG_A0), opnd_create_pc(pc));
    test_instr_encoding_jal_or_branch(dc, OP_jal, instr);
    instr = INSTR_CREATE_jalr(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_immed_int(42, OPSZ_12b));
    test_instr_encoding(dc, OP_jalr, instr);

    instr = INSTR_CREATE_beq(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding_jal_or_branch(dc, OP_beq, instr);
    instr = INSTR_CREATE_bne(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding_jal_or_branch(dc, OP_bne, instr);
    instr = INSTR_CREATE_blt(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding_jal_or_branch(dc, OP_blt, instr);
    instr = INSTR_CREATE_bge(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding_jal_or_branch(dc, OP_bge, instr);
    instr = INSTR_CREATE_bltu(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding_jal_or_branch(dc, OP_bltu, instr);
    instr = INSTR_CREATE_bgeu(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding_jal_or_branch(dc, OP_bgeu, instr);

    /* Compressed */
    instr = INSTR_CREATE_c_j(dc, opnd_create_pc(pc));
    test_instr_encoding_jal_or_branch(dc, OP_c_j, instr);
    instr = INSTR_CREATE_c_jr(dc, opnd_create_reg(DR_REG_A0));
    test_instr_encoding_jal_or_branch(dc, OP_c_jr, instr);
    /* There is no c.jal in RV64. */
    instr = INSTR_CREATE_c_jalr(dc, opnd_create_reg(DR_REG_A0));
    test_instr_encoding_jal_or_branch(dc, OP_c_jalr, instr);
    instr = INSTR_CREATE_c_beqz(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_X8));
    test_instr_encoding_jal_or_branch(dc, OP_c_beqz, instr);
    instr = INSTR_CREATE_c_bnez(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_X8));
    test_instr_encoding_jal_or_branch(dc, OP_c_bnez, instr);
    instr = INSTR_CREATE_c_li(dc, opnd_create_reg(DR_REG_A1),
                              opnd_add_flags(opnd_create_immed_int((1 << 5) - 1, OPSZ_5b),
                                             DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_li, instr);

    /* FIXME i#3544: Need to be better formatted. */
    instr = INSTR_CREATE_c_lui(
        dc, opnd_create_reg(DR_REG_A1),
        opnd_add_flags(opnd_create_immed_int(1, OPSZ_6b), DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_lui, instr);
    instr =
        INSTR_CREATE_c_addi(dc, opnd_create_reg(DR_REG_A1),
                            opnd_add_flags(opnd_create_immed_int((1 << 5) - 1, OPSZ_5b),
                                           DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_addi, instr);
    instr =
        INSTR_CREATE_c_addi16sp(dc,
                                opnd_add_flags(opnd_create_immed_int(1 << 4, OPSZ_10b),
                                               DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_addi16sp, instr);
    instr =
        INSTR_CREATE_c_addi4spn(dc, opnd_create_reg(DR_REG_X8),
                                opnd_add_flags(opnd_create_immed_int(1 << 2, OPSZ_10b),
                                               DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_c_addi4spn, instr);
}

static void
test_csr(void *dc)
{
    instr_t *instr;
    instr = INSTR_CREATE_csrrw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_immed_int(0x42, OPSZ_12b));
    test_instr_encoding(dc, OP_csrrw, instr);
    instr = INSTR_CREATE_csrrs(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_immed_int(0x42, OPSZ_12b));
    test_instr_encoding(dc, OP_csrrs, instr);
    instr = INSTR_CREATE_csrrc(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_immed_int(0x42, OPSZ_12b));
    test_instr_encoding(dc, OP_csrrc, instr);
    instr = INSTR_CREATE_csrrwi(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_add_flags(opnd_create_immed_int(1, OPSZ_5b), DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_immed_int(0x42, OPSZ_12b));
    test_instr_encoding(dc, OP_csrrwi, instr);
    instr = INSTR_CREATE_csrrsi(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_add_flags(opnd_create_immed_int(1, OPSZ_5b), DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_immed_int(0x42, OPSZ_12b));
    test_instr_encoding(dc, OP_csrrsi, instr);
    instr = INSTR_CREATE_csrrci(
        dc, opnd_create_reg(DR_REG_A0),
        opnd_add_flags(opnd_create_immed_int(1, OPSZ_5b), DR_OPND_IMM_PRINT_DECIMAL),
        opnd_create_immed_int(0x42, OPSZ_12b));
    test_instr_encoding(dc, OP_csrrci, instr);
}

static void
test_bit(void *dc)
{
    instr_t *instr;
    instr = INSTR_CREATE_add_uw(dc, opnd_create_reg(DR_REG_A0),
                                opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2));
    test_instr_encoding(dc, OP_add_uw, instr);
    instr = INSTR_CREATE_sh1add(dc, opnd_create_reg(DR_REG_A0),
                                opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2));
    test_instr_encoding(dc, OP_sh1add, instr);
    instr = INSTR_CREATE_sh2add(dc, opnd_create_reg(DR_REG_A0),
                                opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2));
    test_instr_encoding(dc, OP_sh2add, instr);
    instr = INSTR_CREATE_sh3add(dc, opnd_create_reg(DR_REG_A0),
                                opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2));
    test_instr_encoding(dc, OP_sh3add, instr);
    instr =
        INSTR_CREATE_sh1add_uw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_reg(DR_REG_A2));
    test_instr_encoding(dc, OP_sh1add_uw, instr);
    instr =
        INSTR_CREATE_sh2add_uw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_reg(DR_REG_A2));
    test_instr_encoding(dc, OP_sh2add_uw, instr);
    instr =
        INSTR_CREATE_sh3add_uw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_reg(DR_REG_A2));
    test_instr_encoding(dc, OP_sh3add_uw, instr);
    instr =
        INSTR_CREATE_slli_uw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                            DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_slli_uw, instr);

    instr = INSTR_CREATE_andn(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_andn, instr);
    instr = INSTR_CREATE_orn(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_orn, instr);
    instr = INSTR_CREATE_xnor(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_xnor, instr);
    instr = INSTR_CREATE_clz(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_clz, instr);
    instr = INSTR_CREATE_clzw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_clzw, instr);
    instr = INSTR_CREATE_ctz(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_ctz, instr);
    instr = INSTR_CREATE_ctzw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_ctzw, instr);
    instr = INSTR_CREATE_cpop(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_cpop, instr);
    instr =
        INSTR_CREATE_cpopw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_cpopw, instr);
    instr = INSTR_CREATE_max(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_max, instr);
    instr = INSTR_CREATE_maxu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_maxu, instr);
    instr = INSTR_CREATE_min(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_min, instr);
    instr = INSTR_CREATE_minu(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_minu, instr);
    instr =
        INSTR_CREATE_sext_b(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sext_b, instr);
    instr =
        INSTR_CREATE_sext_h(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sext_h, instr);
    instr =
        INSTR_CREATE_zext_h(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_zext_h, instr);
    instr = INSTR_CREATE_rol(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_rol, instr);
    instr = INSTR_CREATE_rolw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_rolw, instr);
    instr = INSTR_CREATE_ror(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_ror, instr);
    instr = INSTR_CREATE_rorw(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_rorw, instr);
    instr = INSTR_CREATE_rori(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                             DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_rori, instr);
    instr =
        INSTR_CREATE_orc_b(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_orc_b, instr);
    instr = INSTR_CREATE_clmul(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                               opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_clmul, instr);
    instr = INSTR_CREATE_clmulh(dc, opnd_create_reg(DR_REG_A0),
                                opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_clmulh, instr);
    instr = INSTR_CREATE_clmulr(dc, opnd_create_reg(DR_REG_A0),
                                opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_clmulr, instr);
    instr = INSTR_CREATE_rev8(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_rev8, instr);
    instr = INSTR_CREATE_bclr(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_bclr, instr);
    instr =
        INSTR_CREATE_bclri(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                           opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                          DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_bclri, instr);
    instr = INSTR_CREATE_bext(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_bext, instr);
    instr =
        INSTR_CREATE_bexti(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                           opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                          DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_bexti, instr);
    instr = INSTR_CREATE_binv(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_binv, instr);
    instr =
        INSTR_CREATE_binvi(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                           opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                          DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_binvi, instr);
    instr = INSTR_CREATE_bset(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_bset, instr);
    instr =
        INSTR_CREATE_bseti(dc, opnd_create_reg(DR_REG_A0), opnd_create_reg(DR_REG_A1),
                           opnd_add_flags(opnd_create_immed_int((1 << 6) - 1, OPSZ_6b),
                                          DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_bseti, instr);
}

static void
test_prefetch(void *dc)
{
    instr_t *instr;
    instr = INSTR_CREATE_prefetch_i(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_X0, DR_REG_NULL, 0, 3 << 5, OPSZ_0),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_prefetch_i, instr);
    instr = INSTR_CREATE_prefetch_r(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, 5 << 5, OPSZ_0),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_prefetch_r, instr);
    instr = INSTR_CREATE_prefetch_w(
        dc,
        opnd_add_flags(opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_0),
                       DR_OPND_IMM_PRINT_DECIMAL));
    test_instr_encoding(dc, OP_prefetch_w, instr);
}

static void
test_misc(void *dc)
{
    instr_t *instr;
    instr = INSTR_CREATE_c_nop(dc);
    test_instr_encoding(dc, OP_c_nop, instr);
    instr = INSTR_CREATE_c_ebreak(dc);
    test_instr_encoding(dc, OP_c_ebreak, instr);
    instr = INSTR_CREATE_ecall(dc);
    test_instr_encoding(dc, OP_ecall, instr);
    instr = INSTR_CREATE_ebreak(dc);
    test_instr_encoding(dc, OP_ebreak, instr);
    instr = INSTR_CREATE_sret(dc);
    test_instr_encoding(dc, OP_sret, instr);
    instr = INSTR_CREATE_mret(dc);
    test_instr_encoding(dc, OP_mret, instr);
    instr = INSTR_CREATE_wfi(dc);
    test_instr_encoding(dc, OP_wfi, instr);

    /* FIXME i#3544: Need to be better formatted. */
    instr = INSTR_CREATE_fence(dc, opnd_create_immed_int(0x0, OPSZ_4b),
                               opnd_create_immed_int(0x0, OPSZ_4b),
                               opnd_create_immed_int(0x0, OPSZ_4b));
    test_instr_encoding(dc, OP_fence, instr);
    instr = INSTR_CREATE_fence_i(dc);
    test_instr_encoding(dc, OP_fence_i, instr);
    instr = INSTR_CREATE_sfence_vma(dc, opnd_create_reg(DR_REG_A1),
                                    opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sfence_vma, instr);
    instr = INSTR_CREATE_sfence_w_inval(dc);
    test_instr_encoding(dc, OP_sfence_w_inval, instr);
    instr = INSTR_CREATE_sinval_vma(dc, opnd_create_reg(DR_REG_A1),
                                    opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_sinval_vma, instr);
    instr = INSTR_CREATE_sfence_inval_ir(dc);
    test_instr_encoding(dc, OP_sfence_inval_ir, instr);
    instr = INSTR_CREATE_cbo_zero(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_0));
    test_instr_encoding(dc, OP_cbo_zero, instr);
    instr = INSTR_CREATE_cbo_clean(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_0));
    test_instr_encoding(dc, OP_cbo_clean, instr);
    instr = INSTR_CREATE_cbo_flush(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_0));
    test_instr_encoding(dc, OP_cbo_flush, instr);
    instr = INSTR_CREATE_cbo_inval(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_0));
    test_instr_encoding(dc, OP_cbo_inval, instr);
}

static void
test_insert_mov_immed_arch(void *dc)
{
    instr_t *instr;
    int i;
    instrlist_t *ilist = instrlist_create(dc);

    /* Single ADDIW cases. */

#define MOV(imm, addiw)                                                                \
    do {                                                                               \
        i = 0;                                                                         \
        instrlist_insert_mov_immed_ptrsz(dc, (imm), opnd_create_reg(DR_REG_A0), ilist, \
                                         NULL, NULL, NULL);                            \
        for (instr = instrlist_first(ilist); instr != NULL;                            \
             instr = instr_get_next(instr)) {                                          \
            switch (i) {                                                               \
            case 0:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_addiw);                           \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_ZERO);          \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (addiw));        \
                break;                                                                 \
            }                                                                          \
            i++;                                                                       \
        }                                                                              \
        ASSERT(i == 1);                                                                \
        instrlist_clear(dc, ilist);                                                    \
    } while (false)

    MOV(0, 0);
    MOV(42, 42);
    MOV(-42, -42 & 0xfff);
    MOV((1 << 11) - 1, (1 << 11) - 1);
    MOV(-(1 << 11), -(1 << 11) & 0xfff);

#undef MOV

    /* Single LUI case. */

#define MOV(imm, lui)                                                                  \
    do {                                                                               \
        i = 0;                                                                         \
        instrlist_insert_mov_immed_ptrsz(dc, (imm), opnd_create_reg(DR_REG_A0), ilist, \
                                         NULL, NULL, NULL);                            \
        for (instr = instrlist_first(ilist); instr != NULL;                            \
             instr = instr_get_next(instr)) {                                          \
            switch (i) {                                                               \
            case 0:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_lui);                             \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 0)) == (lui));          \
                break;                                                                 \
            }                                                                          \
            i++;                                                                       \
        }                                                                              \
        ASSERT(i == 1);                                                                \
        instrlist_clear(dc, ilist);                                                    \
    } while (false)

    MOV(42 << 12, 42);
    MOV(-42 << 12, -42 & 0xfffff);
    MOV(~0xfff, 0xfffff);
    MOV(INT_MIN, 0x80000);

#undef MOV

    /* LUI+ADDIW cases. */

#define MOV(imm, lui, addiw)                                                           \
    do {                                                                               \
        i = 0;                                                                         \
        instrlist_insert_mov_immed_ptrsz(dc, (imm), opnd_create_reg(DR_REG_A0), ilist, \
                                         NULL, NULL, NULL);                            \
        for (instr = instrlist_first(ilist); instr != NULL;                            \
             instr = instr_get_next(instr)) {                                          \
            switch (i) {                                                               \
            case 0:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_lui);                             \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 0)) == (lui));          \
                break;                                                                 \
            case 1:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_addiw);                           \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (addiw));        \
                break;                                                                 \
            }                                                                          \
            i++;                                                                       \
        }                                                                              \
        ASSERT(i == 2);                                                                \
        instrlist_clear(dc, ilist);                                                    \
    } while (false)

    MOV(INT_MAX, 0x80000, 0xfff);
    MOV(0x12abcdef, 0x12abd, 0xdef);

#undef MOV

    /* ADDIW+SLLI cases. */

#define MOV(imm, addiw, slli)                                                          \
    do {                                                                               \
        i = 0;                                                                         \
        instrlist_insert_mov_immed_ptrsz(dc, (imm), opnd_create_reg(DR_REG_A0), ilist, \
                                         NULL, NULL, NULL);                            \
        for (instr = instrlist_first(ilist); instr != NULL;                            \
             instr = instr_get_next(instr)) {                                          \
            switch (i) {                                                               \
            case 0:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_addiw);                           \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_ZERO);          \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (addiw));        \
                break;                                                                 \
            case 1:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_slli);                            \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (slli));         \
                break;                                                                 \
            }                                                                          \
            i++;                                                                       \
        }                                                                              \
        ASSERT(i == 2);                                                                \
        instrlist_clear(dc, ilist);                                                    \
    } while (false)

    MOV(0x7ff0000000, 0x7ff, 28);
    MOV(0xabc00000, 0x2af, 22);

#undef MOV

    /* LUI+ADDIW+SLLI cases. */

#define MOV(imm, lui, addiw, slli)                                                     \
    do {                                                                               \
        i = 0;                                                                         \
        instrlist_insert_mov_immed_ptrsz(dc, (imm), opnd_create_reg(DR_REG_A0), ilist, \
                                         NULL, NULL, NULL);                            \
        for (instr = instrlist_first(ilist); instr != NULL;                            \
             instr = instr_get_next(instr)) {                                          \
            switch (i) {                                                               \
            case 0:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_lui);                             \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 0)) == (lui));          \
                break;                                                                 \
            case 1:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_addiw);                           \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (addiw));        \
                break;                                                                 \
            case 2:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_slli);                            \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (slli));         \
                break;                                                                 \
            }                                                                          \
            i++;                                                                       \
        }                                                                              \
        ASSERT(i == 3);                                                                \
        instrlist_clear(dc, ilist);                                                    \
    } while (false)

    MOV(0x7fffffff0000, 0x80000, 0xfff, 16);
    MOV((int64_t)INT_MAX << 13, 0x80000, 0xfff, 13);

#undef MOV

    /* LUI+ADDIW+SLLI+ADDI cases. */

#define MOV(imm, lui, addiw, slli, addi)                                               \
    do {                                                                               \
        i = 0;                                                                         \
        instrlist_insert_mov_immed_ptrsz(dc, (imm), opnd_create_reg(DR_REG_A0), ilist, \
                                         NULL, NULL, NULL);                            \
        for (instr = instrlist_first(ilist); instr != NULL;                            \
             instr = instr_get_next(instr)) {                                          \
            switch (i) {                                                               \
            case 0:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_lui);                             \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 0)) == (lui));          \
                break;                                                                 \
            case 1:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_addiw);                           \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (addiw));        \
                break;                                                                 \
            case 2:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_slli);                            \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (slli));         \
                break;                                                                 \
            case 3:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_addi);                            \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (addi));         \
                break;                                                                 \
            }                                                                          \
            i++;                                                                       \
        }                                                                              \
        ASSERT(i == 4);                                                                \
        instrlist_clear(dc, ilist);                                                    \
    } while (false)

    MOV(0x7fffffff0123, 0x80000, 0xfff, 16, 0x123);
    MOV(((int64_t)INT_MAX << 26) + 0x7ff, 0x80000, 0xfff, 26, 0x7ff);

#undef MOV

    /* ADDIW+SLLI+ADDI+SLLI+ADDI cases. */

#define MOV(imm, addiw, slli, addi, slli2, addi2)                                      \
    do {                                                                               \
        i = 0;                                                                         \
        instrlist_insert_mov_immed_ptrsz(dc, (imm), opnd_create_reg(DR_REG_A0), ilist, \
                                         NULL, NULL, NULL);                            \
        for (instr = instrlist_first(ilist); instr != NULL;                            \
             instr = instr_get_next(instr)) {                                          \
            switch (i) {                                                               \
            case 0:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_addiw);                           \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_ZERO);          \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (addiw));        \
                break;                                                                 \
            case 1:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_slli);                            \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (slli));         \
                break;                                                                 \
            case 2:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_addi);                            \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (addi));         \
                break;                                                                 \
            case 3:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_slli);                            \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (slli2));        \
                break;                                                                 \
            case 4:                                                                    \
                ASSERT(instr_get_opcode(instr) == OP_addi);                            \
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_A0);            \
                ASSERT(opnd_get_immed_int(instr_get_src(instr, 1)) == (addi2));        \
                break;                                                                 \
            }                                                                          \
            i++;                                                                       \
        }                                                                              \
        ASSERT(i == 5);                                                                \
        instrlist_clear(dc, ilist);                                                    \
    } while (false)

    MOV(((int64_t)INT_MAX << 13) + 0x8ff, 1, 32, -1, 12, -1793);
    MOV(0x8000000080000001L, 0xfff, 32, 1, 31, 1);

#undef MOV

    /* Enough cases have been covered, stop testing more complex numbers. */

    instrlist_destroy(dc, ilist);
}

int
main(int argc, char *argv[])
{
#ifdef STANDALONE_DECODER
    void *dcontext = GLOBAL_DCONTEXT;
#else
    void *dcontext = dr_standalone_init();
#endif

    disassemble_set_syntax(DR_DISASM_RISCV);

    test_insert_mov_immed_arch(dcontext);

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

    test_float_arith(dcontext);
    print("test_float_arith complete\n");

    test_float_compare(dcontext);
    print("test_float_compare complete\n");

    test_hypervisor(dcontext);
    print("test_hypervisor complete\n");

    test_integer(dcontext);
    print("test_integer_arith complete\n");

    test_jump_and_branch(dcontext);
    print("test_jump_and_branch complete\n");

    test_csr(dcontext);
    print("test_csr complete\n");

    test_bit(dcontext);
    print("print_bit complete\n");

    test_prefetch(dcontext);
    print("test_prefetch complete\n");

    test_misc(dcontext);
    print("test_misc complete\n");

    print("All tests complete\n");
    return 0;
}
