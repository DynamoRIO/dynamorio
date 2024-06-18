/* **********************************************************
 * Copyright (c) 2024 Institute of Software Chinese Academy of Sciences (ISCAS).
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
#include "dr_defines.h"
#include "dr_ir_utils.h"
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
test_instr_encoding_failure(void *dc, uint opcode, app_pc instr_pc, instr_t *instr)
{
    byte *pc;

    pc = instr_encode_to_copy(dc, instr, buf, instr_pc);
    ASSERT(pc == NULL);
    instr_destroy(dc, instr);
}

static byte *
test_instr_decoding_failure(void *dc, uint raw_instr)
{
    instr_t *decin;
    byte *pc;

    *(uint *)buf = raw_instr;
    decin = instr_create(dc);
    pc = decode(dc, buf, decin);
    /* Returns NULL on failure. */
    ASSERT(pc == NULL);
    instr_destroy(dc, decin);
    return pc;
}

static void
test_configuration_setting(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vsetivli(dc, opnd_create_reg(DR_REG_A1),
                                  opnd_create_immed_int(0b01010, OPSZ_5b),
                                  opnd_create_immed_int(0b00001000, OPSZ_10b));
    test_instr_encoding(dc, OP_vsetivli, instr);
    instr =
        INSTR_CREATE_vsetvli(dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A2),
                             opnd_create_immed_int(0b000001000, OPSZ_11b));
    test_instr_encoding(dc, OP_vsetvli, instr);
    instr = INSTR_CREATE_vsetvl(dc, opnd_create_reg(DR_REG_A1),
                                opnd_create_reg(DR_REG_A2), opnd_create_reg(DR_REG_A3));
    test_instr_encoding(dc, OP_vsetvl, instr);
}

static void
test_unit_stride(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vlm_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vlm_v, instr);
    instr = INSTR_CREATE_vsm_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0));
    test_instr_encoding(dc, OP_vsm_v, instr);
    instr = INSTR_CREATE_vle8_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vle8_v, instr);
    instr = INSTR_CREATE_vle16_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vle16_v, instr);
    instr = INSTR_CREATE_vle32_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vle32_v, instr);
    instr = INSTR_CREATE_vle64_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vle64_v, instr);
    instr = INSTR_CREATE_vse8_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vse8_v, instr);
    instr = INSTR_CREATE_vse16_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vse16_v, instr);
    instr = INSTR_CREATE_vse32_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vse32_v, instr);
    instr = INSTR_CREATE_vse64_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vse64_v, instr);
}

static void
test_indexed_unordered(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vluxei8_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vluxei8_v, instr);
    instr = INSTR_CREATE_vluxei16_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vluxei16_v, instr);
    instr = INSTR_CREATE_vluxei32_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vluxei32_v, instr);
    instr = INSTR_CREATE_vluxei64_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vluxei64_v, instr);

    instr = INSTR_CREATE_vsuxei8_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsuxei8_v, instr);
    instr = INSTR_CREATE_vsuxei16_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsuxei16_v, instr);
    instr = INSTR_CREATE_vsuxei32_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsuxei32_v, instr);
    instr = INSTR_CREATE_vsuxei64_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsuxei64_v, instr);
}

static void
test_stride(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vlse8_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_A2), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vlse8_v, instr);
    instr = INSTR_CREATE_vlse16_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_A2), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vlse16_v, instr);
    instr = INSTR_CREATE_vlse32_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_A2), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vlse32_v, instr);
    instr = INSTR_CREATE_vlse64_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_A2), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vlse64_v, instr);

    instr = INSTR_CREATE_vsse8_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A2),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsse8_v, instr);
    instr = INSTR_CREATE_vsse16_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A2),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsse16_v, instr);
    instr = INSTR_CREATE_vsse32_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A2),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsse32_v, instr);
    instr = INSTR_CREATE_vsse64_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A2),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsse64_v, instr);
}

static void
test_indexed_ordered(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vloxei8_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vloxei8_v, instr);
    instr = INSTR_CREATE_vloxei16_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vloxei16_v, instr);
    instr = INSTR_CREATE_vloxei32_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vloxei32_v, instr);
    instr = INSTR_CREATE_vloxei64_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b),
        opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vloxei64_v, instr);
    instr = INSTR_CREATE_vsoxei8_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsoxei8_v, instr);
    instr = INSTR_CREATE_vsoxei16_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsoxei16_v, instr);
    instr = INSTR_CREATE_vsoxei32_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsoxei32_v, instr);
    instr = INSTR_CREATE_vsoxei64_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vsoxei64_v, instr);
}

static void
test_unit_stride_faultfirst(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vle8ff_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vle8ff_v, instr);
    instr = INSTR_CREATE_vle16ff_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vle16ff_v, instr);
    instr = INSTR_CREATE_vle32ff_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vle32ff_v, instr);
    instr = INSTR_CREATE_vle64ff_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_immed_int(0b1, OPSZ_1b), opnd_create_immed_int(0b000, OPSZ_3b));
    test_instr_encoding(dc, OP_vle64ff_v, instr);
}

