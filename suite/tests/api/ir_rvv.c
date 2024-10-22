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

#define ISLOAD true
#define ISSTORE false

#define TEST_MEM_WHOLEREG(opcode, mem_type)                         \
    do {                                                            \
        if (mem_type == ISLOAD) {                                   \
            instr = INSTR_CREATE_##opcode(                          \
                dc, opnd_create_reg(DR_REG_VR0),                    \
                opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, \
                                      reg_get_size(DR_REG_VR0)));   \
        } else {                                                    \
            instr = INSTR_CREATE_##opcode(                          \
                dc,                                                 \
                opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, \
                                      reg_get_size(DR_REG_VR0)),    \
                opnd_create_reg(DR_REG_VR0));                       \
        }                                                           \
        test_instr_encoding(dc, OP_##opcode, instr);                \
    } while (0);

#define TEST_MEM_UNIT_STRIDE(opcode, mem_type)                                    \
    do {                                                                          \
        if (mem_type == ISLOAD) {                                                 \
            instr = INSTR_CREATE_##opcode(                                        \
                dc, opnd_create_reg(DR_REG_VR0),                                  \
                opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0,               \
                                      reg_get_size(DR_REG_VR0)),                  \
                opnd_create_immed_int(0b1, OPSZ_1b),                              \
                opnd_create_immed_int(0b000, OPSZ_3b));                           \
        } else {                                                                  \
            instr = INSTR_CREATE_##opcode(                                        \
                dc,                                                               \
                opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0,               \
                                      reg_get_size(DR_REG_VR0)),                  \
                opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b1, OPSZ_1b), \
                opnd_create_immed_int(0b000, OPSZ_3b));                           \
        }                                                                         \
        test_instr_encoding(dc, OP_##opcode, instr);                              \
    } while (0);

#define TEST_MEM_INDEX(opcode, mem_type)                                          \
    do {                                                                          \
        if (mem_type == ISLOAD) {                                                 \
            instr = INSTR_CREATE_##opcode(                                        \
                dc, opnd_create_reg(DR_REG_VR0),                                  \
                opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0,               \
                                      reg_get_size(DR_REG_VR0)),                  \
                opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b), \
                opnd_create_immed_int(0b000, OPSZ_3b));                           \
        } else {                                                                  \
            instr = INSTR_CREATE_##opcode(                                        \
                dc,                                                               \
                opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0,               \
                                      reg_get_size(DR_REG_VR0)),                  \
                opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),         \
                opnd_create_immed_int(0b1, OPSZ_1b),                              \
                opnd_create_immed_int(0b000, OPSZ_3b));                           \
        }                                                                         \
        test_instr_encoding(dc, OP_##opcode, instr);                              \
    } while (0);

#define TEST_MEM_STRIDE(opcode, mem_type)                                        \
    do {                                                                         \
        if (mem_type == ISLOAD) {                                                \
            instr = INSTR_CREATE_##opcode(                                       \
                dc, opnd_create_reg(DR_REG_VR0),                                 \
                opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0,              \
                                      reg_get_size(DR_REG_VR0)),                 \
                opnd_create_reg(DR_REG_A2), opnd_create_immed_int(0b1, OPSZ_1b), \
                opnd_create_immed_int(0b000, OPSZ_3b));                          \
        } else {                                                                 \
            instr = INSTR_CREATE_##opcode(                                       \
                dc,                                                              \
                opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0,              \
                                      reg_get_size(DR_REG_VR0)),                 \
                opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A2),         \
                opnd_create_immed_int(0b1, OPSZ_1b),                             \
                opnd_create_immed_int(0b000, OPSZ_3b));                          \
        }                                                                        \
        test_instr_encoding(dc, OP_##opcode, instr);                             \
    } while (0);

