/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
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
# define DR_FAST_IR 1
#endif

/* Uses the DR CLIENT_INTERFACE API, using DR as a standalone library, rather than
 * being a client library working with DR on a target program.
 */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

static byte buf[8192];

#ifdef STANDALONE_DECODER
# define ASSERT(x) \
    ((void)((!(x)) ? \
        (fprintf(stderr, "ASSERT FAILURE (standalone): %s:%d: %s\n", __FILE__, \
                          __LINE__, #x), \
         abort(), 0) : 0))
#else
# define ASSERT(x) \
    ((void)((!(x)) ? \
        (dr_fprintf(STDERR, "ASSERT FAILURE (client): %s:%d: %s\n", __FILE__, \
                             __LINE__, #x), \
         dr_abort(), 0) : 0))
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
    opnd = opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_X2,
                                         DR_EXTEND_UXTX, false, 0, 0, size);
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
    instr_disassemble(dc, instr, STDOUT); print("\n");

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
    instr = INSTR_CREATE_adc(dc, opnd_create_reg(DR_REG_W0),
                                 opnd_create_reg(DR_REG_W1),
                                 opnd_create_reg(DR_REG_W2));
    test_instr_encoding(dc, OP_adc, instr);

    /* ADC <Xd>, <Xn>, <Xm> */
    instr = INSTR_CREATE_adc(dc, opnd_create_reg(DR_REG_X0),
                                 opnd_create_reg(DR_REG_X1),
                                 opnd_create_reg(DR_REG_X2));
    test_instr_encoding(dc, OP_adc, instr);

    /* Add with carry setting condition flags
     * ADCS <Wd>, <Wn>, <Wm>
     */
    instr = INSTR_CREATE_adcs(dc, opnd_create_reg(DR_REG_W0),
                                  opnd_create_reg(DR_REG_W1),
                                  opnd_create_reg(DR_REG_W2));
    test_instr_encoding(dc, OP_adcs, instr);

    /* ADCS <Xd>, <Xn>, <Xm> */
    instr = INSTR_CREATE_adcs(dc, opnd_create_reg(DR_REG_X0),
                                  opnd_create_reg(DR_REG_X1),
                                  opnd_create_reg(DR_REG_X2));
    test_instr_encoding(dc, OP_adcs, instr);

    /* Add and set flags (shifted register, 32-bit)
     * ADDS <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
     */
    #define adds_shift(reg, shift_type, amount_imm6) \
        instr = INSTR_CREATE_adds_shift(dc, \
            opnd_create_reg(DR_REG_ ## reg ## 0), \
            opnd_create_reg(DR_REG_ ## reg ## 1), \
            opnd_create_reg(DR_REG_ ## reg ## 2), \
            opnd_add_flags(OPND_CREATE_INT(shift_type), DR_OPND_IS_SHIFT), \
            opnd_create_immed_int(amount_imm6, OPSZ_6b)); \
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
    instr = INSTR_CREATE_adds_imm(dc, opnd_create_reg(DR_REG_W0),
                       opnd_create_reg(DR_REG_W1),
                       opnd_create_immed_int(0, OPSZ_12b), OPND_CREATE_INT8(0));
    test_instr_encoding(dc, OP_adds, instr);

    instr = INSTR_CREATE_adds_imm(dc, opnd_create_reg(DR_REG_W0),
                   opnd_create_reg(DR_REG_W1),
                   opnd_create_immed_int(0xFFF, OPSZ_12b), OPND_CREATE_INT8(0));
    test_instr_encoding(dc, OP_adds, instr);

    /* Add and set flags (immediate, 64-bit)
     * ADDS <Xd>, <Xn|SP>, #<imm>{, <shift>}
     */
    instr = INSTR_CREATE_adds_imm(dc, opnd_create_reg(DR_REG_X0),
                       opnd_create_reg(DR_REG_X1),
                       opnd_create_immed_int(0, OPSZ_12b), OPND_CREATE_INT8(0));
    test_instr_encoding(dc, OP_adds, instr);

    instr = INSTR_CREATE_adds_imm(dc, opnd_create_reg(DR_REG_X0),
                   opnd_create_reg(DR_REG_X1),
                   opnd_create_immed_int(0xFFF, OPSZ_12b), OPND_CREATE_INT8(0));
    test_instr_encoding(dc, OP_adds, instr);

    /* Add and set flags (extended register, 32-bit)
     * ADDS <Wd>, <Wn|WSP>, <Wm>{, <extend> {#<amount>}}
     */
    #define adds_extend(reg, extend_type, amount_imm3) \
       instr = INSTR_CREATE_adds_extend(dc, opnd_create_reg(DR_REG_ ## reg ## 0), \
           opnd_create_reg(DR_REG_ ## reg ## 1), \
           opnd_create_reg(DR_REG_ ## reg ## 2), \
           opnd_add_flags(OPND_CREATE_INT(extend_type), DR_OPND_IS_EXTEND), \
           opnd_create_immed_int(amount_imm3, OPSZ_3b)); \
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
    instr = INSTR_CREATE_ldar(dc, opnd_create_reg(DR_REG_W0),
            opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_NULL, 0, false,
                                          0, 0, OPSZ_4));
    test_instr_encoding(dc, OP_ldar, instr);

    /* LDAR <Xt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldar(dc, opnd_create_reg(DR_REG_X0),
            opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_NULL, 0, false,
                                          0, 0, OPSZ_8));
    test_instr_encoding(dc, OP_ldar, instr);

    /* LDARB <Wt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldarb(dc, opnd_create_reg(DR_REG_W0),
            opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_NULL, 0, false,
                                          0, 0, OPSZ_1));
    test_instr_encoding(dc, OP_ldarb, instr);

    /* LDARH <Wt>, [<Xn|SP>{,#0}] */
    instr = INSTR_CREATE_ldarh(dc, opnd_create_reg(DR_REG_W0),
            opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_NULL, 0, false,
                                          0, 0, OPSZ_2));
    test_instr_encoding(dc, OP_ldarh, instr);
}

static void
test_instrs_with_logic_imm(void *dc)
{
    byte *pc;
    instr_t *instr;

    instr = INSTR_CREATE_and(dc, opnd_create_reg(DR_REG_X10),
                             opnd_create_reg(DR_REG_X9), OPND_CREATE_INT(0xFFFF));
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_and(dc, opnd_create_reg(DR_REG_W5),
                             opnd_create_reg(DR_REG_W5), OPND_CREATE_INT(0xFF));
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_ands(dc, opnd_create_reg(DR_REG_X23),
                              opnd_create_reg(DR_REG_X19), OPND_CREATE_INT(0xFFFFFF));
    test_instr_encoding(dc, OP_ands, instr);

    instr = INSTR_CREATE_ands(dc, opnd_create_reg(DR_REG_W3),
                              opnd_create_reg(DR_REG_W8), OPND_CREATE_INT(0xF));
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

    instr = INSTR_CREATE_fmaxnm_vector(dc,
                                       opnd_create_reg(DR_REG_D2),
                                       opnd_create_reg(DR_REG_D27),
                                       opnd_create_reg(DR_REG_D30),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmaxnm_vector(dc,
                                       opnd_create_reg(DR_REG_Q2),
                                       opnd_create_reg(DR_REG_Q27),
                                       opnd_create_reg(DR_REG_Q30),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmla_vector(dc,
                                     opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D29),
                                     opnd_create_reg(DR_REG_D31),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fmla_vector(dc,
                                     opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q29),
                                     opnd_create_reg(DR_REG_Q31),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fadd_vector(dc,
                                     opnd_create_reg(DR_REG_D17),
                                     opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D2),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fadd_vector(dc,
                                     opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q2),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fmulx_vector(dc,
                                      opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D20),
                                      opnd_create_reg(DR_REG_D4),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fmulx_vector(dc,
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q20),
                                      opnd_create_reg(DR_REG_Q4),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc,
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D2),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc,
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q2),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fmax_vector(dc,
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D8),
                                     opnd_create_reg(DR_REG_D22),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_fmax_vector(dc,
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8),
                                     opnd_create_reg(DR_REG_Q22),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_frecps_vector(dc,
                                       opnd_create_reg(DR_REG_D24),
                                       opnd_create_reg(DR_REG_D26),
                                       opnd_create_reg(DR_REG_D18),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_frecps, instr);

    instr = INSTR_CREATE_frecps_vector(dc,
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q26),
                                       opnd_create_reg(DR_REG_Q18),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_frecps, instr);

    instr = INSTR_CREATE_fminnm_vector(dc,
                                       opnd_create_reg(DR_REG_D16),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D11),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fminnm_vector(dc,
                                       opnd_create_reg(DR_REG_Q16),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q11),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fmls_vector(dc,
                                     opnd_create_reg(DR_REG_D19),
                                     opnd_create_reg(DR_REG_D8),
                                     opnd_create_reg(DR_REG_D29),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fmls_vector(dc,
                                     opnd_create_reg(DR_REG_Q19),
                                     opnd_create_reg(DR_REG_Q8),
                                     opnd_create_reg(DR_REG_Q29),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fsub_vector(dc,
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D24),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fsub_vector(dc,
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q24),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fmin_vector(dc,
                                     opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D15),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_fmin_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q15),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc,
                                        opnd_create_reg(DR_REG_D8),
                                        opnd_create_reg(DR_REG_D12),
                                        opnd_create_reg(DR_REG_D19),
                                        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc,
                                        opnd_create_reg(DR_REG_Q8),
                                        opnd_create_reg(DR_REG_Q12),
                                        opnd_create_reg(DR_REG_Q19),
                                        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(dc,
                                        opnd_create_reg(DR_REG_D23),
                                        opnd_create_reg(DR_REG_D15),
                                        opnd_create_reg(DR_REG_D20),
                                        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(dc,
                                        opnd_create_reg(DR_REG_Q23),
                                        opnd_create_reg(DR_REG_Q15),
                                        opnd_create_reg(DR_REG_Q20),
                                        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_faddp_vector(dc,
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D30),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_faddp_vector(dc,
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q30),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_fmul_vector(dc,
                                     opnd_create_reg(DR_REG_D4),
                                     opnd_create_reg(DR_REG_D20),
                                     opnd_create_reg(DR_REG_D10),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fmul_vector(dc,
                                     opnd_create_reg(DR_REG_Q4),
                                     opnd_create_reg(DR_REG_Q20),
                                     opnd_create_reg(DR_REG_Q10),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fcmge_vector(dc,
                                      opnd_create_reg(DR_REG_D14),
                                      opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D15),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_fcmge_vector(dc,
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_facge_vector(dc,
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D31),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_facge_vector(dc,
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q31),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc,
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D5),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc,
                                      opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q5),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fdiv_vector(dc,
                                     opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D23),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fdiv_vector(dc,
                                     opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q23),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fminnmp_vector(dc,
                                        opnd_create_reg(DR_REG_D9),
                                        opnd_create_reg(DR_REG_D7),
                                        opnd_create_reg(DR_REG_D6),
                                        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fminnmp_vector(dc,
                                        opnd_create_reg(DR_REG_Q9),
                                        opnd_create_reg(DR_REG_Q7),
                                        opnd_create_reg(DR_REG_Q6),
                                        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fabd_vector(dc,
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D12),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fabd_vector(dc,
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q12),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc,
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D26),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc,
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q26),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc,
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D17),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc,
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q17),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_fminp_vector(dc,
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D7),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminp, instr);

    instr = INSTR_CREATE_fminp_vector(dc,
                                      opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q7),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fminp, instr);
}

static void
test_asimdsame(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* Advanced SIMD three same */

    instr = INSTR_CREATE_shadd_vector(dc,
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D30),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q30),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc,
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D30),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q30),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc,
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D30),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_shadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q30),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_shadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc,
                                      opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D29),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_Q29),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc,
                                      opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D29),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_Q29),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc,
                                      opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D13),
                                      opnd_create_reg(DR_REG_D29),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_Q29),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_sqadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q13),
                                      opnd_create_reg(DR_REG_Q29),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc,
                                       opnd_create_reg(DR_REG_D31),
                                       opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D10),
                                       OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc,
                                       opnd_create_reg(DR_REG_Q31),
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q10),
                                       OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc,
                                       opnd_create_reg(DR_REG_D31),
                                       opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D10),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc,
                                       opnd_create_reg(DR_REG_Q31),
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q10),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc,
                                       opnd_create_reg(DR_REG_D31),
                                       opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D10),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_srhadd_vector(dc,
                                       opnd_create_reg(DR_REG_Q31),
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q10),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_srhadd, instr);

    instr = INSTR_CREATE_shsub_vector(dc,
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D20),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q20),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc,
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D20),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q20),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc,
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D20),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_shsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q20),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_shsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc,
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D23),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc,
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D23),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc,
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D23),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_sqsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q23),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqsub, instr);

    instr = INSTR_CREATE_cmgt_vector(dc,
                                     opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D8),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc,
                                     opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D8),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc,
                                     opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D8),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmgt_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q8),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmgt, instr);

    instr = INSTR_CREATE_cmge_vector(dc,
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D26),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc,
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q26),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc,
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D26),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc,
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q26),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc,
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D26),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc,
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q26),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_cmge_vector(dc,
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q26),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmge, instr);

    instr = INSTR_CREATE_sshl_vector(dc,
                                     opnd_create_reg(DR_REG_D18),
                                     opnd_create_reg(DR_REG_D16),
                                     opnd_create_reg(DR_REG_D29),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc,
                                     opnd_create_reg(DR_REG_Q18),
                                     opnd_create_reg(DR_REG_Q16),
                                     opnd_create_reg(DR_REG_Q29),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc,
                                     opnd_create_reg(DR_REG_D18),
                                     opnd_create_reg(DR_REG_D16),
                                     opnd_create_reg(DR_REG_D29),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc,
                                     opnd_create_reg(DR_REG_Q18),
                                     opnd_create_reg(DR_REG_Q16),
                                     opnd_create_reg(DR_REG_Q29),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc,
                                     opnd_create_reg(DR_REG_D18),
                                     opnd_create_reg(DR_REG_D16),
                                     opnd_create_reg(DR_REG_D29),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc,
                                     opnd_create_reg(DR_REG_Q18),
                                     opnd_create_reg(DR_REG_Q16),
                                     opnd_create_reg(DR_REG_Q29),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sshl_vector(dc,
                                     opnd_create_reg(DR_REG_Q18),
                                     opnd_create_reg(DR_REG_Q16),
                                     opnd_create_reg(DR_REG_Q29),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc,
                                      opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D23),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q23),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc,
                                      opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D23),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q23),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc,
                                      opnd_create_reg(DR_REG_D11),
                                      opnd_create_reg(DR_REG_D19),
                                      opnd_create_reg(DR_REG_D23),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q23),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_sqshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q11),
                                      opnd_create_reg(DR_REG_Q19),
                                      opnd_create_reg(DR_REG_Q23),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc,
                                      opnd_create_reg(DR_REG_D8),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D15),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q8),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q15),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc,
                                      opnd_create_reg(DR_REG_D8),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D15),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q8),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q15),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc,
                                      opnd_create_reg(DR_REG_D8),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D15),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q8),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q15),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_srshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q8),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q15),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_srshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_D28),
                                       opnd_create_reg(DR_REG_D24),
                                       opnd_create_reg(DR_REG_D2),
                                       OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q2),
                                       OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_D28),
                                       opnd_create_reg(DR_REG_D24),
                                       opnd_create_reg(DR_REG_D2),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q2),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_D28),
                                       opnd_create_reg(DR_REG_D24),
                                       opnd_create_reg(DR_REG_D2),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q2),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_sqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_Q28),
                                       opnd_create_reg(DR_REG_Q24),
                                       opnd_create_reg(DR_REG_Q2),
                                       OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sqrshl, instr);

    instr = INSTR_CREATE_smax_vector(dc,
                                     opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D8),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc,
                                     opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q8),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc,
                                     opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D8),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc,
                                     opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q8),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc,
                                     opnd_create_reg(DR_REG_D0),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D8),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smax_vector(dc,
                                     opnd_create_reg(DR_REG_Q0),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q8),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smax, instr);

    instr = INSTR_CREATE_smin_vector(dc,
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D19),
                                     opnd_create_reg(DR_REG_D23),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc,
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q19),
                                     opnd_create_reg(DR_REG_Q23),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc,
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D19),
                                     opnd_create_reg(DR_REG_D23),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc,
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q19),
                                     opnd_create_reg(DR_REG_Q23),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc,
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D19),
                                     opnd_create_reg(DR_REG_D23),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_smin_vector(dc,
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q19),
                                     opnd_create_reg(DR_REG_Q23),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smin, instr);

    instr = INSTR_CREATE_sabd_vector(dc,
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20),
                                     opnd_create_reg(DR_REG_D28),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc,
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     opnd_create_reg(DR_REG_Q28),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc,
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20),
                                     opnd_create_reg(DR_REG_D28),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc,
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     opnd_create_reg(DR_REG_Q28),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc,
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20),
                                     opnd_create_reg(DR_REG_D28),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_sabd_vector(dc,
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     opnd_create_reg(DR_REG_Q28),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sabd, instr);

    instr = INSTR_CREATE_saba_vector(dc,
                                     opnd_create_reg(DR_REG_D27),
                                     opnd_create_reg(DR_REG_D30),
                                     opnd_create_reg(DR_REG_D4),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc,
                                     opnd_create_reg(DR_REG_Q27),
                                     opnd_create_reg(DR_REG_Q30),
                                     opnd_create_reg(DR_REG_Q4),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc,
                                     opnd_create_reg(DR_REG_D27),
                                     opnd_create_reg(DR_REG_D30),
                                     opnd_create_reg(DR_REG_D4),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc,
                                     opnd_create_reg(DR_REG_Q27),
                                     opnd_create_reg(DR_REG_Q30),
                                     opnd_create_reg(DR_REG_Q4),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc,
                                     opnd_create_reg(DR_REG_D27),
                                     opnd_create_reg(DR_REG_D30),
                                     opnd_create_reg(DR_REG_D4),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_saba_vector(dc,
                                     opnd_create_reg(DR_REG_Q27),
                                     opnd_create_reg(DR_REG_Q30),
                                     opnd_create_reg(DR_REG_Q4),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_saba, instr);

    instr = INSTR_CREATE_add_vector(dc,
                                    opnd_create_reg(DR_REG_D20),
                                    opnd_create_reg(DR_REG_D10),
                                    opnd_create_reg(DR_REG_D14),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc,
                                    opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q10),
                                    opnd_create_reg(DR_REG_Q14),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc,
                                    opnd_create_reg(DR_REG_D20),
                                    opnd_create_reg(DR_REG_D10),
                                    opnd_create_reg(DR_REG_D14),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc,
                                    opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q10),
                                    opnd_create_reg(DR_REG_Q14),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc,
                                    opnd_create_reg(DR_REG_D20),
                                    opnd_create_reg(DR_REG_D10),
                                    opnd_create_reg(DR_REG_D14),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc,
                                    opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q10),
                                    opnd_create_reg(DR_REG_Q14),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_add_vector(dc,
                                    opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q10),
                                    opnd_create_reg(DR_REG_Q14),
                                    OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_add, instr);

    instr = INSTR_CREATE_cmtst_vector(dc,
                                      opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D2),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc,
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q2),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc,
                                      opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D2),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc,
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q2),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc,
                                      opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D2),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc,
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q2),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_cmtst_vector(dc,
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q2),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmtst, instr);

    instr = INSTR_CREATE_mla_vector(dc,
                                    opnd_create_reg(DR_REG_D2),
                                    opnd_create_reg(DR_REG_D19),
                                    opnd_create_reg(DR_REG_D4),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc,
                                    opnd_create_reg(DR_REG_Q2),
                                    opnd_create_reg(DR_REG_Q19),
                                    opnd_create_reg(DR_REG_Q4),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc,
                                    opnd_create_reg(DR_REG_D2),
                                    opnd_create_reg(DR_REG_D19),
                                    opnd_create_reg(DR_REG_D4),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc,
                                    opnd_create_reg(DR_REG_Q2),
                                    opnd_create_reg(DR_REG_Q19),
                                    opnd_create_reg(DR_REG_Q4),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc,
                                    opnd_create_reg(DR_REG_D2),
                                    opnd_create_reg(DR_REG_D19),
                                    opnd_create_reg(DR_REG_D4),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mla_vector(dc,
                                    opnd_create_reg(DR_REG_Q2),
                                    opnd_create_reg(DR_REG_Q19),
                                    opnd_create_reg(DR_REG_Q4),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mla, instr);

    instr = INSTR_CREATE_mul_vector(dc,
                                    opnd_create_reg(DR_REG_D5),
                                    opnd_create_reg(DR_REG_D9),
                                    opnd_create_reg(DR_REG_D24),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc,
                                    opnd_create_reg(DR_REG_Q5),
                                    opnd_create_reg(DR_REG_Q9),
                                    opnd_create_reg(DR_REG_Q24),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc,
                                    opnd_create_reg(DR_REG_D5),
                                    opnd_create_reg(DR_REG_D9),
                                    opnd_create_reg(DR_REG_D24),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc,
                                    opnd_create_reg(DR_REG_Q5),
                                    opnd_create_reg(DR_REG_Q9),
                                    opnd_create_reg(DR_REG_Q24),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc,
                                    opnd_create_reg(DR_REG_D5),
                                    opnd_create_reg(DR_REG_D9),
                                    opnd_create_reg(DR_REG_D24),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_mul_vector(dc,
                                    opnd_create_reg(DR_REG_Q5),
                                    opnd_create_reg(DR_REG_Q9),
                                    opnd_create_reg(DR_REG_Q24),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mul, instr);

    instr = INSTR_CREATE_smaxp_vector(dc,
                                      opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D7),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc,
                                      opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q7),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc,
                                      opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D7),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc,
                                      opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q7),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc,
                                      opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D9),
                                      opnd_create_reg(DR_REG_D7),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_smaxp_vector(dc,
                                      opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q9),
                                      opnd_create_reg(DR_REG_Q7),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_smaxp, instr);

    instr = INSTR_CREATE_sminp_vector(dc,
                                      opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D10),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc,
                                      opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q10),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc,
                                      opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D10),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc,
                                      opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q10),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc,
                                      opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D10),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sminp_vector(dc,
                                      opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q10),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sminp, instr);

    instr = INSTR_CREATE_sqdmulh_vector(dc,
                                        opnd_create_reg(DR_REG_D12),
                                        opnd_create_reg(DR_REG_D22),
                                        opnd_create_reg(DR_REG_D27),
                                        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmulh, instr);

    instr = INSTR_CREATE_sqdmulh_vector(dc,
                                        opnd_create_reg(DR_REG_Q12),
                                        opnd_create_reg(DR_REG_Q22),
                                        opnd_create_reg(DR_REG_Q27),
                                        OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqdmulh, instr);

    instr = INSTR_CREATE_sqdmulh_vector(dc,
                                        opnd_create_reg(DR_REG_D12),
                                        opnd_create_reg(DR_REG_D22),
                                        opnd_create_reg(DR_REG_D27),
                                        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmulh, instr);

    instr = INSTR_CREATE_sqdmulh_vector(dc,
                                        opnd_create_reg(DR_REG_Q12),
                                        opnd_create_reg(DR_REG_Q22),
                                        opnd_create_reg(DR_REG_Q27),
                                        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqdmulh, instr);

    instr = INSTR_CREATE_addp_vector(dc,
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D15),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc,
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q15),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc,
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D15),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc,
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q15),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc,
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D15),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc,
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q15),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_addp_vector(dc,
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q15),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_addp, instr);

    instr = INSTR_CREATE_fmaxnm_vector(dc,
                                       opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D9),
                                       opnd_create_reg(DR_REG_D11),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmaxnm_vector(dc,
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q9),
                                       opnd_create_reg(DR_REG_Q11),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmaxnm_vector(dc,
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q9),
                                       opnd_create_reg(DR_REG_Q11),
                                       OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmaxnm, instr);

    instr = INSTR_CREATE_fmla_vector(dc,
                                     opnd_create_reg(DR_REG_D7),
                                     opnd_create_reg(DR_REG_D29),
                                     opnd_create_reg(DR_REG_D19),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fmla_vector(dc,
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q29),
                                     opnd_create_reg(DR_REG_Q19),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fmla_vector(dc,
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q29),
                                     opnd_create_reg(DR_REG_Q19),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmla, instr);

    instr = INSTR_CREATE_fadd_vector(dc,
                                     opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D11),
                                     opnd_create_reg(DR_REG_D11),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fadd_vector(dc,
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q11),
                                     opnd_create_reg(DR_REG_Q11),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fadd_vector(dc,
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q11),
                                     opnd_create_reg(DR_REG_Q11),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fadd, instr);

    instr = INSTR_CREATE_fmulx_vector(dc,
                                      opnd_create_reg(DR_REG_D30),
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D20),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fmulx_vector(dc,
                                      opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q20),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fmulx_vector(dc,
                                      opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q20),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmulx, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc,
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D14),
                                      opnd_create_reg(DR_REG_D0),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc,
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_Q0),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fcmeq_vector(dc,
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q14),
                                      opnd_create_reg(DR_REG_Q0),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcmeq, instr);

    instr = INSTR_CREATE_fmlal_vector(dc,
                                      opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31));
    test_instr_encoding(dc, OP_fmlal, instr);

    instr = INSTR_CREATE_fmlal_vector(dc,
                                      opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31));
    test_instr_encoding(dc, OP_fmlal, instr);

    instr = INSTR_CREATE_fmax_vector(dc,
                                     opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D21),
                                     opnd_create_reg(DR_REG_D20),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_fmax_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q21),
                                     opnd_create_reg(DR_REG_Q20),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_fmax_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q21),
                                     opnd_create_reg(DR_REG_Q20),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmax, instr);

    instr = INSTR_CREATE_frecps_vector(dc,
                                       opnd_create_reg(DR_REG_D15),
                                       opnd_create_reg(DR_REG_D5),
                                       opnd_create_reg(DR_REG_D16),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_frecps, instr);

    instr = INSTR_CREATE_frecps_vector(dc,
                                       opnd_create_reg(DR_REG_Q15),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q16),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_frecps, instr);

    instr = INSTR_CREATE_frecps_vector(dc,
                                       opnd_create_reg(DR_REG_Q15),
                                       opnd_create_reg(DR_REG_Q5),
                                       opnd_create_reg(DR_REG_Q16),
                                       OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_frecps, instr);

    instr = INSTR_CREATE_and_vector(dc,
                                    opnd_create_reg(DR_REG_D28),
                                    opnd_create_reg(DR_REG_D25),
                                    opnd_create_reg(DR_REG_D10));
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_and_vector(dc,
                                    opnd_create_reg(DR_REG_Q28),
                                    opnd_create_reg(DR_REG_Q25),
                                    opnd_create_reg(DR_REG_Q10));
    test_instr_encoding(dc, OP_and, instr);

    instr = INSTR_CREATE_bic_vector(dc,
                                    opnd_create_reg(DR_REG_D24),
                                    opnd_create_reg(DR_REG_D31),
                                    opnd_create_reg(DR_REG_D15));
    test_instr_encoding(dc, OP_bic, instr);

    instr = INSTR_CREATE_bic_vector(dc,
                                    opnd_create_reg(DR_REG_Q24),
                                    opnd_create_reg(DR_REG_Q31),
                                    opnd_create_reg(DR_REG_Q15));
    test_instr_encoding(dc, OP_bic, instr);

    instr = INSTR_CREATE_fminnm_vector(dc,
                                       opnd_create_reg(DR_REG_D17),
                                       opnd_create_reg(DR_REG_D30),
                                       opnd_create_reg(DR_REG_D31),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fminnm_vector(dc,
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q31),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fminnm_vector(dc,
                                       opnd_create_reg(DR_REG_Q17),
                                       opnd_create_reg(DR_REG_Q30),
                                       opnd_create_reg(DR_REG_Q31),
                                       OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fminnm, instr);

    instr = INSTR_CREATE_fmls_vector(dc,
                                     opnd_create_reg(DR_REG_D4),
                                     opnd_create_reg(DR_REG_D31),
                                     opnd_create_reg(DR_REG_D29),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fmls_vector(dc,
                                     opnd_create_reg(DR_REG_Q4),
                                     opnd_create_reg(DR_REG_Q31),
                                     opnd_create_reg(DR_REG_Q29),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fmls_vector(dc,
                                     opnd_create_reg(DR_REG_Q4),
                                     opnd_create_reg(DR_REG_Q31),
                                     opnd_create_reg(DR_REG_Q29),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmls, instr);

    instr = INSTR_CREATE_fsub_vector(dc,
                                     opnd_create_reg(DR_REG_D25),
                                     opnd_create_reg(DR_REG_D8),
                                     opnd_create_reg(DR_REG_D26),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fsub_vector(dc,
                                     opnd_create_reg(DR_REG_Q25),
                                     opnd_create_reg(DR_REG_Q8),
                                     opnd_create_reg(DR_REG_Q26),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fsub_vector(dc,
                                     opnd_create_reg(DR_REG_Q25),
                                     opnd_create_reg(DR_REG_Q8),
                                     opnd_create_reg(DR_REG_Q26),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fsub, instr);

    instr = INSTR_CREATE_fmlsl_vector(dc,
                                      opnd_create_reg(DR_REG_D0),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31));
    test_instr_encoding(dc, OP_fmlsl, instr);

    instr = INSTR_CREATE_fmlsl_vector(dc,
                                      opnd_create_reg(DR_REG_Q0),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31));
    test_instr_encoding(dc, OP_fmlsl, instr);

    instr = INSTR_CREATE_fmin_vector(dc,
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D31),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_fmin_vector(dc,
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q31),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_fmin_vector(dc,
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q31),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmin, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc,
                                        opnd_create_reg(DR_REG_D10),
                                        opnd_create_reg(DR_REG_D28),
                                        opnd_create_reg(DR_REG_D6),
                                        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc,
                                        opnd_create_reg(DR_REG_Q10),
                                        opnd_create_reg(DR_REG_Q28),
                                        opnd_create_reg(DR_REG_Q6),
                                        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr = INSTR_CREATE_frsqrts_vector(dc,
                                        opnd_create_reg(DR_REG_Q10),
                                        opnd_create_reg(DR_REG_Q28),
                                        opnd_create_reg(DR_REG_Q6),
                                        OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_frsqrts, instr);

    instr = INSTR_CREATE_orr_vector(dc,
                                    opnd_create_reg(DR_REG_D26),
                                    opnd_create_reg(DR_REG_D2),
                                    opnd_create_reg(DR_REG_D0));
    test_instr_encoding(dc, OP_orr, instr);

    instr = INSTR_CREATE_orr_vector(dc,
                                    opnd_create_reg(DR_REG_Q26),
                                    opnd_create_reg(DR_REG_Q2),
                                    opnd_create_reg(DR_REG_Q0));
    test_instr_encoding(dc, OP_orr, instr);

    instr = INSTR_CREATE_orn_vector(dc,
                                    opnd_create_reg(DR_REG_D28),
                                    opnd_create_reg(DR_REG_D4),
                                    opnd_create_reg(DR_REG_D3));
    test_instr_encoding(dc, OP_orn, instr);

    instr = INSTR_CREATE_orn_vector(dc,
                                    opnd_create_reg(DR_REG_Q28),
                                    opnd_create_reg(DR_REG_Q4),
                                    opnd_create_reg(DR_REG_Q3));
    test_instr_encoding(dc, OP_orn, instr);

    instr = INSTR_CREATE_uhadd_vector(dc,
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D9),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q9),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc,
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D9),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q9),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc,
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D9),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uhadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q9),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uhadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc,
                                      opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc,
                                      opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc,
                                      opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D31),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_uqadd_vector(dc,
                                      opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q31),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc,
                                       opnd_create_reg(DR_REG_D8),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D27),
                                       OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc,
                                       opnd_create_reg(DR_REG_Q8),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q27),
                                       OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc,
                                       opnd_create_reg(DR_REG_D8),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D27),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc,
                                       opnd_create_reg(DR_REG_Q8),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q27),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc,
                                       opnd_create_reg(DR_REG_D8),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D27),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_urhadd_vector(dc,
                                       opnd_create_reg(DR_REG_Q8),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q27),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_urhadd, instr);

    instr = INSTR_CREATE_uhsub_vector(dc,
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D21),
                                      opnd_create_reg(DR_REG_D16),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q21),
                                      opnd_create_reg(DR_REG_Q16),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc,
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D21),
                                      opnd_create_reg(DR_REG_D16),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q21),
                                      opnd_create_reg(DR_REG_Q16),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc,
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D21),
                                      opnd_create_reg(DR_REG_D16),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uhsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q21),
                                      opnd_create_reg(DR_REG_Q16),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uhsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc,
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D21),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q21),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc,
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D21),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q21),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc,
                                      opnd_create_reg(DR_REG_D29),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D21),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q21),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_uqsub_vector(dc,
                                      opnd_create_reg(DR_REG_Q29),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q21),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqsub, instr);

    instr = INSTR_CREATE_cmhi_vector(dc,
                                     opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc,
                                     opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc,
                                     opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc,
                                     opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc,
                                     opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D20),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc,
                                     opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhi_vector(dc,
                                     opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q20),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmhi, instr);

    instr = INSTR_CREATE_cmhs_vector(dc,
                                     opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D30),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q30),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc,
                                     opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D30),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q30),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc,
                                     opnd_create_reg(DR_REG_D2),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D30),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q30),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_cmhs_vector(dc,
                                     opnd_create_reg(DR_REG_Q2),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q30),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmhs, instr);

    instr = INSTR_CREATE_ushl_vector(dc,
                                     opnd_create_reg(DR_REG_D1),
                                     opnd_create_reg(DR_REG_D7),
                                     opnd_create_reg(DR_REG_D18),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc,
                                     opnd_create_reg(DR_REG_Q1),
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q18),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc,
                                     opnd_create_reg(DR_REG_D1),
                                     opnd_create_reg(DR_REG_D7),
                                     opnd_create_reg(DR_REG_D18),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc,
                                     opnd_create_reg(DR_REG_Q1),
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q18),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc,
                                     opnd_create_reg(DR_REG_D1),
                                     opnd_create_reg(DR_REG_D7),
                                     opnd_create_reg(DR_REG_D18),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc,
                                     opnd_create_reg(DR_REG_Q1),
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q18),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_ushl_vector(dc,
                                     opnd_create_reg(DR_REG_Q1),
                                     opnd_create_reg(DR_REG_Q7),
                                     opnd_create_reg(DR_REG_Q18),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_ushl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc,
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D18),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q18),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc,
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D18),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q18),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc,
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D15),
                                      opnd_create_reg(DR_REG_D18),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q18),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_uqshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q15),
                                      opnd_create_reg(DR_REG_Q18),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc,
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D6),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q6),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc,
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D6),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q6),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc,
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D2),
                                      opnd_create_reg(DR_REG_D6),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q6),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_urshl_vector(dc,
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q2),
                                      opnd_create_reg(DR_REG_Q6),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_urshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_D18),
                                       opnd_create_reg(DR_REG_D10),
                                       opnd_create_reg(DR_REG_D30),
                                       OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q30),
                                       OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_D18),
                                       opnd_create_reg(DR_REG_D10),
                                       opnd_create_reg(DR_REG_D30),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q30),
                                       OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_D18),
                                       opnd_create_reg(DR_REG_D10),
                                       opnd_create_reg(DR_REG_D30),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q30),
                                       OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_uqrshl_vector(dc,
                                       opnd_create_reg(DR_REG_Q18),
                                       opnd_create_reg(DR_REG_Q10),
                                       opnd_create_reg(DR_REG_Q30),
                                       OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_uqrshl, instr);

    instr = INSTR_CREATE_umax_vector(dc,
                                     opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D23),
                                     opnd_create_reg(DR_REG_D25),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc,
                                     opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q23),
                                     opnd_create_reg(DR_REG_Q25),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc,
                                     opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D23),
                                     opnd_create_reg(DR_REG_D25),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc,
                                     opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q23),
                                     opnd_create_reg(DR_REG_Q25),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc,
                                     opnd_create_reg(DR_REG_D9),
                                     opnd_create_reg(DR_REG_D23),
                                     opnd_create_reg(DR_REG_D25),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umax_vector(dc,
                                     opnd_create_reg(DR_REG_Q9),
                                     opnd_create_reg(DR_REG_Q23),
                                     opnd_create_reg(DR_REG_Q25),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umax, instr);

    instr = INSTR_CREATE_umin_vector(dc,
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D11),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc,
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q11),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc,
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D11),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc,
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q11),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc,
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D22),
                                     opnd_create_reg(DR_REG_D11),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_umin_vector(dc,
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q22),
                                     opnd_create_reg(DR_REG_Q11),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umin, instr);

    instr = INSTR_CREATE_uabd_vector(dc,
                                     opnd_create_reg(DR_REG_D5),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D27),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc,
                                     opnd_create_reg(DR_REG_Q5),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q27),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc,
                                     opnd_create_reg(DR_REG_D5),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D27),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc,
                                     opnd_create_reg(DR_REG_Q5),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q27),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc,
                                     opnd_create_reg(DR_REG_D5),
                                     opnd_create_reg(DR_REG_D12),
                                     opnd_create_reg(DR_REG_D27),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uabd_vector(dc,
                                     opnd_create_reg(DR_REG_Q5),
                                     opnd_create_reg(DR_REG_Q12),
                                     opnd_create_reg(DR_REG_Q27),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uabd, instr);

    instr = INSTR_CREATE_uaba_vector(dc,
                                     opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D6),
                                     opnd_create_reg(DR_REG_D19),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc,
                                     opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q6),
                                     opnd_create_reg(DR_REG_Q19),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc,
                                     opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D6),
                                     opnd_create_reg(DR_REG_D19),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc,
                                     opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q6),
                                     opnd_create_reg(DR_REG_Q19),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc,
                                     opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D6),
                                     opnd_create_reg(DR_REG_D19),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_uaba_vector(dc,
                                     opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q6),
                                     opnd_create_reg(DR_REG_Q19),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uaba, instr);

    instr = INSTR_CREATE_sub_vector(dc,
                                    opnd_create_reg(DR_REG_D29),
                                    opnd_create_reg(DR_REG_D27),
                                    opnd_create_reg(DR_REG_D28),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc,
                                    opnd_create_reg(DR_REG_Q29),
                                    opnd_create_reg(DR_REG_Q27),
                                    opnd_create_reg(DR_REG_Q28),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc,
                                    opnd_create_reg(DR_REG_D29),
                                    opnd_create_reg(DR_REG_D27),
                                    opnd_create_reg(DR_REG_D28),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc,
                                    opnd_create_reg(DR_REG_Q29),
                                    opnd_create_reg(DR_REG_Q27),
                                    opnd_create_reg(DR_REG_Q28),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc,
                                    opnd_create_reg(DR_REG_D29),
                                    opnd_create_reg(DR_REG_D27),
                                    opnd_create_reg(DR_REG_D28),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc,
                                    opnd_create_reg(DR_REG_Q29),
                                    opnd_create_reg(DR_REG_Q27),
                                    opnd_create_reg(DR_REG_Q28),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_sub_vector(dc,
                                    opnd_create_reg(DR_REG_Q29),
                                    opnd_create_reg(DR_REG_Q27),
                                    opnd_create_reg(DR_REG_Q28),
                                    OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_sub, instr);

    instr = INSTR_CREATE_cmeq_vector(dc,
                                     opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D17),
                                     opnd_create_reg(DR_REG_D23),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc,
                                     opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q23),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc,
                                     opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D17),
                                     opnd_create_reg(DR_REG_D23),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc,
                                     opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q23),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc,
                                     opnd_create_reg(DR_REG_D13),
                                     opnd_create_reg(DR_REG_D17),
                                     opnd_create_reg(DR_REG_D23),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc,
                                     opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q23),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_cmeq_vector(dc,
                                     opnd_create_reg(DR_REG_Q13),
                                     opnd_create_reg(DR_REG_Q17),
                                     opnd_create_reg(DR_REG_Q23),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_cmeq, instr);

    instr = INSTR_CREATE_mls_vector(dc,
                                    opnd_create_reg(DR_REG_D7),
                                    opnd_create_reg(DR_REG_D13),
                                    opnd_create_reg(DR_REG_D27),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc,
                                    opnd_create_reg(DR_REG_Q7),
                                    opnd_create_reg(DR_REG_Q13),
                                    opnd_create_reg(DR_REG_Q27),
                                    OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc,
                                    opnd_create_reg(DR_REG_D7),
                                    opnd_create_reg(DR_REG_D13),
                                    opnd_create_reg(DR_REG_D27),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc,
                                    opnd_create_reg(DR_REG_Q7),
                                    opnd_create_reg(DR_REG_Q13),
                                    opnd_create_reg(DR_REG_Q27),
                                    OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc,
                                    opnd_create_reg(DR_REG_D7),
                                    opnd_create_reg(DR_REG_D13),
                                    opnd_create_reg(DR_REG_D27),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_mls_vector(dc,
                                    opnd_create_reg(DR_REG_Q7),
                                    opnd_create_reg(DR_REG_Q13),
                                    opnd_create_reg(DR_REG_Q27),
                                    OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_mls, instr);

    instr = INSTR_CREATE_pmul_vector(dc,
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D24),
                                     opnd_create_reg(DR_REG_D12),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_pmul, instr);

    instr = INSTR_CREATE_pmul_vector(dc,
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q24),
                                     opnd_create_reg(DR_REG_Q12),
                                     OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_pmul, instr);

    instr = INSTR_CREATE_umaxp_vector(dc,
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D5),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc,
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q5),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc,
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D5),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc,
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q5),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc,
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D27),
                                      opnd_create_reg(DR_REG_D5),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_umaxp_vector(dc,
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q27),
                                      opnd_create_reg(DR_REG_Q5),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_umaxp, instr);

    instr = INSTR_CREATE_uminp_vector(dc,
                                      opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D16),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc,
                                      opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q16),
                                      OPND_CREATE_BYTE());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc,
                                      opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D16),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc,
                                      opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q16),
                                      OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc,
                                      opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D16),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_uminp_vector(dc,
                                      opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q16),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_uminp, instr);

    instr = INSTR_CREATE_sqrdmulh_vector(dc,
                                         opnd_create_reg(DR_REG_D23),
                                         opnd_create_reg(DR_REG_D29),
                                         opnd_create_reg(DR_REG_D27),
                                         OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqrdmulh, instr);

    instr = INSTR_CREATE_sqrdmulh_vector(dc,
                                         opnd_create_reg(DR_REG_Q23),
                                         opnd_create_reg(DR_REG_Q29),
                                         opnd_create_reg(DR_REG_Q27),
                                         OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_sqrdmulh, instr);

    instr = INSTR_CREATE_sqrdmulh_vector(dc,
                                         opnd_create_reg(DR_REG_D23),
                                         opnd_create_reg(DR_REG_D29),
                                         opnd_create_reg(DR_REG_D27),
                                         OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqrdmulh, instr);

    instr = INSTR_CREATE_sqrdmulh_vector(dc,
                                         opnd_create_reg(DR_REG_Q23),
                                         opnd_create_reg(DR_REG_Q29),
                                         opnd_create_reg(DR_REG_Q27),
                                         OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_sqrdmulh, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(dc,
                                        opnd_create_reg(DR_REG_D12),
                                        opnd_create_reg(DR_REG_D18),
                                        opnd_create_reg(DR_REG_D29),
                                        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(dc,
                                        opnd_create_reg(DR_REG_Q12),
                                        opnd_create_reg(DR_REG_Q18),
                                        opnd_create_reg(DR_REG_Q29),
                                        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_fmaxnmp_vector(dc,
                                        opnd_create_reg(DR_REG_Q12),
                                        opnd_create_reg(DR_REG_Q18),
                                        opnd_create_reg(DR_REG_Q29),
                                        OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmaxnmp, instr);

    instr = INSTR_CREATE_fmlal2_vector(dc,
                                       opnd_create_reg(DR_REG_D0),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D31));
    test_instr_encoding(dc, OP_fmlal2, instr);

    instr = INSTR_CREATE_fmlal2_vector(dc,
                                       opnd_create_reg(DR_REG_Q0),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q31));
    test_instr_encoding(dc, OP_fmlal2, instr);

    instr = INSTR_CREATE_faddp_vector(dc,
                                      opnd_create_reg(DR_REG_D18),
                                      opnd_create_reg(DR_REG_D31),
                                      opnd_create_reg(DR_REG_D16),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_faddp_vector(dc,
                                      opnd_create_reg(DR_REG_Q18),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q16),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_faddp_vector(dc,
                                      opnd_create_reg(DR_REG_Q18),
                                      opnd_create_reg(DR_REG_Q31),
                                      opnd_create_reg(DR_REG_Q16),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_faddp, instr);

    instr = INSTR_CREATE_fmul_vector(dc,
                                     opnd_create_reg(DR_REG_D25),
                                     opnd_create_reg(DR_REG_D28),
                                     opnd_create_reg(DR_REG_D21),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fmul_vector(dc,
                                     opnd_create_reg(DR_REG_Q25),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q21),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fmul_vector(dc,
                                     opnd_create_reg(DR_REG_Q25),
                                     opnd_create_reg(DR_REG_Q28),
                                     opnd_create_reg(DR_REG_Q21),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmul, instr);

    instr = INSTR_CREATE_fcmge_vector(dc,
                                      opnd_create_reg(DR_REG_D22),
                                      opnd_create_reg(DR_REG_D17),
                                      opnd_create_reg(DR_REG_D30),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_fcmge_vector(dc,
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q17),
                                      opnd_create_reg(DR_REG_Q30),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_fcmge_vector(dc,
                                      opnd_create_reg(DR_REG_Q22),
                                      opnd_create_reg(DR_REG_Q17),
                                      opnd_create_reg(DR_REG_Q30),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcmge, instr);

    instr = INSTR_CREATE_facge_vector(dc,
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D30),
                                      opnd_create_reg(DR_REG_D30),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_facge_vector(dc,
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_Q30),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_facge_vector(dc,
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q30),
                                      opnd_create_reg(DR_REG_Q30),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_facge, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc,
                                      opnd_create_reg(DR_REG_D5),
                                      opnd_create_reg(DR_REG_D23),
                                      opnd_create_reg(DR_REG_D25),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc,
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q25),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fmaxp_vector(dc,
                                      opnd_create_reg(DR_REG_Q5),
                                      opnd_create_reg(DR_REG_Q23),
                                      opnd_create_reg(DR_REG_Q25),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmaxp, instr);

    instr = INSTR_CREATE_fdiv_vector(dc,
                                     opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D26),
                                     opnd_create_reg(DR_REG_D4),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fdiv_vector(dc,
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q4),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_fdiv_vector(dc,
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q26),
                                     opnd_create_reg(DR_REG_Q4),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fdiv, instr);

    instr = INSTR_CREATE_eor_vector(dc,
                                    opnd_create_reg(DR_REG_D19),
                                    opnd_create_reg(DR_REG_D1),
                                    opnd_create_reg(DR_REG_D20));
    test_instr_encoding(dc, OP_eor, instr);

    instr = INSTR_CREATE_eor_vector(dc,
                                    opnd_create_reg(DR_REG_Q19),
                                    opnd_create_reg(DR_REG_Q1),
                                    opnd_create_reg(DR_REG_Q20));
    test_instr_encoding(dc, OP_eor, instr);

    instr = INSTR_CREATE_bsl_vector(dc,
                                    opnd_create_reg(DR_REG_D20),
                                    opnd_create_reg(DR_REG_D4),
                                    opnd_create_reg(DR_REG_D25));
    test_instr_encoding(dc, OP_bsl, instr);

    instr = INSTR_CREATE_bsl_vector(dc,
                                    opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q4),
                                    opnd_create_reg(DR_REG_Q25));
    test_instr_encoding(dc, OP_bsl, instr);

    instr = INSTR_CREATE_fminnmp_vector(dc,
                                        opnd_create_reg(DR_REG_D23),
                                        opnd_create_reg(DR_REG_D18),
                                        opnd_create_reg(DR_REG_D11),
                                        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fminnmp_vector(dc,
                                        opnd_create_reg(DR_REG_Q23),
                                        opnd_create_reg(DR_REG_Q18),
                                        opnd_create_reg(DR_REG_Q11),
                                        OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fminnmp_vector(dc,
                                        opnd_create_reg(DR_REG_Q23),
                                        opnd_create_reg(DR_REG_Q18),
                                        opnd_create_reg(DR_REG_Q11),
                                        OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fminnmp, instr);

    instr = INSTR_CREATE_fmlsl2_vector(dc,
                                       opnd_create_reg(DR_REG_D0),
                                       opnd_create_reg(DR_REG_D29),
                                       opnd_create_reg(DR_REG_D31));
    test_instr_encoding(dc, OP_fmlsl2, instr);

    instr = INSTR_CREATE_fmlsl2_vector(dc,
                                       opnd_create_reg(DR_REG_Q0),
                                       opnd_create_reg(DR_REG_Q29),
                                       opnd_create_reg(DR_REG_Q31));
    test_instr_encoding(dc, OP_fmlsl2, instr);

    instr = INSTR_CREATE_fabd_vector(dc,
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D19),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fabd_vector(dc,
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q19),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fabd_vector(dc,
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q19),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fabd, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc,
                                      opnd_create_reg(DR_REG_D6),
                                      opnd_create_reg(DR_REG_D3),
                                      opnd_create_reg(DR_REG_D14),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc,
                                      opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q14),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_fcmgt_vector(dc,
                                      opnd_create_reg(DR_REG_Q6),
                                      opnd_create_reg(DR_REG_Q3),
                                      opnd_create_reg(DR_REG_Q14),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fcmgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc,
                                      opnd_create_reg(DR_REG_D4),
                                      opnd_create_reg(DR_REG_D26),
                                      opnd_create_reg(DR_REG_D12),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc,
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q12),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_facgt_vector(dc,
                                      opnd_create_reg(DR_REG_Q4),
                                      opnd_create_reg(DR_REG_Q26),
                                      opnd_create_reg(DR_REG_Q12),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_facgt, instr);

    instr = INSTR_CREATE_fminp_vector(dc,
                                      opnd_create_reg(DR_REG_D28),
                                      opnd_create_reg(DR_REG_D1),
                                      opnd_create_reg(DR_REG_D25),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminp, instr);

    instr = INSTR_CREATE_fminp_vector(dc,
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q1),
                                      opnd_create_reg(DR_REG_Q25),
                                      OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fminp, instr);

    instr = INSTR_CREATE_fminp_vector(dc,
                                      opnd_create_reg(DR_REG_Q28),
                                      opnd_create_reg(DR_REG_Q1),
                                      opnd_create_reg(DR_REG_Q25),
                                      OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fminp, instr);

    instr = INSTR_CREATE_bit_vector(dc,
                                    opnd_create_reg(DR_REG_D12),
                                    opnd_create_reg(DR_REG_D21),
                                    opnd_create_reg(DR_REG_D12));
    test_instr_encoding(dc, OP_bit, instr);

    instr = INSTR_CREATE_bit_vector(dc,
                                    opnd_create_reg(DR_REG_Q12),
                                    opnd_create_reg(DR_REG_Q21),
                                    opnd_create_reg(DR_REG_Q12));
    test_instr_encoding(dc, OP_bit, instr);

    instr = INSTR_CREATE_bif_vector(dc,
                                    opnd_create_reg(DR_REG_D20),
                                    opnd_create_reg(DR_REG_D3),
                                    opnd_create_reg(DR_REG_D3));
    test_instr_encoding(dc, OP_bif, instr);

    instr = INSTR_CREATE_bif_vector(dc,
                                    opnd_create_reg(DR_REG_Q20),
                                    opnd_create_reg(DR_REG_Q3),
                                    opnd_create_reg(DR_REG_Q3));
    test_instr_encoding(dc, OP_bif, instr);
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

    print("All tests complete\n");
    return 0;
}
