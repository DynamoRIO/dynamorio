/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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

static byte buf[8192];

reg_id_t Q_registers[31] = { DR_REG_Q1,  DR_REG_Q2,  DR_REG_Q3,  DR_REG_Q4,  DR_REG_Q5,
                             DR_REG_Q6,  DR_REG_Q7,  DR_REG_Q8,  DR_REG_Q9,  DR_REG_Q10,
                             DR_REG_Q11, DR_REG_Q12, DR_REG_Q13, DR_REG_Q14, DR_REG_Q15,
                             DR_REG_Q16, DR_REG_Q17, DR_REG_Q18, DR_REG_Q19, DR_REG_Q21,
                             DR_REG_Q22, DR_REG_Q23, DR_REG_Q24, DR_REG_Q25, DR_REG_Q26,
                             DR_REG_Q27, DR_REG_Q28, DR_REG_Q29, DR_REG_Q30, DR_REG_Q31 };

reg_id_t D_registers[31] = { DR_REG_D1,  DR_REG_D2,  DR_REG_D3,  DR_REG_D4,  DR_REG_D5,
                             DR_REG_D6,  DR_REG_D7,  DR_REG_D8,  DR_REG_D9,  DR_REG_D10,
                             DR_REG_D11, DR_REG_D12, DR_REG_D13, DR_REG_D14, DR_REG_D15,
                             DR_REG_D16, DR_REG_D17, DR_REG_D18, DR_REG_D19, DR_REG_D21,
                             DR_REG_D22, DR_REG_D23, DR_REG_D24, DR_REG_D25, DR_REG_D26,
                             DR_REG_D27, DR_REG_D28, DR_REG_D29, DR_REG_D30, DR_REG_D31 };

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
test_base_disp_extend(dr_extend_type_t ext, bool scaled, opnd_size_t size, uint amount)
{
    opnd_t opnd;
    dr_extend_type_t ext_out;
    bool scaled_out, success;
    uint amount_out;

    opnd = opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_X2, ext, scaled, 0, 0, size);
    ext_out = opnd_get_index_extend(opnd, &scaled_out, &amount_out);
    ASSERT(ext == ext_out && scaled == scaled_out && amount == amount_out);
    opnd = opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_X2, DR_EXTEND_UXTX, false, 0,
                                         0, size);
    success = opnd_set_index_extend(&opnd, ext, scaled);
    ASSERT(success);
    ext_out = opnd_get_index_extend(opnd, &scaled_out, &amount_out);
    ASSERT(ext == ext_out && scaled == scaled_out && amount == amount_out);
}

static void
test_extend(void *dc)
{
    opnd_t opnd;
    dr_extend_type_t ext;
    bool scaled;
    uint amount;

    test_base_disp_extend(DR_EXTEND_UXTW, true, OPSZ_1, 0);
    test_base_disp_extend(DR_EXTEND_UXTX, true, OPSZ_2, 1);
    test_base_disp_extend(DR_EXTEND_SXTW, true, OPSZ_4, 2);
    test_base_disp_extend(DR_EXTEND_SXTX, true, OPSZ_8, 3);
    test_base_disp_extend(DR_EXTEND_UXTW, true, OPSZ_16, 4);
    test_base_disp_extend(DR_EXTEND_UXTX, true, OPSZ_0, 3);
    test_base_disp_extend(DR_EXTEND_SXTW, false, OPSZ_4, 0);
}

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
        print("Dissassembled as:\n");
        instr_disassemble(dc, decin, STDERR);
        print("\n");
        ASSERT(instr_same(instr, decin));
    }

    instr_destroy(dc, instr);
    instr_destroy(dc, decin);
}

static void
test_add(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* Add with carry
     * ADC <Wd>, <Wn>, <Wm>
     */
    instr = INSTR_CREATE_adc(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                             opnd_create_reg(DR_REG_W2));
    test_instr_encoding(dc, OP_adc, instr);

    /* ADC <Xd>, <Xn>, <Xm> */
    instr = INSTR_CREATE_adc(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X1),
                             opnd_create_reg(DR_REG_X2));
    test_instr_encoding(dc, OP_adc, instr);

    /* Add with carry setting condition flags
     * ADCS <Wd>, <Wn>, <Wm>
     */
    instr = INSTR_CREATE_adcs(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                              opnd_create_reg(DR_REG_W2));
    test_instr_encoding(dc, OP_adcs, instr);

    /* ADCS <Xd>, <Xn>, <Xm> */
    instr = INSTR_CREATE_adcs(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X1),
                              opnd_create_reg(DR_REG_X2));
    test_instr_encoding(dc, OP_adcs, instr);

    /* Add to sp (tests shift vs extend). */
    instr = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_SP),
                             opnd_create_reg(DR_REG_X1));
    test_instr_encoding(dc, OP_add, instr);

    /* Sub from sp (tests shift vs extend). */
    instr = INSTR_CREATE_sub(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_SP),
                             opnd_create_reg(DR_REG_X1));
    test_instr_encoding(dc, OP_sub, instr);

/* Add and set flags (shifted register, 32-bit)
 * ADDS <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
 */
#define adds_shift(reg, shift_type, amount_imm6)                                \
    instr = INSTR_CREATE_adds_shift(                                            \
        dc, opnd_create_reg(DR_REG_##reg##0), opnd_create_reg(DR_REG_##reg##1), \
        opnd_create_reg(DR_REG_##reg##2),                                       \
        opnd_add_flags(OPND_CREATE_INT(shift_type), DR_OPND_IS_SHIFT),          \
        opnd_create_immed_int(amount_imm6, OPSZ_6b));                           \
    test_instr_encoding(dc, OP_adds, instr);

    /* Shift range is 0-31 (imm6) for 32 bit variant */
    adds_shift(W, DR_SHIFT_LSL, 0);
    adds_shift(W, DR_SHIFT_LSL, 0x1F);
    adds_shift(W, DR_SHIFT_LSR, 0);
    adds_shift(W, DR_SHIFT_LSR, 0x1F);
    adds_shift(W, DR_SHIFT_ASR, 0);
    adds_shift(W, DR_SHIFT_ASR, 0x1F);

    /* Add and set flags (shifted register, 64-bit)
     * ADDS <Xd>, <Xn>, <Xm>{, <shift> #<amount>}
     * Shift range is 0-63 (imm6) for 64 bit variant
     */
    adds_shift(X, DR_SHIFT_LSL, 0);
    adds_shift(X, DR_SHIFT_LSL, 0x3F);
    adds_shift(X, DR_SHIFT_LSR, 0);
    adds_shift(X, DR_SHIFT_LSR, 0x3F);
    adds_shift(X, DR_SHIFT_ASR, 0);
    adds_shift(X, DR_SHIFT_ASR, 0x3F);

    /* Add and set flags (immediate, 32-bit)
     * ADDS <Wd>, <Wn|WSP>, #<imm>{, <shift>}
     */
    instr =
        INSTR_CREATE_adds_imm(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                              opnd_create_immed_int(0, OPSZ_12b), OPND_CREATE_INT8(0));
    test_instr_encoding(dc, OP_adds, instr);

    instr = INSTR_CREATE_adds_imm(
        dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
        opnd_create_immed_int(0xFFF, OPSZ_12b), OPND_CREATE_INT8(0));
    test_instr_encoding(dc, OP_adds, instr);

    /* Add and set flags (immediate, 64-bit)
     * ADDS <Xd>, <Xn|SP>, #<imm>{, <shift>}
     */
    instr =
        INSTR_CREATE_adds_imm(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X1),
                              opnd_create_immed_int(0, OPSZ_12b), OPND_CREATE_INT8(0));
    test_instr_encoding(dc, OP_adds, instr);

    instr = INSTR_CREATE_adds_imm(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X1),
        opnd_create_immed_int(0xFFF, OPSZ_12b), OPND_CREATE_INT8(0));
    test_instr_encoding(dc, OP_adds, instr);

/* Add and set flags (extended register, 32-bit)
 * ADDS <Wd>, <Wn|WSP>, <Wm>{, <extend> {#<amount>}}
 */
#define adds_extend(reg, extend_type, amount_imm3)                              \
    instr = INSTR_CREATE_adds_extend(                                           \
        dc, opnd_create_reg(DR_REG_##reg##0), opnd_create_reg(DR_REG_##reg##1), \
        opnd_create_reg(DR_REG_##reg##2),                                       \
        opnd_add_flags(OPND_CREATE_INT(extend_type), DR_OPND_IS_EXTEND),        \
        opnd_create_immed_int(amount_imm3, OPSZ_3b));                           \
    test_instr_encoding(dc, OP_adds, instr);

    adds_extend(W, DR_EXTEND_UXTB, 0);
    adds_extend(W, DR_EXTEND_UXTH, 1);
    adds_extend(W, DR_EXTEND_UXTW, 2);
    adds_extend(W, DR_EXTEND_SXTB, 4);
    adds_extend(W, DR_EXTEND_SXTH, 0);
    adds_extend(W, DR_EXTEND_SXTW, 1);

    adds_extend(X, DR_EXTEND_UXTX, 3);
    adds_extend(X, DR_EXTEND_SXTX, 2);
}

static void
adr(void *dc)
{
    instr_t *instr =
        INSTR_CREATE_adr(dc, opnd_create_reg(DR_REG_X1),
                         OPND_CREATE_ABSMEM((void *)0x0000000010010208, OPSZ_0));
    test_instr_encoding(dc, OP_adr, instr);

    print("adr complete\n");
}

static void
adrp(void *dc)
{
    instr_t *instr =
        INSTR_CREATE_adrp(dc, opnd_create_reg(DR_REG_X1),
                          OPND_CREATE_ABSMEM((void *)0x0000000020208000, OPSZ_0));
    test_instr_encoding(dc, OP_adrp, instr);

    print("adrp complete\n");
}

static void
ldpsw_base_post_index(void *dc)
{
    int dst_reg_0[] = { DR_REG_X1, DR_REG_X15, DR_REG_X29 };
    int dst_reg_1[] = { DR_REG_X2, DR_REG_X16, DR_REG_X30 };
    int src_reg[] = { DR_REG_X0, DR_REG_X14, DR_REG_X28 };
    int value[] = { 0, 4, 252, -256 };

    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 4; ii++) {
            instr_t *instr = INSTR_CREATE_ldpsw(
                dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
                opnd_create_reg(src_reg[i]),
                opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                              OPSZ_8),
                OPND_CREATE_INT(value[ii]));
            test_instr_encoding(dc, OP_ldpsw, instr);
        }
    }
    print("ldpsw base post-index complete\n");
}

static void
ldpsw_base_pre_index(void *dc)
{
    int dst_reg_0[] = { DR_REG_X1, DR_REG_X15, DR_REG_X29 };
    int dst_reg_1[] = { DR_REG_X2, DR_REG_X16, DR_REG_X30 };
    int src_reg[] = { DR_REG_X0, DR_REG_X14, DR_REG_X28 };
    int value[] = { 0, 4, 252, -256 };

    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 4; ii++) {
            instr_t *instr = INSTR_CREATE_ldpsw(
                dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
                opnd_create_reg(src_reg[i]),
                opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false,
                                              value[ii], 0, OPSZ_8),
                OPND_CREATE_INT(value[ii]));
            test_instr_encoding(dc, OP_ldpsw, instr);
        }
    }
    print("ldpsw base pre-index complete\n");
}

static void
ldpsw_base_signed_offset(void *dc)
{
    int dst_reg_0[] = { DR_REG_X1, DR_REG_X15, DR_REG_X29 };
    int dst_reg_1[] = { DR_REG_X2, DR_REG_X16, DR_REG_X30 };
    int src_reg[] = { DR_REG_X0, DR_REG_X14, DR_REG_X28 };
    int value[] = { 8, 4, 252, -256 };

    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 4; ii++) {
            instr_t *instr = INSTR_CREATE_ldpsw_2(
                dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
                opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false,
                                              value[ii], 0, OPSZ_8));
            test_instr_encoding(dc, OP_ldpsw, instr);
        }
    }
    print("ldpsw base signed offset complete\n");
}

static void
ldpsw(void *dc)
{
    ldpsw_base_post_index(dc);
    ldpsw_base_pre_index(dc);
    ldpsw_base_signed_offset(dc);
    print("ldpsw complete\n");
}

static void
test_ldar(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* LDAR <Wt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldar(
        dc, opnd_create_reg(DR_REG_W0),
        opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_NULL, 0, false, 0, 0, OPSZ_4));
    test_instr_encoding(dc, OP_ldar, instr);

    /* LDAR <Xt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldar(
        dc, opnd_create_reg(DR_REG_X0),
        opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_NULL, 0, false, 0, 0, OPSZ_8));
    test_instr_encoding(dc, OP_ldar, instr);

    /* LDARB <Wt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldarb(
        dc, opnd_create_reg(DR_REG_W0),
        opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_NULL, 0, false, 0, 0, OPSZ_1));
    test_instr_encoding(dc, OP_ldarb, instr);

    /* LDARH <Wt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldarh(
        dc, opnd_create_reg(DR_REG_W0),
        opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_NULL, 0, false, 0, 0, OPSZ_2));
    test_instr_encoding(dc, OP_ldarh, instr);
}

/* TODO: Add this to a new suite/tests/api/ir_aarch64_v83.c file */
static void
test_ldapr(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* LDAPR <Wt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldapr(
        dc, opnd_create_reg(DR_REG_W0),
        opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_NULL, 0, false, 0, 0, OPSZ_4));
    test_instr_encoding(dc, OP_ldapr, instr);

    /* LDAPR <Xt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldapr(
        dc, opnd_create_reg(DR_REG_X2),
        opnd_create_base_disp_aarch64(DR_REG_X3, DR_REG_NULL, 0, false, 0, 0, OPSZ_8));
    test_instr_encoding(dc, OP_ldapr, instr);

    /* LDAPRB <Wt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldaprb(
        dc, opnd_create_reg(DR_REG_W4),
        opnd_create_base_disp_aarch64(DR_REG_X5, DR_REG_NULL, 0, false, 0, 0, OPSZ_1));
    test_instr_encoding(dc, OP_ldaprb, instr);

    /* LDAPRH <Wt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldaprh(
        dc, opnd_create_reg(DR_REG_W6),
        opnd_create_base_disp_aarch64(DR_REG_X7, DR_REG_NULL, 0, false, 0, 0, OPSZ_2));
    test_instr_encoding(dc, OP_ldaprh, instr);
}

static void
ld2_simdfp_multiple_structures_no_offset(void *dc)
{
    const int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q14, DR_REG_Q28 };
    const int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q15, DR_REG_Q29 };
    const int src_reg[] = { DR_REG_X2, DR_REG_X16, DR_REG_X30 };

    /*
     * LD2 { <Vt>.<T>, <Vt2>.<T> }[<imm>], [<Xn|SP>]
     */

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld2_multi(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_32),
            OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld2, instr);
    }
    print("ld2 simdfp multiple structures no offset complete\n");
}

static void
ld2_simdfp_multiple_structures_post_index(void *dc)
{
    const int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q29 };
    const int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q30 };
    const int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };
    const int offset_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };

    /*
     * # LD2     { <Vt1>.B, <Vt2>.B }, [<Xn|SP>], <Xm>
     */

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld2_multi_2(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(src_reg[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_32),
            opnd_create_reg(offset_reg[i]), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld2, instr);
    }
    /*
     * # LD2     { <Vt1>.D, <Vt2>.D }, [<Xn|SP>], #16
     * # LD2     { <Vt1>.S, <Vt2>.S }, [<Xn|SP>], #8
     */

    const int dst_reg_post_index_0[] = { DR_REG_Q0, DR_REG_D2 };
    const int dst_reg_post_index_1[] = { DR_REG_Q1, DR_REG_D3 };
    const int bit_size[] = { OPSZ_32, OPSZ_16 };
    const int imm[] = { 0x20, 0x10 };
    const opnd_t elsz[] = { OPND_CREATE_DOUBLE(), OPND_CREATE_SINGLE() };

    for (int i = 0; i < 2; i++) {
        instr_t *instr = INSTR_CREATE_ld2_multi_2(
            dc, opnd_create_reg(dst_reg_post_index_0[i]),
            opnd_create_reg(dst_reg_post_index_1[i]), opnd_create_reg(src_reg[0]),
            opnd_create_base_disp_aarch64(src_reg[0], DR_REG_NULL, 0, false, 0, 0,
                                          bit_size[i]),
            opnd_create_immed_uint(imm[i], OPSZ_PTR), elsz[i]);
        test_instr_encoding(dc, OP_ld2, instr);
    }
    print("ld2 simdfp multiple structures post-index complete\n");
}

static void
ld2_simdfp_single_structure_no_offset(void *dc)
{
    for (int index = 0; index < 16; index++) {
        instr_t *instr = INSTR_CREATE_ld2(
            dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
            opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_2),
            opnd_create_immed_uint(index, OPSZ_1), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld2, instr);
    }

    const int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q29 };
    const int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q30 };
    const int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };
    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld2(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_2),
            opnd_create_immed_uint(0x01, OPSZ_1), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld2, instr);
    }

    print("ld2 simdfp single structure no offset complete\n");
}

static void
ld2_simdfp_single_structure_post_index(void *dc)
{

    /*
     * LD2 { <Vt>.B, <Vt2>.B }[<index>], [<Xn|SP>], #2
     */

    for (int index = 1; index < 16; index++) {
        instr_t *instr = INSTR_CREATE_ld2_2(
            dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
            opnd_create_reg(DR_REG_X0),
            opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_2),
            opnd_create_immed_uint(index, OPSZ_1), opnd_create_immed_uint(0x02, OPSZ_PTR),
            OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld2, instr);
    }

    const int dst_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q29 };
    const int dst_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q30 };
    const int src[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };
    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld2_2(
            dc, opnd_create_reg(dst_0[i]), opnd_create_reg(dst_1[i]),
            opnd_create_reg(src[i]),
            opnd_create_base_disp_aarch64(src[i], DR_REG_NULL, 0, false, 0, 0, OPSZ_2),
            opnd_create_immed_uint(0x01, OPSZ_1), opnd_create_immed_uint(0x02, OPSZ_PTR),
            OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld2, instr);
    }

    /*
     * LD2 { <Vt>.B, <Vt2>.B }[<index>], [<Xn|SP>], #2
     * LD2 { <Vt>.H, <Vt2>.H }[<index>], [<Xn|SP>], #4
     * LD2 { <Vt>.S, <Vt2>.S }[<index>], [<Xn|SP>], #8
     * LD2 { <Vt>.D, <Vt2>.D }[<index>], [<Xn|SP>], #16
     */

    const int opsz[] = { OPSZ_2, OPSZ_4, OPSZ_8, OPSZ_16 };
    const opnd_t elsz[] = { OPND_CREATE_BYTE(), OPND_CREATE_HALF(), OPND_CREATE_SINGLE(),
                            OPND_CREATE_DOUBLE() };
    const int imm[] = { 2, 4, 8, 16 };
    for (int i = 0; i < 4; i++) {
        instr_t *instr =
            INSTR_CREATE_ld2_2(dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
                               opnd_create_reg(DR_REG_X0),
                               opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0,
                                                             false, 0, 0, opsz[i]),
                               opnd_create_immed_uint(0x01, OPSZ_1),
                               opnd_create_immed_uint(imm[i], OPSZ_PTR), elsz[i]);
        test_instr_encoding(dc, OP_ld2, instr);
    }

    /*
     * LD2 { <Vt>.D, <Vt2>.D }[<index>], [<Xn|SP>], <Xm>
     */

    const int offset_reg[] = { DR_REG_X1, DR_REG_X16, DR_REG_X29 };
    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld2_2(
            dc, opnd_create_reg(dst_0[i]), opnd_create_reg(dst_1[i]),
            opnd_create_reg(src[i]),
            opnd_create_base_disp_aarch64(src[i], DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
            opnd_create_immed_uint(0x01, OPSZ_1), opnd_create_reg(offset_reg[i]),
            OPND_CREATE_DOUBLE());
        test_instr_encoding(dc, OP_ld2, instr);
    }

    print("ld2 simdfp single structure post-index complete\n");
}

static void
ld2(void *dc)
{
    ld2_simdfp_multiple_structures_no_offset(dc);
    ld2_simdfp_multiple_structures_post_index(dc);
    ld2_simdfp_single_structure_no_offset(dc);
    ld2_simdfp_single_structure_post_index(dc);

    print("ld2 complete\n");
}

static void
ld3_simdfp_multiple_structures_no_offset(void *dc)
{
    const int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q14, DR_REG_Q28 };
    const int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q15, DR_REG_Q29 };
    const int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q16, DR_REG_Q30 };
    const int src_reg[] = { DR_REG_X2, DR_REG_X16, DR_REG_X30 };

    /*
     * LD3 { <Vt>.<T>, <Vt2>.<T>, <Vt3>.<T> }, [<Xn|SP>]
     */

    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 1; ii++) {
            instr_t *instr = INSTR_CREATE_ld3_multi(
                dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
                opnd_create_reg(dst_reg_2[i]),
                opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                              OPSZ_48),
                OPND_CREATE_BYTE());
            test_instr_encoding(dc, OP_ld3, instr);
        }
    }
    print("ld3 simdfp multiple structures no offset complete\n");
}

static void
ld3_simdfp_multiple_structures_post_index(void *dc)
{
    const int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q28 };
    const int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q29 };
    const int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q30 };
    const int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };
    const int offset_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };

    /*
     * LD3 { <Vt>.<T>, <Vt2>.<T>, <Vt3>.<T> }, [<Xn|SP>], <Xm>
     */

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld3_multi_2(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(dst_reg_2[i]), opnd_create_reg(src_reg[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_48),
            opnd_create_reg(offset_reg[i]), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld3, instr);
    }

    const int dst_reg_post_index_0[] = { DR_REG_D0, DR_REG_Q0 };
    const int dst_reg_post_index_1[] = { DR_REG_D1, DR_REG_Q1 };
    const int dst_reg_post_index_2[] = { DR_REG_D2, DR_REG_Q2 };
    const int imm[] = { 0x18, 0x30 };
    const int opsz[] = { OPSZ_24, OPSZ_48 };

    /*
     * LD3 { <Vt>.<T>, <Vt2>.<T>, <Vt3>.<T> }, [<Xn|SP>], <imm>
     */

    for (int i = 0; i < 2; i++) {
        instr_t *instr = INSTR_CREATE_ld3_multi_2(
            dc, opnd_create_reg(dst_reg_post_index_0[i]),
            opnd_create_reg(dst_reg_post_index_1[i]),
            opnd_create_reg(dst_reg_post_index_2[i]), opnd_create_reg(src_reg[0]),
            opnd_create_base_disp_aarch64(src_reg[0], DR_REG_NULL, 0, false, 0, 0,
                                          opsz[i]),
            opnd_create_immed_uint(imm[i], OPSZ_1), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld3, instr);
    }
    print("ld3 simdfp multiple structures post-index complete\n");
}

static void
ld3_simdfp_single_structure_no_offset(void *dc)
{

    /*
     * LD3 { <Vt>.B, <Vt2>.B, <Vt3>.B }[<index>], [<Xn|SP>]
     */

    for (int index = 0; index < 16; index++) {
        instr_t *instr = INSTR_CREATE_ld3(
            dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
            opnd_create_reg(DR_REG_Q2),
            opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_3),
            opnd_create_immed_uint(index, OPSZ_1), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld3, instr);
    }

    const int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q28 };
    const int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q29 };
    const int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q30 };
    const int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };

    /*
     * LD3 { <Vt>.S, <Vt2>.S, <Vt3>.S }[<index>], [<Xn|SP>]
     */

    for (int i = 0; i < 3; i++) {
        instr_t *instr =
            INSTR_CREATE_ld3(dc, opnd_create_reg(dst_reg_0[i]),
                             opnd_create_reg(dst_reg_1[i]), opnd_create_reg(dst_reg_2[i]),
                             opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0,
                                                           false, 0, 0, OPSZ_12),
                             opnd_create_immed_uint(0x01, OPSZ_1), OPND_CREATE_SINGLE());
        test_instr_encoding(dc, OP_ld3, instr);
    }
    print("ld3 simdfp single structure no offset complete\n");
}

static void
ld3_simdfp_single_structure_post_index(void *dc)
{
    /*
     * LD3 { <Vt>.B, <Vt2>.B, <Vt3>.B }[<index>], [<Xn|SP>], #3
     */

    for (int index = 0; index < 16; index++) {
        instr_t *instr = INSTR_CREATE_ld3_2(
            dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
            opnd_create_reg(DR_REG_Q2), opnd_create_reg(DR_REG_X0),
            opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_3),
            opnd_create_immed_uint(index, OPSZ_1), opnd_create_immed_uint(0x03, OPSZ_1),
            OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld3, instr);
    }

    /*
     * LD3 { <Vt>.H, <Vt2>.H, <Vt3>.H }[<index>], [<Xn|SP>], #6
     */

    const int dst_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q28 };
    const int dst_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q29 };
    const int dst_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q30 };
    const int src[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };
    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld3_2(
            dc, opnd_create_reg(dst_0[i]), opnd_create_reg(dst_1[i]),
            opnd_create_reg(dst_2[i]), opnd_create_reg(src[i]),
            opnd_create_base_disp_aarch64(src[i], DR_REG_NULL, 0, false, 0, 0, OPSZ_6),
            opnd_create_immed_uint(0x01, OPSZ_1), opnd_create_immed_uint(0x06, OPSZ_1),
            OPND_CREATE_HALF());
        test_instr_encoding(dc, OP_ld3, instr);
    }

    /*
     * LD3 { <Vt>.B, <Vt2>.B, <Vt3>.B }[<index>], [<Xn|SP>], #3
     * LD3 { <Vt>.H, <Vt2>.H, <Vt3>.H }[<index>], [<Xn|SP>], #6
     * LD3 { <Vt>.S, <Vt2>.S, <Vt3>.S }[<index>], [<Xn|SP>], #12
     * LD3 { <Vt>.D, <Vt2>.D, <Vt3>.D }[<index>], [<Xn|SP>], #24
     */

    const int opsz[] = { OPSZ_3, OPSZ_6, OPSZ_12, OPSZ_24 };
    const int imm[] = { 3, 6, 12, 24 };
    const opnd_t elsz[] = { OPND_CREATE_BYTE(), OPND_CREATE_HALF(), OPND_CREATE_SINGLE(),
                            OPND_CREATE_DOUBLE() };
    for (int i = 0; i < 4; i++) {
        instr_t *instr =
            INSTR_CREATE_ld3_2(dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
                               opnd_create_reg(DR_REG_Q2), opnd_create_reg(DR_REG_X0),
                               opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0,
                                                             false, 0, 0, opsz[i]),
                               opnd_create_immed_uint(0x01, OPSZ_1),
                               opnd_create_immed_uint(imm[i], OPSZ_1), elsz[i]);
        test_instr_encoding(dc, OP_ld3, instr);
    }

    /*
     * LD3 { <Vt>.B, <Vt2>.B, <Vt3>.B }[<index>], [<Xn|SP>], #3
     */

    const int offset_reg[] = { DR_REG_X1, DR_REG_X16, DR_REG_X29 };
    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld3_2(
            dc, opnd_create_reg(dst_0[i]), opnd_create_reg(dst_1[i]),
            opnd_create_reg(dst_2[i]), opnd_create_reg(src[i]),
            opnd_create_base_disp_aarch64(src[i], DR_REG_NULL, 0, false, 0, 0, OPSZ_3),
            opnd_create_immed_uint(0x01, OPSZ_1), opnd_create_reg(offset_reg[i]),
            OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld3, instr);
    }
    print("ld3 simdfp single structure post-index complete\n");
}