static void
test_whole_register(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vl1re8_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl1re8_v, instr);
    instr = INSTR_CREATE_vl1re16_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl1re16_v, instr);
    instr = INSTR_CREATE_vl1re32_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl1re32_v, instr);
    instr = INSTR_CREATE_vl1re64_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl1re64_v, instr);

    instr = INSTR_CREATE_vl2re8_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl2re8_v, instr);
    instr = INSTR_CREATE_vl2re16_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl2re16_v, instr);
    instr = INSTR_CREATE_vl2re32_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl2re32_v, instr);
    instr = INSTR_CREATE_vl2re64_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl2re64_v, instr);

    instr = INSTR_CREATE_vl4re8_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl4re8_v, instr);
    instr = INSTR_CREATE_vl4re16_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl4re16_v, instr);
    instr = INSTR_CREATE_vl4re32_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl4re32_v, instr);
    instr = INSTR_CREATE_vl4re64_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl4re64_v, instr);

    instr = INSTR_CREATE_vl8re8_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl8re8_v, instr);
    instr = INSTR_CREATE_vl8re16_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl8re16_v, instr);
    instr = INSTR_CREATE_vl8re32_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl8re32_v, instr);
    instr = INSTR_CREATE_vl8re64_v(
        dc, opnd_create_reg(DR_REG_VR0),
        opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)));
    test_instr_encoding(dc, OP_vl8re64_v, instr);

    instr = INSTR_CREATE_vs1r_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0));
    test_instr_encoding(dc, OP_vs1r_v, instr);
    instr = INSTR_CREATE_vs2r_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0));
    test_instr_encoding(dc, OP_vs2r_v, instr);
    instr = INSTR_CREATE_vs4r_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0));
    test_instr_encoding(dc, OP_vs4r_v, instr);
    instr = INSTR_CREATE_vs8r_v(
        dc, opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, reg_get_size(DR_REG_VR0)),
        opnd_create_reg(DR_REG_VR0));
    test_instr_encoding(dc, OP_vs8r_v, instr);
}

static void
test_load_store(void *dc)
{
    test_unit_stride(dc);
    test_indexed_unordered(dc);
    test_stride(dc);
    test_indexed_ordered(dc);
    test_unit_stride_faultfirst(dc);
    test_whole_register(dc);
}

static void
test_FVF(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vfadd_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfadd_vf, instr);
    instr = INSTR_CREATE_vfsub_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfsub_vf, instr);
    instr = INSTR_CREATE_vfmin_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmin_vf, instr);
    instr = INSTR_CREATE_vfmax_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmax_vf, instr);
    instr = INSTR_CREATE_vfsgnj_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfsgnj_vf, instr);
    instr = INSTR_CREATE_vfsgnjn_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfsgnjn_vf, instr);
    instr = INSTR_CREATE_vfsgnjx_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfsgnjx_vf, instr);
    instr = INSTR_CREATE_vfslide1up_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfslide1up_vf, instr);
    instr = INSTR_CREATE_vfslide1down_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfslide1down_vf, instr);
    instr = INSTR_CREATE_vfmv_s_f(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_vfmv_s_f, instr);

    instr =
        INSTR_CREATE_vfmerge_vfm(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vfmerge_vfm, instr);
    instr = INSTR_CREATE_vfmv_v_f(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_vfmv_v_f, instr);
    instr = INSTR_CREATE_vmfeq_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmfeq_vf, instr);
    instr = INSTR_CREATE_vmfle_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmfle_vf, instr);
    instr = INSTR_CREATE_vmflt_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmflt_vf, instr);
    instr = INSTR_CREATE_vmfne_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmfne_vf, instr);
    instr = INSTR_CREATE_vmfgt_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmfgt_vf, instr);
    instr = INSTR_CREATE_vmfge_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmfge_vf, instr);

    instr = INSTR_CREATE_vfrdiv_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfrdiv_vf, instr);
    instr = INSTR_CREATE_vfmul_vf(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmul_vf, instr);
    instr = INSTR_CREATE_vfrsub_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfrsub_vf, instr);
    instr = INSTR_CREATE_vfmadd_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmadd_vf, instr);
    instr = INSTR_CREATE_vfnmadd_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfnmadd_vf, instr);
    instr = INSTR_CREATE_vfmsub_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmsub_vf, instr);
    instr = INSTR_CREATE_vfnmsub_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfnmsub_vf, instr);
    instr = INSTR_CREATE_vfmacc_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmacc_vf, instr);
    instr =
        INSTR_CREATE_vfmerge_vfm(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vfmerge_vfm, instr);
    instr = INSTR_CREATE_vfnmacc_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfnmacc_vf, instr);
    instr = INSTR_CREATE_vfmsac_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmsac_vf, instr);
    instr = INSTR_CREATE_vfnmsac_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfnmsac_vf, instr);

    instr = INSTR_CREATE_vfwadd_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwadd_vf, instr);
    instr = INSTR_CREATE_vfwsub_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwsub_vf, instr);
    instr = INSTR_CREATE_vfwadd_wf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwadd_wf, instr);
    instr = INSTR_CREATE_vfwsub_wf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwsub_wf, instr);
    instr = INSTR_CREATE_vfwmul_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwmul_vf, instr);
    instr = INSTR_CREATE_vfwmacc_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwmacc_vf, instr);
    instr = INSTR_CREATE_vfwnmacc_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwnmacc_vf, instr);
    instr = INSTR_CREATE_vfwmsac_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwmsac_vf, instr);
    instr = INSTR_CREATE_vfwnmsac_vf(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwnmsac_vf, instr);
}

