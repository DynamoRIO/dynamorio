/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
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

#ifndef USE_DYNAMO
#    error NEED USE_DYNAMO
#endif

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

#define VERBOSE 0

#ifdef STANDALONE_DECODER
#    define ASSERT(x)                                                              \
        ((void)((!(x)) ? (fprintf(stderr, "ASSERT FAILURE: %s:%d: %s\n", __FILE__, \
                                  __LINE__, #x),                                   \
                          abort(), 0)                                              \
                       : 0))
#else
#    define ASSERT(x)                                                                 \
        ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s\n", __FILE__, \
                                     __LINE__, #x),                                   \
                          dr_abort(), 0)                                              \
                       : 0))
#endif

static byte buf[8192];

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
    ASSERT(pc != NULL);
    decin = instr_create(dc);
    decode(dc, buf, decin);
    ASSERT(instr_same(instr, decin));

    instr_destroy(dc, instr);
    instr_destroy(dc, decin);
}

/***************************************************************************
 * XXX i#1686: we need to add the IR consistency checks for ARM that we have on
 * x86, ensuring that these are all consistent with each other:
 * - decode
 * - INSTR_CREATE_
 * - encode
 */

/*
 ***************************************************************************/

static void
test_pred(void *dc)
{
    byte *pc;
    instr_t *inst;
    dr_isa_mode_t old_mode;
    dr_set_isa_mode(dc, DR_ISA_ARM_A32, &old_mode);
    inst = INSTR_PRED(INSTR_CREATE_sel(dc, opnd_create_reg(DR_REG_R0),
                                       opnd_create_reg(DR_REG_R1),
                                       opnd_create_reg(DR_REG_R1)),
                      DR_PRED_EQ);
    ASSERT(instr_get_eflags(inst, DR_QUERY_INCLUDE_COND_SRCS) == EFLAGS_READ_ARITH);
    ASSERT(instr_get_eflags(inst, 0) == (EFLAGS_READ_ARITH & (~EFLAGS_READ_GE)));
    instr_destroy(dc, inst);
    inst = INSTR_CREATE_sel(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
                            opnd_create_reg(DR_REG_R1));
    ASSERT(instr_get_eflags(inst, DR_QUERY_INCLUDE_COND_SRCS) == EFLAGS_READ_GE);
    ASSERT(instr_get_eflags(inst, 0) == EFLAGS_READ_GE);
    instr_destroy(dc, inst);
    dr_set_isa_mode(dc, old_mode, NULL);
}

static void
test_pcrel(void *dc)
{
    instr_t *inst;
    const int offs = 128;
    inst = INSTR_CREATE_ldr(dc, opnd_create_reg(DR_REG_R0),
                            opnd_create_rel_addr((void *)&buf[offs], OPSZ_PTR));
    /* On decoding our rel-addr opnd turns into a base-disp:
     *   ldr    <rel> 0x0009d314[4byte] -> %r0
     *   ldr    +0x7c(%pc)[4byte] -> %r0
     * Thus we have our own version of the test_instr_encoding() code.
     */
    instr_t *decin;
    byte *pc;
    instr_disassemble(dc, inst, STDERR);
    print("\n");

    ASSERT(instr_is_encoding_possible(inst));
    pc = instr_encode(dc, inst, buf);
    decin = instr_create(dc);
    decode(dc, buf, decin);
    ASSERT(instr_get_opcode(inst) == instr_get_opcode(decin));
    /* The ISA defines the base point for PC relative as +4 from instruction start. */
#define THUMB_CUR_PC_OFFS 4
    instr_t *base_disp =
        INSTR_CREATE_ldr(dc, opnd_create_reg(DR_REG_R0),
                         OPND_CREATE_MEMPTR(DR_REG_PC, offs - THUMB_CUR_PC_OFFS));
    ASSERT(instr_same(decin, base_disp));

    instr_destroy(dc, inst);
    instr_destroy(dc, decin);
    instr_destroy(dc, base_disp);
}

static void
test_opnd(void *dc)
{
    uint amount;
    opnd_t op1 = opnd_create_base_disp_arm(DR_REG_R4, DR_REG_R7, DR_SHIFT_ASR, 4, 0,
                                           DR_OPND_NEGATED, OPSZ_PTR);
    dr_opnd_flags_t orig_flags = opnd_get_flags(op1);
    bool ok = opnd_replace_reg(&op1, DR_REG_R7, DR_REG_R9);
    ASSERT(opnd_get_base(op1) == DR_REG_R4);
    ASSERT(opnd_get_index(op1) == DR_REG_R9);
    ASSERT(opnd_get_disp(op1) == 0);
    ASSERT(opnd_get_size(op1) == OPSZ_PTR);
    /* Ensure ARM-specific fields are preserved (i#1847) */
    ASSERT(opnd_get_flags(op1) == orig_flags);
    ASSERT(opnd_get_index_shift(op1, &amount) == DR_SHIFT_ASR && amount == 4);

    /* XXX: test other routines like opnd_defines_use() */
}

static void
test_flags(void *dc)
{
    /* Some sanity checks for i#1885: logical instrs do not write all the flags! */
    byte *pc;
    instr_t *inst;
    inst = INSTR_CREATE_lsls(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
                             OPND_CREATE_INT(4));
    ASSERT(!TEST(EFLAGS_WRITE_V, instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)));
    instr_destroy(dc, inst);

    inst = INSTR_CREATE_movs(dc, opnd_create_reg(DR_REG_R0), OPND_CREATE_INT(4));
    ASSERT(TEST(EFLAGS_READ_C, instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)));
    ASSERT(!TEST(EFLAGS_WRITE_V, instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)));
    ASSERT(TESTALL(EFLAGS_WRITE_N | EFLAGS_WRITE_Z | EFLAGS_WRITE_C,
                   instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)));
    instr_destroy(dc, inst);

    inst = INSTR_CREATE_movs(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1));
    ASSERT(!TESTANY(EFLAGS_WRITE_C | EFLAGS_WRITE_V,
                    instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)));
    ASSERT(TESTALL(EFLAGS_WRITE_N | EFLAGS_WRITE_Z,
                   instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)));
    instr_destroy(dc, inst);
}

static void
test_xinst(void *dc)
{
    instr_t *instr;
    /* Sanity check of misc XINST_CREATE_ macros. */

    /* XXX i#1686: add tests of remaining XINST_CREATE macros */
    /* TODO i#1686: add expected patterns to ir_arm.expect */
    instr = XINST_CREATE_call_reg(dc, opnd_create_reg(DR_REG_R5));
    test_instr_encoding(dc, OP_blx_ind, instr);
}

int
main(int argc, char *argv[])
{
#ifdef STANDALONE_DECODER
    void *dcontext = GLOBAL_DCONTEXT;
#else
    void *dcontext = dr_standalone_init();
#endif

    /* XXX i#1686: add tests of all opcodes for internal consistency */

    test_xinst(dcontext);
    print("test_xinst complete\n");

    test_pcrel(dcontext);

    test_pred(dcontext);

    test_opnd(dcontext);

    test_flags(dcontext);

    print("all done\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    return 0;
}