static void
ld3(void *dc)
{
    ld3_simdfp_multiple_structures_no_offset(dc);
    ld3_simdfp_multiple_structures_post_index(dc);
    ld3_simdfp_single_structure_no_offset(dc);
    ld3_simdfp_single_structure_post_index(dc);

    print("ld3 complete\n");
}

static void
ld4_simdfp_multiple_structures_no_offset(void *dc)
{
    const int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q14, DR_REG_Q27 };
    const int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q15, DR_REG_Q28 };
    const int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q16, DR_REG_Q29 };
    const int dst_reg_3[] = { DR_REG_Q3, DR_REG_Q17, DR_REG_Q30 };
    const int src_reg[] = { DR_REG_X2, DR_REG_X16, DR_REG_X30 };

    /*
     * LD4 { <Vt>.<T>, <Vt2>.<T>, <Vt3>.<T>, <Vt4>.<T> }, [<Xn|SP>]
     */

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld4_multi(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(dst_reg_2[i]), opnd_create_reg(dst_reg_3[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_64),
            OPND_CREATE_SINGLE());
        test_instr_encoding(dc, OP_ld4, instr);
    }
    print("ld4 simdfp multiple structures no offset complete\n");
}

static void
ld4_simdfp_multiple_structures_post_index(void *dc)
{
    const int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q27 };
    const int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q28 };
    const int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q29 };
    const int dst_reg_3[] = { DR_REG_Q3, DR_REG_Q18, DR_REG_Q30 };
    const int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };
    const int offset_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld4_multi_2(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(dst_reg_2[i]), opnd_create_reg(dst_reg_3[i]),
            opnd_create_reg(src_reg[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_64),
            opnd_create_reg(offset_reg[i]), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld4, instr);
    }

    const int dst_reg_post_index_0[] = { DR_REG_D0, DR_REG_Q0 };
    const int dst_reg_post_index_1[] = { DR_REG_D1, DR_REG_Q1 };
    const int dst_reg_post_index_2[] = { DR_REG_D2, DR_REG_Q2 };
    const int dst_reg_post_index_3[] = { DR_REG_D3, DR_REG_Q3 };
    const int imm[] = { 0x20, 0x40 };
    const int opsz[] = { OPSZ_32, OPSZ_64 };

    for (int i = 0; i < 2; i++) {
        instr_t *instr = INSTR_CREATE_ld4_multi_2(
            dc, opnd_create_reg(dst_reg_post_index_0[i]),
            opnd_create_reg(dst_reg_post_index_1[i]),
            opnd_create_reg(dst_reg_post_index_2[i]),
            opnd_create_reg(dst_reg_post_index_3[i]), opnd_create_reg(src_reg[0]),
            opnd_create_base_disp_aarch64(src_reg[0], DR_REG_NULL, 0, false, 0, 0,
                                          opsz[i]),
            opnd_create_immed_uint(imm[i], OPSZ_1), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld4, instr);
    }
    print("ld4 simdfp multiple structures post-index complete\n");
}

static void
ld4_simdfp_single_structure_no_offset(void *dc)
{
    for (int index = 0; index < 16; index++) {
        instr_t *instr = INSTR_CREATE_ld4(
            dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
            opnd_create_reg(DR_REG_Q2), opnd_create_reg(DR_REG_Q3),
            opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_4),
            opnd_create_immed_uint(index, OPSZ_1), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld4, instr);
    }

    const int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q27 };
    const int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q28 };
    const int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q29 };
    const int dst_reg_3[] = { DR_REG_Q3, DR_REG_Q18, DR_REG_Q30 };
    const int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };
    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld4(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(dst_reg_2[i]), opnd_create_reg(dst_reg_3[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_4),
            opnd_create_immed_uint(0x01, OPSZ_1), OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld4, instr);
    }
    print("ld4 simdfp single structure no offset complete\n");
}

static void
ld4_simdfp_single_structure_post_index(void *dc)
{
    for (int index = 0; index < 16; index++) {
        instr_t *instr = INSTR_CREATE_ld4_2(
            dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
            opnd_create_reg(DR_REG_Q2), opnd_create_reg(DR_REG_Q3),
            opnd_create_reg(DR_REG_X0),
            opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_4),
            opnd_create_immed_uint(index, OPSZ_1), opnd_create_immed_uint(0x04, OPSZ_PTR),
            OPND_CREATE_BYTE());
        test_instr_encoding(dc, OP_ld4, instr);
    }

    const int dst_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q27 };
    const int dst_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q28 };
    const int dst_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q29 };
    const int dst_3[] = { DR_REG_Q3, DR_REG_Q18, DR_REG_Q30 };
    const int src[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };
    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld4_2(
            dc, opnd_create_reg(dst_0[i]), opnd_create_reg(dst_1[i]),
            opnd_create_reg(dst_2[i]), opnd_create_reg(dst_3[i]), opnd_create_reg(src[i]),
            opnd_create_base_disp_aarch64(src[i], DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
            opnd_create_immed_uint(0x01, OPSZ_1), opnd_create_immed_uint(0x10, OPSZ_PTR),
            OPND_CREATE_SINGLE());
        test_instr_encoding(dc, OP_ld4, instr);
    }

    const int opsz[] = { OPSZ_4, OPSZ_8, OPSZ_16, OPSZ_32 };
    const opnd_t elsz[] = { OPND_CREATE_BYTE(), OPND_CREATE_HALF(), OPND_CREATE_SINGLE(),
                            OPND_CREATE_DOUBLE() };

    const int imm[] = { 4, 8, 16, 32 };
    for (int i = 0; i < 4; i++) {
        instr_t *instr =
            INSTR_CREATE_ld4_2(dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
                               opnd_create_reg(DR_REG_Q2), opnd_create_reg(DR_REG_Q3),
                               opnd_create_reg(DR_REG_X0),
                               opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0,
                                                             false, 0, 0, opsz[i]),
                               opnd_create_immed_uint(0x01, OPSZ_1),
                               opnd_create_immed_uint(imm[i], OPSZ_PTR), elsz[i]);
        test_instr_encoding(dc, OP_ld4, instr);
    }

    const int offset_reg[] = { DR_REG_X1, DR_REG_X16, DR_REG_X29 };
    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld4_2(
            dc, opnd_create_reg(dst_0[i]), opnd_create_reg(dst_1[i]),
            opnd_create_reg(dst_2[i]), opnd_create_reg(dst_3[i]), opnd_create_reg(src[i]),
            opnd_create_base_disp_aarch64(src[i], DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
            opnd_create_immed_uint(0x01, OPSZ_1), opnd_create_reg(offset_reg[i]),
            OPND_CREATE_HALF());
        test_instr_encoding(dc, OP_ld4, instr);
    }

    print("ld4 simdfp single structure post-index complete\n");
}

static void
ld4(void *dc)
{
    ld4_simdfp_multiple_structures_no_offset(dc);
    ld4_simdfp_multiple_structures_post_index(dc);
    ld4_simdfp_single_structure_no_offset(dc);
    ld4_simdfp_single_structure_post_index(dc);

    print("ld4 complete\n");
}

static void
ld2r_simdfp_no_offset(void *dc)
{
    int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q29 };
    int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q30 };
    int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X16 };

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld2r(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_8),
            OPND_CREATE_SINGLE());
        test_instr_encoding(dc, OP_ld2r, instr);
    }
    print("ld2r simdfp no offset complete\n");
}

static void
ld2r_simdfp_post_index(void *dc)
{
    int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q29 };
    int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q30 };
    int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X29 };
    int offset_reg[] = { DR_REG_X1, DR_REG_X16, DR_REG_X30 };

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld2r_2(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(src_reg[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_8),
            opnd_create_reg(offset_reg[i]), OPND_CREATE_SINGLE());
        test_instr_encoding(dc, OP_ld2r, instr);
    }

    int opsz[] = { OPSZ_2, OPSZ_4, OPSZ_8, OPSZ_16 };
    int imm[] = { 2, 4, 8, 16 };
    const opnd_t elsz[] = { OPND_CREATE_BYTE(), OPND_CREATE_HALF(), OPND_CREATE_SINGLE(),
                            OPND_CREATE_DOUBLE() };

    for (int i = 0; i < 4; i++) {
        instr_t *instr = INSTR_CREATE_ld2r_2(
            dc, opnd_create_reg(dst_reg_0[0]), opnd_create_reg(dst_reg_1[0]),
            opnd_create_reg(src_reg[0]),
            opnd_create_base_disp_aarch64(src_reg[0], DR_REG_NULL, 0, false, 0, 0,
                                          opsz[i]),
            opnd_create_immed_uint(imm[i], OPSZ_1), elsz[i]);
        test_instr_encoding(dc, OP_ld2r, instr);
    }
    print("ld2r simdfp post-index complete\n");
}

static void
ld2r(void *dc)
{
    ld2r_simdfp_no_offset(dc);
    ld2r_simdfp_post_index(dc);

    print("ld2r complete\n");
}

static void
ld3r_simdfp_no_offset(void *dc)
{
    int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q28 };
    int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q29 };
    int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q30 };
    int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld3r(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(dst_reg_2[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_24),
            OPND_CREATE_DOUBLE());
        test_instr_encoding(dc, OP_ld3r, instr);
    }
    print("ld3r simdfp no offset complete\n");
}

static void
ld3r_simdfp_post_index(void *dc)
{
    int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q28 };
    int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q29 };
    int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q30 };
    int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X29 };
    int offset_reg[] = { DR_REG_X1, DR_REG_X16, DR_REG_X30 };

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld3r_2(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(dst_reg_2[i]), opnd_create_reg(src_reg[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_24),
            opnd_create_reg(offset_reg[i]), OPND_CREATE_DOUBLE());
        test_instr_encoding(dc, OP_ld3r, instr);
    }

    int opsz[] = { OPSZ_3, OPSZ_6, OPSZ_12, OPSZ_24 };
    int imm[] = { 3, 6, 12, 24 };
    const opnd_t elsz[] = { OPND_CREATE_BYTE(), OPND_CREATE_HALF(), OPND_CREATE_SINGLE(),
                            OPND_CREATE_DOUBLE() };

    for (int i = 0; i < 4; i++) {
        instr_t *instr = INSTR_CREATE_ld3r_2(
            dc, opnd_create_reg(dst_reg_0[0]), opnd_create_reg(dst_reg_1[0]),
            opnd_create_reg(dst_reg_2[0]), opnd_create_reg(src_reg[0]),
            opnd_create_base_disp_aarch64(src_reg[0], DR_REG_NULL, 0, false, 0, 0,
                                          opsz[i]),
            opnd_create_immed_uint(imm[i], OPSZ_1), elsz[i]);
        test_instr_encoding(dc, OP_ld3r, instr);
    }
    print("ld3r simdfp post-index complete\n");
}

static void
ld3r(void *dc)
{
    ld3r_simdfp_no_offset(dc);
    ld3r_simdfp_post_index(dc);

    print("ld3r complete\n");
}

static void
ld4r_simdfp_no_offset(void *dc)
{
    int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q27 };
    int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q28 };
    int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q29 };
    int dst_reg_3[] = { DR_REG_Q3, DR_REG_Q18, DR_REG_Q30 };
    int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X30 };

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld4r(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(dst_reg_2[i]), opnd_create_reg(dst_reg_3[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_32),
            OPND_CREATE_DOUBLE());
        test_instr_encoding(dc, OP_ld4r, instr);
    }
    print("ld4r simdfp no offset complete\n");
}

static void
ld4r_simdfp_post_index(void *dc)
{
    int dst_reg_0[] = { DR_REG_Q0, DR_REG_Q15, DR_REG_Q27 };
    int dst_reg_1[] = { DR_REG_Q1, DR_REG_Q16, DR_REG_Q28 };
    int dst_reg_2[] = { DR_REG_Q2, DR_REG_Q17, DR_REG_Q29 };
    int dst_reg_3[] = { DR_REG_Q3, DR_REG_Q18, DR_REG_Q30 };
    int src_reg[] = { DR_REG_X0, DR_REG_X15, DR_REG_X29 };
    int offset_reg[] = { DR_REG_X1, DR_REG_X16, DR_REG_X30 };

    for (int i = 0; i < 3; i++) {
        instr_t *instr = INSTR_CREATE_ld4r_2(
            dc, opnd_create_reg(dst_reg_0[i]), opnd_create_reg(dst_reg_1[i]),
            opnd_create_reg(dst_reg_2[i]), opnd_create_reg(dst_reg_3[i]),
            opnd_create_reg(src_reg[i]),
            opnd_create_base_disp_aarch64(src_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                          OPSZ_32),
            opnd_create_reg(offset_reg[i]), OPND_CREATE_DOUBLE());
        test_instr_encoding(dc, OP_ld4r, instr);
    }

    int opsz[] = { OPSZ_4, OPSZ_8, OPSZ_16, OPSZ_32 };
    int imm[] = { 4, 8, 16, 32 };
    const opnd_t elsz[] = { OPND_CREATE_BYTE(), OPND_CREATE_HALF(), OPND_CREATE_SINGLE(),
                            OPND_CREATE_DOUBLE() };
    for (int i = 0; i < 4; i++) {
        instr_t *instr = INSTR_CREATE_ld4r_2(
            dc, opnd_create_reg(dst_reg_0[0]), opnd_create_reg(dst_reg_1[0]),
            opnd_create_reg(dst_reg_2[0]), opnd_create_reg(dst_reg_3[0]),
            opnd_create_reg(src_reg[0]),
            opnd_create_base_disp_aarch64(src_reg[0], DR_REG_NULL, 0, false, 0, 0,
                                          opsz[i]),
            opnd_create_immed_uint(imm[i], OPSZ_1), elsz[i]);
        test_instr_encoding(dc, OP_ld4r, instr);
    }
    print("ld4r simdfp post index complete\n");
}

static void
ld4r(void *dc)
{
    ld4r_simdfp_no_offset(dc);
    ld4r_simdfp_post_index(dc);

    print("ld4r complete\n");
}

static void
test_ldur_stur(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* LDUR <Bt>, [<Xn|SP>{, #<simm>}] */

    /* LDUR B0, X0 */
    instr =
        INSTR_CREATE_ldur(dc, opnd_create_reg(DR_REG_B0),
                          opnd_create_base_disp(DR_REG_X0, DR_REG_NULL, 0, 0, OPSZ_1));
    test_instr_encoding(dc, OP_ldur, instr);

    /* LDUR B1, X1, 255 */
    instr =
        INSTR_CREATE_ldur(dc, opnd_create_reg(DR_REG_B1),
                          opnd_create_base_disp(DR_REG_X1, DR_REG_NULL, 0, 255, OPSZ_1));
    test_instr_encoding(dc, OP_ldur, instr);

    /* LDUR H2, X2 */
    instr =
        INSTR_CREATE_ldur(dc, opnd_create_reg(DR_REG_H2),
                          opnd_create_base_disp(DR_REG_X2, DR_REG_NULL, 0, 0, OPSZ_2));
    test_instr_encoding(dc, OP_ldur, instr);

    /* LDUR H3, X3, -256 */
    instr =
        INSTR_CREATE_ldur(dc, opnd_create_reg(DR_REG_H3),
                          opnd_create_base_disp(DR_REG_X3, DR_REG_NULL, 0, -256, OPSZ_2));
    test_instr_encoding(dc, OP_ldur, instr);

    /* LDUR S4, X4 */
    instr =
        INSTR_CREATE_ldur(dc, opnd_create_reg(DR_REG_S4),
                          opnd_create_base_disp(DR_REG_X4, DR_REG_NULL, 0, 0, OPSZ_4));
    test_instr_encoding(dc, OP_ldur, instr);

    /* LDUR S5, X5, -256 */
    instr =
        INSTR_CREATE_ldur(dc, opnd_create_reg(DR_REG_S5),
                          opnd_create_base_disp(DR_REG_X5, DR_REG_NULL, 0, -256, OPSZ_4));
    test_instr_encoding(dc, OP_ldur, instr);

    /* LDUR D6, X6 */
    instr =
        INSTR_CREATE_ldur(dc, opnd_create_reg(DR_REG_D6),
                          opnd_create_base_disp(DR_REG_X6, DR_REG_NULL, 0, 0, OPSZ_8));
    test_instr_encoding(dc, OP_ldur, instr);

    /* LDUR D7, X7, -256 */
    instr =
        INSTR_CREATE_ldur(dc, opnd_create_reg(DR_REG_D7),
                          opnd_create_base_disp(DR_REG_X7, DR_REG_NULL, 0, -256, OPSZ_8));
    test_instr_encoding(dc, OP_ldur, instr);

    /* LDUR Q8, X8 */
    instr =
        INSTR_CREATE_ldur(dc, opnd_create_reg(DR_REG_Q8),
                          opnd_create_base_disp(DR_REG_X8, DR_REG_NULL, 0, 0, OPSZ_16));
    test_instr_encoding(dc, OP_ldur, instr);

    /* LDUR Q9, X9, -256 */
    instr = INSTR_CREATE_ldur(
        dc, opnd_create_reg(DR_REG_Q9),
        opnd_create_base_disp(DR_REG_X9, DR_REG_NULL, 0, -256, OPSZ_16));
    test_instr_encoding(dc, OP_ldur, instr);

    /* STUR <Bt>, [<Xn|SP>{, #<simm>}] */

    /* STUR B10, X10 */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X10, DR_REG_NULL, 0, 0, OPSZ_1),
        opnd_create_reg(DR_REG_B10));
    test_instr_encoding(dc, OP_stur, instr);

    /* STUR B11, X11, 0xFF */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X11, DR_REG_NULL, 0, 0xFF, OPSZ_1),
        opnd_create_reg(DR_REG_B11));
    test_instr_encoding(dc, OP_stur, instr);

    /* STUR H12, X12 */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X12, DR_REG_NULL, 0, 0, OPSZ_2),
        opnd_create_reg(DR_REG_H12));
    test_instr_encoding(dc, OP_stur, instr);

    /* STUR H13, X13, 0xFF */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X13, DR_REG_NULL, 0, 0xFF, OPSZ_2),
        opnd_create_reg(DR_REG_H13));
    test_instr_encoding(dc, OP_stur, instr);

    /* STUR S14, X14 */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X14, DR_REG_NULL, 0, 0, OPSZ_4),
        opnd_create_reg(DR_REG_S14));
    test_instr_encoding(dc, OP_stur, instr);

    /* STUR S15, X15, 0xFF */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X15, DR_REG_NULL, 0, 0xFF, OPSZ_4),
        opnd_create_reg(DR_REG_S15));
    test_instr_encoding(dc, OP_stur, instr);

    /* STUR D16, X16 */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X16, DR_REG_NULL, 0, 0, OPSZ_8),
        opnd_create_reg(DR_REG_D16));
    test_instr_encoding(dc, OP_stur, instr);

    /* STUR D17, X17, 0xFF */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X17, DR_REG_NULL, 0, 0xFF, OPSZ_8),
        opnd_create_reg(DR_REG_D17));
    test_instr_encoding(dc, OP_stur, instr);

    /* STUR Q18, X18 */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X18, DR_REG_NULL, 0, 0, OPSZ_16),
        opnd_create_reg(DR_REG_Q18));
    test_instr_encoding(dc, OP_stur, instr);

    /* STUR Q19, X19, 0xFF */
    instr = INSTR_CREATE_stur(
        dc, opnd_create_base_disp(DR_REG_X19, DR_REG_NULL, 0, 0xFF, OPSZ_16),
        opnd_create_reg(DR_REG_Q19));
    test_instr_encoding(dc, OP_stur, instr);
}

static void
test_instrs_with_logic_imm(void *dc)
{
    byte *pc;
    instr_t *instr;

    instr = INSTR_CREATE_and(dc, opnd_create_reg(DR_REG_X10), opnd_create_reg(DR_REG_X9),
                             OPND_CREATE_INT(0xFFFF));
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_and(dc, opnd_create_reg(DR_REG_W5), opnd_create_reg(DR_REG_W5),
                             OPND_CREATE_INT(0xFF));
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_ands(dc, opnd_create_reg(DR_REG_X23),
                              opnd_create_reg(DR_REG_X19), OPND_CREATE_INT(0xFFFFFF));
    test_instr_encoding(dc, OP_ands, instr);

    instr = INSTR_CREATE_ands(dc, opnd_create_reg(DR_REG_W3), opnd_create_reg(DR_REG_W8),
                              OPND_CREATE_INT(0xF));
    test_instr_encoding(dc, OP_ands, instr);
}

static void
ldr_base_immediate_post_index(void *dc)
{
    int reg_32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg_64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int reg_dest[] = { DR_REG_X1, DR_REG_X17, DR_REG_X30 };
    int value[] = { 129, 255, -256, 170, 85, -86, -171 };

    for (int i = 0; i < 3; i++) {
        for (int l = 0; l < 7; l++) {
            instr_t *instr = INSTR_CREATE_ldr_imm(
                dc, opnd_create_reg(reg_32[i]), opnd_create_reg(reg_dest[i]),
                opnd_create_base_disp_aarch64(reg_dest[i], DR_REG_NULL, 0, false, 0, 0,
                                              OPSZ_4),
                OPND_CREATE_INT(value[l]));
            test_instr_encoding(dc, OP_ldr, instr);

            instr = INSTR_CREATE_ldr_imm(
                dc, opnd_create_reg(reg_64[i]), opnd_create_reg(reg_dest[i]),
                opnd_create_base_disp_aarch64(reg_dest[i], DR_REG_NULL, 0, false, 0, 0,
                                              OPSZ_8),
                OPND_CREATE_INT(value[l]));
            test_instr_encoding(dc, OP_ldr, instr);
        }
    }
    print("ldr base immediate post-index complete\n");
}

static void
ldr_base_immediate_pre_index(void *dc)
{
    int reg_32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg_64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int reg_dst[] = { DR_REG_X1, DR_REG_X17, DR_REG_X30 };
    int value[] = { 129, 255, -256, 170, 85, -86, -171 };

    for (int i = 0; i < 3; i++) {
        for (int l = 0; l < 7; l++) {
            instr_t *instr = INSTR_CREATE_ldr_imm(
                dc, opnd_create_reg(reg_32[i]), opnd_create_reg(reg_dst[i]),
                opnd_create_base_disp_aarch64(reg_dst[i], DR_REG_NULL, 0, false, value[l],
                                              0, OPSZ_4),
                OPND_CREATE_INT(value[l]));
            test_instr_encoding(dc, OP_ldr, instr);

            instr = INSTR_CREATE_ldr_imm(
                dc, opnd_create_reg(reg_64[i]), opnd_create_reg(reg_dst[i]),
                opnd_create_base_disp_aarch64(reg_dst[i], DR_REG_NULL, 0, false, value[l],
                                              0, OPSZ_8),
                OPND_CREATE_INT(value[l]));
            test_instr_encoding(dc, OP_ldr, instr);
        }
    }
    print("ldr base immediate pre-index complete\n");
}

static void
ldr_base_immediate_offset(void *dc)
{
    int reg_32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg_64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int reg_dst[] = { DR_REG_X1, DR_REG_X17, DR_REG_X30 };
    int value[] = { 0, 16380, 0b101010101010, 0b010101010101 };

    for (int i = 0; i < 3; i++) {
        for (int l = 0; l < 4; l++) {
            instr_t *instr = INSTR_CREATE_ldr(
                dc, opnd_create_reg(reg_32[i]),
                opnd_create_base_disp_aarch64(reg_dst[i], DR_REG_NULL, 0, false,
                                              value[l] & 0b111111111100, 0, OPSZ_4));
            test_instr_encoding(dc, OP_ldr, instr);

            instr = INSTR_CREATE_ldr(
                dc, opnd_create_reg(reg_64[i]),
                opnd_create_base_disp_aarch64(reg_dst[i], DR_REG_NULL, 0, false,
                                              value[l] & 0b111111111000, 0, OPSZ_8));
            test_instr_encoding(dc, OP_ldr, instr);
        }
    }
    print("ldr base immediate offset complete\n");
}

static void
ldr_base_literal(void *dc)
{
    int reg_32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg_64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int64_t value[] = { 0x0000000000000000, 0x000000000007ffff };
    for (int i = 0; i < 3; i++) {
        for (int l = 0; l < 2; l++) {
            instr_t *instr =
                INSTR_CREATE_ldr(dc, opnd_create_reg(reg_32[i]),
                                 OPND_CREATE_ABSMEM((void *)value[i], OPSZ_4));
            test_instr_encoding(dc, OP_ldr, instr);

            instr = INSTR_CREATE_ldr(dc, opnd_create_reg(reg_64[i]),
                                     OPND_CREATE_ABSMEM((void *)value[i], OPSZ_8));
            test_instr_encoding(dc, OP_ldr, instr);
        }
    }
    print("ldr base literal complete\n");
}