static void
test_FVV(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vfadd_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfadd_vv, instr);
    instr = INSTR_CREATE_vfredusum_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfredusum_vs, instr);
    instr = INSTR_CREATE_vfsub_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfsub_vv, instr);
    instr = INSTR_CREATE_vfredosum_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfredosum_vs, instr);
    instr = INSTR_CREATE_vfmin_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmin_vv, instr);
    instr = INSTR_CREATE_vfredmin_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfredmin_vs, instr);
    instr = INSTR_CREATE_vfmax_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmax_vv, instr);
    instr = INSTR_CREATE_vfredmax_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfredmax_vs, instr);
    instr = INSTR_CREATE_vfsgnj_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfsgnj_vv, instr);
    instr = INSTR_CREATE_vfsgnjn_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfsgnjn_vv, instr);
    instr = INSTR_CREATE_vfsgnjx_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfsgnjx_vv, instr);
    instr = INSTR_CREATE_vfmv_f_s(dc, opnd_create_reg(DR_REG_A1),
                                  opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vfmv_f_s, instr);

    instr = INSTR_CREATE_vmfeq_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmfeq_vv, instr);
    instr = INSTR_CREATE_vmfle_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmfle_vv, instr);
    instr = INSTR_CREATE_vmflt_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmflt_vv, instr);
    instr = INSTR_CREATE_vmfne_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmfne_vv, instr);

    instr = INSTR_CREATE_vfdiv_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfdiv_vv, instr);
    instr = INSTR_CREATE_vfmul_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmul_vv, instr);
    instr = INSTR_CREATE_vfmadd_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmadd_vv, instr);
    instr = INSTR_CREATE_vfnmadd_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfnmadd_vv, instr);
    instr = INSTR_CREATE_vfmsub_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmsub_vv, instr);
    instr = INSTR_CREATE_vfnmsub_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfnmsub_vv, instr);
    instr = INSTR_CREATE_vfmacc_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmacc_vv, instr);
    instr = INSTR_CREATE_vfnmacc_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfnmacc_vv, instr);
    instr = INSTR_CREATE_vfmsac_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmsac_vv, instr);
    instr = INSTR_CREATE_vfnmsac_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfnmsac_vv, instr);

    instr = INSTR_CREATE_vfcvt_xu_f_v(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfcvt_xu_f_v, instr);
    instr = INSTR_CREATE_vfcvt_x_f_v(dc, opnd_create_reg(DR_REG_VR0),
                                     opnd_create_reg(DR_REG_VR1),
                                     opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfcvt_x_f_v, instr);
    instr = INSTR_CREATE_vfcvt_f_xu_v(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfcvt_f_xu_v, instr);
    instr = INSTR_CREATE_vfcvt_f_x_v(dc, opnd_create_reg(DR_REG_VR0),
                                     opnd_create_reg(DR_REG_VR1),
                                     opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfcvt_f_x_v, instr);
    instr = INSTR_CREATE_vfcvt_rtz_xu_f_v(dc, opnd_create_reg(DR_REG_VR0),
                                          opnd_create_reg(DR_REG_VR1),
                                          opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfcvt_rtz_xu_f_v, instr);
    instr = INSTR_CREATE_vfcvt_rtz_x_f_v(dc, opnd_create_reg(DR_REG_VR0),
                                         opnd_create_reg(DR_REG_VR1),
                                         opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfcvt_rtz_x_f_v, instr);

    instr = INSTR_CREATE_vfcvt_xu_f_v(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfcvt_xu_f_v, instr);
    instr = INSTR_CREATE_vfwcvt_x_f_v(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwcvt_x_f_v, instr);
    instr = INSTR_CREATE_vfwcvt_f_xu_v(dc, opnd_create_reg(DR_REG_VR0),
                                       opnd_create_reg(DR_REG_VR1),
                                       opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwcvt_f_xu_v, instr);
    instr = INSTR_CREATE_vfwcvt_f_x_v(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwcvt_f_x_v, instr);
    instr = INSTR_CREATE_vfwcvt_f_f_v(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwcvt_f_f_v, instr);
    instr = INSTR_CREATE_vfwcvt_rtz_xu_f_v(dc, opnd_create_reg(DR_REG_VR0),
                                           opnd_create_reg(DR_REG_VR1),
                                           opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwcvt_rtz_xu_f_v, instr);
    instr = INSTR_CREATE_vfwcvt_rtz_x_f_v(dc, opnd_create_reg(DR_REG_VR0),
                                          opnd_create_reg(DR_REG_VR1),
                                          opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwcvt_rtz_x_f_v, instr);

    instr = INSTR_CREATE_vfncvt_xu_f_w(dc, opnd_create_reg(DR_REG_VR0),
                                       opnd_create_reg(DR_REG_VR1),
                                       opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfncvt_xu_f_w, instr);
    instr = INSTR_CREATE_vfncvt_x_f_w(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfncvt_x_f_w, instr);
    instr = INSTR_CREATE_vfncvt_f_xu_w(dc, opnd_create_reg(DR_REG_VR0),
                                       opnd_create_reg(DR_REG_VR1),
                                       opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfncvt_f_xu_w, instr);
    instr = INSTR_CREATE_vfncvt_f_x_w(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfncvt_f_x_w, instr);
    instr = INSTR_CREATE_vfncvt_f_f_w(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfncvt_f_f_w, instr);
    instr = INSTR_CREATE_vfncvt_rod_f_f_w(dc, opnd_create_reg(DR_REG_VR0),
                                          opnd_create_reg(DR_REG_VR1),
                                          opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfncvt_rod_f_f_w, instr);
    instr = INSTR_CREATE_vfncvt_rtz_xu_f_w(dc, opnd_create_reg(DR_REG_VR0),
                                           opnd_create_reg(DR_REG_VR1),
                                           opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfncvt_rtz_xu_f_w, instr);
    instr = INSTR_CREATE_vfncvt_rtz_x_f_w(dc, opnd_create_reg(DR_REG_VR0),
                                          opnd_create_reg(DR_REG_VR1),
                                          opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfncvt_rtz_x_f_w, instr);

    instr = INSTR_CREATE_vfsqrt_v(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_VR1),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfsqrt_v, instr);
    instr = INSTR_CREATE_vfrsqrt7_v(dc, opnd_create_reg(DR_REG_VR0),
                                    opnd_create_reg(DR_REG_VR1),
                                    opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfrsqrt7_v, instr);
    instr = INSTR_CREATE_vfrec7_v(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_VR1),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfrec7_v, instr);
    instr = INSTR_CREATE_vfclass_v(dc, opnd_create_reg(DR_REG_VR0),
                                   opnd_create_reg(DR_REG_VR1),
                                   opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfclass_v, instr);

    instr = INSTR_CREATE_vfwadd_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwadd_vv, instr);
    instr = INSTR_CREATE_vfwredusum_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwredusum_vs, instr);
    instr = INSTR_CREATE_vfwsub_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwsub_vv, instr);
    instr = INSTR_CREATE_vfwredosum_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwredosum_vs, instr);
    instr = INSTR_CREATE_vfwadd_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwadd_wv, instr);
    instr = INSTR_CREATE_vfwsub_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwsub_wv, instr);
    instr = INSTR_CREATE_vfwmul_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwmul_vv, instr);
    instr = INSTR_CREATE_vfwmacc_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwmacc_vv, instr);
    instr = INSTR_CREATE_vfwnmacc_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwnmacc_vv, instr);
    instr = INSTR_CREATE_vfnmacc_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfnmacc_vv, instr);
    instr = INSTR_CREATE_vfmsac_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfmsac_vv, instr);
    instr = INSTR_CREATE_vfwnmsac_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfwnmsac_vv, instr);
}

