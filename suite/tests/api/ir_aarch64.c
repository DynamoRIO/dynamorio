/* **********************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
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

/* Uses the DR CLIENT_INTERFACE API, using DR as a standalone library, rather than
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
    instr_disassemble(dc, instr, STDOUT);
    print("\n");

    ASSERT(instr_is_encoding_possible(instr));
    pc = instr_encode(dc, instr, buf);
    decin = instr_create(dc);
    decode(dc, buf, decin);
    ASSERT(instr_same(instr, decin));

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
    adds_extend(W, DR_EXTEND_UXTX, 3);
    adds_extend(W, DR_EXTEND_SXTB, 4);
    adds_extend(W, DR_EXTEND_SXTH, 0);
    adds_extend(W, DR_EXTEND_SXTW, 1);
    adds_extend(W, DR_EXTEND_SXTX, 2);

    adds_extend(X, DR_EXTEND_UXTB, 0);
    adds_extend(X, DR_EXTEND_UXTH, 1);
    adds_extend(X, DR_EXTEND_UXTW, 2);
    adds_extend(X, DR_EXTEND_UXTX, 3);
    adds_extend(X, DR_EXTEND_SXTB, 4);
    adds_extend(X, DR_EXTEND_SXTH, 0);
    adds_extend(X, DR_EXTEND_SXTW, 1);
    adds_extend(X, DR_EXTEND_SXTX, 2);
}

static void
test_pc_addr(void *dc)
{
    byte *pc;
    instr_t *instr;

#if 0 /* TODO: implement OPSZ_21b */
    instr = INSTR_CREATE_adr(dc, opnd_create_reg(DR_REG_X0),
                                 opnd_create_immed_int(0, OPSZ_21b));
    test_instr_encoding(dc, OP_adr, instr);
#endif
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

    instr = INSTR_CREATE_fmov_general(dc, opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_X10));
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

    instr = INSTR_CREATE_fmlal_vector(dc, opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31));
    test_instr_encoding(dc, OP_fmlal, instr);

    instr = INSTR_CREATE_fmlal_vector(dc, opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31));
    test_instr_encoding(dc, OP_fmlal, instr);

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

    instr = INSTR_CREATE_fmlsl_vector(dc, opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31));
    test_instr_encoding(dc, OP_fmlsl, instr);

    instr = INSTR_CREATE_fmlsl_vector(dc, opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31));
    test_instr_encoding(dc, OP_fmlsl, instr);

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

    instr = INSTR_CREATE_fmlal2_vector(dc, opnd_create_reg(DR_REG_D0),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D31));
    test_instr_encoding(dc, OP_fmlal2, instr);

    instr = INSTR_CREATE_fmlal2_vector(dc, opnd_create_reg(DR_REG_Q0),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q31));
    test_instr_encoding(dc, OP_fmlal2, instr);

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

    instr = INSTR_CREATE_fmlsl2_vector(dc, opnd_create_reg(DR_REG_D0),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D31));
    test_instr_encoding(dc, OP_fmlsl2, instr);

    instr = INSTR_CREATE_fmlsl2_vector(dc, opnd_create_reg(DR_REG_Q0),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q31));
    test_instr_encoding(dc, OP_fmlsl2, instr);

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
        dc, opnd_create_reg(DR_REG_Q0),
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
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_4),
        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.8H }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.2S }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_2),
        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.4S }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_4),
        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.1D }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_1),
        OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_ld1, instr);

    /* LD1 { <Vt>.2D }, [<Xn|SP>] */
    instr = INSTR_CREATE_ld1_multi_1(
        dc, opnd_create_reg(DR_REG_Q0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_2),
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
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.16B }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc,
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_16),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.4H }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_4),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.8H }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_8),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.2S }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_2),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.4S }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_4),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.1D }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_1),
        opnd_create_reg(DR_REG_Q0), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_st1, instr);

    /* ST1 { <Vt>.2D }, [<Xn|SP>] */
    instr = INSTR_CREATE_st1_multi_1(
        dc, opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 0, 0, OPSZ_2),
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
test_sve_int_bin_pred_log(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* SVE bitwise logical operations (predicated) */

    instr = INSTR_CREATE_orr_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P7),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z13), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_orr, instr);

    instr = INSTR_CREATE_orr_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P7),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z13), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_orr, instr);

    instr = INSTR_CREATE_orr_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P7),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z13), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_orr, instr);

    instr = INSTR_CREATE_orr_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P7),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z13), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_orr, instr);

    instr = INSTR_CREATE_eor_sve_pred(
        dc, opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_P4),
        opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_Z2), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_eor, instr);

    instr = INSTR_CREATE_eor_sve_pred(
        dc, opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_P4),
        opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_Z2), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_eor, instr);

    instr = INSTR_CREATE_eor_sve_pred(
        dc, opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_P4),
        opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_Z2), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_eor, instr);

    instr = INSTR_CREATE_eor_sve_pred(
        dc, opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_P4),
        opnd_create_reg(DR_REG_Z29), opnd_create_reg(DR_REG_Z2), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_eor, instr);

    instr = INSTR_CREATE_and_sve_pred(
        dc, opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_P1),
        opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_Z23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_and_sve_pred(
        dc, opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_P1),
        opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_Z23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_and_sve_pred(
        dc, opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_P1),
        opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_Z23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_and_sve_pred(
        dc, opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_P1),
        opnd_create_reg(DR_REG_Z31), opnd_create_reg(DR_REG_Z23), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_bic_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P2),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z24), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_bic, instr);

    instr = INSTR_CREATE_bic_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P2),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z24), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_bic, instr);

    instr = INSTR_CREATE_bic_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P2),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z24), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_bic, instr);

    instr = INSTR_CREATE_bic_sve_pred(
        dc, opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_P2),
        opnd_create_reg(DR_REG_Z2), opnd_create_reg(DR_REG_Z24), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_bic, instr);
}