static void
ldr_base_register(void *dc)
{
    int extend[] = { DR_EXTEND_UXTW, DR_EXTEND_UXTX, DR_EXTEND_SXTW, DR_EXTEND_SXTX };
    int reg32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int dest_0[] = { DR_REG_X1, DR_REG_X17, DR_REG_X29 };
    int dest_1[] = { DR_REG_W2, DR_REG_X18, DR_REG_W28, DR_REG_X27 };
    for (int i = 0; i < 4; i++) {
        for (int ii = 0; ii < 3; ii++) {
            instr_t *instr = INSTR_CREATE_ldr(
                dc, opnd_create_reg(reg32[ii]),
                opnd_create_base_disp_aarch64(dest_0[ii], dest_1[i], extend[i], false, 0,
                                              0, OPSZ_4));
            test_instr_encoding(dc, OP_ldr, instr);

            instr = INSTR_CREATE_ldr(dc, opnd_create_reg(reg64[ii]),
                                     opnd_create_base_disp_aarch64(dest_0[ii], dest_1[i],
                                                                   extend[i], false, 0, 0,
                                                                   OPSZ_8));
            test_instr_encoding(dc, OP_ldr, instr);
        }
    }
    print("ldr base register complete\n");
}

static void
ldr_base_register_extend(void *dc)
{
    int extend[] = { DR_EXTEND_UXTW, DR_EXTEND_UXTX, DR_EXTEND_SXTW, DR_EXTEND_SXTX };
    int reg32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int dest_0[] = { DR_REG_X1, DR_REG_X17, DR_REG_X29 };
    int dest_1[] = { DR_REG_W2, DR_REG_X18, DR_REG_W28, DR_REG_X27 };
    for (int i = 0; i < 4; i++) {
        for (int ii = 0; ii < 3; ii++) {
            opnd_t opnd = opnd_create_base_disp_aarch64(
                dest_0[ii], dest_1[i], extend[i], false, 0, DR_OPND_SHIFTED, OPSZ_4);
            opnd_set_index_extend(&opnd, extend[i], 2);
            instr_t *instr = INSTR_CREATE_ldr(dc, opnd_create_reg(reg32[ii]), opnd);
            test_instr_encoding(dc, OP_ldr, instr);

            opnd = opnd_create_base_disp_aarch64(dest_0[ii], dest_1[i], extend[i], false,
                                                 0, DR_OPND_SHIFTED, OPSZ_8);
            opnd_set_index_extend(&opnd, extend[i], 3);
            instr = INSTR_CREATE_ldr(dc, opnd_create_reg(reg64[ii]), opnd);
            test_instr_encoding(dc, OP_ldr, instr);
        }
    }
    print("ldr base register extend complete\n");
}

static void
ldr(void *dc)
{
    ldr_base_immediate_post_index(dc);
    ldr_base_immediate_pre_index(dc);
    ldr_base_immediate_offset(dc);
#if 0 /* TODO i#4847: address memory touching instructions that fail to encode */
        ldr_base_literal(dc);
#endif
    ldr_base_register(dc);
    ldr_base_register_extend(dc);

    print("ldr complete\n");
}

static void
str_base_immediate_post_index(void *dc)
{
    int reg_32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W29 };
    int reg_64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X29 };
    int dest_reg[] = { DR_REG_X0, DR_REG_X16, DR_REG_X29 };
    int value[] = { 0, 129, 255, -256, 170, 85, -86, -171 };
    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 8; ii++) {
            instr_t *instr = INSTR_CREATE_str_imm(
                dc,
                opnd_create_base_disp_aarch64(dest_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                              OPSZ_4),
                opnd_create_reg(reg_32[i]), opnd_create_reg(dest_reg[i]),
                OPND_CREATE_INT(value[ii]));
            test_instr_encoding(dc, OP_str, instr);

            instr = INSTR_CREATE_str_imm(
                dc,
                opnd_create_base_disp_aarch64(dest_reg[i], DR_REG_NULL, 0, false, 0, 0,
                                              OPSZ_8),
                opnd_create_reg(reg_64[i]), opnd_create_reg(dest_reg[i]),
                OPND_CREATE_INT(value[ii]));
            test_instr_encoding(dc, OP_str, instr);
        }
    }
    print("str base immediate post-index complete\n");
}

static void
str_base_immediate_pre_index(void *dc)
{
    int reg_32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg_64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int dest_reg[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int value[] = { 0, 129, 255, -256, 170, 85, -86, -171 };
    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 8; ii++) {
            instr_t *instr = INSTR_CREATE_str_imm(
                dc,
                opnd_create_base_disp_aarch64(dest_reg[i], DR_REG_NULL, 0, false,
                                              value[ii], 0, OPSZ_4),
                opnd_create_reg(reg_32[i]), opnd_create_reg(dest_reg[i]),
                OPND_CREATE_INT(value[ii]));
            test_instr_encoding(dc, OP_str, instr);

            instr = INSTR_CREATE_str_imm(
                dc,
                opnd_create_base_disp_aarch64(dest_reg[i], DR_REG_NULL, 0, false,
                                              value[ii], 0, OPSZ_8),
                opnd_create_reg(reg_64[i]), opnd_create_reg(dest_reg[i]),
                OPND_CREATE_INT(value[ii]));
            test_instr_encoding(dc, OP_str, instr);
        }
    }
    print("str base immediate pre-index complete\n");
}

static void
str_base_immediate_unsigned_offset(void *dc)
{
    int reg_32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg_64[] = {
        DR_REG_X0,
        DR_REG_X16,
        DR_REG_X30,
    };
    int reg_dest[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int val_32[] = { 0, 0x204, 0b111111111100, 0b101010101100, 0b010101010100 };
    int val_64[] = { 0, 0x1020, 0b1111111111000, 0b101010101000, 0b010101011000 };

    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 4; ii++) {
            instr_t *instr = INSTR_CREATE_str(
                dc,
                opnd_create_base_disp_aarch64(reg_dest[i], DR_REG_NULL, 0, false,
                                              val_32[ii], 0, OPSZ_4),
                opnd_create_reg(reg_32[i]));
            test_instr_encoding(dc, OP_str, instr);

            instr = INSTR_CREATE_str(dc,
                                     opnd_create_base_disp_aarch64(reg_dest[i],
                                                                   DR_REG_NULL, 0, false,
                                                                   val_64[ii], 0, OPSZ_8),
                                     opnd_create_reg(reg_64[i]));
            test_instr_encoding(dc, OP_str, instr);
        }
    }
    print("str base immediate unsigned offset complete\n");
}

static void
str_base_register(void *dc)
{
    int reg_32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg_64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int reg_dest_1[] = { DR_REG_X0, DR_REG_X15, DR_REG_X29 };
    int reg_dest_2[] = { DR_REG_W1, DR_REG_X16, DR_REG_W30, DR_REG_X28 };
    int extend[] = { DR_EXTEND_UXTW, DR_EXTEND_UXTX, DR_EXTEND_SXTW, DR_EXTEND_SXTX };

    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 4; ii++) {
            instr_t *instr = INSTR_CREATE_str(
                dc,
                opnd_create_base_disp_aarch64(reg_dest_1[i], reg_dest_2[ii], extend[ii],
                                              false, 0, 0, OPSZ_4),
                opnd_create_reg(reg_32[i]));
            test_instr_encoding(dc, OP_str, instr);

            instr = INSTR_CREATE_str(
                dc,
                opnd_create_base_disp_aarch64(reg_dest_1[i], reg_dest_2[ii], extend[ii],
                                              false, 0, 0, OPSZ_8),
                opnd_create_reg(reg_64[i]));
            test_instr_encoding(dc, OP_str, instr);
        }
    }
    print("str base register complete\n");
}

static void
str_base_register_extend(void *dc)
{
    int reg_32[] = { DR_REG_W0, DR_REG_W16, DR_REG_W30 };
    int reg_64[] = { DR_REG_X0, DR_REG_X16, DR_REG_X30 };
    int reg_dest_1[] = { DR_REG_X0, DR_REG_X15, DR_REG_X29 };
    int reg_dest_2[] = { DR_REG_W1, DR_REG_X16, DR_REG_W30, DR_REG_X28 };
    int extend[] = { DR_EXTEND_UXTW, DR_EXTEND_UXTX, DR_EXTEND_SXTW, DR_EXTEND_SXTX };

    for (int i = 0; i < 3; i++) {
        for (int ii = 0; ii < 4; ii++) {
            opnd_t opnd =
                opnd_create_base_disp_aarch64(reg_dest_1[i], reg_dest_2[ii], extend[ii],
                                              false, 0, DR_OPND_SHIFTED, OPSZ_4);
            opnd_set_index_extend(&opnd, extend[ii], 2);
            instr_t *instr = INSTR_CREATE_str(dc, opnd, opnd_create_reg(reg_32[i]));
            test_instr_encoding(dc, OP_str, instr);

            opnd =
                opnd_create_base_disp_aarch64(reg_dest_1[i], reg_dest_2[ii], extend[ii],
                                              false, 0, DR_OPND_SHIFTED, OPSZ_8);
            opnd_set_index_extend(&opnd, extend[ii], 3);
            instr = INSTR_CREATE_str(dc, opnd, opnd_create_reg(reg_64[i]));
            test_instr_encoding(dc, OP_str, instr);
        }
    }

    print("str base register extend complete\n");
}

static void
str(void *dc)
{
    str_base_immediate_post_index(dc);
    str_base_immediate_pre_index(dc);
    str_base_immediate_unsigned_offset(dc);
    str_base_register(dc);
    str_base_register_extend(dc);

    print("str complete\n");
}

static void
test_fmov_general(void *dc)
{
    byte *pc;
    instr_t *instr;
    instr = INSTR_CREATE_fmov_general(dc, opnd_create_reg(DR_REG_H10),
                                      opnd_create_reg(DR_REG_W9));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_general(dc, opnd_create_reg(DR_REG_S14),
                                      opnd_create_reg(DR_REG_W4));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_general(dc, opnd_create_reg(DR_REG_H23),
                                      opnd_create_reg(DR_REG_X8));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_general(dc, opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_X24));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_upper_vec(dc, opnd_create_reg(DR_REG_Q9),
                                        opnd_create_reg(DR_REG_X10));
    test_instr_encoding(dc, OP_fmov, instr);
}

static void
test_fmov_vector(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* FMOV <Vd>.<T>, #<imm> (v8.2 half-precision)
     * 16 bit floating-point values encoded in instruction's 8 bit field, so
     * there is a fixed, limited set of floating-point values which can be set,
     * see 'Table C2-2 Floating-point constant values' in section 'Modified
     * immediate constants in A64 floating-point instructions.' of the Arm
     * Reference Manual.
     */
    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q1),
                                     opnd_create_immed_float(1.0f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q1),
                                     opnd_create_immed_float(2.0f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_immed_float(-1.0f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q3),
                                     opnd_create_immed_float(-2.0f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q4),
                                     opnd_create_immed_float(3.5f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q5),
                                     opnd_create_immed_float(4.25f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q6),
                                     opnd_create_immed_float(1.125f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q7),
                                     opnd_create_immed_float(-0.25f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q8),
                                     opnd_create_immed_float(7.0f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q9),
                                         opnd_create_immed_float(1.9375f),
                                         OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q10),
                                         opnd_create_immed_float(0.2109375f),
                                         OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);

    instr =
        INSTR_CREATE_fmov_vector_imm(dc, opnd_create_reg(DR_REG_Q31),
                                     opnd_create_immed_float(31.0f), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmov, instr);
}

static void
test_fmov_scalar(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* FMOV <Sd>, #<imm> (32 bit scalar register) */
    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(1.0f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(-1.0f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(2.0f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(-2.0f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(3.5f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(4.25f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(1.125f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(-1.125f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(0.25f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(-0.25f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(1.9375f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(0.2109375f));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_S0),
                                         opnd_create_immed_float(31.0f));
    test_instr_encoding(dc, OP_fmov, instr);

    /* FMOV <Dd>, #<imm> (64 bit scalar register) */
    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(1.0));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(-1.0));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(2.0));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(-2.0));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(3.5));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(4.25));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(1.125));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(-1.125));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(0.25));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(-0.25));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(1.9375));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(0.2109375));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar_imm(dc, opnd_create_reg(DR_REG_D0),
                                         opnd_create_immed_double(31.0));
    test_instr_encoding(dc, OP_fmov, instr);
}