static void
test_IVX(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vadd_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vadd_vx, instr);
    instr = INSTR_CREATE_vsub_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsub_vx, instr);
    instr = INSTR_CREATE_vrsub_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vrsub_vx, instr);
    instr = INSTR_CREATE_vminu_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vminu_vx, instr);
    instr = INSTR_CREATE_vmin_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmin_vx, instr);
    instr = INSTR_CREATE_vmaxu_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmaxu_vx, instr);
    instr = INSTR_CREATE_vmax_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmax_vx, instr);
    instr = INSTR_CREATE_vand_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vand_vx, instr);
    instr = INSTR_CREATE_vor_vx(dc, opnd_create_reg(DR_REG_VR0),
                                opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vor_vx, instr);
    instr = INSTR_CREATE_vxor_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vxor_vx, instr);
    instr = INSTR_CREATE_vrgather_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vrgather_vx, instr);
    instr = INSTR_CREATE_vslideup_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vslideup_vx, instr);
    instr = INSTR_CREATE_vslidedown_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vslidedown_vx, instr);

    instr =
        INSTR_CREATE_vadc_vxm(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vadc_vxm, instr);
    instr =
        INSTR_CREATE_vmadc_vxm(dc, opnd_create_reg(DR_REG_VR0),
                               opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmadc_vxm, instr);
    instr =
        INSTR_CREATE_vmadc_vx(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmadc_vx, instr);
    instr =
        INSTR_CREATE_vsbc_vxm(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vsbc_vxm, instr);
    instr =
        INSTR_CREATE_vmsbc_vxm(dc, opnd_create_reg(DR_REG_VR0),
                               opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmsbc_vxm, instr);
    instr =
        INSTR_CREATE_vmsbc_vx(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
                              opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmsbc_vx, instr);
    instr =
        INSTR_CREATE_vmerge_vxm(dc, opnd_create_reg(DR_REG_VR0),
                                opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmerge_vxm, instr);
    instr =
        INSTR_CREATE_vmv_v_x(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_vmv_v_x, instr);
    instr = INSTR_CREATE_vmseq_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmseq_vx, instr);
    instr = INSTR_CREATE_vmsne_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsne_vx, instr);
    instr = INSTR_CREATE_vmsltu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsltu_vx, instr);
    instr = INSTR_CREATE_vmslt_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmslt_vx, instr);
    instr = INSTR_CREATE_vmsleu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsleu_vx, instr);
    instr = INSTR_CREATE_vmsle_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsle_vx, instr);
    instr = INSTR_CREATE_vmsgtu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsgtu_vx, instr);
    instr = INSTR_CREATE_vmsgt_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsgt_vx, instr);

    instr = INSTR_CREATE_vsaddu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsaddu_vx, instr);
    instr = INSTR_CREATE_vsadd_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsadd_vx, instr);
    instr = INSTR_CREATE_vssubu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssubu_vx, instr);
    instr = INSTR_CREATE_vssub_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssub_vx, instr);
    instr = INSTR_CREATE_vsll_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsll_vx, instr);
    instr = INSTR_CREATE_vsmul_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsmul_vx, instr);
    instr = INSTR_CREATE_vsrl_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsrl_vx, instr);

    instr = INSTR_CREATE_vsra_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsra_vx, instr);
    instr = INSTR_CREATE_vssrl_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssrl_vx, instr);
    instr = INSTR_CREATE_vssra_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssra_vx, instr);
    instr = INSTR_CREATE_vnsrl_wx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnsrl_wx, instr);
    instr = INSTR_CREATE_vnsra_wx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnsra_wx, instr);
    instr = INSTR_CREATE_vnclipu_wx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnclipu_wx, instr);
    instr = INSTR_CREATE_vnclip_wx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnclip_wx, instr);
}