#define TEST_Vd_Rs1_Vs2_vm(opcode)                                         \
    instr = INSTR_CREATE_##opcode(                                         \
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_A1),       \
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b)); \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Vd_Rs1_Vs2(opcode)                                                         \
    instr =                                                                             \
        INSTR_CREATE_##opcode(dc, opnd_create_reg(DR_REG_VR0),                          \
                              opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_VR2)); \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Vd_Rs1(opcode)                                        \
    instr = INSTR_CREATE_##opcode(dc, opnd_create_reg(DR_REG_VR0), \
                                  opnd_create_reg(DR_REG_A1));     \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Vd_Vs1_Vs2_vm(opcode)                                         \
    instr = INSTR_CREATE_##opcode(                                         \
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_reg(DR_REG_VR1),      \
        opnd_create_reg(DR_REG_VR2), opnd_create_immed_int(0b1, OPSZ_1b)); \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Vd_Vs1_Vs2(opcode)                                                          \
    instr =                                                                              \
        INSTR_CREATE_##opcode(dc, opnd_create_reg(DR_REG_VR0),                           \
                              opnd_create_reg(DR_REG_VR1), opnd_create_reg(DR_REG_VR2)); \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Vd_IMM_Vs2_vm(opcode)                                                \
    instr = INSTR_CREATE_##opcode(                                                \
        dc, opnd_create_reg(DR_REG_VR0), opnd_create_immed_int(0b10100, OPSZ_5b), \
        opnd_create_reg(DR_REG_VR1), opnd_create_immed_int(0b1, OPSZ_1b));        \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Vd_IMM_Vs2(opcode)                                            \
    instr = INSTR_CREATE_##opcode(dc, opnd_create_reg(DR_REG_VR0),         \
                                  opnd_create_immed_int(0b10100, OPSZ_5b), \
                                  opnd_create_reg(DR_REG_VR1));            \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Vd_Vs1_vm(opcode)                                          \
    instr = INSTR_CREATE_##opcode(dc, opnd_create_reg(DR_REG_VR0),      \
                                  opnd_create_reg(DR_REG_VR1),          \
                                  opnd_create_immed_int(0b1, OPSZ_1b)); \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Vd_Vs1(opcode)                                        \
    instr = INSTR_CREATE_##opcode(dc, opnd_create_reg(DR_REG_VR0), \
                                  opnd_create_reg(DR_REG_VR1));    \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Rd_Vs1(opcode)                                       \
    instr = INSTR_CREATE_##opcode(dc, opnd_create_reg(DR_REG_A1), \
                                  opnd_create_reg(DR_REG_VR0));   \
    test_instr_encoding(dc, OP_##opcode, instr);

#define TEST_Rd_Vs1_vm(opcode)                                          \
    instr = INSTR_CREATE_##opcode(dc, opnd_create_reg(DR_REG_A1),       \
                                  opnd_create_reg(DR_REG_VR0),          \
                                  opnd_create_immed_int(0b1, OPSZ_1b)); \
    test_instr_encoding(dc, OP_##opcode, instr);

instr_t *instr;

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
test_configuration_setting(void *dc)
{
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
    TEST_MEM_WHOLEREG(vlm_v, ISLOAD);
    TEST_MEM_WHOLEREG(vsm_v, ISSTORE);

    TEST_MEM_UNIT_STRIDE(vle8_v, ISLOAD);
    TEST_MEM_UNIT_STRIDE(vle16_v, ISLOAD);
    TEST_MEM_UNIT_STRIDE(vle32_v, ISLOAD);
    TEST_MEM_UNIT_STRIDE(vle64_v, ISLOAD);

    TEST_MEM_UNIT_STRIDE(vse8_v, ISSTORE);
    TEST_MEM_UNIT_STRIDE(vse16_v, ISSTORE);
    TEST_MEM_UNIT_STRIDE(vse32_v, ISSTORE);
    TEST_MEM_UNIT_STRIDE(vse64_v, ISSTORE);
}

static void
test_indexed_unordered(void *dc)
{
    TEST_MEM_INDEX(vluxei8_v, ISLOAD);
    TEST_MEM_INDEX(vluxei16_v, ISLOAD);
    TEST_MEM_INDEX(vluxei32_v, ISLOAD);
    TEST_MEM_INDEX(vluxei64_v, ISLOAD);

    TEST_MEM_INDEX(vsuxei8_v, ISSTORE);
    TEST_MEM_INDEX(vsuxei16_v, ISSTORE);
    TEST_MEM_INDEX(vsuxei32_v, ISSTORE);
    TEST_MEM_INDEX(vsuxei64_v, ISSTORE);
}

static void
test_stride(void *dc)
{
    TEST_MEM_STRIDE(vlse8_v, ISLOAD);
    TEST_MEM_STRIDE(vlse16_v, ISLOAD);
    TEST_MEM_STRIDE(vlse32_v, ISLOAD);
    TEST_MEM_STRIDE(vlse64_v, ISLOAD);

    TEST_MEM_STRIDE(vsse8_v, ISSTORE);
    TEST_MEM_STRIDE(vsse16_v, ISSTORE);
    TEST_MEM_STRIDE(vsse32_v, ISSTORE);
    TEST_MEM_STRIDE(vsse64_v, ISSTORE);
}

static void
test_indexed_ordered(void *dc)
{
    TEST_MEM_INDEX(vloxei8_v, ISLOAD);
    TEST_MEM_INDEX(vloxei16_v, ISLOAD);
    TEST_MEM_INDEX(vloxei32_v, ISLOAD);
    TEST_MEM_INDEX(vloxei64_v, ISLOAD);

    TEST_MEM_INDEX(vsoxei8_v, ISSTORE);
    TEST_MEM_INDEX(vsoxei16_v, ISSTORE);
    TEST_MEM_INDEX(vsoxei32_v, ISSTORE);
    TEST_MEM_INDEX(vsoxei64_v, ISSTORE);
}

static void
test_unit_stride_faultfirst(void *dc)
{
    TEST_MEM_UNIT_STRIDE(vle8ff_v, ISLOAD);
    TEST_MEM_UNIT_STRIDE(vle16ff_v, ISLOAD);
    TEST_MEM_UNIT_STRIDE(vle32ff_v, ISLOAD);
    TEST_MEM_UNIT_STRIDE(vle64ff_v, ISLOAD);
}

static void
test_whole_register(void *dc)
{
    TEST_MEM_WHOLEREG(vl1re8_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl1re16_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl1re32_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl1re64_v, ISLOAD);

    TEST_MEM_WHOLEREG(vl2re8_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl2re16_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl2re32_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl2re64_v, ISLOAD);

    TEST_MEM_WHOLEREG(vl4re8_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl4re16_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl4re32_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl4re64_v, ISLOAD);

    TEST_MEM_WHOLEREG(vl8re8_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl8re16_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl8re32_v, ISLOAD);
    TEST_MEM_WHOLEREG(vl8re64_v, ISLOAD);

    TEST_MEM_WHOLEREG(vs1r_v, ISSTORE);
    TEST_MEM_WHOLEREG(vs2r_v, ISSTORE);
    TEST_MEM_WHOLEREG(vs4r_v, ISSTORE);
    TEST_MEM_WHOLEREG(vs8r_v, ISSTORE);
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
    TEST_Vd_Rs1_Vs2_vm(vfadd_vf);
    TEST_Vd_Rs1_Vs2_vm(vfsub_vf);
    TEST_Vd_Rs1_Vs2_vm(vfmin_vf);
    TEST_Vd_Rs1_Vs2_vm(vfmax_vf);
    TEST_Vd_Rs1_Vs2_vm(vfsgnj_vf);
    TEST_Vd_Rs1_Vs2_vm(vfsgnjn_vf);
    TEST_Vd_Rs1_Vs2_vm(vfsgnjx_vf);
    TEST_Vd_Rs1_Vs2_vm(vfslide1up_vf);
    TEST_Vd_Rs1_Vs2_vm(vfslide1down_vf);

    TEST_Vd_Rs1(vfmv_s_f);
    TEST_Vd_Rs1(vfmv_v_f);

    TEST_Vd_Rs1_Vs2(vfmerge_vfm);
    TEST_Vd_Rs1_Vs2_vm(vmfeq_vf);
    TEST_Vd_Rs1_Vs2_vm(vmfle_vf);
    TEST_Vd_Rs1_Vs2_vm(vmflt_vf);
    TEST_Vd_Rs1_Vs2_vm(vmfne_vf);
    TEST_Vd_Rs1_Vs2_vm(vmfgt_vf);
    TEST_Vd_Rs1_Vs2_vm(vmfge_vf);

    TEST_Vd_Rs1_Vs2_vm(vfrdiv_vf);
    TEST_Vd_Rs1_Vs2_vm(vfmul_vf);
    TEST_Vd_Rs1_Vs2_vm(vfrsub_vf);
    TEST_Vd_Rs1_Vs2_vm(vfmadd_vf);
    TEST_Vd_Rs1_Vs2_vm(vfnmadd_vf);
    TEST_Vd_Rs1_Vs2_vm(vfmsub_vf);
    TEST_Vd_Rs1_Vs2_vm(vfnmsub_vf);
    TEST_Vd_Rs1_Vs2_vm(vfmacc_vf);
    TEST_Vd_Rs1_Vs2_vm(vfnmacc_vf);
    TEST_Vd_Rs1_Vs2_vm(vfmsac_vf);
    TEST_Vd_Rs1_Vs2_vm(vfnmsac_vf);
    TEST_Vd_Rs1_Vs2_vm(vfwadd_vf);
    TEST_Vd_Rs1_Vs2_vm(vfwsub_vf);
    TEST_Vd_Rs1_Vs2_vm(vfwadd_wf);
    TEST_Vd_Rs1_Vs2_vm(vfwsub_wf);
    TEST_Vd_Rs1_Vs2_vm(vfwmul_vf);
    TEST_Vd_Rs1_Vs2_vm(vfwmacc_vf);
    TEST_Vd_Rs1_Vs2_vm(vfwnmacc_vf);
    TEST_Vd_Rs1_Vs2_vm(vfwmsac_vf);
    TEST_Vd_Rs1_Vs2_vm(vfwnmsac_vf);
}

static void
test_FVV(void *dc)
{
    TEST_Vd_Vs1_Vs2_vm(vfadd_vv);
    TEST_Vd_Vs1_Vs2_vm(vfredusum_vs);
    TEST_Vd_Vs1_Vs2_vm(vfsub_vv);
    TEST_Vd_Vs1_Vs2_vm(vfredosum_vs);
    TEST_Vd_Vs1_Vs2_vm(vfmin_vv);
    TEST_Vd_Vs1_Vs2_vm(vfredmin_vs);
    TEST_Vd_Vs1_Vs2_vm(vfmax_vv);
    TEST_Vd_Vs1_Vs2_vm(vfredmax_vs);
    TEST_Vd_Vs1_Vs2_vm(vfsgnj_vv);
    TEST_Vd_Vs1_Vs2_vm(vfsgnjn_vv);
    TEST_Vd_Vs1_Vs2_vm(vfsgnjx_vv);
    TEST_Rd_Vs1(vfmv_f_s);

    TEST_Vd_Vs1_Vs2_vm(vmfeq_vv);
    TEST_Vd_Vs1_Vs2_vm(vmfle_vv);
    TEST_Vd_Vs1_Vs2_vm(vmflt_vv);
    TEST_Vd_Vs1_Vs2_vm(vmfne_vv);
    TEST_Vd_Vs1_Vs2_vm(vfdiv_vv);
    TEST_Vd_Vs1_Vs2_vm(vfmul_vv);
    TEST_Vd_Vs1_Vs2_vm(vfmadd_vv);
    TEST_Vd_Vs1_Vs2_vm(vfnmadd_vv);
    TEST_Vd_Vs1_Vs2_vm(vfmsub_vv);
    TEST_Vd_Vs1_Vs2_vm(vfnmsub_vv);
    TEST_Vd_Vs1_Vs2_vm(vfmacc_vv);
    TEST_Vd_Vs1_Vs2_vm(vfnmacc_vv);
    TEST_Vd_Vs1_Vs2_vm(vfmsac_vv);
    TEST_Vd_Vs1_Vs2_vm(vfnmsac_vv);

    TEST_Vd_Vs1_vm(vfcvt_xu_f_v);
    TEST_Vd_Vs1_vm(vfcvt_x_f_v);
    TEST_Vd_Vs1_vm(vfcvt_f_xu_v);
    TEST_Vd_Vs1_vm(vfcvt_f_x_v);
    TEST_Vd_Vs1_vm(vfcvt_rtz_xu_f_v);
    TEST_Vd_Vs1_vm(vfcvt_rtz_x_f_v);
    TEST_Vd_Vs1_vm(vfwcvt_x_f_v);
    TEST_Vd_Vs1_vm(vfwcvt_f_xu_v);
    TEST_Vd_Vs1_vm(vfwcvt_f_x_v);
    TEST_Vd_Vs1_vm(vfwcvt_f_f_v);
    TEST_Vd_Vs1_vm(vfwcvt_rtz_xu_f_v);
    TEST_Vd_Vs1_vm(vfwcvt_rtz_x_f_v);

    TEST_Vd_Vs1_vm(vfncvt_xu_f_w);
    TEST_Vd_Vs1_vm(vfncvt_x_f_w);
    TEST_Vd_Vs1_vm(vfncvt_f_xu_w);
    TEST_Vd_Vs1_vm(vfncvt_f_x_w);
    TEST_Vd_Vs1_vm(vfncvt_f_f_w);
    TEST_Vd_Vs1_vm(vfncvt_rod_f_f_w);
    TEST_Vd_Vs1_vm(vfncvt_rtz_xu_f_w);
    TEST_Vd_Vs1_vm(vfncvt_rtz_x_f_w);

    TEST_Vd_Vs1_vm(vfsqrt_v);
    TEST_Vd_Vs1_vm(vfrsqrt7_v);
    TEST_Vd_Vs1_vm(vfrec7_v);
    TEST_Vd_Vs1_vm(vfclass_v);

    TEST_Vd_Vs1_Vs2_vm(vfwadd_vv);
    TEST_Vd_Vs1_Vs2_vm(vfwredusum_vs);
    TEST_Vd_Vs1_Vs2_vm(vfwsub_vv);
    TEST_Vd_Vs1_Vs2_vm(vfwredosum_vs);
    TEST_Vd_Vs1_Vs2_vm(vfwadd_wv);
    TEST_Vd_Vs1_Vs2_vm(vfwsub_wv);
    TEST_Vd_Vs1_Vs2_vm(vfwmul_vv);
    TEST_Vd_Vs1_Vs2_vm(vfwmacc_vv);
    TEST_Vd_Vs1_Vs2_vm(vfwnmacc_vv);
    TEST_Vd_Vs1_Vs2_vm(vfnmacc_vv);
    TEST_Vd_Vs1_Vs2_vm(vfmsac_vv);
    TEST_Vd_Vs1_Vs2_vm(vfwnmsac_vv);
}

static void
test_IVX(void *dc)
{
    TEST_Vd_Rs1_Vs2_vm(vadd_vx);
    TEST_Vd_Rs1_Vs2_vm(vsub_vx);
    TEST_Vd_Rs1_Vs2_vm(vrsub_vx);
    TEST_Vd_Rs1_Vs2_vm(vminu_vx);
    TEST_Vd_Rs1_Vs2_vm(vmin_vx);
    TEST_Vd_Rs1_Vs2_vm(vmaxu_vx);
    TEST_Vd_Rs1_Vs2_vm(vmax_vx);
    TEST_Vd_Rs1_Vs2_vm(vand_vx);
    TEST_Vd_Rs1_Vs2_vm(vor_vx);
    TEST_Vd_Rs1_Vs2_vm(vxor_vx);
    TEST_Vd_Rs1_Vs2_vm(vrgather_vx);
    TEST_Vd_Rs1_Vs2_vm(vslideup_vx);
    TEST_Vd_Rs1_Vs2_vm(vslidedown_vx);

    TEST_Vd_Rs1_Vs2(vadc_vxm);
    TEST_Vd_Rs1_Vs2(vmadc_vxm);
    TEST_Vd_Rs1_Vs2(vmadc_vx);
    TEST_Vd_Rs1_Vs2(vsbc_vxm);
    TEST_Vd_Rs1_Vs2(vmsbc_vxm);
    TEST_Vd_Rs1_Vs2(vmsbc_vx);
    TEST_Vd_Rs1_Vs2(vmerge_vxm);
    TEST_Vd_Rs1(vmv_v_x);

    TEST_Vd_Rs1_Vs2_vm(vmseq_vx);
    TEST_Vd_Rs1_Vs2_vm(vmsne_vx);
    TEST_Vd_Rs1_Vs2_vm(vmsltu_vx);
    TEST_Vd_Rs1_Vs2_vm(vmslt_vx);
    TEST_Vd_Rs1_Vs2_vm(vmsleu_vx);
    TEST_Vd_Rs1_Vs2_vm(vmsle_vx);
    TEST_Vd_Rs1_Vs2_vm(vmsgtu_vx);
    TEST_Vd_Rs1_Vs2_vm(vmsgt_vx);
    TEST_Vd_Rs1_Vs2_vm(vsaddu_vx);
    TEST_Vd_Rs1_Vs2_vm(vsadd_vx);
    TEST_Vd_Rs1_Vs2_vm(vssubu_vx);
    TEST_Vd_Rs1_Vs2_vm(vssub_vx);
    TEST_Vd_Rs1_Vs2_vm(vsll_vx);
    TEST_Vd_Rs1_Vs2_vm(vsmul_vx);
    TEST_Vd_Rs1_Vs2_vm(vsrl_vx);
    TEST_Vd_Rs1_Vs2_vm(vsra_vx);
    TEST_Vd_Rs1_Vs2_vm(vssrl_vx);
    TEST_Vd_Rs1_Vs2_vm(vssra_vx);
    TEST_Vd_Rs1_Vs2_vm(vnsrl_wx);
    TEST_Vd_Rs1_Vs2_vm(vnsra_wx);
    TEST_Vd_Rs1_Vs2_vm(vnclipu_wx);
    TEST_Vd_Rs1_Vs2_vm(vnclip_wx);
}

static void
test_IVV(void *dc)
{
    TEST_Vd_Vs1_Vs2_vm(vadd_vv);
    TEST_Vd_Vs1_Vs2_vm(vsub_vv);
    TEST_Vd_Vs1_Vs2_vm(vminu_vv);
    TEST_Vd_Vs1_Vs2_vm(vmin_vv);
    TEST_Vd_Vs1_Vs2_vm(vmaxu_vv);
    TEST_Vd_Vs1_Vs2_vm(vmax_vv);
    TEST_Vd_Vs1_Vs2_vm(vand_vv);
    TEST_Vd_Vs1_Vs2_vm(vor_vv);
    TEST_Vd_Vs1_Vs2_vm(vxor_vv);
    TEST_Vd_Vs1_Vs2_vm(vrgather_vv);
    TEST_Vd_Vs1_Vs2_vm(vrgatherei16_vv);

    TEST_Vd_Vs1_Vs2(vadc_vvm);
    TEST_Vd_Vs1_Vs2(vmadc_vvm);
    TEST_Vd_Vs1_Vs2(vmadc_vv);
    TEST_Vd_Vs1_Vs2(vsbc_vvm);
    TEST_Vd_Vs1_Vs2(vmsbc_vvm);
    TEST_Vd_Vs1_Vs2(vmsbc_vv);
    TEST_Vd_Vs1_Vs2(vmerge_vvm);
    TEST_Vd_Vs1(vmv_v_v);
    TEST_Vd_Vs1_Vs2_vm(vmseq_vv);
    TEST_Vd_Vs1_Vs2_vm(vmsne_vv);
    TEST_Vd_Vs1_Vs2_vm(vmsltu_vv);
    TEST_Vd_Vs1_Vs2_vm(vmslt_vv);
    TEST_Vd_Vs1_Vs2_vm(vmsleu_vv);
    TEST_Vd_Vs1_Vs2_vm(vmsle_vv);

    TEST_Vd_Vs1_Vs2_vm(vsaddu_vv);
    TEST_Vd_Vs1_Vs2_vm(vsadd_vv);
    TEST_Vd_Vs1_Vs2_vm(vssubu_vv);
    TEST_Vd_Vs1_Vs2_vm(vssub_vv);
    TEST_Vd_Vs1_Vs2_vm(vsll_vv);
    TEST_Vd_Vs1_Vs2_vm(vsmul_vv);
    TEST_Vd_Vs1_Vs2_vm(vsrl_vv);
    TEST_Vd_Vs1_Vs2_vm(vsra_vv);
    TEST_Vd_Vs1_Vs2_vm(vssrl_vv);
    TEST_Vd_Vs1_Vs2_vm(vssra_vv);
    TEST_Vd_Vs1_Vs2_vm(vnsrl_wv);
    TEST_Vd_Vs1_Vs2_vm(vnsra_wv);
    TEST_Vd_Vs1_Vs2_vm(vnclipu_wv);
    TEST_Vd_Vs1_Vs2_vm(vnclip_wv);

    TEST_Vd_Vs1_Vs2_vm(vwredsumu_vs);
    TEST_Vd_Vs1_Vs2_vm(vwredsum_vs);
}

static void
test_IVI(void *dc)
{
    TEST_Vd_IMM_Vs2_vm(vadd_vi);
    TEST_Vd_IMM_Vs2_vm(vrsub_vi);
    TEST_Vd_IMM_Vs2_vm(vand_vi);
    TEST_Vd_IMM_Vs2_vm(vor_vi);
    TEST_Vd_IMM_Vs2_vm(vxor_vi);
    TEST_Vd_IMM_Vs2_vm(vrgather_vi);
    TEST_Vd_IMM_Vs2_vm(vslideup_vi);
    TEST_Vd_IMM_Vs2_vm(vslidedown_vi);

    TEST_Vd_IMM_Vs2(vadc_vim);
    TEST_Vd_IMM_Vs2(vmadc_vim);
    TEST_Vd_IMM_Vs2(vmadc_vi);
    TEST_Vd_IMM_Vs2(vmerge_vim);

    instr = INSTR_CREATE_vmv_v_i(dc, opnd_create_reg(DR_REG_VR0),
                                 opnd_create_immed_int(0b10100, OPSZ_5b));
    test_instr_encoding(dc, OP_vmv_v_i, instr);

    TEST_Vd_IMM_Vs2_vm(vmseq_vi);
    TEST_Vd_IMM_Vs2_vm(vmsne_vi);
    TEST_Vd_IMM_Vs2_vm(vmsleu_vi);
    TEST_Vd_IMM_Vs2_vm(vmsle_vi);
    TEST_Vd_IMM_Vs2_vm(vmsgtu_vi);
    TEST_Vd_IMM_Vs2_vm(vmsgt_vi);

    TEST_Vd_IMM_Vs2_vm(vsaddu_vi);
    TEST_Vd_IMM_Vs2_vm(vsadd_vi);
    TEST_Vd_IMM_Vs2_vm(vsll_vi);

    TEST_Vd_Vs1(vmv1r_v);
    TEST_Vd_Vs1(vmv2r_v);
    TEST_Vd_Vs1(vmv4r_v);
    TEST_Vd_Vs1(vmv8r_v);

    TEST_Vd_IMM_Vs2_vm(vsaddu_vi);
    TEST_Vd_IMM_Vs2_vm(vsrl_vi);
    TEST_Vd_IMM_Vs2_vm(vsra_vi);
    TEST_Vd_IMM_Vs2_vm(vssrl_vi);
    TEST_Vd_IMM_Vs2_vm(vssra_vi);
    TEST_Vd_IMM_Vs2_vm(vnsrl_wi);
    TEST_Vd_IMM_Vs2_vm(vnsra_wi);
    TEST_Vd_IMM_Vs2_vm(vnclipu_wi);
    TEST_Vd_IMM_Vs2_vm(vnclip_wi);
}

static void
test_MVV(void *dc)
{
    TEST_Vd_Vs1_Vs2_vm(vredsum_vs);
    TEST_Vd_Vs1_Vs2_vm(vredand_vs);
    TEST_Vd_Vs1_Vs2_vm(vredor_vs);
    TEST_Vd_Vs1_Vs2_vm(vredxor_vs);
    TEST_Vd_Vs1_Vs2_vm(vredminu_vs);
    TEST_Vd_Vs1_Vs2_vm(vredmin_vs);
    TEST_Vd_Vs1_Vs2_vm(vredmaxu_vs);
    TEST_Vd_Vs1_Vs2_vm(vredmax_vs);
    TEST_Vd_Vs1_Vs2_vm(vaaddu_vv);
    TEST_Vd_Vs1_Vs2_vm(vaadd_vv);
    TEST_Vd_Vs1_Vs2_vm(vasubu_vv);
    TEST_Vd_Vs1_Vs2_vm(vasub_vv);

    TEST_Rd_Vs1(vmv_x_s);
}

static void
test_MVX(void *dc)
{
    TEST_Vd_Rs1_Vs2_vm(vaaddu_vx);
    TEST_Vd_Rs1_Vs2_vm(vaadd_vx);
    TEST_Vd_Rs1_Vs2_vm(vasubu_vx);
    TEST_Vd_Rs1_Vs2_vm(vasub_vx);

    TEST_Vd_Rs1(vmv_s_x);

    TEST_Vd_Rs1_Vs2_vm(vslide1up_vx);
    TEST_Vd_Rs1_Vs2_vm(vslide1down_vx);

    TEST_Vd_Rs1_Vs2_vm(vdivu_vx);
    TEST_Vd_Rs1_Vs2_vm(vdiv_vx);
    TEST_Vd_Rs1_Vs2_vm(vremu_vx);
    TEST_Vd_Rs1_Vs2_vm(vrem_vx);
    TEST_Vd_Rs1_Vs2_vm(vmulhu_vx);
    TEST_Vd_Rs1_Vs2_vm(vmul_vx);
    TEST_Vd_Rs1_Vs2_vm(vmulhsu_vx);
    TEST_Vd_Rs1_Vs2_vm(vmulh_vx);
    TEST_Vd_Rs1_Vs2_vm(vmadd_vx);
    TEST_Vd_Rs1_Vs2_vm(vnmsub_vx);
    TEST_Vd_Rs1_Vs2_vm(vmacc_vx);
    TEST_Vd_Rs1_Vs2_vm(vnmsac_vx);

    TEST_Vd_Rs1_Vs2_vm(vwaddu_vx);
    TEST_Vd_Rs1_Vs2_vm(vwadd_vx);
    TEST_Vd_Rs1_Vs2_vm(vwsubu_vx);
    TEST_Vd_Rs1_Vs2_vm(vwsub_vx);
    TEST_Vd_Rs1_Vs2_vm(vwaddu_wx);
    TEST_Vd_Rs1_Vs2_vm(vwadd_wx);
    TEST_Vd_Rs1_Vs2_vm(vwsubu_wx);
    TEST_Vd_Rs1_Vs2_vm(vwsub_wx);
    TEST_Vd_Rs1_Vs2_vm(vwmulu_vx);
    TEST_Vd_Rs1_Vs2_vm(vwmulsu_vx);
    TEST_Vd_Rs1_Vs2_vm(vwmul_vx);
    TEST_Vd_Rs1_Vs2_vm(vwmaccu_vx);
    TEST_Vd_Rs1_Vs2_vm(vwmacc_vx);
    TEST_Vd_Rs1_Vs2_vm(vwmaccus_vx);
    TEST_Vd_Rs1_Vs2_vm(vwmaccsu_vx);
}

static void
test_int_extension(void *dc)
{
    TEST_Vd_Vs1_vm(vzext_vf8);
    TEST_Vd_Vs1_vm(vsext_vf8);
    TEST_Vd_Vs1_vm(vzext_vf4);
    TEST_Vd_Vs1_vm(vsext_vf4);
    TEST_Vd_Vs1_vm(vzext_vf2);
    TEST_Vd_Vs1_vm(vsext_vf2);

    TEST_Vd_Vs1_Vs2(vcompress_vm);
    TEST_Vd_Vs1_Vs2(vmandn_mm);
    TEST_Vd_Vs1_Vs2(vmand_mm);
    TEST_Vd_Vs1_Vs2(vmor_mm);
    TEST_Vd_Vs1_Vs2(vmxor_mm);
    TEST_Vd_Vs1_Vs2(vmorn_mm);
    TEST_Vd_Vs1_Vs2(vmnand_mm);
    TEST_Vd_Vs1_Vs2(vmnor_mm);
    TEST_Vd_Vs1_Vs2(vmxnor_mm);

    TEST_Vd_Vs1_vm(vmsbf_m);
    TEST_Vd_Vs1_vm(vmsof_m);
    TEST_Vd_Vs1_vm(vmsif_m);
    TEST_Vd_Vs1_vm(viota_m);

    instr = INSTR_CREATE_vid_v(dc, opnd_create_reg(DR_REG_VR0),
                               opnd_create_immed_int(0b1, OPSZ_1b));
    test_instr_encoding(dc, OP_vid_v, instr);

    TEST_Rd_Vs1_vm(vcpop_m);
    TEST_Rd_Vs1_vm(vfirst_m);

    TEST_Vd_Vs1_Vs2_vm(vdivu_vv);
    TEST_Vd_Vs1_Vs2_vm(vdiv_vv);
    TEST_Vd_Vs1_Vs2_vm(vremu_vv);
    TEST_Vd_Vs1_Vs2_vm(vrem_vv);
    TEST_Vd_Vs1_Vs2_vm(vmulhu_vv);
    TEST_Vd_Vs1_Vs2_vm(vmul_vv);
    TEST_Vd_Vs1_Vs2_vm(vmulhsu_vv);
    TEST_Vd_Vs1_Vs2_vm(vmulh_vv);
    TEST_Vd_Vs1_Vs2_vm(vmadd_vv);
    TEST_Vd_Vs1_Vs2_vm(vnmsub_vv);
    TEST_Vd_Vs1_Vs2_vm(vmacc_vv);
    TEST_Vd_Vs1_Vs2_vm(vnmsac_vv);
    TEST_Vd_Vs1_Vs2_vm(vnmsac_vv);
    TEST_Vd_Vs1_Vs2_vm(vwadd_vv);
    TEST_Vd_Vs1_Vs2_vm(vwsubu_vv);
    TEST_Vd_Vs1_Vs2_vm(vwsub_vv);
    TEST_Vd_Vs1_Vs2_vm(vwaddu_wv);
    TEST_Vd_Vs1_Vs2_vm(vwadd_wv);
    TEST_Vd_Vs1_Vs2_vm(vwsubu_wv);
    TEST_Vd_Vs1_Vs2_vm(vwsub_wv);
    TEST_Vd_Vs1_Vs2_vm(vwmulu_vv);
    TEST_Vd_Vs1_Vs2_vm(vwmulsu_vv);
    TEST_Vd_Vs1_Vs2_vm(vwmul_vv);
    TEST_Vd_Vs1_Vs2_vm(vwmaccu_vv);
    TEST_Vd_Vs1_Vs2_vm(vwmacc_vv);
    TEST_Vd_Vs1_Vs2_vm(vwmaccsu_vv);
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