static void
test_asimdsamefp16(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* Advanced SIMD three same (FP16) */

    instr = INSTR_CREATE_fmaxnm_vector(dc, opnd_create_reg(DR_REG_D2),
                                       opnd_create_reg(DR_REG_D27),
                                       opnd_create_reg(DR_REG_D30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmaxnm_vector(dc, opnd_create_reg(DR_REG_Q2),
                                       opnd_create_reg(DR_REG_Q27),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmla_vector(dc, opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D29),
                                     opnd_create_reg(DR_REG_D31), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fmla_vector(dc, opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q29),
                                     opnd_create_reg(DR_REG_Q31), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fadd_vector(dc, opnd_create_reg(DR_REG_D17),
                                     opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fadd_vector(dc, opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fmulx_vector(dc, opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D20),
                                      opnd_create_reg(DR_REG_D4), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fmulx_vector(dc, opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q20),
                                      opnd_create_reg(DR_REG_Q4), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc, opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc, opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fmax_vector(dc, opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D8),
                                     opnd_create_reg(DR_REG_D22), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_fmax_vector(dc, opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8),
                                     opnd_create_reg(DR_REG_Q22), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_frecps_vector(dc, opnd_create_reg(DR_REG_D24),
                                       opnd_create_reg(DR_REG_D26),
                                       opnd_create_reg(DR_REG_D18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_frecps, instr);

    instr = INSTR_CREATE_frecps_vector(dc, opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q26),
                                       opnd_create_reg(DR_REG_Q18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_frecps, instr);

    instr = INSTR_CREATE_fminnm_vector(dc, opnd_create_reg(DR_REG_D16),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D11), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fminnm_vector(dc, opnd_create_reg(DR_REG_Q16),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q11), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fmls_vector(dc, opnd_create_reg(DR_REG_D19),
                                     opnd_create_reg(DR_REG_D8),
                                     opnd_create_reg(DR_REG_D29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fmls_vector(dc, opnd_create_reg(DR_REG_Q19),
                                     opnd_create_reg(DR_REG_Q8),
                                     opnd_create_reg(DR_REG_Q29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fsub_vector(dc, opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D24), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fsub_vector(dc, opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q24), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fmin_vector(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_fmin_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc, opnd_create_reg(DR_REG_D8),
                                        opnd_create_reg(DR_REG_D12),
                                        opnd_create_reg(DR_REG_D19), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc, opnd_create_reg(DR_REG_Q8),
                                        opnd_create_reg(DR_REG_Q12),
                                        opnd_create_reg(DR_REG_Q19), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(dc, opnd_create_reg(DR_REG_D23),
                                        opnd_create_reg(DR_REG_D15),
                                        opnd_create_reg(DR_REG_D20), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(dc, opnd_create_reg(DR_REG_Q23),
                                        opnd_create_reg(DR_REG_Q15),
                                        opnd_create_reg(DR_REG_Q20), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_faddp_vector(dc, opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_faddp_vector(dc, opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_D4),
                                     opnd_create_reg(DR_REG_D20),
                                     opnd_create_reg(DR_REG_D10), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_Q4),
                                     opnd_create_reg(DR_REG_Q20),
                                     opnd_create_reg(DR_REG_Q10), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fcmge_vector(dc, opnd_create_reg(DR_REG_D14),
                                      opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_fcmge_vector(dc, opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_facge_vector(dc, opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D31), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_facge_vector(dc, opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q31), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc, opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D5), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc, opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q5), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fdiv_vector(dc, opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fdiv_vector(dc, opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fminnmp_vector(dc, opnd_create_reg(DR_REG_D9),
                                        opnd_create_reg(DR_REG_D7),
                                        opnd_create_reg(DR_REG_D6), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fminnmp_vector(dc, opnd_create_reg(DR_REG_Q9),
                                        opnd_create_reg(DR_REG_Q7),
                                        opnd_create_reg(DR_REG_Q6), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fabd_vector(dc, opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D12), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fabd_vector(dc, opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q12), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc, opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D26), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc, opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q26), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc, opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D17), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc, opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q17), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_fminp_vector(dc, opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D7), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminp, instr);

    instr = INSTR_CREATE_fminp_vector(dc, opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q7), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminp, instr);
}

static void
test_asimdsame(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* Advanced SIMD three same */

    instr = INSTR_CREATE_shadd_vector(dc, opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D30), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc, opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q30), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc, opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc, opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc, opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc, opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D29), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_Q29), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_Q29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_Q29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_Q29), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc, opnd_create_reg(DR_REG_D31),
                                       opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D10), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc, opnd_create_reg(DR_REG_Q31),
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q10), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc, opnd_create_reg(DR_REG_D31),
                                       opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D10), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc, opnd_create_reg(DR_REG_Q31),
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q10), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc, opnd_create_reg(DR_REG_D31),
                                       opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D10), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc, opnd_create_reg(DR_REG_Q31),
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q10), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_shsub_vector(dc, opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D20), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc, opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q20), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc, opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D20), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc, opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q20), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc, opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc, opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_cmgt_vector(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D8), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D8), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D8), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmge_vector(dc, opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D26), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc, opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q26), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc, opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D26), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc, opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q26), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc, opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D26), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc, opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q26), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc, opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q26), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_sshl_vector(dc, opnd_create_reg(DR_REG_D18),
                                     opnd_create_reg(DR_REG_D16),
                                     opnd_create_reg(DR_REG_D29), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                     opnd_create_reg(DR_REG_Q16),
                                     opnd_create_reg(DR_REG_Q29), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc, opnd_create_reg(DR_REG_D18),
                                     opnd_create_reg(DR_REG_D16),
                                     opnd_create_reg(DR_REG_D29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                     opnd_create_reg(DR_REG_Q16),
                                     opnd_create_reg(DR_REG_Q29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc, opnd_create_reg(DR_REG_D18),
                                     opnd_create_reg(DR_REG_D16),
                                     opnd_create_reg(DR_REG_D29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                     opnd_create_reg(DR_REG_Q16),
                                     opnd_create_reg(DR_REG_Q29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                     opnd_create_reg(DR_REG_Q16),
                                     opnd_create_reg(DR_REG_Q29), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc, opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc, opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc, opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc, opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc, opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc, opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc, opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q23), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc, opnd_create_reg(DR_REG_D8),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D15), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc, opnd_create_reg(DR_REG_Q8),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q15), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc, opnd_create_reg(DR_REG_D8),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc, opnd_create_reg(DR_REG_Q8),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc, opnd_create_reg(DR_REG_D8),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D15), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc, opnd_create_reg(DR_REG_Q8),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q15), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc, opnd_create_reg(DR_REG_Q8),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q15), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc, opnd_create_reg(DR_REG_D28),
                                       opnd_create_reg(DR_REG_D24),
                                       opnd_create_reg(DR_REG_D2), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc, opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc, opnd_create_reg(DR_REG_D28),
                                       opnd_create_reg(DR_REG_D24),
                                       opnd_create_reg(DR_REG_D2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc, opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc, opnd_create_reg(DR_REG_D28),
                                       opnd_create_reg(DR_REG_D24),
                                       opnd_create_reg(DR_REG_D2), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc, opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc, opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_smax_vector(dc, opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D8), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc, opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q8), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc, opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D8), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc, opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q8), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc, opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D8), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc, opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q8), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smin_vector(dc, opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D19),
                                     opnd_create_reg(DR_REG_D23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc, opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q19),
                                     opnd_create_reg(DR_REG_Q23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc, opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D19),
                                     opnd_create_reg(DR_REG_D23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc, opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q19),
                                     opnd_create_reg(DR_REG_Q23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc, opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D19),
                                     opnd_create_reg(DR_REG_D23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc, opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q19),
                                     opnd_create_reg(DR_REG_Q23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_sabd_vector(dc, opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20),
                                     opnd_create_reg(DR_REG_D28), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc, opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     opnd_create_reg(DR_REG_Q28), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc, opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20),
                                     opnd_create_reg(DR_REG_D28), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc, opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     opnd_create_reg(DR_REG_Q28), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc, opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20),
                                     opnd_create_reg(DR_REG_D28), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc, opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     opnd_create_reg(DR_REG_Q28), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_saba_vector(dc, opnd_create_reg(DR_REG_D27),
                                     opnd_create_reg(DR_REG_D30),
                                     opnd_create_reg(DR_REG_D4), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc, opnd_create_reg(DR_REG_Q27),
                                     opnd_create_reg(DR_REG_Q30),
                                     opnd_create_reg(DR_REG_Q4), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc, opnd_create_reg(DR_REG_D27),
                                     opnd_create_reg(DR_REG_D30),
                                     opnd_create_reg(DR_REG_D4), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc, opnd_create_reg(DR_REG_Q27),
                                     opnd_create_reg(DR_REG_Q30),
                                     opnd_create_reg(DR_REG_Q4), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc, opnd_create_reg(DR_REG_D27),
                                     opnd_create_reg(DR_REG_D30),
                                     opnd_create_reg(DR_REG_D4), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc, opnd_create_reg(DR_REG_Q27),
                                     opnd_create_reg(DR_REG_Q30),
                                     opnd_create_reg(DR_REG_Q4), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_D20),
                                    opnd_create_reg(DR_REG_D10),
                                    opnd_create_reg(DR_REG_D14), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q10),
                                    opnd_create_reg(DR_REG_Q14), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_D20),
                                    opnd_create_reg(DR_REG_D10),
                                    opnd_create_reg(DR_REG_D14), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q10),
                                    opnd_create_reg(DR_REG_Q14), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_D20),
                                    opnd_create_reg(DR_REG_D10),
                                    opnd_create_reg(DR_REG_D14), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q10),
                                    opnd_create_reg(DR_REG_Q14), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q10),
                                    opnd_create_reg(DR_REG_Q14), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_cmtst_vector(dc, opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D2), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc, opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q2), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc, opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc, opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc, opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D2), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc, opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q2), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc, opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q2), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_mla_vector(dc, opnd_create_reg(DR_REG_D2),
                                    opnd_create_reg(DR_REG_D19),
                                    opnd_create_reg(DR_REG_D4), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc, opnd_create_reg(DR_REG_Q2),
                                    opnd_create_reg(DR_REG_Q19),
                                    opnd_create_reg(DR_REG_Q4), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc, opnd_create_reg(DR_REG_D2),
                                    opnd_create_reg(DR_REG_D19),
                                    opnd_create_reg(DR_REG_D4), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc, opnd_create_reg(DR_REG_Q2),
                                    opnd_create_reg(DR_REG_Q19),
                                    opnd_create_reg(DR_REG_Q4), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc, opnd_create_reg(DR_REG_D2),
                                    opnd_create_reg(DR_REG_D19),
                                    opnd_create_reg(DR_REG_D4), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc, opnd_create_reg(DR_REG_Q2),
                                    opnd_create_reg(DR_REG_Q19),
                                    opnd_create_reg(DR_REG_Q4), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mul_vector(dc, opnd_create_reg(DR_REG_D5),
                                    opnd_create_reg(DR_REG_D9),
                                    opnd_create_reg(DR_REG_D24), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc, opnd_create_reg(DR_REG_Q5),
                                    opnd_create_reg(DR_REG_Q9),
                                    opnd_create_reg(DR_REG_Q24), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc, opnd_create_reg(DR_REG_D5),
                                    opnd_create_reg(DR_REG_D9),
                                    opnd_create_reg(DR_REG_D24), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc, opnd_create_reg(DR_REG_Q5),
                                    opnd_create_reg(DR_REG_Q9),
                                    opnd_create_reg(DR_REG_Q24), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc, opnd_create_reg(DR_REG_D5),
                                    opnd_create_reg(DR_REG_D9),
                                    opnd_create_reg(DR_REG_D24), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc, opnd_create_reg(DR_REG_Q5),
                                    opnd_create_reg(DR_REG_Q9),
                                    opnd_create_reg(DR_REG_Q24), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_smaxp_vector(dc, opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D7), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc, opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q7), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc, opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D7), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc, opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q7), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc, opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D7), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc, opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q7), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_sminp_vector(dc, opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D10), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q10), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc, opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D10), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q10), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc, opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D10), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q10), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sqdmulh_vector(dc, opnd_create_reg(DR_REG_D12),
                                        opnd_create_reg(DR_REG_D22),
                                        opnd_create_reg(DR_REG_D27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmulh, instr);

    instr = INSTR_CREATE_sqdmulh_vector(dc, opnd_create_reg(DR_REG_Q12),
                                        opnd_create_reg(DR_REG_Q22),
                                        opnd_create_reg(DR_REG_Q27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmulh, instr);

    instr = INSTR_CREATE_sqdmulh_vector(
        dc, opnd_create_reg(DR_REG_D12), opnd_create_reg(DR_REG_D22),
        opnd_create_reg(DR_REG_D27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmulh, instr);

    instr = INSTR_CREATE_sqdmulh_vector(
        dc, opnd_create_reg(DR_REG_Q12), opnd_create_reg(DR_REG_Q22),
        opnd_create_reg(DR_REG_Q27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmulh, instr);

    instr = INSTR_CREATE_addp_vector(dc, opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D15), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc, opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q15), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc, opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc, opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc, opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D15), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc, opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q15), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc, opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q15), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_fmaxnm_vector(dc, opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D9),
                                       opnd_create_reg(DR_REG_D11), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmaxnm_vector(dc, opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q9),
                                       opnd_create_reg(DR_REG_Q11), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmaxnm_vector(dc, opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q9),
                                       opnd_create_reg(DR_REG_Q11), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmla_vector(dc, opnd_create_reg(DR_REG_D7),
                                     opnd_create_reg(DR_REG_D29),
                                     opnd_create_reg(DR_REG_D19), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fmla_vector(dc, opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q29),
                                     opnd_create_reg(DR_REG_Q19), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fmla_vector(dc, opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q29),
                                     opnd_create_reg(DR_REG_Q19), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fadd_vector(dc, opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D11),
                                     opnd_create_reg(DR_REG_D11), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fadd_vector(dc, opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q11),
                                     opnd_create_reg(DR_REG_Q11), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fadd_vector(dc, opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q11),
                                     opnd_create_reg(DR_REG_Q11), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fmulx_vector(dc, opnd_create_reg(DR_REG_D30),
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fmulx_vector(dc, opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fmulx_vector(dc, opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q20), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc, opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D14),
                                      opnd_create_reg(DR_REG_D0), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc, opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_Q0), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc, opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_Q0), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fmax_vector(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D21),
                                     opnd_create_reg(DR_REG_D20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_fmax_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q21),
                                     opnd_create_reg(DR_REG_Q20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_fmax_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q21),
                                     opnd_create_reg(DR_REG_Q20), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_frecps_vector(dc, opnd_create_reg(DR_REG_D15),
                                       opnd_create_reg(DR_REG_D5),
                                       opnd_create_reg(DR_REG_D16), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_frecps, instr);

    instr = INSTR_CREATE_frecps_vector(dc, opnd_create_reg(DR_REG_Q15),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q16), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_frecps, instr);

    instr = INSTR_CREATE_frecps_vector(dc, opnd_create_reg(DR_REG_Q15),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q16), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_frecps, instr);

    instr =
        INSTR_CREATE_and_vector(dc, opnd_create_reg(DR_REG_D28),
                                opnd_create_reg(DR_REG_D25), opnd_create_reg(DR_REG_D10));
    test_instr_encoding(dc, OP_and, instr);

    instr =
        INSTR_CREATE_and_vector(dc, opnd_create_reg(DR_REG_Q28),
                                opnd_create_reg(DR_REG_Q25), opnd_create_reg(DR_REG_Q10));
    test_instr_encoding(dc, OP_and, instr);

    instr =
        INSTR_CREATE_bic_vector(dc, opnd_create_reg(DR_REG_D24),
                                opnd_create_reg(DR_REG_D31), opnd_create_reg(DR_REG_D15));
    test_instr_encoding(dc, OP_bic, instr);

    instr =
        INSTR_CREATE_bic_vector(dc, opnd_create_reg(DR_REG_Q24),
                                opnd_create_reg(DR_REG_Q31), opnd_create_reg(DR_REG_Q15));
    test_instr_encoding(dc, OP_bic, instr);

    instr = INSTR_CREATE_fminnm_vector(dc, opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D30),
                                       opnd_create_reg(DR_REG_D31), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fminnm_vector(dc, opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q31), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fminnm_vector(dc, opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q31), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fmls_vector(dc, opnd_create_reg(DR_REG_D4),
                                     opnd_create_reg(DR_REG_D31),
                                     opnd_create_reg(DR_REG_D29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fmls_vector(dc, opnd_create_reg(DR_REG_Q4),
                                     opnd_create_reg(DR_REG_Q31),
                                     opnd_create_reg(DR_REG_Q29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fmls_vector(dc, opnd_create_reg(DR_REG_Q4),
                                     opnd_create_reg(DR_REG_Q31),
                                     opnd_create_reg(DR_REG_Q29), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fsub_vector(dc, opnd_create_reg(DR_REG_D25),
                                     opnd_create_reg(DR_REG_D8),
                                     opnd_create_reg(DR_REG_D26), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fsub_vector(dc, opnd_create_reg(DR_REG_Q25),
                                     opnd_create_reg(DR_REG_Q8),
                                     opnd_create_reg(DR_REG_Q26), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fsub_vector(dc, opnd_create_reg(DR_REG_Q25),
                                     opnd_create_reg(DR_REG_Q8),
                                     opnd_create_reg(DR_REG_Q26), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fmin_vector(dc, opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D31), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_fmin_vector(dc, opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q31), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_fmin_vector(dc, opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q31), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc, opnd_create_reg(DR_REG_D10),
                                        opnd_create_reg(DR_REG_D28),
                                        opnd_create_reg(DR_REG_D6), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc, opnd_create_reg(DR_REG_Q10),
                                        opnd_create_reg(DR_REG_Q28),
                                        opnd_create_reg(DR_REG_Q6), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc, opnd_create_reg(DR_REG_Q10),
                                        opnd_create_reg(DR_REG_Q28),
                                        opnd_create_reg(DR_REG_Q6), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr =
        INSTR_CREATE_orr_vector(dc, opnd_create_reg(DR_REG_D26),
                                opnd_create_reg(DR_REG_D2), opnd_create_reg(DR_REG_D0));
    test_instr_encoding(dc, OP_orr, instr);

    instr =
        INSTR_CREATE_orr_vector(dc, opnd_create_reg(DR_REG_Q26),
                                opnd_create_reg(DR_REG_Q2), opnd_create_reg(DR_REG_Q0));
    test_instr_encoding(dc, OP_orr, instr);

    instr =
        INSTR_CREATE_orn_vector(dc, opnd_create_reg(DR_REG_D28),
                                opnd_create_reg(DR_REG_D4), opnd_create_reg(DR_REG_D3));
    test_instr_encoding(dc, OP_orn, instr);

    instr =
        INSTR_CREATE_orn_vector(dc, opnd_create_reg(DR_REG_Q28),
                                opnd_create_reg(DR_REG_Q4), opnd_create_reg(DR_REG_Q3));
    test_instr_encoding(dc, OP_orn, instr);

    instr = INSTR_CREATE_uhadd_vector(dc, opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D9), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc, opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q9), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc, opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D9), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc, opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q9), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc, opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D9), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc, opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q9), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc, opnd_create_reg(DR_REG_D8),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D27), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc, opnd_create_reg(DR_REG_Q8),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q27), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc, opnd_create_reg(DR_REG_D8),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc, opnd_create_reg(DR_REG_Q8),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc, opnd_create_reg(DR_REG_D8),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc, opnd_create_reg(DR_REG_Q8),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_uhsub_vector(dc, opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D21),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc, opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q21),
                                      opnd_create_reg(DR_REG_Q16), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc, opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D21),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc, opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q21),
                                      opnd_create_reg(DR_REG_Q16), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc, opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D21),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc, opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q21),
                                      opnd_create_reg(DR_REG_Q16), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D21), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q21), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D21), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q21), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D21), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q21), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q21), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_cmhi_vector(dc, opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc, opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc, opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc, opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc, opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc, opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc, opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhs_vector(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D30), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q30), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc, opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q30), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_ushl_vector(dc, opnd_create_reg(DR_REG_D1),
                                     opnd_create_reg(DR_REG_D7),
                                     opnd_create_reg(DR_REG_D18), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc, opnd_create_reg(DR_REG_Q1),
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q18), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc, opnd_create_reg(DR_REG_D1),
                                     opnd_create_reg(DR_REG_D7),
                                     opnd_create_reg(DR_REG_D18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc, opnd_create_reg(DR_REG_Q1),
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc, opnd_create_reg(DR_REG_D1),
                                     opnd_create_reg(DR_REG_D7),
                                     opnd_create_reg(DR_REG_D18), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc, opnd_create_reg(DR_REG_Q1),
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q18), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc, opnd_create_reg(DR_REG_Q1),
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q18), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc, opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D18), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc, opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q18), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc, opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc, opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc, opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D18), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc, opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q18), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc, opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q18), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc, opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D6), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc, opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q6), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc, opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D6), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc, opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q6), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc, opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D6), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc, opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q6), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc, opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q6), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc, opnd_create_reg(DR_REG_D18),
                                       opnd_create_reg(DR_REG_D10),
                                       opnd_create_reg(DR_REG_D30), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc, opnd_create_reg(DR_REG_D18),
                                       opnd_create_reg(DR_REG_D10),
                                       opnd_create_reg(DR_REG_D30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc, opnd_create_reg(DR_REG_D18),
                                       opnd_create_reg(DR_REG_D10),
                                       opnd_create_reg(DR_REG_D30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_umax_vector(dc, opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D23),
                                     opnd_create_reg(DR_REG_D25), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc, opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q23),
                                     opnd_create_reg(DR_REG_Q25), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc, opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D23),
                                     opnd_create_reg(DR_REG_D25), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc, opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q23),
                                     opnd_create_reg(DR_REG_Q25), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc, opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D23),
                                     opnd_create_reg(DR_REG_D25), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc, opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q23),
                                     opnd_create_reg(DR_REG_Q25), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umin_vector(dc, opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D11), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc, opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q11), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc, opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D11), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc, opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q11), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc, opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D11), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc, opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q11), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_uabd_vector(dc, opnd_create_reg(DR_REG_D5),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D27), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc, opnd_create_reg(DR_REG_Q5),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q27), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc, opnd_create_reg(DR_REG_D5),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc, opnd_create_reg(DR_REG_Q5),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc, opnd_create_reg(DR_REG_D5),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc, opnd_create_reg(DR_REG_Q5),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uaba_vector(dc, opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D6),
                                     opnd_create_reg(DR_REG_D19), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc, opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q6),
                                     opnd_create_reg(DR_REG_Q19), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc, opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D6),
                                     opnd_create_reg(DR_REG_D19), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc, opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q6),
                                     opnd_create_reg(DR_REG_Q19), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc, opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D6),
                                     opnd_create_reg(DR_REG_D19), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc, opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q6),
                                     opnd_create_reg(DR_REG_Q19), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_D29),
                                    opnd_create_reg(DR_REG_D27),
                                    opnd_create_reg(DR_REG_D28), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_Q29),
                                    opnd_create_reg(DR_REG_Q27),
                                    opnd_create_reg(DR_REG_Q28), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_D29),
                                    opnd_create_reg(DR_REG_D27),
                                    opnd_create_reg(DR_REG_D28), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_Q29),
                                    opnd_create_reg(DR_REG_Q27),
                                    opnd_create_reg(DR_REG_Q28), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_D29),
                                    opnd_create_reg(DR_REG_D27),
                                    opnd_create_reg(DR_REG_D28), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_Q29),
                                    opnd_create_reg(DR_REG_Q27),
                                    opnd_create_reg(DR_REG_Q28), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_Q29),
                                    opnd_create_reg(DR_REG_Q27),
                                    opnd_create_reg(DR_REG_Q28), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_cmeq_vector(dc, opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D17),
                                     opnd_create_reg(DR_REG_D23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc, opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc, opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D17),
                                     opnd_create_reg(DR_REG_D23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc, opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc, opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D17),
                                     opnd_create_reg(DR_REG_D23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc, opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc, opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q23), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_mls_vector(dc, opnd_create_reg(DR_REG_D7),
                                    opnd_create_reg(DR_REG_D13),
                                    opnd_create_reg(DR_REG_D27), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc, opnd_create_reg(DR_REG_Q7),
                                    opnd_create_reg(DR_REG_Q13),
                                    opnd_create_reg(DR_REG_Q27), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc, opnd_create_reg(DR_REG_D7),
                                    opnd_create_reg(DR_REG_D13),
                                    opnd_create_reg(DR_REG_D27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc, opnd_create_reg(DR_REG_Q7),
                                    opnd_create_reg(DR_REG_Q13),
                                    opnd_create_reg(DR_REG_Q27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc, opnd_create_reg(DR_REG_D7),
                                    opnd_create_reg(DR_REG_D13),
                                    opnd_create_reg(DR_REG_D27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc, opnd_create_reg(DR_REG_Q7),
                                    opnd_create_reg(DR_REG_Q13),
                                    opnd_create_reg(DR_REG_Q27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_pmul_vector(dc, opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D12), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_pmul, instr);

    instr = INSTR_CREATE_pmul_vector(dc, opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q12), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_pmul, instr);

    instr = INSTR_CREATE_umaxp_vector(dc, opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D5), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc, opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q5), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc, opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D5), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc, opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q5), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc, opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D5), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc, opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q5), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_uminp_vector(dc, opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc, opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q16), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc, opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc, opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q16), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc, opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc, opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q16), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_sqrdmulh_vector(dc, opnd_create_reg(DR_REG_D23),
                                         opnd_create_reg(DR_REG_D29),
                                         opnd_create_reg(DR_REG_D27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqrdmulh, instr);

    instr = INSTR_CREATE_sqrdmulh_vector(dc, opnd_create_reg(DR_REG_Q23),
                                         opnd_create_reg(DR_REG_Q29),
                                         opnd_create_reg(DR_REG_Q27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqrdmulh, instr);

    instr = INSTR_CREATE_sqrdmulh_vector(
        dc, opnd_create_reg(DR_REG_D23), opnd_create_reg(DR_REG_D29),
        opnd_create_reg(DR_REG_D27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqrdmulh, instr);

    instr = INSTR_CREATE_sqrdmulh_vector(
        dc, opnd_create_reg(DR_REG_Q23), opnd_create_reg(DR_REG_Q29),
        opnd_create_reg(DR_REG_Q27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqrdmulh, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(
        dc, opnd_create_reg(DR_REG_D12), opnd_create_reg(DR_REG_D18),
        opnd_create_reg(DR_REG_D29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(
        dc, opnd_create_reg(DR_REG_Q12), opnd_create_reg(DR_REG_Q18),
        opnd_create_reg(DR_REG_Q29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(
        dc, opnd_create_reg(DR_REG_Q12), opnd_create_reg(DR_REG_Q18),
        opnd_create_reg(DR_REG_Q29), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_faddp_vector(dc, opnd_create_reg(DR_REG_D18),
                                      opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_faddp_vector(dc, opnd_create_reg(DR_REG_Q18),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q16), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_faddp_vector(dc, opnd_create_reg(DR_REG_Q18),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q16), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_D25),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D21), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_Q25),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q21), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_Q25),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q21), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fcmge_vector(dc, opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D17),
                                      opnd_create_reg(DR_REG_D30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_fcmge_vector(dc, opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q17),
                                      opnd_create_reg(DR_REG_Q30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_fcmge_vector(dc, opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q17),
                                      opnd_create_reg(DR_REG_Q30), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_facge_vector(dc, opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D30),
                                      opnd_create_reg(DR_REG_D30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_facge_vector(dc, opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_Q30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_facge_vector(dc, opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_Q30), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc, opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D25), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc, opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q25), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc, opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q25), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fdiv_vector(dc, opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D4), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fdiv_vector(dc, opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q4), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fdiv_vector(dc, opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q4), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr =
        INSTR_CREATE_eor_vector(dc, opnd_create_reg(DR_REG_D19),
                                opnd_create_reg(DR_REG_D1), opnd_create_reg(DR_REG_D20));
    test_instr_encoding(dc, OP_eor, instr);

    instr =
        INSTR_CREATE_eor_vector(dc, opnd_create_reg(DR_REG_Q19),
                                opnd_create_reg(DR_REG_Q1), opnd_create_reg(DR_REG_Q20));
    test_instr_encoding(dc, OP_eor, instr);

    instr =
        INSTR_CREATE_bsl_vector(dc, opnd_create_reg(DR_REG_D20),
                                opnd_create_reg(DR_REG_D4), opnd_create_reg(DR_REG_D25));
    test_instr_encoding(dc, OP_bsl, instr);

    instr =
        INSTR_CREATE_bsl_vector(dc, opnd_create_reg(DR_REG_Q20),
                                opnd_create_reg(DR_REG_Q4), opnd_create_reg(DR_REG_Q25));
    test_instr_encoding(dc, OP_bsl, instr);

    instr = INSTR_CREATE_fminnmp_vector(
        dc, opnd_create_reg(DR_REG_D23), opnd_create_reg(DR_REG_D18),
        opnd_create_reg(DR_REG_D11), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fminnmp_vector(
        dc, opnd_create_reg(DR_REG_Q23), opnd_create_reg(DR_REG_Q18),
        opnd_create_reg(DR_REG_Q11), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fminnmp_vector(
        dc, opnd_create_reg(DR_REG_Q23), opnd_create_reg(DR_REG_Q18),
        opnd_create_reg(DR_REG_Q11), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fabd_vector(dc, opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D19), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fabd_vector(dc, opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q19), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fabd_vector(dc, opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q19), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc, opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_D14), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q14), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q14), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc, opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D12), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc, opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q12), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc, opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q12), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_fminp_vector(dc, opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D1),
                                      opnd_create_reg(DR_REG_D25), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminp, instr);

    instr = INSTR_CREATE_fminp_vector(dc, opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q1),
                                      opnd_create_reg(DR_REG_Q25), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminp, instr);

    instr = INSTR_CREATE_fminp_vector(dc, opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q1),
                                      opnd_create_reg(DR_REG_Q25), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fminp, instr);

    instr =
        INSTR_CREATE_bit_vector(dc, opnd_create_reg(DR_REG_D12),
                                opnd_create_reg(DR_REG_D21), opnd_create_reg(DR_REG_D12));
    test_instr_encoding(dc, OP_bit, instr);

    instr =
        INSTR_CREATE_bit_vector(dc, opnd_create_reg(DR_REG_Q12),
                                opnd_create_reg(DR_REG_Q21), opnd_create_reg(DR_REG_Q12));
    test_instr_encoding(dc, OP_bit, instr);

    instr =
        INSTR_CREATE_bif_vector(dc, opnd_create_reg(DR_REG_D20),
                                opnd_create_reg(DR_REG_D3), opnd_create_reg(DR_REG_D3));
    test_instr_encoding(dc, OP_bif, instr);

    instr =
        INSTR_CREATE_bif_vector(dc, opnd_create_reg(DR_REG_Q20),
                                opnd_create_reg(DR_REG_Q3), opnd_create_reg(DR_REG_Q3));
    test_instr_encoding(dc, OP_bif, instr);
}

static void
test_asimd_mem(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* Advanced SIMD memory (multiple structures) */

    /* Load multiple 1-element structures (to 1, 2, 3 or 4 registers)
       Naming convention based on official mnemonics:
       INSTR_CREATE_ld1_multi_<n>() where <n> is 1, 2, 3 or 4

       LD1 { <Vt>.<T> }, [<Xn|SP>]
       LD1 { <Vt>.<T>, <Vt2>.<T> }, [<Xn|SP>]
       LD1 { <Vt>.<T>, <Vt2>.<T>, <Vt3>.<T> }, [<Xn|SP>]
       LD1 { <Vt>.<T>, <Vt2>.<T>, <Vt3>.<T>, <Vt4>.<T> }, [<Xn|SP>]

       <T> is one of 8B, 16B, 4H, 8H, 2S, 4S, 1D, 2D */

    /* LD1 { <Vt>.8B }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_D0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.16B }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
        OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.4H }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_D0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.8H }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.2S }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_D0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.4S }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.1D }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_D0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.2D }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
        OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_ld1, instr);

    /* Store multiple 1-element structures (to 1, 2, 3 or 4 registers)
       Naming convention based on official mnemonics:
       INSTR_CREATE_st1_multi_<n>() where <n> is 1, 2, 3 or 4

       ST1 { <Vt>.<T> }, [<Xn|SP>]
       ST1 { <Vt>.<T>, <Vt2>.<T> }, [<Xn|SP>]
       ST1 { <Vt>.<T>, <Vt2>.<T>, <Vt3>.<T> }, [<Xn|SP>]
       ST1 { <Vt>.<T>, <Vt2>.<T>, <Vt3>.<T>, <Vt4>.<T> }, [<Xn|SP>]

       <T> is one of 8B, 16B, 4H, 8H, 2S, 4S, 1D, 2D */

    /* ST1 { <Vt>.8B }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        opnd_create_reg(DR_REG_D0), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.16B }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc,
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.4H }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        opnd_create_reg(DR_REG_D0), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.8H }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc,
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.2S }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        opnd_create_reg(DR_REG_D0), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.4S }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc,
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.1D }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        opnd_create_reg(DR_REG_D0), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.2D }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc,
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_st1, instr);
}

static void
test_floatdp1(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* Floating-point data-processing (1 source) */

    instr = INSTR_CREATE_fmov_scalar(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D27));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar(dc, opnd_create_reg(DR_REG_S2),
                                     opnd_create_reg(DR_REG_S27));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fmov_scalar(dc, opnd_create_reg(DR_REG_H2),
                                     opnd_create_reg(DR_REG_H27));
    test_instr_encoding(dc, OP_fmov, instr);

    instr = INSTR_CREATE_fabs_scalar(dc, opnd_create_reg(DR_REG_D30),
                                     opnd_create_reg(DR_REG_D0));
    test_instr_encoding(dc, OP_fabs, instr);

    instr = INSTR_CREATE_fabs_scalar(dc, opnd_create_reg(DR_REG_S30),
                                     opnd_create_reg(DR_REG_S0));
    test_instr_encoding(dc, OP_fabs, instr);

    instr = INSTR_CREATE_fabs_scalar(dc, opnd_create_reg(DR_REG_H30),
                                     opnd_create_reg(DR_REG_H0));
    test_instr_encoding(dc, OP_fabs, instr);

    instr = INSTR_CREATE_fneg_scalar(dc, opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D29));
    test_instr_encoding(dc, OP_fneg, instr);

    instr = INSTR_CREATE_fneg_scalar(dc, opnd_create_reg(DR_REG_S13),
                                     opnd_create_reg(DR_REG_S29));
    test_instr_encoding(dc, OP_fneg, instr);

    instr = INSTR_CREATE_fneg_scalar(dc, opnd_create_reg(DR_REG_H13),
                                     opnd_create_reg(DR_REG_H29));
    test_instr_encoding(dc, OP_fneg, instr);

    instr = INSTR_CREATE_fsqrt_scalar(dc, opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D17));
    test_instr_encoding(dc, OP_fsqrt, instr);

    instr = INSTR_CREATE_fsqrt_scalar(dc, opnd_create_reg(DR_REG_S31),
                                      opnd_create_reg(DR_REG_S17));
    test_instr_encoding(dc, OP_fsqrt, instr);

    instr = INSTR_CREATE_fsqrt_scalar(dc, opnd_create_reg(DR_REG_H31),
                                      opnd_create_reg(DR_REG_H17));
    test_instr_encoding(dc, OP_fsqrt, instr);

    instr = INSTR_CREATE_fcvt_scalar(dc, opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_S2));
    test_instr_encoding(dc, OP_fcvt, instr);

    instr = INSTR_CREATE_fcvt_scalar(dc, opnd_create_reg(DR_REG_H31),
                                     opnd_create_reg(DR_REG_S20));
    test_instr_encoding(dc, OP_fcvt, instr);

    instr = INSTR_CREATE_frintn_scalar(dc, opnd_create_reg(DR_REG_D4),
                                       opnd_create_reg(DR_REG_D15));
    test_instr_encoding(dc, OP_frintn, instr);

    instr = INSTR_CREATE_frintn_scalar(dc, opnd_create_reg(DR_REG_S4),
                                       opnd_create_reg(DR_REG_S15));
    test_instr_encoding(dc, OP_frintn, instr);

    instr = INSTR_CREATE_frintn_scalar(dc, opnd_create_reg(DR_REG_H4),
                                       opnd_create_reg(DR_REG_H15));
    test_instr_encoding(dc, OP_frintn, instr);

    instr = INSTR_CREATE_frintp_scalar(dc, opnd_create_reg(DR_REG_D23),
                                       opnd_create_reg(DR_REG_D2));
    test_instr_encoding(dc, OP_frintp, instr);

    instr = INSTR_CREATE_frintp_scalar(dc, opnd_create_reg(DR_REG_S23),
                                       opnd_create_reg(DR_REG_S2));
    test_instr_encoding(dc, OP_frintp, instr);

    instr = INSTR_CREATE_frintp_scalar(dc, opnd_create_reg(DR_REG_H23),
                                       opnd_create_reg(DR_REG_H2));
    test_instr_encoding(dc, OP_frintp, instr);

    instr = INSTR_CREATE_frintm_scalar(dc, opnd_create_reg(DR_REG_D26),
                                       opnd_create_reg(DR_REG_D8));
    test_instr_encoding(dc, OP_frintm, instr);

    instr = INSTR_CREATE_frintm_scalar(dc, opnd_create_reg(DR_REG_S26),
                                       opnd_create_reg(DR_REG_S8));
    test_instr_encoding(dc, OP_frintm, instr);

    instr = INSTR_CREATE_frintm_scalar(dc, opnd_create_reg(DR_REG_H26),
                                       opnd_create_reg(DR_REG_H8));
    test_instr_encoding(dc, OP_frintm, instr);

    instr = INSTR_CREATE_frintz_scalar(dc, opnd_create_reg(DR_REG_D22),
                                       opnd_create_reg(DR_REG_D24));
    test_instr_encoding(dc, OP_frintz, instr);

    instr = INSTR_CREATE_frintz_scalar(dc, opnd_create_reg(DR_REG_S22),
                                       opnd_create_reg(DR_REG_S24));
    test_instr_encoding(dc, OP_frintz, instr);

    instr = INSTR_CREATE_frintz_scalar(dc, opnd_create_reg(DR_REG_H22),
                                       opnd_create_reg(DR_REG_H24));
    test_instr_encoding(dc, OP_frintz, instr);

    instr = INSTR_CREATE_frinta_scalar(dc, opnd_create_reg(DR_REG_D26),
                                       opnd_create_reg(DR_REG_D18));
    test_instr_encoding(dc, OP_frinta, instr);

    instr = INSTR_CREATE_frinta_scalar(dc, opnd_create_reg(DR_REG_S26),
                                       opnd_create_reg(DR_REG_S18));
    test_instr_encoding(dc, OP_frinta, instr);

    instr = INSTR_CREATE_frinta_scalar(dc, opnd_create_reg(DR_REG_H26),
                                       opnd_create_reg(DR_REG_H18));
    test_instr_encoding(dc, OP_frinta, instr);

    instr = INSTR_CREATE_frintx_scalar(dc, opnd_create_reg(DR_REG_D16),
                                       opnd_create_reg(DR_REG_D29));
    test_instr_encoding(dc, OP_frintx, instr);

    instr = INSTR_CREATE_frintx_scalar(dc, opnd_create_reg(DR_REG_S16),
                                       opnd_create_reg(DR_REG_S29));
    test_instr_encoding(dc, OP_frintx, instr);

    instr = INSTR_CREATE_frintx_scalar(dc, opnd_create_reg(DR_REG_H16),
                                       opnd_create_reg(DR_REG_H29));
    test_instr_encoding(dc, OP_frintx, instr);

    instr = INSTR_CREATE_frinti_scalar(dc, opnd_create_reg(DR_REG_D11),
                                       opnd_create_reg(DR_REG_D19));
    test_instr_encoding(dc, OP_frinti, instr);

    instr = INSTR_CREATE_frinti_scalar(dc, opnd_create_reg(DR_REG_S11),
                                       opnd_create_reg(DR_REG_S19));
    test_instr_encoding(dc, OP_frinti, instr);

    instr = INSTR_CREATE_frinti_scalar(dc, opnd_create_reg(DR_REG_H11),
                                       opnd_create_reg(DR_REG_H19));
    test_instr_encoding(dc, OP_frinti, instr);

    instr = INSTR_CREATE_fcvt_scalar(dc, opnd_create_reg(DR_REG_S23),
                                     opnd_create_reg(DR_REG_D8));
    test_instr_encoding(dc, OP_fcvt, instr);

    instr = INSTR_CREATE_fcvt_scalar(dc, opnd_create_reg(DR_REG_H29),
                                     opnd_create_reg(DR_REG_D15));
    test_instr_encoding(dc, OP_fcvt, instr);

    instr = INSTR_CREATE_fcvt_scalar(dc, opnd_create_reg(DR_REG_S28),
                                     opnd_create_reg(DR_REG_H24));
    test_instr_encoding(dc, OP_fcvt, instr);

    instr = INSTR_CREATE_fcvt_scalar(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_H0));
    test_instr_encoding(dc, OP_fcvt, instr);
}