static void
test_IVV(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vadd_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vadd_vv, instr);
    instr = INSTR_CREATE_vsub_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsub_vv, instr);
    instr = INSTR_CREATE_vminu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vminu_vv, instr);
    instr = INSTR_CREATE_vmin_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmin_vv, instr);
    instr = INSTR_CREATE_vmaxu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmaxu_vv, instr);
    instr = INSTR_CREATE_vmax_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmax_vv, instr);
    instr = INSTR_CREATE_vand_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vand_vv, instr);
    instr = INSTR_CREATE_vor_vv(dc, opnd_create_reg(DR_REG_VR0),
                                opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vor_vv, instr);
    instr = INSTR_CREATE_vxor_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vxor_vv, instr);
    instr = INSTR_CREATE_vrgather_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vrgather_vv, instr);
    instr = INSTR_CREATE_vrgatherei16_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vrgatherei16_vv, instr);

    instr =
        INSTR_CREATE_vadc_vvm(dc, opnd_create_reg(DR_REG_VR0),
                              opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vadc_vvm, instr);
    instr =
        INSTR_CREATE_vmadc_vvm(dc, opnd_create_reg(DR_REG_VR0),
                               opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmadc_vvm, instr);
    instr =
        INSTR_CREATE_vmadc_vv(dc, opnd_create_reg(DR_REG_VR0),
                              opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmadc_vv, instr);
    instr =
        INSTR_CREATE_vsbc_vvm(dc, opnd_create_reg(DR_REG_VR0),
                              opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vsbc_vvm, instr);
    instr =
        INSTR_CREATE_vmsbc_vvm(dc, opnd_create_reg(DR_REG_VR0),
                               opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmsbc_vvm, instr);
    instr =
        INSTR_CREATE_vmsbc_vv(dc, opnd_create_reg(DR_REG_VR0),
                              opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmsbc_vv, instr);
    instr =
        INSTR_CREATE_vmerge_vvm(dc, opnd_create_reg(DR_REG_VR0),
                                opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmerge_vvm, instr);
    instr = INSTR_CREATE_vmv_v_v(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vmv_v_v, instr);
    instr = INSTR_CREATE_vmseq_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmseq_vv, instr);
    instr = INSTR_CREATE_vmsne_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsne_vv, instr);
    instr = INSTR_CREATE_vmsltu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsltu_vv, instr);
    instr = INSTR_CREATE_vmslt_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmslt_vv, instr);
    instr = INSTR_CREATE_vmsleu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsleu_vv, instr);
    instr = INSTR_CREATE_vmsle_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsle_vv, instr);

    instr = INSTR_CREATE_vsaddu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsaddu_vv, instr);
    instr = INSTR_CREATE_vsadd_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsadd_vv, instr);
    instr = INSTR_CREATE_vssubu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssubu_vv, instr);
    instr = INSTR_CREATE_vssub_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssub_vv, instr);
    instr = INSTR_CREATE_vsll_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsll_vv, instr);
    instr = INSTR_CREATE_vsmul_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsmul_vv, instr);
    instr = INSTR_CREATE_vsrl_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsrl_vv, instr);
    instr = INSTR_CREATE_vsra_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsra_vv, instr);
    instr = INSTR_CREATE_vssrl_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssrl_vv, instr);
    instr = INSTR_CREATE_vssra_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssra_vv, instr);
    instr = INSTR_CREATE_vnsrl_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnsrl_wv, instr);
    instr = INSTR_CREATE_vnsra_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnsra_wv, instr);
    instr = INSTR_CREATE_vnclipu_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnclipu_wv, instr);
    instr = INSTR_CREATE_vnclip_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnclip_wv, instr);

    instr = INSTR_CREATE_vwredsumu_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwredsumu_vs, instr);
    instr = INSTR_CREATE_vwredsum_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwredsum_vs, instr);
}

