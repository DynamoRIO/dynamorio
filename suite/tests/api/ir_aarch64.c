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
test_fmul(void *dc)
{
    byte *pc;
    instr_t *instr;

    /* FMUL scalar half precision
     * FMUL <Sd>, <Sn>, <Sm>
     */
    instr = INSTR_CREATE_fmul_scalar(dc, opnd_create_reg(DR_REG_H7),
                                     opnd_create_reg(DR_REG_H6),
                                     opnd_create_reg(DR_REG_H5));
    test_instr_encoding(dc, OP_fmul, instr);


    /* FMUL scalar single precision
     * FMUL <Sd>, <Sn>, <Sm>
     */
    instr = INSTR_CREATE_fmul_scalar(dc, opnd_create_reg(DR_REG_S0),
                                     opnd_create_reg(DR_REG_S1),
                                     opnd_create_reg(DR_REG_S2));
    test_instr_encoding(dc, OP_fmul, instr);

    /* FMUL scalar single precision
     * FMUL <Dd>, <Dn>, <Dm>
     */
    instr = INSTR_CREATE_fmul_scalar(dc, opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D18));
    test_instr_encoding(dc, OP_fmul, instr);

    /* FMUL 64 bit vector half precision
     * FMUL <Vd>.4h, <Vn>.4h, <Vd>.4h
     */
    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D18),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmul, instr);

    /* FMUL 64 bit vector single precision
     * FMUL <Vd>.2s, <Vn>.2s, <Vd>.2s
     */
    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_D10),
                                     opnd_create_reg(DR_REG_D15),
                                     opnd_create_reg(DR_REG_D18),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmul, instr);

    /* FMUL 128 bit vector half precision
     * FMUL <Vd>.8h, <Vn>.8h, <Vd>.8h
     */
    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q18),
                                     OPND_CREATE_HALF());
    test_instr_encoding(dc, OP_fmul, instr);

    /* FMUL 128 bit vector single precision
     * FMUL <Vd>.4s, <Vn>.4s, <Vd>.4s
     */
    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q18),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmul, instr);

    /* FMUL 128 bit vector single precision
     * FMUL <Vd>.4s, <Vn>.4s, <Vd>.4s
     */
    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q18),
                                     OPND_CREATE_SINGLE());
    test_instr_encoding(dc, OP_fmul, instr);

    /* FMUL 128 bit vector double precision
     * FMUL <Vd>.2d, <Vn>.2d, <Vd>.2d
     */
    instr = INSTR_CREATE_fmul_vector(dc, opnd_create_reg(DR_REG_Q10),
                                     opnd_create_reg(DR_REG_Q15),
                                     opnd_create_reg(DR_REG_Q18),
                                     OPND_CREATE_DOUBLE());
    test_instr_encoding(dc, OP_fmul, instr);

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

    test_fmul(dcontext);
    print("test_fmul complete\n");


    print("All tests complete\n");
    return 0;
}