static void
test_floatdp2(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* Floating-point data-processing (2 source) */

    instr = INSTR_CREATE_fmul_scalar(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D27),
                                     opnd_create_reg(DR_REG_D30));
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fmul_scalar(dc, opnd_create_reg(DR_REG_S2),
                                     opnd_create_reg(DR_REG_S27),
                                     opnd_create_reg(DR_REG_S30));
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fmul_scalar(dc, opnd_create_reg(DR_REG_H2),
                                     opnd_create_reg(DR_REG_H27),
                                     opnd_create_reg(DR_REG_H30));
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fdiv_scalar(dc, opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D29));
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fdiv_scalar(dc, opnd_create_reg(DR_REG_S0),
                                     opnd_create_reg(DR_REG_S13),
                                     opnd_create_reg(DR_REG_S29));
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fdiv_scalar(dc, opnd_create_reg(DR_REG_H0),
                                     opnd_create_reg(DR_REG_H13),
                                     opnd_create_reg(DR_REG_H29));
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fadd_scalar(dc, opnd_create_reg(DR_REG_D31),
                                     opnd_create_reg(DR_REG_D17),
                                     opnd_create_reg(DR_REG_D10));
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fadd_scalar(dc, opnd_create_reg(DR_REG_S31),
                                     opnd_create_reg(DR_REG_S17),
                                     opnd_create_reg(DR_REG_S10));
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fadd_scalar(dc, opnd_create_reg(DR_REG_H31),
                                     opnd_create_reg(DR_REG_H17),
                                     opnd_create_reg(DR_REG_H10));
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fsub_scalar(dc, opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D31),
                                     opnd_create_reg(DR_REG_D20));
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fsub_scalar(dc, opnd_create_reg(DR_REG_S2),
                                     opnd_create_reg(DR_REG_S31),
                                     opnd_create_reg(DR_REG_S20));
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fsub_scalar(dc, opnd_create_reg(DR_REG_H2),
                                     opnd_create_reg(DR_REG_H31),
                                     opnd_create_reg(DR_REG_H20));
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fmax_scalar(dc, opnd_create_reg(DR_REG_D4),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D23));
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_fmax_scalar(dc, opnd_create_reg(DR_REG_S4),
                                     opnd_create_reg(DR_REG_S15),
                                     opnd_create_reg(DR_REG_S23));
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_fmax_scalar(dc, opnd_create_reg(DR_REG_H4),
                                     opnd_create_reg(DR_REG_H15),
                                     opnd_create_reg(DR_REG_H23));
    test_instr_encoding(dc, OP_fmax, instr);

    instr =
        INSTR_CREATE_fmin_scalar(dc, opnd_create_reg(DR_REG_D2),
                                 opnd_create_reg(DR_REG_D26), opnd_create_reg(DR_REG_D8));
    test_instr_encoding(dc, OP_fmin, instr);

    instr =
        INSTR_CREATE_fmin_scalar(dc, opnd_create_reg(DR_REG_S2),
                                 opnd_create_reg(DR_REG_S26), opnd_create_reg(DR_REG_S8));
    test_instr_encoding(dc, OP_fmin, instr);

    instr =
        INSTR_CREATE_fmin_scalar(dc, opnd_create_reg(DR_REG_H2),
                                 opnd_create_reg(DR_REG_H26), opnd_create_reg(DR_REG_H8));
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_fmaxnm_scalar(dc, opnd_create_reg(DR_REG_D22),
                                       opnd_create_reg(DR_REG_D24),
                                       opnd_create_reg(DR_REG_D26));
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmaxnm_scalar(dc, opnd_create_reg(DR_REG_S22),
                                       opnd_create_reg(DR_REG_S24),
                                       opnd_create_reg(DR_REG_S26));
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmaxnm_scalar(dc, opnd_create_reg(DR_REG_H22),
                                       opnd_create_reg(DR_REG_H24),
                                       opnd_create_reg(DR_REG_H26));
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fminnm_scalar(dc, opnd_create_reg(DR_REG_D18),
                                       opnd_create_reg(DR_REG_D16),
                                       opnd_create_reg(DR_REG_D29));
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fminnm_scalar(dc, opnd_create_reg(DR_REG_S18),
                                       opnd_create_reg(DR_REG_S16),
                                       opnd_create_reg(DR_REG_S29));
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fminnm_scalar(dc, opnd_create_reg(DR_REG_H18),
                                       opnd_create_reg(DR_REG_H16),
                                       opnd_create_reg(DR_REG_H29));
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fnmul_scalar(dc, opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D23));
    test_instr_encoding(dc, OP_fnmul, instr);

    instr = INSTR_CREATE_fnmul_scalar(dc, opnd_create_reg(DR_REG_S11),
                                      opnd_create_reg(DR_REG_S19),
                                      opnd_create_reg(DR_REG_S23));
    test_instr_encoding(dc, OP_fnmul, instr);

    instr = INSTR_CREATE_fnmul_scalar(dc, opnd_create_reg(DR_REG_H11),
                                      opnd_create_reg(DR_REG_H19),
                                      opnd_create_reg(DR_REG_H23));
    test_instr_encoding(dc, OP_fnmul, instr);
}

static void
test_floatdp3(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* Floating-point data-processing (3 source) */

    instr = INSTR_CREATE_fmadd_scalar(
        dc, opnd_create_reg(DR_REG_D2), opnd_create_reg(DR_REG_D27),
        opnd_create_reg(DR_REG_D30), opnd_create_reg(DR_REG_D0));
    test_instr_encoding(dc, OP_fmadd, instr);

    instr = INSTR_CREATE_fmadd_scalar(
        dc, opnd_create_reg(DR_REG_S2), opnd_create_reg(DR_REG_S27),
        opnd_create_reg(DR_REG_S30), opnd_create_reg(DR_REG_S0));
    test_instr_encoding(dc, OP_fmadd, instr);

    instr = INSTR_CREATE_fmadd_scalar(
        dc, opnd_create_reg(DR_REG_H2), opnd_create_reg(DR_REG_H27),
        opnd_create_reg(DR_REG_H30), opnd_create_reg(DR_REG_H0));
    test_instr_encoding(dc, OP_fmadd, instr);

    instr = INSTR_CREATE_fmsub_scalar(
        dc, opnd_create_reg(DR_REG_D13), opnd_create_reg(DR_REG_D29),
        opnd_create_reg(DR_REG_D31), opnd_create_reg(DR_REG_D17));
    test_instr_encoding(dc, OP_fmsub, instr);

    instr = INSTR_CREATE_fmsub_scalar(
        dc, opnd_create_reg(DR_REG_S13), opnd_create_reg(DR_REG_S29),
        opnd_create_reg(DR_REG_S31), opnd_create_reg(DR_REG_S17));
    test_instr_encoding(dc, OP_fmsub, instr);

    instr = INSTR_CREATE_fmsub_scalar(
        dc, opnd_create_reg(DR_REG_H13), opnd_create_reg(DR_REG_H29),
        opnd_create_reg(DR_REG_H31), opnd_create_reg(DR_REG_H17));
    test_instr_encoding(dc, OP_fmsub, instr);

    instr = INSTR_CREATE_fnmadd_scalar(
        dc, opnd_create_reg(DR_REG_D10), opnd_create_reg(DR_REG_D2),
        opnd_create_reg(DR_REG_D31), opnd_create_reg(DR_REG_D20));
    test_instr_encoding(dc, OP_fnmadd, instr);

    instr = INSTR_CREATE_fnmadd_scalar(
        dc, opnd_create_reg(DR_REG_S10), opnd_create_reg(DR_REG_S2),
        opnd_create_reg(DR_REG_S31), opnd_create_reg(DR_REG_S20));
    test_instr_encoding(dc, OP_fnmadd, instr);

    instr = INSTR_CREATE_fnmadd_scalar(
        dc, opnd_create_reg(DR_REG_H10), opnd_create_reg(DR_REG_H2),
        opnd_create_reg(DR_REG_H31), opnd_create_reg(DR_REG_H20));
    test_instr_encoding(dc, OP_fnmadd, instr);

    instr = INSTR_CREATE_fnmsub_scalar(
        dc, opnd_create_reg(DR_REG_D4), opnd_create_reg(DR_REG_D15),
        opnd_create_reg(DR_REG_D23), opnd_create_reg(DR_REG_D2));
    test_instr_encoding(dc, OP_fnmsub, instr);

    instr = INSTR_CREATE_fnmsub_scalar(
        dc, opnd_create_reg(DR_REG_S4), opnd_create_reg(DR_REG_S15),
        opnd_create_reg(DR_REG_S23), opnd_create_reg(DR_REG_S2));
    test_instr_encoding(dc, OP_fnmsub, instr);

    instr = INSTR_CREATE_fnmsub_scalar(
        dc, opnd_create_reg(DR_REG_H4), opnd_create_reg(DR_REG_H15),
        opnd_create_reg(DR_REG_H23), opnd_create_reg(DR_REG_H2));
    test_instr_encoding(dc, OP_fnmsub, instr);
}

static void
test_asimddiff(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* Advanced SIMD Three Different */
    instr = INSTR_CREATE_saddl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D18), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_saddl, instr);

    instr = INSTR_CREATE_saddl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_saddl, instr);

    instr = INSTR_CREATE_saddl_vector(dc, opnd_create_reg(DR_REG_Q18),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D18), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_saddl, instr);

    instr = INSTR_CREATE_saddl2_vector(dc, opnd_create_reg(DR_REG_Q6),
                                       opnd_create_reg(DR_REG_Q26),
                                       opnd_create_reg(DR_REG_Q26), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_saddl2, instr);

    instr = INSTR_CREATE_saddl2_vector(dc, opnd_create_reg(DR_REG_Q6),
                                       opnd_create_reg(DR_REG_Q26),
                                       opnd_create_reg(DR_REG_Q26), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_saddl2, instr);

    instr = INSTR_CREATE_saddl2_vector(dc, opnd_create_reg(DR_REG_Q6),
                                       opnd_create_reg(DR_REG_Q26),
                                       opnd_create_reg(DR_REG_Q26), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_saddl2, instr);

    instr = INSTR_CREATE_saddw_vector(dc, opnd_create_reg(DR_REG_Q20),
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_saddw, instr);

    instr = INSTR_CREATE_saddw_vector(dc, opnd_create_reg(DR_REG_Q20),
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_saddw, instr);

    instr = INSTR_CREATE_saddw_vector(dc, opnd_create_reg(DR_REG_Q20),
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_D16), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_saddw, instr);

    instr = INSTR_CREATE_saddw2_vector(dc, opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q7),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_saddw2, instr);

    instr = INSTR_CREATE_saddw2_vector(dc, opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q7),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_saddw2, instr);

    instr = INSTR_CREATE_saddw2_vector(dc, opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q7),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_saddw2, instr);

    instr = INSTR_CREATE_ssubl_vector(dc, opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_D1),
                                      opnd_create_reg(DR_REG_D27), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ssubl, instr);

    instr = INSTR_CREATE_ssubl_vector(dc, opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_D1),
                                      opnd_create_reg(DR_REG_D27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ssubl, instr);

    instr = INSTR_CREATE_ssubl_vector(dc, opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_D1),
                                      opnd_create_reg(DR_REG_D27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ssubl, instr);

    instr = INSTR_CREATE_ssubl2_vector(dc, opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q12),
                                       opnd_create_reg(DR_REG_Q9), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ssubl2, instr);

    instr = INSTR_CREATE_ssubl2_vector(dc, opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q12),
                                       opnd_create_reg(DR_REG_Q9), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ssubl2, instr);

    instr = INSTR_CREATE_ssubl2_vector(dc, opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q12),
                                       opnd_create_reg(DR_REG_Q9), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ssubl2, instr);

    instr = INSTR_CREATE_ssubw_vector(dc, opnd_create_reg(DR_REG_Q21),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_D21), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ssubw, instr);

    instr = INSTR_CREATE_ssubw_vector(dc, opnd_create_reg(DR_REG_Q21),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_D21), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ssubw, instr);

    instr = INSTR_CREATE_ssubw_vector(dc, opnd_create_reg(DR_REG_Q21),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_D21), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ssubw, instr);

    instr = INSTR_CREATE_ssubw2_vector(dc, opnd_create_reg(DR_REG_Q21),
                                       opnd_create_reg(DR_REG_Q4),
                                       opnd_create_reg(DR_REG_Q24), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ssubw2, instr);

    instr = INSTR_CREATE_ssubw2_vector(dc, opnd_create_reg(DR_REG_Q21),
                                       opnd_create_reg(DR_REG_Q4),
                                       opnd_create_reg(DR_REG_Q24), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ssubw2, instr);

    instr = INSTR_CREATE_ssubw2_vector(dc, opnd_create_reg(DR_REG_Q21),
                                       opnd_create_reg(DR_REG_Q4),
                                       opnd_create_reg(DR_REG_Q24), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ssubw2, instr);

    instr = INSTR_CREATE_addhn_vector(dc, opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q19), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_addhn, instr);

    instr = INSTR_CREATE_addhn_vector(dc, opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q19), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_addhn, instr);

    instr = INSTR_CREATE_addhn_vector(dc, opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q19), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_addhn, instr);

    instr = INSTR_CREATE_addhn2_vector(dc, opnd_create_reg(DR_REG_Q1),
                                       opnd_create_reg(DR_REG_Q0),
                                       opnd_create_reg(DR_REG_Q4), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_addhn2, instr);

    instr = INSTR_CREATE_addhn2_vector(dc, opnd_create_reg(DR_REG_Q1),
                                       opnd_create_reg(DR_REG_Q0),
                                       opnd_create_reg(DR_REG_Q4), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_addhn2, instr);

    instr = INSTR_CREATE_addhn2_vector(dc, opnd_create_reg(DR_REG_Q1),
                                       opnd_create_reg(DR_REG_Q0),
                                       opnd_create_reg(DR_REG_Q4), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_addhn2, instr);

    instr = INSTR_CREATE_sabal_vector(dc, opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D11), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sabal, instr);

    instr = INSTR_CREATE_sabal_vector(dc, opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D11), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sabal, instr);

    instr = INSTR_CREATE_sabal_vector(dc, opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D11), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sabal, instr);

    instr = INSTR_CREATE_sabal2_vector(dc, opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q17), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sabal2, instr);

    instr = INSTR_CREATE_sabal2_vector(dc, opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q17), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sabal2, instr);

    instr = INSTR_CREATE_sabal2_vector(dc, opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q17), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sabal2, instr);

    instr = INSTR_CREATE_subhn_vector(dc, opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q18), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_subhn, instr);

    instr = INSTR_CREATE_subhn_vector(dc, opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_subhn, instr);

    instr = INSTR_CREATE_subhn_vector(dc, opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q18), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_subhn, instr);

    instr = INSTR_CREATE_subhn2_vector(dc, opnd_create_reg(DR_REG_Q27),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q7), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_subhn2, instr);

    instr = INSTR_CREATE_subhn2_vector(dc, opnd_create_reg(DR_REG_Q27),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q7), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_subhn2, instr);

    instr = INSTR_CREATE_subhn2_vector(dc, opnd_create_reg(DR_REG_Q27),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q7), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_subhn2, instr);

    instr = INSTR_CREATE_sabdl_vector(dc, opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_D1),
                                      opnd_create_reg(DR_REG_D15), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sabdl, instr);

    instr = INSTR_CREATE_sabdl_vector(dc, opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_D1),
                                      opnd_create_reg(DR_REG_D15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sabdl, instr);

    instr = INSTR_CREATE_sabdl_vector(dc, opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_D1),
                                      opnd_create_reg(DR_REG_D15), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sabdl, instr);

    instr = INSTR_CREATE_sabdl2_vector(dc, opnd_create_reg(DR_REG_Q26),
                                       opnd_create_reg(DR_REG_Q21),
                                       opnd_create_reg(DR_REG_Q17), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sabdl2, instr);

    instr = INSTR_CREATE_sabdl2_vector(dc, opnd_create_reg(DR_REG_Q26),
                                       opnd_create_reg(DR_REG_Q21),
                                       opnd_create_reg(DR_REG_Q17), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sabdl2, instr);

    instr = INSTR_CREATE_sabdl2_vector(dc, opnd_create_reg(DR_REG_Q26),
                                       opnd_create_reg(DR_REG_Q21),
                                       opnd_create_reg(DR_REG_Q17), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sabdl2, instr);

    instr = INSTR_CREATE_smlal_vector(dc, opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D18), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smlal, instr);

    instr = INSTR_CREATE_smlal_vector(dc, opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smlal, instr);

    instr = INSTR_CREATE_smlal_vector(dc, opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D18), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smlal, instr);

    instr = INSTR_CREATE_smlal2_vector(dc, opnd_create_reg(DR_REG_Q11),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q3), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smlal2, instr);

    instr = INSTR_CREATE_smlal2_vector(dc, opnd_create_reg(DR_REG_Q11),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q3), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smlal2, instr);

    instr = INSTR_CREATE_smlal2_vector(dc, opnd_create_reg(DR_REG_Q11),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q3), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smlal2, instr);

    instr = INSTR_CREATE_sqdmlal_vector(dc, opnd_create_reg(DR_REG_Q24),
                                        opnd_create_reg(DR_REG_D3),
                                        opnd_create_reg(DR_REG_D5), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmlal, instr);

    instr = INSTR_CREATE_sqdmlal_vector(dc, opnd_create_reg(DR_REG_Q24),
                                        opnd_create_reg(DR_REG_D3),
                                        opnd_create_reg(DR_REG_D5), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmlal, instr);

    instr = INSTR_CREATE_sqdmlal2_vector(dc, opnd_create_reg(DR_REG_Q25),
                                         opnd_create_reg(DR_REG_Q30),
                                         opnd_create_reg(DR_REG_Q13), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmlal2, instr);

    instr = INSTR_CREATE_sqdmlal2_vector(
        dc, opnd_create_reg(DR_REG_Q25), opnd_create_reg(DR_REG_Q30),
        opnd_create_reg(DR_REG_Q13), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmlal2, instr);

    instr = INSTR_CREATE_smlsl_vector(dc, opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_D7),
                                      opnd_create_reg(DR_REG_D8), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smlsl, instr);

    instr = INSTR_CREATE_smlsl_vector(dc, opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_D7),
                                      opnd_create_reg(DR_REG_D8), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smlsl, instr);

    instr = INSTR_CREATE_smlsl_vector(dc, opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_D7),
                                      opnd_create_reg(DR_REG_D8), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smlsl, instr);

    instr = INSTR_CREATE_smlsl2_vector(dc, opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q3), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smlsl2, instr);

    instr = INSTR_CREATE_smlsl2_vector(dc, opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q3), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smlsl2, instr);

    instr = INSTR_CREATE_smlsl2_vector(dc, opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q3), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smlsl2, instr);

    instr = INSTR_CREATE_sqdmlsl_vector(dc, opnd_create_reg(DR_REG_Q14),
                                        opnd_create_reg(DR_REG_D5),
                                        opnd_create_reg(DR_REG_D20), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmlsl, instr);

    instr = INSTR_CREATE_sqdmlsl_vector(
        dc, opnd_create_reg(DR_REG_Q14), opnd_create_reg(DR_REG_D5),
        opnd_create_reg(DR_REG_D20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmlsl, instr);

    instr = INSTR_CREATE_sqdmlsl2_vector(dc, opnd_create_reg(DR_REG_Q26),
                                         opnd_create_reg(DR_REG_Q24),
                                         opnd_create_reg(DR_REG_Q15), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmlsl2, instr);

    instr = INSTR_CREATE_sqdmlsl2_vector(
        dc, opnd_create_reg(DR_REG_Q26), opnd_create_reg(DR_REG_Q24),
        opnd_create_reg(DR_REG_Q15), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmlsl2, instr);

    instr = INSTR_CREATE_smull_vector(dc, opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D0), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smull, instr);

    instr = INSTR_CREATE_smull_vector(dc, opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D0), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smull, instr);

    instr = INSTR_CREATE_smull_vector(dc, opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D0), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smull, instr);

    instr = INSTR_CREATE_smull2_vector(dc, opnd_create_reg(DR_REG_Q22),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q10), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smull2, instr);

    instr = INSTR_CREATE_smull2_vector(dc, opnd_create_reg(DR_REG_Q22),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q10), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smull2, instr);

    instr = INSTR_CREATE_smull2_vector(dc, opnd_create_reg(DR_REG_Q22),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q10), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smull2, instr);

    instr = INSTR_CREATE_sqdmull_vector(dc, opnd_create_reg(DR_REG_Q2),
                                        opnd_create_reg(DR_REG_D14),
                                        opnd_create_reg(DR_REG_D18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmull, instr);

    instr = INSTR_CREATE_sqdmull_vector(
        dc, opnd_create_reg(DR_REG_Q2), opnd_create_reg(DR_REG_D14),
        opnd_create_reg(DR_REG_D18), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmull, instr);

    instr = INSTR_CREATE_sqdmull2_vector(dc, opnd_create_reg(DR_REG_Q12),
                                         opnd_create_reg(DR_REG_Q27),
                                         opnd_create_reg(DR_REG_Q21), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmull2, instr);

    instr = INSTR_CREATE_sqdmull2_vector(
        dc, opnd_create_reg(DR_REG_Q12), opnd_create_reg(DR_REG_Q27),
        opnd_create_reg(DR_REG_Q21), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmull2, instr);

    instr = INSTR_CREATE_pmull_vector(dc, opnd_create_reg(DR_REG_Q16),
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D2), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_pmull, instr);

    instr = INSTR_CREATE_pmull_vector(dc, opnd_create_reg(DR_REG_Q16),
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D2), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_pmull, instr);

    instr = INSTR_CREATE_pmull2_vector(dc, opnd_create_reg(DR_REG_Q16),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_pmull2, instr);

    instr = INSTR_CREATE_pmull2_vector(dc, opnd_create_reg(DR_REG_Q16),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_pmull2, instr);

    instr = INSTR_CREATE_uaddl_vector(dc, opnd_create_reg(DR_REG_Q7),
                                      opnd_create_reg(DR_REG_D16),
                                      opnd_create_reg(DR_REG_D29), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uaddl, instr);

    instr = INSTR_CREATE_uaddl_vector(dc, opnd_create_reg(DR_REG_Q7),
                                      opnd_create_reg(DR_REG_D16),
                                      opnd_create_reg(DR_REG_D29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uaddl, instr);

    instr = INSTR_CREATE_uaddl_vector(dc, opnd_create_reg(DR_REG_Q7),
                                      opnd_create_reg(DR_REG_D16),
                                      opnd_create_reg(DR_REG_D29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uaddl, instr);

    instr = INSTR_CREATE_uaddl2_vector(dc, opnd_create_reg(DR_REG_Q16),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uaddl2, instr);

    instr = INSTR_CREATE_uaddl2_vector(dc, opnd_create_reg(DR_REG_Q16),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uaddl2, instr);

    instr = INSTR_CREATE_uaddl2_vector(dc, opnd_create_reg(DR_REG_Q16),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uaddl2, instr);

    instr = INSTR_CREATE_uaddw_vector(dc, opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_D12), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uaddw, instr);

    instr = INSTR_CREATE_uaddw_vector(dc, opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_D12), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uaddw, instr);

    instr = INSTR_CREATE_uaddw_vector(dc, opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_D12), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uaddw, instr);

    instr = INSTR_CREATE_uaddw2_vector(dc, opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q17), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uaddw2, instr);

    instr = INSTR_CREATE_uaddw2_vector(dc, opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q17), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uaddw2, instr);

    instr = INSTR_CREATE_uaddw2_vector(dc, opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q17), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uaddw2, instr);

    instr = INSTR_CREATE_usubl_vector(dc, opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D14), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_usubl, instr);

    instr = INSTR_CREATE_usubl_vector(dc, opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D14), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_usubl, instr);

    instr = INSTR_CREATE_usubl_vector(dc, opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D14), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_usubl, instr);

    instr = INSTR_CREATE_usubl2_vector(dc, opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q1), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_usubl2, instr);

    instr = INSTR_CREATE_usubl2_vector(dc, opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q1), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_usubl2, instr);

    instr = INSTR_CREATE_usubl2_vector(dc, opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q1), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_usubl2, instr);

    instr = INSTR_CREATE_usubw_vector(dc, opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q17),
                                      opnd_create_reg(DR_REG_D1), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_usubw, instr);

    instr = INSTR_CREATE_usubw_vector(dc, opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q17),
                                      opnd_create_reg(DR_REG_D1), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_usubw, instr);

    instr = INSTR_CREATE_usubw_vector(dc, opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q17),
                                      opnd_create_reg(DR_REG_D1), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_usubw, instr);

    instr = INSTR_CREATE_usubw2_vector(dc, opnd_create_reg(DR_REG_Q2),
                                       opnd_create_reg(DR_REG_Q3),
                                       opnd_create_reg(DR_REG_Q14), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_usubw2, instr);

    instr = INSTR_CREATE_usubw2_vector(dc, opnd_create_reg(DR_REG_Q2),
                                       opnd_create_reg(DR_REG_Q3),
                                       opnd_create_reg(DR_REG_Q14), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_usubw2, instr);

    instr = INSTR_CREATE_usubw2_vector(dc, opnd_create_reg(DR_REG_Q2),
                                       opnd_create_reg(DR_REG_Q3),
                                       opnd_create_reg(DR_REG_Q14), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_usubw2, instr);

    instr = INSTR_CREATE_raddhn_vector(dc, opnd_create_reg(DR_REG_D31),
                                       opnd_create_reg(DR_REG_Q15),
                                       opnd_create_reg(DR_REG_Q14), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_raddhn, instr);

    instr = INSTR_CREATE_raddhn_vector(dc, opnd_create_reg(DR_REG_D31),
                                       opnd_create_reg(DR_REG_Q15),
                                       opnd_create_reg(DR_REG_Q14), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_raddhn, instr);

    instr = INSTR_CREATE_raddhn_vector(dc, opnd_create_reg(DR_REG_D31),
                                       opnd_create_reg(DR_REG_Q15),
                                       opnd_create_reg(DR_REG_Q14), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_raddhn, instr);

    instr = INSTR_CREATE_raddhn2_vector(dc, opnd_create_reg(DR_REG_Q13),
                                        opnd_create_reg(DR_REG_Q16),
                                        opnd_create_reg(DR_REG_Q14), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_raddhn2, instr);

    instr = INSTR_CREATE_raddhn2_vector(dc, opnd_create_reg(DR_REG_Q13),
                                        opnd_create_reg(DR_REG_Q16),
                                        opnd_create_reg(DR_REG_Q14), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_raddhn2, instr);

    instr = INSTR_CREATE_raddhn2_vector(
        dc, opnd_create_reg(DR_REG_Q13), opnd_create_reg(DR_REG_Q16),
        opnd_create_reg(DR_REG_Q14), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_raddhn2, instr);

    instr = INSTR_CREATE_uabal_vector(dc, opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D22), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uabal, instr);

    instr = INSTR_CREATE_uabal_vector(dc, opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D22), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uabal, instr);

    instr = INSTR_CREATE_uabal_vector(dc, opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D22), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uabal, instr);

    instr = INSTR_CREATE_uabal2_vector(dc, opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q20),
                                       opnd_create_reg(DR_REG_Q29), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uabal2, instr);

    instr = INSTR_CREATE_uabal2_vector(dc, opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q20),
                                       opnd_create_reg(DR_REG_Q29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uabal2, instr);

    instr = INSTR_CREATE_uabal2_vector(dc, opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q20),
                                       opnd_create_reg(DR_REG_Q29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uabal2, instr);

    instr = INSTR_CREATE_rsubhn_vector(dc, opnd_create_reg(DR_REG_D4),
                                       opnd_create_reg(DR_REG_Q7),
                                       opnd_create_reg(DR_REG_Q19), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_rsubhn, instr);

    instr = INSTR_CREATE_rsubhn_vector(dc, opnd_create_reg(DR_REG_D4),
                                       opnd_create_reg(DR_REG_Q7),
                                       opnd_create_reg(DR_REG_Q19), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_rsubhn, instr);

    instr = INSTR_CREATE_rsubhn_vector(dc, opnd_create_reg(DR_REG_D4),
                                       opnd_create_reg(DR_REG_Q7),
                                       opnd_create_reg(DR_REG_Q19), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_rsubhn, instr);

    instr = INSTR_CREATE_rsubhn2_vector(dc, opnd_create_reg(DR_REG_Q21),
                                        opnd_create_reg(DR_REG_Q20),
                                        opnd_create_reg(DR_REG_Q18), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_rsubhn2, instr);

    instr = INSTR_CREATE_rsubhn2_vector(dc, opnd_create_reg(DR_REG_Q21),
                                        opnd_create_reg(DR_REG_Q20),
                                        opnd_create_reg(DR_REG_Q18), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_rsubhn2, instr);

    instr = INSTR_CREATE_rsubhn2_vector(
        dc, opnd_create_reg(DR_REG_Q21), opnd_create_reg(DR_REG_Q20),
        opnd_create_reg(DR_REG_Q18), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_rsubhn2, instr);

    instr = INSTR_CREATE_uabdl_vector(dc, opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D25), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uabdl, instr);

    instr = INSTR_CREATE_uabdl_vector(dc, opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D25), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uabdl, instr);

    instr = INSTR_CREATE_uabdl_vector(dc, opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D25), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uabdl, instr);

    instr = INSTR_CREATE_uabdl2_vector(dc, opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q27), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uabdl2, instr);

    instr = INSTR_CREATE_uabdl2_vector(dc, opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q27), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uabdl2, instr);

    instr = INSTR_CREATE_uabdl2_vector(dc, opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q13),
                                       opnd_create_reg(DR_REG_Q27), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uabdl2, instr);

    instr = INSTR_CREATE_umlal_vector(dc, opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D1), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umlal, instr);

    instr = INSTR_CREATE_umlal_vector(dc, opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D1), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umlal, instr);

    instr = INSTR_CREATE_umlal_vector(dc, opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D1), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umlal, instr);

    instr = INSTR_CREATE_umlal2_vector(dc, opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umlal2, instr);

    instr = INSTR_CREATE_umlal2_vector(dc, opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umlal2, instr);

    instr = INSTR_CREATE_umlal2_vector(dc, opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umlal2, instr);

    instr = INSTR_CREATE_umlsl_vector(dc, opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D21), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umlsl, instr);

    instr = INSTR_CREATE_umlsl_vector(dc, opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D21), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umlsl, instr);

    instr = INSTR_CREATE_umlsl_vector(dc, opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D21), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umlsl, instr);

    instr = INSTR_CREATE_umlsl2_vector(dc, opnd_create_reg(DR_REG_Q4),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q29), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umlsl2, instr);

    instr = INSTR_CREATE_umlsl2_vector(dc, opnd_create_reg(DR_REG_Q4),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umlsl2, instr);

    instr = INSTR_CREATE_umlsl2_vector(dc, opnd_create_reg(DR_REG_Q4),
                                       opnd_create_reg(DR_REG_Q19),
                                       opnd_create_reg(DR_REG_Q29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umlsl2, instr);

    instr = INSTR_CREATE_umull_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D2), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umull, instr);

    instr = INSTR_CREATE_umull_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umull, instr);

    instr = INSTR_CREATE_umull_vector(dc, opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D2), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umull, instr);

    instr = INSTR_CREATE_umull2_vector(dc, opnd_create_reg(DR_REG_Q6),
                                       opnd_create_reg(DR_REG_Q6),
                                       opnd_create_reg(DR_REG_Q3), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umull2, instr);

    instr = INSTR_CREATE_umull2_vector(dc, opnd_create_reg(DR_REG_Q6),
                                       opnd_create_reg(DR_REG_Q6),
                                       opnd_create_reg(DR_REG_Q3), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umull2, instr);

    instr = INSTR_CREATE_umull2_vector(dc, opnd_create_reg(DR_REG_Q6),
                                       opnd_create_reg(DR_REG_Q6),
                                       opnd_create_reg(DR_REG_Q3), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umull2, instr);
}