static void
test_IVI(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vadd_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vadd_vi, instr);
    instr = INSTR_CREATE_vrsub_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vrsub_vi, instr);
    instr = INSTR_CREATE_vand_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vand_vi, instr);
    instr = INSTR_CREATE_vor_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vor_vi, instr);
    instr = INSTR_CREATE_vxor_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vxor_vi, instr);
    instr = INSTR_CREATE_vrgather_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vrgather_vi, instr);
    instr = INSTR_CREATE_vslideup_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vslideup_vi, instr);
    instr = INSTR_CREATE_vslidedown_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vslidedown_vi, instr);

    instr = INSTR_CREATE_vadc_vim(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_immed_int(0b10100, OPSZ_5b),
                                  opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vadc_vim, instr);
    instr = INSTR_CREATE_vmadc_vim(dc, opnd_create_reg(DR_REG_VR0),
                                   opnd_create_immed_int(0b10100, OPSZ_5b),
                                   opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vmadc_vim, instr);
    instr = INSTR_CREATE_vmadc_vi(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_immed_int(0b10100, OPSZ_5b),
                                  opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vmadc_vi, instr);
    instr = INSTR_CREATE_vmerge_vim(dc, opnd_create_reg(DR_REG_VR0),
                                    opnd_create_immed_int(0b10100, OPSZ_5b),
                                    opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vmerge_vim, instr);
    instr = INSTR_CREATE_vmv_v_i(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_immed_int(0b10100, OPSZ_5b));
    test_instr_encoding(dc, OP_vmv_v_i, instr);
    instr = INSTR_CREATE_vmseq_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmseq_vi, instr);
    instr = INSTR_CREATE_vmsne_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsne_vi, instr);
    instr = INSTR_CREATE_vmsleu_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsleu_vi, instr);
    instr = INSTR_CREATE_vmsle_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsle_vi, instr);
    instr = INSTR_CREATE_vmsgtu_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsgtu_vi, instr);
    instr = INSTR_CREATE_vmsgt_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsgt_vi, instr);

    instr = INSTR_CREATE_vsaddu_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsaddu_vi, instr);
    instr = INSTR_CREATE_vsadd_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsadd_vi, instr);
    instr = INSTR_CREATE_vsll_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsll_vi, instr);
    instr = INSTR_CREATE_vmv1r_v(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vmv1r_v, instr);
    instr = INSTR_CREATE_vmv2r_v(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vmv2r_v, instr);
    instr = INSTR_CREATE_vmv4r_v(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vmv4r_v, instr);
    instr = INSTR_CREATE_vmv8r_v(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vmv8r_v, instr);
    instr = INSTR_CREATE_vsaddu_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsaddu_vi, instr);
    instr = INSTR_CREATE_vsrl_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsrl_vi, instr);
    instr = INSTR_CREATE_vsra_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsra_vi, instr);
    instr = INSTR_CREATE_vssrl_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssrl_vi, instr);
    instr = INSTR_CREATE_vssra_vi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vssra_vi, instr);
    instr = INSTR_CREATE_vnsrl_wi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnsrl_wi, instr);
    instr = INSTR_CREATE_vnsra_wi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnsra_wi, instr);
    instr = INSTR_CREATE_vnclipu_wi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnclipu_wi, instr);
    instr = INSTR_CREATE_vnclip_wi(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b),
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnclip_wi, instr);
}