static void
test_neon_sve_int_bin_cons_arit_0(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* SVE integer add/subtract vectors (unpredicated) */

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_Z2),
                                    opnd_create_reg(DR_REG_Z27),
                                    opnd_create_reg(DR_REG_Z30), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_Z2),
                                    opnd_create_reg(DR_REG_Z27),
                                    opnd_create_reg(DR_REG_Z30), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_Z2),
                                    opnd_create_reg(DR_REG_Z27),
                                    opnd_create_reg(DR_REG_Z30), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc, opnd_create_reg(DR_REG_Z2),
                                    opnd_create_reg(DR_REG_Z27),
                                    opnd_create_reg(DR_REG_Z30), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_Z0),
                                    opnd_create_reg(DR_REG_Z13),
                                    opnd_create_reg(DR_REG_Z29), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_Z0),
                                    opnd_create_reg(DR_REG_Z13),
                                    opnd_create_reg(DR_REG_Z29), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_Z0),
                                    opnd_create_reg(DR_REG_Z13),
                                    opnd_create_reg(DR_REG_Z29), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc, opnd_create_reg(DR_REG_Z0),
                                    opnd_create_reg(DR_REG_Z13),
                                    opnd_create_reg(DR_REG_Z29), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_Z31),
                                      opnd_create_reg(DR_REG_Z17),
                                      opnd_create_reg(DR_REG_Z10), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_Z31),
                                      opnd_create_reg(DR_REG_Z17),
                                      opnd_create_reg(DR_REG_Z10), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_Z31),
                                      opnd_create_reg(DR_REG_Z17),
                                      opnd_create_reg(DR_REG_Z10), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc, opnd_create_reg(DR_REG_Z31),
                                      opnd_create_reg(DR_REG_Z17),
                                      opnd_create_reg(DR_REG_Z10), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_Z2),
                                      opnd_create_reg(DR_REG_Z31),
                                      opnd_create_reg(DR_REG_Z20), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_Z2),
                                      opnd_create_reg(DR_REG_Z31),
                                      opnd_create_reg(DR_REG_Z20), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_Z2),
                                      opnd_create_reg(DR_REG_Z31),
                                      opnd_create_reg(DR_REG_Z20), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc, opnd_create_reg(DR_REG_Z2),
                                      opnd_create_reg(DR_REG_Z31),
                                      opnd_create_reg(DR_REG_Z20), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_Z4),
                                      opnd_create_reg(DR_REG_Z15),
                                      opnd_create_reg(DR_REG_Z23), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_Z4),
                                      opnd_create_reg(DR_REG_Z15),
                                      opnd_create_reg(DR_REG_Z23), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_Z4),
                                      opnd_create_reg(DR_REG_Z15),
                                      opnd_create_reg(DR_REG_Z23), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc, opnd_create_reg(DR_REG_Z4),
                                      opnd_create_reg(DR_REG_Z15),
                                      opnd_create_reg(DR_REG_Z23), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_Z2),
                                      opnd_create_reg(DR_REG_Z26),
                                      opnd_create_reg(DR_REG_Z8), OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_Z2),
                                      opnd_create_reg(DR_REG_Z26),
                                      opnd_create_reg(DR_REG_Z8), OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_Z2),
                                      opnd_create_reg(DR_REG_Z26),
                                      opnd_create_reg(DR_REG_Z8), OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc, opnd_create_reg(DR_REG_Z2),
                                      opnd_create_reg(DR_REG_Z26),
                                      opnd_create_reg(DR_REG_Z8), OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqsub, instr);
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

    test_instrs_with_logic_imm(dcontext);
    print("test_instrs_with_logic_imm complete\n");

    test_fmov_general(dcontext);
    print("test_fmov_general complete\n");

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

    test_sve_int_bin_pred_log(dcontext);
    print("test_sve_int_bin_pred_log complete\n");

    test_neon_sve_int_bin_cons_arit_0(dcontext);
    print("test_sve_int_bin_cons_arit_0 complete\n");

    test_asimddiff(dcontext);
    print("test_asimddiff complete\n");

    print("All tests complete\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    return 0;
}