/* Macro expansion generates sequence of tests for creating cache instructions
 * which are aliases of SYS instruction variants:
 * DC ZVA, Xt        DC CVAC, Xt        DC CVAU, Xt        DC CIVAC Xt
 * DC IVAC Xt        DC ISW Xt          DC CSW Xt          DC CISW Xt
 * IC IVAU, Xt       IC IALLU           IC IALLUIS
 */

#define SYS_CACHE_TEST_ALL_REGS(cache, op)                               \
    do {                                                                 \
        for (int x = DR_REG_START_GPR; x < DR_REG_STOP_GPR; x++) {       \
            instr = INSTR_CREATE_##cache##_##op(dc, opnd_create_reg(x)); \
            test_instr_encoding(dc, OP_##cache##_##op, instr);           \
        }                                                                \
    } while (0)

static void
test_sys_cache(void *dc)
{
    instr_t *instr;

    /* SYS #<op1>, <Cn>, <Cm>, #<op2>{, <Xt>}
     *
     * Data cache operations are aliases of SYS:
     * DC <dc_op>, <Xt>
     * is equivalent to
     * SYS #<op1>, C7, <Cm>, #<op2>, <Xt>
     */

    /* DC ZVA, Xt => SYS #3, C7, C4, #1, Xt */
    instr = INSTR_CREATE_dc_zva(dc, opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_dc_zva, instr);
    SYS_CACHE_TEST_ALL_REGS(dc, zva);

    /* DC CVAC, Xt => SYS #3, C7, C10, #1, Xt */
    instr = INSTR_CREATE_dc_cvac(dc, opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_dc_cvac, instr);
    SYS_CACHE_TEST_ALL_REGS(dc, cvac);

    /* DC CVAU, Xt => SYS #3, C7, C11, #1, Xt */
    instr = INSTR_CREATE_dc_cvau(dc, opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_dc_cvau, instr);
    SYS_CACHE_TEST_ALL_REGS(dc, cvau);

    /* DC CIVAC Xt => SYS #3, C7, C14, #1, Xt */
    instr = INSTR_CREATE_dc_civac(dc, opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_dc_civac, instr);
    SYS_CACHE_TEST_ALL_REGS(dc, civac);

    /* DC IVAC Xt => SYS #0 C7 C6 #1, Xt */
    instr = INSTR_CREATE_dc_ivac(dc, opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_dc_ivac, instr);
    SYS_CACHE_TEST_ALL_REGS(dc, ivac);

    /* These instructions do not use the input register to hold a virtual
     * address. The register holds SetWay and cache level input.
     * DC ISW Xt => SYS #0 C7 C6 #2, Xt
     */
    instr = INSTR_CREATE_dc_isw(dc, opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_dc_isw, instr);
    SYS_CACHE_TEST_ALL_REGS(dc, isw);

    /* DC CSW Xt => SYS #0 C7 C10 #2, Xt */
    instr = INSTR_CREATE_dc_csw(dc, opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_dc_csw, instr);
    SYS_CACHE_TEST_ALL_REGS(dc, csw);

    /* DC CISW Xt => SYS #0 C7 C14 #2, Xt */
    instr = INSTR_CREATE_dc_cisw(dc, opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_dc_cisw, instr);
    SYS_CACHE_TEST_ALL_REGS(dc, cisw);

    /*
     * Similarly, instruction cache operations are also aliases of SYS:
     * IC <ic_op>{, <Xt>}
     * is equivalent to
     * SYS #<op1>, C7, <Cm>, #<op2>{, <Xt>}
     */

    /* IC IVAU, Xt => SYS #3, C7, C5, #1, Xt */
    instr = INSTR_CREATE_ic_ivau(dc, opnd_create_reg(DR_REG_X0));
    test_instr_encoding(dc, OP_ic_ivau, instr);
    SYS_CACHE_TEST_ALL_REGS(ic, ivau);

    /* IC IALLU => SYS #0 C7 C5 #0 */
    instr = INSTR_CREATE_ic_iallu(dc);
    test_instr_encoding(dc, OP_ic_iallu, instr);

    /* IC IALLUIS => SYS #0 C7 C1 #0 */
    instr = INSTR_CREATE_ic_ialluis(dc);
    test_instr_encoding(dc, OP_ic_ialluis, instr);
}

static void
test_exclusive_memops(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_ldxr(dc, opnd_create_reg(DR_REG_X0),
                              OPND_CREATE_MEM64(DR_REG_X1, 0));
    ASSERT(instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_ldxr, instr);
    instr = INSTR_CREATE_ldxrb(dc, opnd_create_reg(DR_REG_W0),
                               OPND_CREATE_MEM8(DR_REG_X1, 0));
    ASSERT(instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_ldxrb, instr);
    instr = INSTR_CREATE_ldxrh(dc, opnd_create_reg(DR_REG_W0),
                               OPND_CREATE_MEM16(DR_REG_X1, 0));
    ASSERT(instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_ldxrh, instr);
    instr = INSTR_CREATE_ldxp(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                              OPND_CREATE_MEM64(DR_REG_X2, 0));
    ASSERT(instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_ldxp, instr);
    instr =
        INSTR_CREATE_ldxp(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X1),
                          opnd_create_base_disp(DR_REG_X2, DR_REG_NULL, 0, 0, OPSZ_16));
    ASSERT(instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_ldxp, instr);
    instr = INSTR_CREATE_ldaxr(dc, opnd_create_reg(DR_REG_X0),
                               OPND_CREATE_MEM64(DR_REG_X1, 0));
    ASSERT(instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_ldaxr, instr);
    instr = INSTR_CREATE_ldaxrb(dc, opnd_create_reg(DR_REG_W0),
                                OPND_CREATE_MEM8(DR_REG_X1, 0));
    ASSERT(instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_ldaxrb, instr);
    instr = INSTR_CREATE_ldaxrh(dc, opnd_create_reg(DR_REG_W0),
                                OPND_CREATE_MEM16(DR_REG_X1, 0));
    ASSERT(instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_ldaxrh, instr);
    instr = INSTR_CREATE_ldaxp(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                               OPND_CREATE_MEM64(DR_REG_X2, 0));
    ASSERT(instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_ldaxp, instr);

    instr = INSTR_CREATE_stxr(dc, OPND_CREATE_MEM64(DR_REG_X1, 0),
                              opnd_create_reg(DR_REG_W2), opnd_create_reg(DR_REG_X0));
    ASSERT(instr_is_exclusive_store(instr));
    test_instr_encoding(dc, OP_stxr, instr);
    instr = INSTR_CREATE_stxrb(dc, OPND_CREATE_MEM8(DR_REG_X1, 0),
                               opnd_create_reg(DR_REG_W2), opnd_create_reg(DR_REG_W0));
    ASSERT(instr_is_exclusive_store(instr));
    test_instr_encoding(dc, OP_stxrb, instr);
    instr = INSTR_CREATE_stxrh(dc, OPND_CREATE_MEM16(DR_REG_X1, 0),
                               opnd_create_reg(DR_REG_W2), opnd_create_reg(DR_REG_W0));
    ASSERT(instr_is_exclusive_store(instr));
    test_instr_encoding(dc, OP_stxrh, instr);
    instr =
        INSTR_CREATE_stxp(dc, OPND_CREATE_MEM64(DR_REG_X2, 0), opnd_create_reg(DR_REG_W3),
                          opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1));
    ASSERT(instr_is_exclusive_store(instr));
    test_instr_encoding(dc, OP_stxp, instr);
    instr = INSTR_CREATE_stxp(
        dc, opnd_create_base_disp(DR_REG_X2, DR_REG_NULL, 0, 0, OPSZ_16),
        opnd_create_reg(DR_REG_W3), opnd_create_reg(DR_REG_X0),
        opnd_create_reg(DR_REG_X1));
    ASSERT(instr_is_exclusive_store(instr));
    test_instr_encoding(dc, OP_stxp, instr);
    instr = INSTR_CREATE_stlxr(dc, OPND_CREATE_MEM64(DR_REG_X1, 0),
                               opnd_create_reg(DR_REG_W2), opnd_create_reg(DR_REG_X0));
    ASSERT(instr_is_exclusive_store(instr));
    test_instr_encoding(dc, OP_stlxr, instr);
    instr = INSTR_CREATE_stlxrb(dc, OPND_CREATE_MEM8(DR_REG_X1, 0),
                                opnd_create_reg(DR_REG_W2), opnd_create_reg(DR_REG_W0));
    ASSERT(instr_is_exclusive_store(instr));
    test_instr_encoding(dc, OP_stlxrb, instr);
    instr = INSTR_CREATE_stlxrh(dc, OPND_CREATE_MEM16(DR_REG_X1, 0),
                                opnd_create_reg(DR_REG_W2), opnd_create_reg(DR_REG_W0));
    ASSERT(instr_is_exclusive_store(instr));
    test_instr_encoding(dc, OP_stlxrh, instr);
    instr = INSTR_CREATE_stlxp(dc, OPND_CREATE_MEM64(DR_REG_X2, 0),
                               opnd_create_reg(DR_REG_W3), opnd_create_reg(DR_REG_W0),
                               opnd_create_reg(DR_REG_W1));
    ASSERT(instr_is_exclusive_store(instr));
    test_instr_encoding(dc, OP_stlxp, instr);

    instr = INSTR_CREATE_clrex(dc);
    ASSERT(!instr_is_exclusive_store(instr) && !instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_clrex, instr);

    instr = INSTR_CREATE_clrex_imm(dc, 2);
    ASSERT(!instr_is_exclusive_store(instr) && !instr_is_exclusive_load(instr));
    test_instr_encoding(dc, OP_clrex, instr);
}

static void
test_xinst(void *dc)
{
    instr_t *instr;
    /* Sanity check of misc XINST_CREATE_ macros. */

    instr =
        XINST_CREATE_load_pair(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                               OPND_CREATE_MEM64(DR_REG_X2, 0));
    test_instr_encoding(dc, OP_ldp, instr);
    instr =
        XINST_CREATE_store_pair(dc, OPND_CREATE_MEM64(DR_REG_X2, 0),
                                opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1));
    test_instr_encoding(dc, OP_stp, instr);
    instr = XINST_CREATE_call_reg(dc, opnd_create_reg(DR_REG_X5));
    test_instr_encoding(dc, OP_blr, instr);
}

static void
test_opnd(void *dc)
{
    opnd_t op = opnd_create_reg_ex(DR_REG_X28, OPSZ_4, DR_OPND_EXTENDED);
    ASSERT(opnd_get_reg(op) == DR_REG_X28);
    ASSERT(opnd_is_reg_partial(op));
    ASSERT(opnd_get_size(op) == OPSZ_4);
    ASSERT(opnd_get_flags(op) == DR_OPND_EXTENDED);

    /* Ensure extra fields are preserved by opnd_replace_reg(). */
    bool found = opnd_replace_reg(&op, DR_REG_W28, DR_REG_W0);
    ASSERT(!found);
    found = opnd_replace_reg(&op, DR_REG_X28, DR_REG_X0);
    ASSERT(found);
    ASSERT(opnd_get_reg(op) == DR_REG_X0);
    ASSERT(opnd_is_reg_partial(op));
    ASSERT(opnd_get_size(op) == OPSZ_4);
    ASSERT(opnd_get_flags(op) == DR_OPND_EXTENDED);

    op = opnd_create_base_disp_aarch64(DR_REG_X7, DR_REG_NULL, DR_EXTEND_SXTX, true, 42,
                                       DR_OPND_EXTENDED, OPSZ_8);
    ASSERT(opnd_get_base(op) == DR_REG_X7);
    ASSERT(opnd_get_flags(op) == DR_OPND_EXTENDED);
    bool scaled;
    uint amount;
    dr_extend_type_t extend = opnd_get_index_extend(op, &scaled, &amount);
    ASSERT(extend == DR_EXTEND_SXTX && scaled && amount == 3);

    /* Ensure extra fields are preserved by opnd_replace_reg(). */
    found = opnd_replace_reg(&op, DR_REG_W7, DR_REG_W1);
    ASSERT(!found);
    found = opnd_replace_reg(&op, DR_REG_X7, DR_REG_X1);
    ASSERT(found);
    ASSERT(opnd_get_base(op) == DR_REG_X1);
    ASSERT(opnd_get_flags(op) == DR_OPND_EXTENDED);
    extend = opnd_get_index_extend(op, &scaled, &amount);
    ASSERT(extend == DR_EXTEND_SXTX && scaled && amount == 3);

    /* Another test but this time replacing an index register. */
    op = opnd_create_base_disp_aarch64(DR_REG_X7, DR_REG_X4, DR_EXTEND_UXTW, true, 0,
                                       DR_OPND_EXTENDED, OPSZ_8);
    ASSERT(opnd_get_base(op) == DR_REG_X7);
    ASSERT(opnd_get_index(op) == DR_REG_X4);
    ASSERT(opnd_get_flags(op) == DR_OPND_EXTENDED);
    extend = opnd_get_index_extend(op, &scaled, &amount);
    ASSERT(extend == DR_EXTEND_UXTW && scaled && amount == 3);

    /* Ensure extra fields are preserved by opnd_replace_reg(). */
    found = opnd_replace_reg(&op, DR_REG_W4, DR_REG_W1);
    ASSERT(!found);
    found = opnd_replace_reg(&op, DR_REG_X4, DR_REG_X1);
    ASSERT(found);
    ASSERT(opnd_get_base(op) == DR_REG_X7);
    ASSERT(opnd_get_index(op) == DR_REG_X1);
    ASSERT(opnd_get_flags(op) == DR_OPND_EXTENDED);
    extend = opnd_get_index_extend(op, &scaled, &amount);
    ASSERT(extend == DR_EXTEND_UXTW && scaled && amount == 3);

    instr_t *instr =
        INSTR_CREATE_stxp(dc, OPND_CREATE_MEM64(DR_REG_X2, 0), opnd_create_reg(DR_REG_W3),
                          opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1));
    found = instr_replace_reg_resize(instr, DR_REG_X3, DR_REG_X28);
    ASSERT(found);
    found = instr_replace_reg_resize(instr, DR_REG_W2, DR_REG_W14);
    ASSERT(found);
    ASSERT(opnd_get_base(instr_get_dst(instr, 0)) == DR_REG_X14);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 1)) == DR_REG_W28);
    ASSERT(opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_W0);
    ASSERT(opnd_get_reg(instr_get_src(instr, 1)) == DR_REG_W1);
    instr_destroy(dc, instr);

    /* Test reg corner cases. */
    ASSERT(reg_to_pointer_sized(DR_REG_WZR) == DR_REG_XZR);
    ASSERT(reg_32_to_64(DR_REG_WZR) == DR_REG_XZR);
    ASSERT(reg_64_to_32(DR_REG_XZR) == DR_REG_WZR);
    ASSERT(reg_resize_to_opsz(DR_REG_XZR, OPSZ_4) == DR_REG_WZR);
    ASSERT(reg_resize_to_opsz(DR_REG_WZR, OPSZ_8) == DR_REG_XZR);
    ASSERT(!reg_is_gpr(DR_REG_XZR));
    ASSERT(!reg_is_gpr(DR_REG_WZR));

    /* XXX: test other routines like opnd_defines_use(); test every flag such as
     * register negate and shift across replace and other operations.
     */
}

static void
test_mov_instr_addr_encoding(void *dc, instr_t *instr, uint opcode, uint target_off,
                             uint right_shift_amt, uint mask)
{
    ASSERT(opcode == OP_movz || opcode == OP_movk);
    instr_t *decin;
    byte *pc;

    ASSERT(instr_get_opcode(instr) == opcode);
    ASSERT(instr_is_encoding_possible(instr));

    pc = instr_encode(dc, instr, buf);
    decin = instr_create(dc);
    decode(dc, buf, decin);

    ASSERT(instr_get_opcode(decin) == opcode);

    uint src_op = opcode == OP_movz ? 0 : 1;
    uint expected_imm = (((ptr_int_t)buf + target_off) >> right_shift_amt) & mask;
    ASSERT(opnd_get_immed_int(instr_get_src(decin, src_op)) == expected_imm);

    instr_destroy(dc, instr);
    instr_destroy(dc, decin);
}

#ifdef DR_FAST_IR /* For offset field access. */
static void
test_mov_instr_addr(void *dc)
{
    instr_t *label_instr = instr_create_0dst_0src(dc, OP_LABEL);
    label_instr->offset = 0x100;

    instr_t *movz_instr_sh0_2b = INSTR_CREATE_movz(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_instr_ex(label_instr, OPSZ_2, 0),
        OPND_CREATE_INT(0));
    movz_instr_sh0_2b->offset = 0x10;
    test_mov_instr_addr_encoding(dc, movz_instr_sh0_2b, OP_movz, 0xf0, 0, 0xffff);

    instr_t *movz_instr_sh16_2b = INSTR_CREATE_movk(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_instr_ex(label_instr, OPSZ_2, 16),
        OPND_CREATE_INT(0));
    movz_instr_sh16_2b->offset = 0x20;
    test_mov_instr_addr_encoding(dc, movz_instr_sh16_2b, OP_movk, 0xe0, 16, 0xffff);

    instr_t *movz_instr_sh32_2b = INSTR_CREATE_movz(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_instr_ex(label_instr, OPSZ_2, 32),
        OPND_CREATE_INT(0));
    movz_instr_sh32_2b->offset = 0x30;
    test_mov_instr_addr_encoding(dc, movz_instr_sh32_2b, OP_movz, 0xd0, 32, 0xffff);

    instr_t *movz_instr_sh48_2b = INSTR_CREATE_movk(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_instr_ex(label_instr, OPSZ_2, 48),
        OPND_CREATE_INT(0));
    movz_instr_sh48_2b->offset = 0x40;
    test_mov_instr_addr_encoding(dc, movz_instr_sh48_2b, OP_movk, 0xc0, 48, 0xffff);

    instr_t *movz_instr_sh0_1b = INSTR_CREATE_movk(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_instr_ex(label_instr, OPSZ_1, 0),
        OPND_CREATE_INT(0));
    movz_instr_sh0_1b->offset = 0x10;
    test_mov_instr_addr_encoding(dc, movz_instr_sh0_1b, OP_movk, 0xf0, 0, 0xff);

    instr_t *movz_instr_sh16_1b = INSTR_CREATE_movz(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_instr_ex(label_instr, OPSZ_1, 16),
        OPND_CREATE_INT(0));
    movz_instr_sh16_1b->offset = 0x20;
    test_mov_instr_addr_encoding(dc, movz_instr_sh16_1b, OP_movz, 0xe0, 16, 0xff);

    instr_t *movz_instr_sh32_1b = INSTR_CREATE_movk(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_instr_ex(label_instr, OPSZ_1, 32),
        OPND_CREATE_INT(0));
    movz_instr_sh32_1b->offset = 0x30;
    test_mov_instr_addr_encoding(dc, movz_instr_sh32_1b, OP_movk, 0xd0, 32, 0xff);

    instr_t *movz_instr_sh48_1b = INSTR_CREATE_movz(
        dc, opnd_create_reg(DR_REG_X0), opnd_create_instr_ex(label_instr, OPSZ_1, 48),
        OPND_CREATE_INT(0));
    movz_instr_sh48_1b->offset = 0x40;
    test_mov_instr_addr_encoding(dc, movz_instr_sh48_1b, OP_movz, 0xc0, 48, 0xff);

    instr_destroy(dc, label_instr);
}
#endif

static void
test_fcvtas_scalar(void *dc)
{
    instr_t *instr;
    /* FCVTAS <Wd>, <Sn> */
    instr = INSTR_CREATE_fcvtas_scalar(dc, opnd_create_reg(DR_REG_W20),
                                       opnd_create_reg(DR_REG_S1));
    test_instr_encoding(dc, OP_fcvtas, instr);

    /* FCVTAS <Xd>, <Sn> */
    instr = INSTR_CREATE_fcvtas_scalar(dc, opnd_create_reg(DR_REG_X7),
                                       opnd_create_reg(DR_REG_S3));
    test_instr_encoding(dc, OP_fcvtas, instr);

    /* FCVTAS <Wd>, <Dn> */
    instr = INSTR_CREATE_fcvtas_scalar(dc, opnd_create_reg(DR_REG_W0),
                                       opnd_create_reg(DR_REG_D22));
    test_instr_encoding(dc, OP_fcvtas, instr);

    /* FCVTAS <Xd>, <Dn> */
    instr = INSTR_CREATE_fcvtas_scalar(dc, opnd_create_reg(DR_REG_X21),
                                       opnd_create_reg(DR_REG_D0));
    test_instr_encoding(dc, OP_fcvtas, instr);
}