static void
test_MVV(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vredsum_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vredsum_vs, instr);
    instr = INSTR_CREATE_vredand_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vredand_vs, instr);
    instr = INSTR_CREATE_vredor_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vredor_vs, instr);
    instr = INSTR_CREATE_vredxor_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vredxor_vs, instr);
    instr = INSTR_CREATE_vredminu_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vredminu_vs, instr);
    instr = INSTR_CREATE_vredmin_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vredmin_vs, instr);
    instr = INSTR_CREATE_vredmaxu_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vredmaxu_vs, instr);
    instr = INSTR_CREATE_vredmax_vs(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vredmax_vs, instr);
    instr = INSTR_CREATE_vaaddu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vaaddu_vv, instr);
    instr = INSTR_CREATE_vaadd_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vaadd_vv, instr);
    instr = INSTR_CREATE_vasubu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vasubu_vv, instr);
    instr = INSTR_CREATE_vasub_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vasub_vv, instr);

    instr =
        INSTR_CREATE_vmv_x_s(dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR1));
    test_instr_encoding(dc, OP_vmv_x_s, instr);
}

static void
test_MVX(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vaaddu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vaaddu_vx, instr);
    instr = INSTR_CREATE_vaadd_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vaadd_vx, instr);
    instr = INSTR_CREATE_vasubu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vasubu_vx, instr);
    instr = INSTR_CREATE_vasub_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vasub_vx, instr);

    instr =
        INSTR_CREATE_vmv_s_x(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_vmv_s_x, instr);
    instr = INSTR_CREATE_vslide1up_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vslide1up_vx, instr);
    instr = INSTR_CREATE_vslide1down_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vslide1down_vx, instr);

    instr = INSTR_CREATE_vdivu_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vdivu_vx, instr);
    instr = INSTR_CREATE_vdiv_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vdiv_vx, instr);
    instr = INSTR_CREATE_vremu_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vremu_vx, instr);
    instr = INSTR_CREATE_vrem_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vrem_vx, instr);
    instr = INSTR_CREATE_vmulhu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmulhu_vx, instr);
    instr = INSTR_CREATE_vmul_vx(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmul_vx, instr);
    instr = INSTR_CREATE_vmulhsu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmulhsu_vx, instr);
    instr = INSTR_CREATE_vmulh_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmulh_vx, instr);
    instr = INSTR_CREATE_vmadd_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmadd_vx, instr);
    instr = INSTR_CREATE_vnmsub_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnmsub_vx, instr);
    instr = INSTR_CREATE_vmacc_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmacc_vx, instr);
    instr = INSTR_CREATE_vnmsac_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnmsac_vx, instr);

    instr = INSTR_CREATE_vwaddu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwaddu_vx, instr);
    instr = INSTR_CREATE_vwadd_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwadd_vx, instr);
    instr = INSTR_CREATE_vwsubu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwsubu_vx, instr);
    instr = INSTR_CREATE_vwsub_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwsub_vx, instr);
    instr = INSTR_CREATE_vwaddu_wx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwaddu_wx, instr);
    instr = INSTR_CREATE_vwadd_wx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwadd_wx, instr);
    instr = INSTR_CREATE_vwsubu_wx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwsubu_wx, instr);
    instr = INSTR_CREATE_vwsub_wx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwsub_wx, instr);
    instr = INSTR_CREATE_vwmulu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmulu_vx, instr);
    instr = INSTR_CREATE_vwmulsu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmulsu_vx, instr);
    instr = INSTR_CREATE_vwmul_vx(dc, opnd_create_reg(DR_REG_VR0),
                                  opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2),
                                  opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmul_vx, instr);
    instr = INSTR_CREATE_vwmaccu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmaccu_vx, instr);
    instr = INSTR_CREATE_vwmacc_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmacc_vx, instr);
    instr = INSTR_CREATE_vwmaccus_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmaccus_vx, instr);
    instr = INSTR_CREATE_vwmaccsu_vx(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmaccsu_vx, instr);
}