static void
test_fcvtas_vector(void *dc)
{
    instr_t *instr;

    /* FCVTAS <Vd>.<T>, <Vn>.<T> */
    /* FCVTAS <Vd>.2S, <Vn>.2S */
    instr = INSTR_CREATE_fcvtas_vector(dc, opnd_create_reg(DR_REG_D7),
                                       opnd_create_reg(DR_REG_D1), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtas, instr);

    /* FCVTAS <Vd>.4S, <Vn>.4S */
    instr = INSTR_CREATE_fcvtas_vector(dc, opnd_create_reg(DR_REG_Q0),
                                       opnd_create_reg(DR_REG_Q9), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtas, instr);

    /* FCVTAS <Vd>.2D, <Vn>.2D */
    instr = INSTR_CREATE_fcvtas_vector(dc, opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q29), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtas, instr);

    /* FCVTAS <V><d>, <V><n> */
    /* FCVTAS <V>S, <V>S */
    instr = INSTR_CREATE_fcvtas_scalar(dc, opnd_create_reg(DR_REG_S30),
                                       opnd_create_reg(DR_REG_S30));
    test_instr_encoding(dc, OP_fcvtas, instr);

    /* FCVTAS <V>D, <V>D */
    instr = INSTR_CREATE_fcvtas_scalar(dc, opnd_create_reg(DR_REG_D7),
                                       opnd_create_reg(DR_REG_D12));
    test_instr_encoding(dc, OP_fcvtas, instr);
}

static void
test_fcvtns_scalar(void *dc)
{
    instr_t *instr;

    /* FCVTNS <Wd>, <Sn> */
    instr = INSTR_CREATE_fcvtns_scalar(dc, opnd_create_reg(DR_REG_W21),
                                       opnd_create_reg(DR_REG_S8));
    test_instr_encoding(dc, OP_fcvtns, instr);

    /* FCVTNS <Xd>, <Sn> */
    instr = INSTR_CREATE_fcvtns_scalar(dc, opnd_create_reg(DR_REG_X14),
                                       opnd_create_reg(DR_REG_S21));
    test_instr_encoding(dc, OP_fcvtns, instr);

    /* FCVTNS <Wd>, <Dn> */
    instr = INSTR_CREATE_fcvtns_scalar(dc, opnd_create_reg(DR_REG_W7),
                                       opnd_create_reg(DR_REG_D29));
    test_instr_encoding(dc, OP_fcvtns, instr);

    /* FCVTNS <Xd>, <Dn> */
    instr = INSTR_CREATE_fcvtns_scalar(dc, opnd_create_reg(DR_REG_X9),
                                       opnd_create_reg(DR_REG_D17));
    test_instr_encoding(dc, OP_fcvtns, instr);
}

static void
test_fcvtns_vector(void *dc)
{
    instr_t *instr;

    /* FCVTNS <Vd>.<T>, <Vn>.<T> */
    /* FCVTNS <Vd>.2S, <Vn>.2S */
    instr = INSTR_CREATE_fcvtns_vector(dc, opnd_create_reg(DR_REG_D5),
                                       opnd_create_reg(DR_REG_D9), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtns, instr);

    /* FCVTNS <Vd>.4S, <Vn>.4S */
    instr = INSTR_CREATE_fcvtns_vector(dc, opnd_create_reg(DR_REG_Q1),
                                       opnd_create_reg(DR_REG_Q19), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtns, instr);

    /* FCVTNS <Vd>.2D, <Vn>.2D */
    instr = INSTR_CREATE_fcvtns_vector(dc, opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q11), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtns, instr);

    /* FCVTNS <V><d>, <V><n> */
    /* FCVTNS <V>S, <V>S */
    instr = INSTR_CREATE_fcvtns_scalar(dc, opnd_create_reg(DR_REG_S9),
                                       opnd_create_reg(DR_REG_S2));
    test_instr_encoding(dc, OP_fcvtns, instr);

    /* FCVTNS <V>D, <V>D */
    instr = INSTR_CREATE_fcvtns_scalar(dc, opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D7));
    test_instr_encoding(dc, OP_fcvtns, instr);
}

static void
test_fcvtps_scalar(void *dc)
{
    instr_t *instr;

    /* FCVTPS <Wd>, <Sn> */
    instr = INSTR_CREATE_fcvtps_scalar(dc, opnd_create_reg(DR_REG_W19),
                                       opnd_create_reg(DR_REG_S7));
    test_instr_encoding(dc, OP_fcvtps, instr);

    /* FCVTPS <Xd>, <Sn> */
    instr = INSTR_CREATE_fcvtps_scalar(dc, opnd_create_reg(DR_REG_X5),
                                       opnd_create_reg(DR_REG_S4));
    test_instr_encoding(dc, OP_fcvtps, instr);

    /* FCVTPS <Wd>, <Dn> */
    instr = INSTR_CREATE_fcvtps_scalar(dc, opnd_create_reg(DR_REG_W8),
                                       opnd_create_reg(DR_REG_D10));
    test_instr_encoding(dc, OP_fcvtps, instr);

    /* FCVTPS <Xd>, <Dn> */
    instr = INSTR_CREATE_fcvtps_scalar(dc, opnd_create_reg(DR_REG_X9),
                                       opnd_create_reg(DR_REG_D18));
    test_instr_encoding(dc, OP_fcvtps, instr);
}

static void
test_fcvtps_vector(void *dc)
{
    instr_t *instr;

    /* FCVTPS <Vd>.<T>, <Vn>.<T> */
    /* FCVTPS <Vd>.2S, <Vn>.2S */
    instr = INSTR_CREATE_fcvtps_vector(dc, opnd_create_reg(DR_REG_D6),
                                       opnd_create_reg(DR_REG_D9), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtps, instr);

    /* FCVTPS <Vd>.4S, <Vn>.4S */
    instr = INSTR_CREATE_fcvtps_vector(dc, opnd_create_reg(DR_REG_Q4),
                                       opnd_create_reg(DR_REG_Q20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtps, instr);

    /* FCVTPS <Vd>.2D, <Vn>.2D */
    instr = INSTR_CREATE_fcvtps_vector(dc, opnd_create_reg(DR_REG_Q15),
                                       opnd_create_reg(DR_REG_Q0), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtps, instr);

    /* FCVTPS <V><d>, <V><n> */
    /* FCVTPS <V>S, <V>S */
    instr = INSTR_CREATE_fcvtps_scalar(dc, opnd_create_reg(DR_REG_S29),
                                       opnd_create_reg(DR_REG_S4));
    test_instr_encoding(dc, OP_fcvtps, instr);

    /* FCVTPS <V>D, <V>D */
    instr = INSTR_CREATE_fcvtps_scalar(dc, opnd_create_reg(DR_REG_D12),
                                       opnd_create_reg(DR_REG_D16));
    test_instr_encoding(dc, OP_fcvtps, instr);
}

static void
test_fcvtpu_scalar(void *dc)
{
    instr_t *instr;

    /* FCVTPU <Wd>, <Sn> */
    instr = INSTR_CREATE_fcvtpu_scalar(dc, opnd_create_reg(DR_REG_W1),
                                       opnd_create_reg(DR_REG_S2));
    test_instr_encoding(dc, OP_fcvtpu, instr);

    /* FCVTPU <Xd>, <Sn> */
    instr = INSTR_CREATE_fcvtpu_scalar(dc, opnd_create_reg(DR_REG_X14),
                                       opnd_create_reg(DR_REG_S14));
    test_instr_encoding(dc, OP_fcvtpu, instr);

    /* FCVTPU <Wd>, <Dn> */
    instr = INSTR_CREATE_fcvtpu_scalar(dc, opnd_create_reg(DR_REG_W4),
                                       opnd_create_reg(DR_REG_D2));
    test_instr_encoding(dc, OP_fcvtpu, instr);

    /* FCVTPU <Xd>, <Dn> */
    instr = INSTR_CREATE_fcvtpu_scalar(dc, opnd_create_reg(DR_REG_X9),
                                       opnd_create_reg(DR_REG_D1));
    test_instr_encoding(dc, OP_fcvtpu, instr);
}

static void
test_fcvtpu_vector(void *dc)
{
    instr_t *instr;

    /* FCVTPU <Vd>.<T>, <Vn>.<T> */
    /* FCVTPU <Vd>.2S, <Vn>.2S */
    instr = INSTR_CREATE_fcvtpu_vector(dc, opnd_create_reg(DR_REG_D1),
                                       opnd_create_reg(DR_REG_D24), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtpu, instr);

    /* FCVTPU <Vd>.4S, <Vn>.4S */
    instr = INSTR_CREATE_fcvtpu_vector(dc, opnd_create_reg(DR_REG_Q22),
                                       opnd_create_reg(DR_REG_Q21), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtpu, instr);

    /* FCVTPU <Vd>.2D, <Vn>.2D */
    instr = INSTR_CREATE_fcvtpu_vector(dc, opnd_create_reg(DR_REG_Q11),
                                       opnd_create_reg(DR_REG_Q11), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtpu, instr);

    /* FCVTPU <V><d>, <V><n> */
    /* FCVTPU <V>S, <V>S */
    instr = INSTR_CREATE_fcvtpu_scalar(dc, opnd_create_reg(DR_REG_S27),
                                       opnd_create_reg(DR_REG_S21));
    test_instr_encoding(dc, OP_fcvtpu, instr);

    /* FCVTPU <V>D, <V>D */
    instr = INSTR_CREATE_fcvtpu_scalar(dc, opnd_create_reg(DR_REG_D12),
                                       opnd_create_reg(DR_REG_D18));
    test_instr_encoding(dc, OP_fcvtpu, instr);
}

static void
test_fcvtzs_scalar(void *dc)
{
    instr_t *instr;
    /* FCVTZS <Wd>, <Sn> */
    instr = INSTR_CREATE_fcvtzs_scalar(dc, opnd_create_reg(DR_REG_W11),
                                       opnd_create_reg(DR_REG_S8));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <Xd>, <Sn> */
    instr = INSTR_CREATE_fcvtzs_scalar(dc, opnd_create_reg(DR_REG_X14),
                                       opnd_create_reg(DR_REG_S3));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <Wd>, <Dn> */
    instr = INSTR_CREATE_fcvtzs_scalar(dc, opnd_create_reg(DR_REG_W0),
                                       opnd_create_reg(DR_REG_D28));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <Xd>, <Dn> */
    instr = INSTR_CREATE_fcvtzs_scalar(dc, opnd_create_reg(DR_REG_X9),
                                       opnd_create_reg(DR_REG_D1));
    test_instr_encoding(dc, OP_fcvtzs, instr);
}

static void
test_fcvtzs_vector(void *dc)
{
    instr_t *instr;

    /* FCVTZS <Vd>.<T>, <Vn>.<T> */
    /* FCVTZS <Vd>.2S, <Vn>.2S */
    instr = INSTR_CREATE_fcvtzs_vector(dc, opnd_create_reg(DR_REG_D3),
                                       opnd_create_reg(DR_REG_D8), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <Vd>.4S, <Vn>.4S */
    instr = INSTR_CREATE_fcvtzs_vector(dc, opnd_create_reg(DR_REG_Q9),
                                       opnd_create_reg(DR_REG_Q21), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <Vd>.2D, <Vn>.2D */
    instr = INSTR_CREATE_fcvtzs_vector(dc, opnd_create_reg(DR_REG_Q11),
                                       opnd_create_reg(DR_REG_Q2), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <V><d>, <V><n> */
    /* FCVTZS <V>S, <V>S */
    instr = INSTR_CREATE_fcvtzs_scalar(dc, opnd_create_reg(DR_REG_S3),
                                       opnd_create_reg(DR_REG_S3));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <V>D, <V>D */
    instr = INSTR_CREATE_fcvtzs_scalar(dc, opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D7));
    test_instr_encoding(dc, OP_fcvtzs, instr);
}

static void
test_fcvtzs_scalar_fixed_gpr(void *dc)
{
    instr_t *instr;

    /* FCVTZS <Wd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_W7),
                                             opnd_create_reg(DR_REG_S8),
                                             opnd_create_immed_int(4, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <Xd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_X13),
                                             opnd_create_reg(DR_REG_S21),
                                             opnd_create_immed_int(16, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <Wd>, <Dn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_W30),
                                             opnd_create_reg(DR_REG_D9),
                                             opnd_create_immed_int(32, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <Xd>, <Dn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_X12),
                                             opnd_create_reg(DR_REG_D15),
                                             opnd_create_immed_int(64, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);
}

static void
test_fcvtzs_scalar_fixed(void *dc)
{
    instr_t *instr;

    /* FCVTZS <Sd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_S0),
                                             opnd_create_reg(DR_REG_S1),
                                             opnd_create_immed_int(1, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_S2),
                                             opnd_create_reg(DR_REG_S3),
                                             opnd_create_immed_int(2, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_S4),
                                             opnd_create_reg(DR_REG_S5),
                                             opnd_create_immed_int(4, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_S6),
                                             opnd_create_reg(DR_REG_S7),
                                             opnd_create_immed_int(8, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_S8),
                                             opnd_create_reg(DR_REG_S9),
                                             opnd_create_immed_int(16, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_S10),
                                             opnd_create_reg(DR_REG_S11),
                                             opnd_create_immed_int(32, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_S28),
                                             opnd_create_reg(DR_REG_S29),
                                             opnd_create_immed_int(21, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_S30),
                                             opnd_create_reg(DR_REG_S31),
                                             opnd_create_immed_int(31, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    /* FCVTZS <Dd>, <Dn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_D0),
                                             opnd_create_reg(DR_REG_D1),
                                             opnd_create_immed_int(1, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_D2),
                                             opnd_create_reg(DR_REG_D3),
                                             opnd_create_immed_int(2, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_D4),
                                             opnd_create_reg(DR_REG_D5),
                                             opnd_create_immed_int(4, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_D6),
                                             opnd_create_reg(DR_REG_D7),
                                             opnd_create_immed_int(8, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_D8),
                                             opnd_create_reg(DR_REG_D9),
                                             opnd_create_immed_int(16, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_D10),
                                             opnd_create_reg(DR_REG_D11),
                                             opnd_create_immed_int(32, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_D12),
                                             opnd_create_reg(DR_REG_D13),
                                             opnd_create_immed_int(64, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_D28),
                                             opnd_create_reg(DR_REG_D29),
                                             opnd_create_immed_int(21, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);

    instr = INSTR_CREATE_fcvtzs_scalar_fixed(dc, opnd_create_reg(DR_REG_D30),
                                             opnd_create_reg(DR_REG_D31),
                                             opnd_create_immed_int(42, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzs, instr);
}

static void
test_fcvtzu_scalar(void *dc)
{
    instr_t *instr;
    /* FCVTZU <Wd>, <Sn> */
    instr = INSTR_CREATE_fcvtzu_scalar(dc, opnd_create_reg(DR_REG_W7),
                                       opnd_create_reg(DR_REG_S8));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Xd>, <Sn> */
    instr = INSTR_CREATE_fcvtzu_scalar(dc, opnd_create_reg(DR_REG_X13),
                                       opnd_create_reg(DR_REG_S21));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Wd>, <Dn> */
    instr = INSTR_CREATE_fcvtzu_scalar(dc, opnd_create_reg(DR_REG_W0),
                                       opnd_create_reg(DR_REG_D9));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Xd>, <Dn> */
    instr = INSTR_CREATE_fcvtzu_scalar(dc, opnd_create_reg(DR_REG_X12),
                                       opnd_create_reg(DR_REG_D12));
    test_instr_encoding(dc, OP_fcvtzu, instr);
}

static void
test_fcvtzu_vector(void *dc)
{
    instr_t *instr;

    /* FCVTZU <Vd>.<T>, <Vn>.<T> */
    /* FCVTZU <Vd>.2S, <Vn>.2S */
    instr = INSTR_CREATE_fcvtzu_vector(dc, opnd_create_reg(DR_REG_D7),
                                       opnd_create_reg(DR_REG_D9), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Vd>.4S, <Vn>.4S */
    instr = INSTR_CREATE_fcvtzu_vector(dc, opnd_create_reg(DR_REG_Q1),
                                       opnd_create_reg(DR_REG_Q24), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Vd>.2D, <Vn>.2D */
    instr = INSTR_CREATE_fcvtzu_vector(dc, opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q18), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <V><d>, <V><n> */
    /* FCVTZU <V>S, <V>S */
    instr = INSTR_CREATE_fcvtzu_scalar(dc, opnd_create_reg(DR_REG_S9),
                                       opnd_create_reg(DR_REG_S10));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <V>D, <V>D */
    instr = INSTR_CREATE_fcvtzu_scalar(dc, opnd_create_reg(DR_REG_D11),
                                       opnd_create_reg(DR_REG_D0));
    test_instr_encoding(dc, OP_fcvtzu, instr);
}

static void
test_fcvtzu_scalar_fixed_gpr(void *dc)
{
    instr_t *instr;

    /* FCVTZU <Wd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_W7),
                                             opnd_create_reg(DR_REG_S8),
                                             opnd_create_immed_int(4, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Xd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_X13),
                                             opnd_create_reg(DR_REG_S21),
                                             opnd_create_immed_int(16, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Wd>, <Dn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_W30),
                                             opnd_create_reg(DR_REG_D9),
                                             opnd_create_immed_int(32, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Xd>, <Dn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_X12),
                                             opnd_create_reg(DR_REG_D15),
                                             opnd_create_immed_int(64, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);
}

static void
test_fcvtzu_scalar_fixed(void *dc)
{
    instr_t *instr;

    /* FCVTZU <Sd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_S0),
                                             opnd_create_reg(DR_REG_S1),
                                             opnd_create_immed_int(1, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_S2),
                                             opnd_create_reg(DR_REG_S3),
                                             opnd_create_immed_int(2, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_S4),
                                             opnd_create_reg(DR_REG_S5),
                                             opnd_create_immed_int(4, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_S6),
                                             opnd_create_reg(DR_REG_S7),
                                             opnd_create_immed_int(8, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_S8),
                                             opnd_create_reg(DR_REG_S9),
                                             opnd_create_immed_int(16, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_S10),
                                             opnd_create_reg(DR_REG_S11),
                                             opnd_create_immed_int(32, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_S28),
                                             opnd_create_reg(DR_REG_S29),
                                             opnd_create_immed_int(21, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_S30),
                                             opnd_create_reg(DR_REG_S31),
                                             opnd_create_immed_int(31, OPSZ_5b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Dd>, <Dn>, #<fbits> */
    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_D0),
                                             opnd_create_reg(DR_REG_D1),
                                             opnd_create_immed_int(1, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_D2),
                                             opnd_create_reg(DR_REG_D3),
                                             opnd_create_immed_int(2, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_D4),
                                             opnd_create_reg(DR_REG_D5),
                                             opnd_create_immed_int(4, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_D6),
                                             opnd_create_reg(DR_REG_D7),
                                             opnd_create_immed_int(8, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_D8),
                                             opnd_create_reg(DR_REG_D9),
                                             opnd_create_immed_int(16, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_D10),
                                             opnd_create_reg(DR_REG_D11),
                                             opnd_create_immed_int(32, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_D12),
                                             opnd_create_reg(DR_REG_D13),
                                             opnd_create_immed_int(64, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_D28),
                                             opnd_create_reg(DR_REG_D29),
                                             opnd_create_immed_int(21, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_scalar_fixed(dc, opnd_create_reg(DR_REG_D30),
                                             opnd_create_reg(DR_REG_D31),
                                             opnd_create_immed_int(42, OPSZ_6b));
    test_instr_encoding(dc, OP_fcvtzu, instr);
}

static void
test_fcvtzu_vector_fixed(void *dc)
{
    instr_t *instr;

    /* FCVTZU <Vd>.<T>, <Vn>.<T>, #<fbits> */

    /* FCVTZU <Vd>.4s, <Vn>.4s, #<fbits> */
    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
        opnd_create_immed_int(1, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q2), opnd_create_reg(DR_REG_Q3),
        opnd_create_immed_int(2, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q4), opnd_create_reg(DR_REG_Q5),
        opnd_create_immed_int(4, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q6), opnd_create_reg(DR_REG_Q7),
        opnd_create_immed_int(8, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q8), opnd_create_reg(DR_REG_Q9),
        opnd_create_immed_int(16, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q10), opnd_create_reg(DR_REG_Q11),
        opnd_create_immed_int(32, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q28), opnd_create_reg(DR_REG_Q29),
        opnd_create_immed_int(21, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q30), opnd_create_reg(DR_REG_Q31),
        opnd_create_immed_int(31, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Vd>.2d, <Vn>.2d, #<fbits> */
    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q0), opnd_create_reg(DR_REG_Q1),
        opnd_create_immed_int(1, OPSZ_6b), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q2), opnd_create_reg(DR_REG_Q3),
        opnd_create_immed_int(2, OPSZ_6b), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q4), opnd_create_reg(DR_REG_Q5),
        opnd_create_immed_int(4, OPSZ_6b), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q6), opnd_create_reg(DR_REG_Q7),
        opnd_create_immed_int(8, OPSZ_6b), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q8), opnd_create_reg(DR_REG_Q9),
        opnd_create_immed_int(16, OPSZ_6b), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q10), opnd_create_reg(DR_REG_Q11),
        opnd_create_immed_int(32, OPSZ_6b), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q12), opnd_create_reg(DR_REG_Q13),
        opnd_create_immed_int(64, OPSZ_6b), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q28), opnd_create_reg(DR_REG_Q29),
        opnd_create_immed_int(21, OPSZ_6b), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_Q30), opnd_create_reg(DR_REG_Q31),
        opnd_create_immed_int(42, OPSZ_6b), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    /* FCVTZU <Vd>.2s, <Vn>.2s, #<fbits> */
    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_D0), opnd_create_reg(DR_REG_D1),
        opnd_create_immed_int(1, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_D2), opnd_create_reg(DR_REG_D3),
        opnd_create_immed_int(2, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_D4), opnd_create_reg(DR_REG_D5),
        opnd_create_immed_int(4, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_D6), opnd_create_reg(DR_REG_D7),
        opnd_create_immed_int(8, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_D8), opnd_create_reg(DR_REG_D9),
        opnd_create_immed_int(16, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_D10), opnd_create_reg(DR_REG_D11),
        opnd_create_immed_int(32, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_D28), opnd_create_reg(DR_REG_D29),
        opnd_create_immed_int(21, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);

    instr = INSTR_CREATE_fcvtzu_vector_fixed(
        dc, opnd_create_reg(DR_REG_D30), opnd_create_reg(DR_REG_D31),
        opnd_create_immed_int(31, OPSZ_5b), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcvtzu, instr);
}

static void
test_sli_vector(void *dc)
{
    instr_t *instr;

    /* SLI <Vd>.<T>, <Vn>.<T>, #<shift> */

    /* SLI <Vd>.16b, <Vn>.16b, #<shift> */
    for (uint shift_amount = 0; shift_amount <= 7; shift_amount++) {
        instr = INSTR_CREATE_sli_vector(
            dc, opnd_create_reg(Q_registers[(2 * shift_amount - 1) % 30]),
            opnd_create_reg(Q_registers[(2 * shift_amount - 2) % 30]), OPND_CREATE_BYTE(),
            opnd_create_immed_int(shift_amount, OPSZ_3b));
        test_instr_encoding(dc, OP_sli, instr);
    }

    /* SLI <Vd>.8h, <Vn>.8h, #<shift> */
    for (uint shift_amount = 0; shift_amount <= 15; shift_amount++) {
        instr = INSTR_CREATE_sli_vector(
            dc, opnd_create_reg(Q_registers[(2 * shift_amount - 1) % 30]),
            opnd_create_reg(Q_registers[(2 * shift_amount - 2) % 30]), OPND_CREATE_HALF(),
            opnd_create_immed_int(shift_amount, OPSZ_4b));
        test_instr_encoding(dc, OP_sli, instr);
    }

    /* SLI <Vd>.4s, <Vn>.4s, #<shift> */
    for (uint shift_amount = 0; shift_amount <= 31; shift_amount++) {
        instr = INSTR_CREATE_sli_vector(
            dc, opnd_create_reg(Q_registers[(2 * shift_amount - 1) % 30]),
            opnd_create_reg(Q_registers[(2 * shift_amount - 2) % 30]),
            OPND_CREATE_SINGLE(), opnd_create_immed_int(shift_amount, OPSZ_5b));
        test_instr_encoding(dc, OP_sli, instr);
    }

    /* SLI <Vd>.2d, <Vn>.2d, #<shift> */
    for (uint shift_amount = 0; shift_amount <= 63; shift_amount++) {
        instr = INSTR_CREATE_sli_vector(
            dc, opnd_create_reg(Q_registers[(2 * shift_amount - 1) % 30]),
            opnd_create_reg(Q_registers[(2 * shift_amount - 2) % 30]),
            OPND_CREATE_DOUBLE(), opnd_create_immed_int(shift_amount, OPSZ_6b));
        test_instr_encoding(dc, OP_sli, instr);
    }
}

static void
test_uqshrn_vector(void *dc)
{
    instr_t *instr;

    /* UQSHRN{2} <Vd>.<Tb>, <Vn>.<Ta>, #<shift> */

    /* UQSHRN <Vd>.8b, <Vn>.8h, #<shift> */
    for (uint shift_amount = 1; shift_amount <= 8; shift_amount++) {
        instr = INSTR_CREATE_uqshrn_vector(
            dc, opnd_create_reg(D_registers[(2 * shift_amount - 1) % 30]),
            opnd_create_reg(Q_registers[(2 * shift_amount - 2) % 30]), OPND_CREATE_HALF(),
            opnd_create_immed_int(shift_amount, OPSZ_3b));
        test_instr_encoding(dc, OP_uqshrn, instr);
    }

    /* UQSHRN <Vd>.4h, <Vn>.4s, #<shift> */
    for (uint shift_amount = 1; shift_amount <= 16; shift_amount++) {
        instr = INSTR_CREATE_uqshrn_vector(
            dc, opnd_create_reg(D_registers[(2 * shift_amount - 1) % 30]),
            opnd_create_reg(Q_registers[(2 * shift_amount - 2) % 30]),
            OPND_CREATE_SINGLE(), opnd_create_immed_int(shift_amount, OPSZ_4b));
        test_instr_encoding(dc, OP_uqshrn, instr);
    }

    /* UQSHRN <Vd>.2s, <Vn>.2d, #<shift> */
    for (uint shift_amount = 1; shift_amount <= 32; shift_amount++) {
        instr = INSTR_CREATE_uqshrn_vector(
            dc, opnd_create_reg(D_registers[(2 * shift_amount - 1) % 30]),
            opnd_create_reg(Q_registers[(2 * shift_amount - 2) % 30]),
            OPND_CREATE_DOUBLE(), opnd_create_immed_int(shift_amount, OPSZ_5b));
        test_instr_encoding(dc, OP_uqshrn, instr);
    }
}

static void
test_ucvtf_scalar(void *dc)
{
    instr_t *instr;
    instr = INSTR_CREATE_ucvtf_scalar(dc, opnd_create_reg(DR_REG_S4),
                                      opnd_create_reg(DR_REG_W9));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar(dc, opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_W28));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar(dc, opnd_create_reg(DR_REG_S1),
                                      opnd_create_reg(DR_REG_X21));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar(dc, opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_X2));
    test_instr_encoding(dc, OP_ucvtf, instr);
}

static void
test_ucvtf_vector(void *dc)
{
    instr_t *instr;

    /* UCVTF <Vd>.<T>, <Vn>.<T> */
    /* UCVTF <Vd>.2S, <Vn>.2S */
    instr = INSTR_CREATE_ucvtf_vector(dc, opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D7), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ucvtf, instr);

    /* UCVTF <Vd>.4S, <Vn>.4S */
    instr = INSTR_CREATE_ucvtf_vector(dc, opnd_create_reg(DR_REG_Q12),
                                      opnd_create_reg(DR_REG_Q24), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ucvtf, instr);

    /* UCVTF <Vd>.2D, <Vn>.2D */
    instr = INSTR_CREATE_ucvtf_vector(dc, opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q1), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_ucvtf, instr);

    /* UCVTF <V><d>, <V><n> */
    /* UCVTF <V>S, <V>S */
    instr = INSTR_CREATE_ucvtf_scalar(dc, opnd_create_reg(DR_REG_S17),
                                      opnd_create_reg(DR_REG_S20));
    test_instr_encoding(dc, OP_ucvtf, instr);

    /* UCVTF <V>D, <V>D */
    instr = INSTR_CREATE_ucvtf_scalar(dc, opnd_create_reg(DR_REG_D14),
                                      opnd_create_reg(DR_REG_D14));
    test_instr_encoding(dc, OP_ucvtf, instr);
}

static void
test_ucvtf_scalar_fixed_gpr(void *dc)
{
    instr_t *instr;

    /* UCVTF <Sd>, <Wn>, #<fbits> */
    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S5),
                                            opnd_create_reg(DR_REG_W8),
                                            opnd_create_immed_int(4, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    /* UCVTF <Sd>, <Xn>, #<fbits> */
    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S13),
                                            opnd_create_reg(DR_REG_X7),
                                            opnd_create_immed_int(16, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    /* UCVTF <Dd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D17),
                                            opnd_create_reg(DR_REG_W0),
                                            opnd_create_immed_int(32, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    /* UCVTF <Dd>, <Xn>, #<fbits> */
    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D13),
                                            opnd_create_reg(DR_REG_X11),
                                            opnd_create_immed_int(64, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);
}

static void
test_ucvtf_scalar_fixed(void *dc)
{
    instr_t *instr;

    /* UCVTF <Sd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S9),
                                            opnd_create_reg(DR_REG_S8),
                                            opnd_create_immed_int(1, OPSZ_5b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S21),
                                            opnd_create_reg(DR_REG_S4),
                                            opnd_create_immed_int(2, OPSZ_5b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S20),
                                            opnd_create_reg(DR_REG_S19),
                                            opnd_create_immed_int(4, OPSZ_5b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S6),
                                            opnd_create_reg(DR_REG_S7),
                                            opnd_create_immed_int(8, OPSZ_5b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S12),
                                            opnd_create_reg(DR_REG_S30),
                                            opnd_create_immed_int(16, OPSZ_5b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S18),
                                            opnd_create_reg(DR_REG_S9),
                                            opnd_create_immed_int(32, OPSZ_5b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S22),
                                            opnd_create_reg(DR_REG_S21),
                                            opnd_create_immed_int(21, OPSZ_5b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S11),
                                            opnd_create_reg(DR_REG_S19),
                                            opnd_create_immed_int(31, OPSZ_5b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    /* UCVTF <Dd>, <Dn>, #<fbits> */
    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D13),
                                            opnd_create_reg(DR_REG_D11),
                                            opnd_create_immed_int(1, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D2),
                                            opnd_create_reg(DR_REG_D3),
                                            opnd_create_immed_int(2, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D19),
                                            opnd_create_reg(DR_REG_D17),
                                            opnd_create_immed_int(4, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D30),
                                            opnd_create_reg(DR_REG_D9),
                                            opnd_create_immed_int(8, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D17),
                                            opnd_create_reg(DR_REG_D11),
                                            opnd_create_immed_int(16, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D8),
                                            opnd_create_reg(DR_REG_D4),
                                            opnd_create_immed_int(32, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D29),
                                            opnd_create_reg(DR_REG_D21),
                                            opnd_create_immed_int(64, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D30),
                                            opnd_create_reg(DR_REG_D29),
                                            opnd_create_immed_int(21, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);

    instr = INSTR_CREATE_ucvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D17),
                                            opnd_create_reg(DR_REG_D13),
                                            opnd_create_immed_int(42, OPSZ_6b));
    test_instr_encoding(dc, OP_ucvtf, instr);
}

static void
test_ucvtf_vector_fixed(void *dc)
{
    instr_t *instr;

    /* UCVTF <Vd>.<T>, <Vn>.<T>, #<fbits> */

    /* UCVTF <Vd>.4s, <Vn>.4s, #<fbits> */
    for (uint fbits = 1; fbits <= 32; fbits *= 2) {
        instr = INSTR_CREATE_ucvtf_vector_fixed(
            dc, opnd_create_reg(Q_registers[(fbits - 1) % 30]),
            opnd_create_reg(Q_registers[(fbits - 1) % 30]),
            opnd_create_immed_int(fbits, OPSZ_5b), OPND_CREATE_SINGLE());
        test_instr_encoding(dc, OP_ucvtf, instr);
    }

    /* UCVTF <Vd>.2d, <Vn>.2d, #<fbits> */
    for (uint fbits = 1; fbits <= 64; fbits *= 2) {
        instr = INSTR_CREATE_ucvtf_vector_fixed(
            dc, opnd_create_reg(Q_registers[(fbits - 1) % 30]),
            opnd_create_reg(Q_registers[(fbits - 1) % 30]),
            opnd_create_immed_int(fbits, OPSZ_6b), OPND_CREATE_DOUBLE());
        test_instr_encoding(dc, OP_ucvtf, instr);
    }

    /* UCVTF <Vd>.2s, <Vn>.2s, #<fbits> */
    for (uint fbits = 1; fbits <= 32; fbits *= 2) {
        instr = INSTR_CREATE_ucvtf_vector_fixed(
            dc, opnd_create_reg(D_registers[(fbits - 1) % 30]),
            opnd_create_reg(D_registers[(fbits - 1) % 30]),
            opnd_create_immed_int(fbits, OPSZ_5b), OPND_CREATE_SINGLE());
        test_instr_encoding(dc, OP_ucvtf, instr);
    }
}

static void
test_scvtf_scalar(void *dc)
{
    instr_t *instr;
    instr = INSTR_CREATE_scvtf_scalar(dc, opnd_create_reg(DR_REG_S4),
                                      opnd_create_reg(DR_REG_W9));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar(dc, opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_W28));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar(dc, opnd_create_reg(DR_REG_S1),
                                      opnd_create_reg(DR_REG_X21));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar(dc, opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_X2));
    test_instr_encoding(dc, OP_scvtf, instr);
}

static void
test_scvtf_vector(void *dc)
{
    instr_t *instr;

    /* SCVTF <Vd>.<T>, <Vn>.<T> */
    /* SCVTF <Vd>.2S, <Vn>.2S */
    instr = INSTR_CREATE_scvtf_vector(dc, opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D7), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_scvtf, instr);

    /* SCVTF <Vd>.4S, <Vn>.4S */
    instr = INSTR_CREATE_scvtf_vector(dc, opnd_create_reg(DR_REG_Q12),
                                      opnd_create_reg(DR_REG_Q24), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_scvtf, instr);

    /* SCVTF <Vd>.2D, <Vn>.2D */
    instr = INSTR_CREATE_scvtf_vector(dc, opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q1), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_scvtf, instr);

    /* SCVTF <V><d>, <V><n> */
    /* SCVTF <V>S, <V>S */
    instr = INSTR_CREATE_scvtf_scalar(dc, opnd_create_reg(DR_REG_S17),
                                      opnd_create_reg(DR_REG_S20));
    test_instr_encoding(dc, OP_scvtf, instr);

    /* SCVTF <V>D, <V>D */
    instr = INSTR_CREATE_scvtf_scalar(dc, opnd_create_reg(DR_REG_D14),
                                      opnd_create_reg(DR_REG_D14));
    test_instr_encoding(dc, OP_scvtf, instr);
}

static void
test_scvtf_scalar_fixed_gpr(void *dc)
{
    instr_t *instr;

    /* SCVTF <Sd>, <Wn>, #<fbits> */
    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S5),
                                            opnd_create_reg(DR_REG_W8),
                                            opnd_create_immed_int(4, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    /* SCVTF <Sd>, <Xn>, #<fbits> */
    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S13),
                                            opnd_create_reg(DR_REG_X7),
                                            opnd_create_immed_int(16, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    /* SCVTF <Dd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D17),
                                            opnd_create_reg(DR_REG_W0),
                                            opnd_create_immed_int(32, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    /* SCVTF <Dd>, <Xn>, #<fbits> */
    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D13),
                                            opnd_create_reg(DR_REG_X11),
                                            opnd_create_immed_int(64, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);
}

static void
test_scvtf_scalar_fixed(void *dc)
{
    instr_t *instr;

    /* SCVTF <Sd>, <Sn>, #<fbits> */
    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S9),
                                            opnd_create_reg(DR_REG_S8),
                                            opnd_create_immed_int(1, OPSZ_5b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S21),
                                            opnd_create_reg(DR_REG_S4),
                                            opnd_create_immed_int(2, OPSZ_5b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S20),
                                            opnd_create_reg(DR_REG_S19),
                                            opnd_create_immed_int(4, OPSZ_5b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S6),
                                            opnd_create_reg(DR_REG_S7),
                                            opnd_create_immed_int(8, OPSZ_5b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S12),
                                            opnd_create_reg(DR_REG_S30),
                                            opnd_create_immed_int(16, OPSZ_5b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S18),
                                            opnd_create_reg(DR_REG_S9),
                                            opnd_create_immed_int(32, OPSZ_5b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S22),
                                            opnd_create_reg(DR_REG_S21),
                                            opnd_create_immed_int(21, OPSZ_5b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_S11),
                                            opnd_create_reg(DR_REG_S19),
                                            opnd_create_immed_int(31, OPSZ_5b));
    test_instr_encoding(dc, OP_scvtf, instr);

    /* SCVTF <Dd>, <Dn>, #<fbits> */
    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D13),
                                            opnd_create_reg(DR_REG_D11),
                                            opnd_create_immed_int(1, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D2),
                                            opnd_create_reg(DR_REG_D3),
                                            opnd_create_immed_int(2, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D19),
                                            opnd_create_reg(DR_REG_D17),
                                            opnd_create_immed_int(4, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D30),
                                            opnd_create_reg(DR_REG_D9),
                                            opnd_create_immed_int(8, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D17),
                                            opnd_create_reg(DR_REG_D11),
                                            opnd_create_immed_int(16, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D8),
                                            opnd_create_reg(DR_REG_D4),
                                            opnd_create_immed_int(32, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D29),
                                            opnd_create_reg(DR_REG_D21),
                                            opnd_create_immed_int(64, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D30),
                                            opnd_create_reg(DR_REG_D29),
                                            opnd_create_immed_int(21, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);

    instr = INSTR_CREATE_scvtf_scalar_fixed(dc, opnd_create_reg(DR_REG_D17),
                                            opnd_create_reg(DR_REG_D13),
                                            opnd_create_immed_int(42, OPSZ_6b));
    test_instr_encoding(dc, OP_scvtf, instr);
}

static void
test_scvtf_vector_fixed(void *dc)
{
    instr_t *instr;

    /* SCVTF <Vd>.<T>, <Vn>.<T>, #<fbits> */

    /* SCVTF <Vd>.4s, <Vn>.4s, #<fbits> */
    for (uint fbits = 1; fbits <= 32; fbits *= 2) {
        instr = INSTR_CREATE_scvtf_vector_fixed(
            dc, opnd_create_reg(Q_registers[(fbits - 1) % 30]),
            opnd_create_reg(Q_registers[(fbits - 1) % 30]),
            opnd_create_immed_int(fbits, OPSZ_5b), OPND_CREATE_SINGLE());
        test_instr_encoding(dc, OP_scvtf, instr);
    }

    /* SCVTF <Vd>.2s, <Vn>.2s, #<fbits> */
    for (uint fbits = 1; fbits <= 32; fbits *= 2) {
        instr = INSTR_CREATE_scvtf_vector_fixed(
            dc, opnd_create_reg(D_registers[(fbits - 1) % 30]),
            opnd_create_reg(D_registers[(fbits - 1) % 30]),
            opnd_create_immed_int(fbits, OPSZ_5b), OPND_CREATE_SINGLE());
        test_instr_encoding(dc, OP_scvtf, instr);
    }
}

/* Macro expansion generates sequence of instruction creation tests for:
 * CCMP <Wn>, #<imm>, #<nzcv>, <cond>
 * CCMP <Xn>, #<imm>, #<nzcv>, <cond>
 * CCMN <Wn>, #<imm>, #<nzcv>, <cond>
 * CCMN <Xn>, #<imm>, #<nzcv>, <cond>
 */
#define CCM_R_I(cmptype, reg)                                                     \
    do {                                                                          \
        nzcv = 0;                                                                 \
        cond = 0;                                                                 \
        imm = 0;                                                                  \
        for (int rsrc = DR_REG_##reg##0; rsrc < DR_REG_##reg##SP; rsrc++) {       \
            instr = INSTR_CREATE_ccm##cmptype(                                    \
                dc, opnd_create_reg(rsrc), opnd_create_immed_int(imm++, OPSZ_5b), \
                opnd_create_immed_int(nzcv++, OPSZ_4b), conds[cond++]);           \
            test_instr_encoding(dc, OP_ccm##cmptype, instr);                      \
            imm = imm > 31 ? 0 : imm;                                             \
            nzcv = nzcv > 15 ? 0 : nzcv;                                          \
            cond = cond > 17 ? 0 : cond;                                          \
        }                                                                         \
    } while (0)

/* Macro expansion generates sequence of instruction creation tests for:
 * CCMP <Wn>, <Wm>, #<nzcv>, <cond>
 * CCMP <Xn>, <Xm>, #<nzcv>, <cond>
 * CCMN <Wn>, <Wm>, #<nzcv>, <cond>
 * CCMN <Xn>, <Xm>, #<nzcv>, <cond>
 */
#define CCM_R_R(cmptype, reg)                                                  \
    do {                                                                       \
        nzcv = 0;                                                              \
        cond = 0;                                                              \
        rsrc1 = DR_REG_##reg##30;                                              \
        for (int rsrc0 = DR_REG_##reg##0; rsrc0 < DR_REG_##reg##SP; rsrc0++) { \
            instr = INSTR_CREATE_ccm##cmptype(                                 \
                dc, opnd_create_reg(rsrc0), opnd_create_reg(rsrc1--),          \
                opnd_create_immed_int(nzcv++, OPSZ_4b), conds[cond++]);        \
            test_instr_encoding(dc, OP_ccm##cmptype, instr);                   \
            nzcv = nzcv > 15 ? 0 : nzcv;                                       \
            cond = cond > 17 ? 0 : cond;                                       \
        }                                                                      \
    } while (0)

static void
test_ccmp_ccmn(void *dc)
{
    instr_t *instr;

    dr_pred_type_t conds[] = { DR_PRED_EQ, DR_PRED_NE, DR_PRED_CS, DR_PRED_CC, DR_PRED_MI,
                               DR_PRED_PL, DR_PRED_VS, DR_PRED_VC, DR_PRED_HI, DR_PRED_LS,
                               DR_PRED_GE, DR_PRED_LT, DR_PRED_GT, DR_PRED_LE, DR_PRED_AL,
                               DR_PRED_NV, DR_PRED_HS, DR_PRED_LO };

    // CCMP <Wn>, #<imm>, #<nzcv>, GE
    instr = INSTR_CREATE_ccmp(dc, opnd_create_reg(DR_REG_W28),
                              opnd_create_immed_int(10, OPSZ_5b),
                              opnd_create_immed_int(0b1010, OPSZ_4b), DR_PRED_GE);
    test_instr_encoding(dc, OP_ccmp, instr);

    int imm;
    int nzcv;
    int cond;
    CCM_R_I(p, W);

    // CCMP <Xn>, #<imm>, #<nzcv>, EQ
    instr = INSTR_CREATE_ccmp(dc, opnd_create_reg(DR_REG_X0),
                              opnd_create_immed_int(10, OPSZ_5b),
                              opnd_create_immed_int(0b1010, OPSZ_4b), DR_PRED_EQ);
    test_instr_encoding(dc, OP_ccmp, instr);

    CCM_R_I(p, X);

    // CCMP <Wn>, <Wm>, #<nzcv>, GE
    instr =
        INSTR_CREATE_ccmp(dc, opnd_create_reg(DR_REG_W28), opnd_create_reg(DR_REG_W29),
                          opnd_create_immed_int(0b1010, OPSZ_4b), DR_PRED_GE);
    test_instr_encoding(dc, OP_ccmp, instr);

    int rsrc1;
    CCM_R_R(p, W);

    // CCMP <Xn>, <Xm>, #<nzcv>, GE
    instr =
        INSTR_CREATE_ccmp(dc, opnd_create_reg(DR_REG_X28), opnd_create_reg(DR_REG_X29),
                          opnd_create_immed_int(0b1010, OPSZ_4b), DR_PRED_GE);
    test_instr_encoding(dc, OP_ccmp, instr);

    CCM_R_R(p, X);

    // CCMN <Wn>, #<imm>, #<nzcv>, EQ
    instr = INSTR_CREATE_ccmn(dc, opnd_create_reg(DR_REG_W0),
                              opnd_create_immed_int(0x1f, OPSZ_5b),
                              opnd_create_immed_int(0b0101, OPSZ_4b), DR_PRED_EQ);
    test_instr_encoding(dc, OP_ccmn, instr);

    CCM_R_I(n, W);

    // CCMN <Xn>, #<imm>, #<nzcv>, LT
    instr = INSTR_CREATE_ccmn(dc, opnd_create_reg(DR_REG_X15),
                              opnd_create_immed_int(0, OPSZ_5b),
                              opnd_create_immed_int(0b1001, OPSZ_4b), DR_PRED_LT);
    test_instr_encoding(dc, OP_ccmn, instr);

    CCM_R_I(n, X);

    // CCMN <Wn>, <Wm>, #<nzcv>, VS
    instr =
        INSTR_CREATE_ccmn(dc, opnd_create_reg(DR_REG_W30), opnd_create_reg(DR_REG_W29),
                          opnd_create_immed_int(0b1010, OPSZ_4b), DR_PRED_VS);
    test_instr_encoding(dc, OP_ccmn, instr);

    CCM_R_R(n, W);

    // CCMN <Xn>, <Xm>, #<nzcv>, PL
    instr = INSTR_CREATE_ccmn(dc, opnd_create_reg(DR_REG_X9), opnd_create_reg(DR_REG_X10),
                              opnd_create_immed_int(0b1111, OPSZ_4b), DR_PRED_PL);
    test_instr_encoding(dc, OP_ccmn, instr);

    CCM_R_R(n, X);
}

static void
test_internal_encode(void *dcontext)
{
    instr_t *label = INSTR_CREATE_label(dcontext);
    /* Normally a client would use drmgr_reserve_note_range() but we don't want to
     * pull in those libraries.  We know DR's used values are very high and that
     * 7 is safe (and un-aligned to test i#5297).
     */
    instr_set_note(label, (void *)(ptr_uint_t)7);
    instr_t *jmp = INSTR_CREATE_b(dcontext, opnd_create_instr(label));
    /* Make sure debug build doesn't assert or warn here. */
    uint flags = instr_get_arith_flags(jmp, DR_QUERY_DEFAULT);
    instr_destroy(dcontext, label);
    instr_destroy(dcontext, jmp);
}

int
main(int argc, char *argv[])
{
#ifdef STANDALONE_DECODER
    void *dcontext = GLOBAL_DCONTEXT;
#else
    void *dcontext = dr_standalone_init();
#endif

    test_extend(dcontext);
    print("test_extend complete\n");

    test_add(dcontext);
    print("test_add complete\n");

    test_ldar(dcontext);
    print("test_ldar complete\n");

    test_ldapr(dcontext);
    print("test_ldapr complete\n");

    test_ldur_stur(dcontext);
    print("test_ldur_stur complete\n");

    test_instrs_with_logic_imm(dcontext);
    print("test_instrs_with_logic_imm complete\n");

    test_fmov_general(dcontext);
    print("test_fmov_general complete\n");

    test_fmov_vector(dcontext);
    print("test_fmov_vector complete\n");

    test_fmov_scalar(dcontext);
    print("test_fmov_scalar complete\n");

    test_asimdsamefp16(dcontext);
    print("test_asimdsamefp16 complete\n");

    test_asimdsame(dcontext);
    print("test_asimdsame complete\n");

    test_asimd_mem(dcontext);
    print("test_asimd_mem complete\n");

    test_floatdp1(dcontext);
    print("test_floatdp1 complete\n");

    test_floatdp2(dcontext);
    print("test_floatdp2 complete\n");

    test_floatdp3(dcontext);
    print("test_floatdp3 complete\n");

    test_asimddiff(dcontext);
    print("test_asimddiff complete\n");

    test_sys_cache(dcontext);
    print("test_sys_cache complete\n");

    test_exclusive_memops(dcontext);
    print("test_exclusive_memops complete\n");

    test_xinst(dcontext);
    print("test_xinst complete\n");

    test_opnd(dcontext);
    print("test_opnd complete\n");

#ifdef DR_FAST_IR /* For offset field access. */
    test_mov_instr_addr(dcontext);
#endif
    print("test_mov_instr_addr complete\n");

    test_fcvtas_scalar(dcontext);
    print("test_fcvtas_scalar complete\n");

    test_fcvtas_vector(dcontext);
    print("test_fcvtas_vector complete\n");

    test_fcvtns_scalar(dcontext);
    print("test_fcvtns_scalar complete\n");

    test_fcvtns_vector(dcontext);
    print("test_fcvtns_vector complete\n");

    test_fcvtps_scalar(dcontext);
    print("test_fcvtps_scalar complete\n");

    test_fcvtps_vector(dcontext);
    print("test_fcvtps_vector complete\n");

    test_fcvtpu_scalar(dcontext);
    print("test_fcvtpu_scalar complete\n");

    test_fcvtpu_vector(dcontext);
    print("test_fcvtpu_vector complete\n");

    test_fcvtzs_scalar(dcontext);
    print("test_fcvtzs_scalar complete\n");

    test_fcvtzs_vector(dcontext);
    print("test_fcvtzs_vector complete\n");

    test_fcvtzs_scalar_fixed_gpr(dcontext);
    print("test_fcvtzs_scalar_fixed_gpr complete\n");

    test_fcvtzs_scalar_fixed(dcontext);
    print("test_fcvtzs_scalar_fixed complete\n");

    test_fcvtzu_scalar(dcontext);
    print("test_fcvtzu_scalar complete\n");

    test_fcvtzu_vector(dcontext);
    print("test_fcvtzu_vector complete\n");

    test_fcvtzu_scalar_fixed_gpr(dcontext);
    print("test_fcvtzu_scalar_fixed_gpr complete\n");

    test_fcvtzu_scalar_fixed(dcontext);
    print("test_fcvtzu_scalar_fixed complete\n");

    test_fcvtzu_vector_fixed(dcontext);
    print("test_fcvtzu_vector_fixed complete\n");

    test_sli_vector(dcontext);
    print("test_sli_vector_fixed complete\n");

    test_uqshrn_vector(dcontext);
    print("test_uqshrn_vector_fixed complete\n");

    test_ucvtf_scalar(dcontext);
    print("test_ucvtf_scalar complete\n");

    test_ucvtf_vector(dcontext);
    print("test_ucvtf_vector complete\n");

    test_ucvtf_scalar_fixed_gpr(dcontext);
    print("test_ucvtf_scalar_fixed_gpr complete\n");

    test_ucvtf_scalar_fixed(dcontext);
    print("test_ucvtf_scalar_fixed complete\n");

    test_ucvtf_vector_fixed(dcontext);
    print("test_ucvtf_vector_fixed complete\n");

    test_scvtf_scalar(dcontext);
    print("test_scvtf_scalar complete\n");

    test_scvtf_vector(dcontext);
    print("test_scvtf_vector complete\n");

    test_scvtf_scalar_fixed_gpr(dcontext);
    print("test_scvtf_scalar_fixed_gpr complete\n");

    test_scvtf_scalar_fixed(dcontext);
    print("test_scvtf_scalar_fixed complete\n");

    test_scvtf_vector_fixed(dcontext);
    print("test_scvtf_vector_fixed complete\n");

    test_ccmp_ccmn(dcontext);
    print("test_ccmp_ccmn complete\n");

    ldr(dcontext);
    str(dcontext);

#if 0 /* TODO i#4847: add memory touching instructions */
        adr(dcontext);
        adrp(dcontext);
#endif
    ldpsw(dcontext);
    ld2(dcontext);
    ld3(dcontext);
    ld4(dcontext);
    ld2r(dcontext);
    ld3r(dcontext);
    ld4r(dcontext);

    test_internal_encode(dcontext);

    print("All tests complete\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    return 0;
}