static void
test_int_extension(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_vzext_vf8(dc, opnd_create_reg(DR_REG_VR0),
                                   opnd_create_reg(DR_REG_VR1),
                                   opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vzext_vf8, instr);
    instr = INSTR_CREATE_vsext_vf8(dc, opnd_create_reg(DR_REG_VR0),
                                   opnd_create_reg(DR_REG_VR1),
                                   opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsext_vf8, instr);
    instr = INSTR_CREATE_vzext_vf4(dc, opnd_create_reg(DR_REG_VR0),
                                   opnd_create_reg(DR_REG_VR1),
                                   opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vzext_vf4, instr);
    instr = INSTR_CREATE_vsext_vf4(dc, opnd_create_reg(DR_REG_VR0),
                                   opnd_create_reg(DR_REG_VR1),
                                   opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsext_vf4, instr);
    instr = INSTR_CREATE_vzext_vf2(dc, opnd_create_reg(DR_REG_VR0),
                                   opnd_create_reg(DR_REG_VR1),
                                   opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vzext_vf2, instr);
    instr = INSTR_CREATE_vsext_vf2(dc, opnd_create_reg(DR_REG_VR0),
                                   opnd_create_reg(DR_REG_VR1),
                                   opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vsext_vf2, instr);

    instr = INSTR_CREATE_vcompress_vm(dc, opnd_create_reg(DR_REG_VR0),
                                      opnd_create_reg(DR_REG_VR1),
                                      opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vcompress_vm, instr);
    instr =
        INSTR_CREATE_vmandn_mm(dc, opnd_create_reg(DR_REG_VR0),
                               opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmandn_mm, instr);
    instr =
        INSTR_CREATE_vmand_mm(dc, opnd_create_reg(DR_REG_VR0),
                              opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmand_mm, instr);
    instr =
        INSTR_CREATE_vmor_mm(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
                             opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmor_mm, instr);
    instr =
        INSTR_CREATE_vmxor_mm(dc, opnd_create_reg(DR_REG_VR0),
                              opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmxor_mm, instr);
    instr =
        INSTR_CREATE_vmorn_mm(dc, opnd_create_reg(DR_REG_VR0),
                              opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmorn_mm, instr);
    instr =
        INSTR_CREATE_vmnand_mm(dc, opnd_create_reg(DR_REG_VR0),
                               opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmnand_mm, instr);
    instr =
        INSTR_CREATE_vmnor_mm(dc, opnd_create_reg(DR_REG_VR0),
                              opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmnor_mm, instr);
    instr =
        INSTR_CREATE_vmxnor_mm(dc, opnd_create_reg(DR_REG_VR0),
                               opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2));
    test_instr_encoding(dc, OP_vmxnor_mm, instr);

    instr =
        INSTR_CREATE_vmsbf_m(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
                             opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsbf_m, instr);
    instr =
        INSTR_CREATE_vmsof_m(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
                             opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsof_m, instr);
    instr =
        INSTR_CREATE_vmsif_m(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
                             opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmsif_m, instr);
    instr =
        INSTR_CREATE_viota_m(dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
                             opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_viota_m, instr);
    instr = INSTR_CREATE_vid_v(dc, opnd_create_reg(DR_REG_VR0),
                               opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vid_v, instr);
    instr =
        INSTR_CREATE_vcpop_m(dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR1),
                             opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vcpop_m, instr);
    instr =
        INSTR_CREATE_vfirst_m(dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR1),
                              opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vfirst_m, instr);

    instr = INSTR_CREATE_vdivu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vdivu_vv, instr);
    instr = INSTR_CREATE_vdiv_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vdiv_vv, instr);
    instr = INSTR_CREATE_vremu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vremu_vv, instr);
    instr = INSTR_CREATE_vrem_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vrem_vv, instr);
    instr = INSTR_CREATE_vmulhu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmulhu_vv, instr);
    instr = INSTR_CREATE_vmul_vv(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2),
                                 opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmul_vv, instr);
    instr = INSTR_CREATE_vmulhsu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmulhsu_vv, instr);
    instr = INSTR_CREATE_vmulh_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmulh_vv, instr);
    instr = INSTR_CREATE_vmadd_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmadd_vv, instr);
    instr = INSTR_CREATE_vnmsub_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnmsub_vv, instr);
    instr = INSTR_CREATE_vmacc_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vmacc_vv, instr);
    instr = INSTR_CREATE_vnmsac_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnmsac_vv, instr);

    instr = INSTR_CREATE_vnmsac_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vnmsac_vv, instr);
    instr = INSTR_CREATE_vwadd_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwadd_vv, instr);
    instr = INSTR_CREATE_vwsubu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwsubu_vv, instr);
    instr = INSTR_CREATE_vwsub_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwsub_vv, instr);
    instr = INSTR_CREATE_vwaddu_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwaddu_wv, instr);
    instr = INSTR_CREATE_vwadd_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwadd_wv, instr);
    instr = INSTR_CREATE_vwsubu_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwsubu_wv, instr);
    instr = INSTR_CREATE_vwsub_wv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwsub_wv, instr);
    instr = INSTR_CREATE_vwmulu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmulu_vv, instr);
    instr = INSTR_CREATE_vwmulsu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmulsu_vv, instr);
    instr = INSTR_CREATE_vwmul_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmul_vv, instr);
    instr = INSTR_CREATE_vwmaccu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmaccu_vv, instr);
    instr = INSTR_CREATE_vwmacc_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmacc_vv, instr);
    instr = INSTR_CREATE_vwmaccsu_vv(
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vwmaccsu_vv, instr);
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

    test_configuration_setting(dcontext);
    print("test_configuration_setting complete\n");

    test_load_store(dcontext);
    print("test_load_store complete\n");

    test_FVF(dcontext);
    print("test_FVF complete\n");

    test_FVV(dcontext);
    print("test_FVV complete\n");

    test_IVX(dcontext);
    print("test_IVX complete\n");

    test_IVV(dcontext);
    print("test_IVV complete\n");

    test_IVI(dcontext);
    print("test_IVI complete\n");

    test_MVV(dcontext);
    print("test_MVV complete\n");

    test_MVX(dcontext);
    print("test_MVX complete\n");

    test_int_extension(dcontext);
    print("test_int_extension complete\n");

    print("All tests complete\n");
    return 0;
}
