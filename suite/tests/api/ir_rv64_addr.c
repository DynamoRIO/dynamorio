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

#include "ir_riscv64.h"

static void
test_jump_and_branch(void *dc)
{
    byte *pc = (byte *)&buf;
    instr_t *instr;

    /* (3 << 12) is a random offset with lowest 12 bits zeroed (see notes below) */
    instr = INSTR_CREATE_auipc(dc, opnd_create_reg(DR_REG_A0),
                               OPND_CREATE_ABSMEM(pc + (3 << 12), OPSZ_0));
    test_instr_encoding_copy(dc, OP_auipc, pc, instr);

    instr = INSTR_CREATE_auipc(dc, opnd_create_reg(DR_REG_A0),
                               OPND_CREATE_ABSMEM(pc + (3 << 12), OPSZ_0));
    test_instr_encoding_copy(dc, OP_auipc, pc, instr);

    instr = INSTR_CREATE_auipc(dc, opnd_create_reg(DR_REG_A0),
                               OPND_CREATE_ABSMEM(pc + (3 << 12), OPSZ_0));
    /* This is expected to fail since we are using an unaligned PC (i.e. target_pc -
     * instr_encode_pc has non-zero lower 12 bits, which cannot be encoded in a
     * single auipc instruction). */
    test_instr_encoding_failure(dc, OP_auipc, pc + 4, instr);

    instr = INSTR_CREATE_jal(dc, opnd_create_reg(DR_REG_A0), opnd_create_pc(pc));
    test_instr_encoding(dc, OP_jal, instr);

    instr = INSTR_CREATE_beq(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_beq, instr);
    instr = INSTR_CREATE_bne(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_bne, instr);
    instr = INSTR_CREATE_blt(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_blt, instr);
    instr = INSTR_CREATE_bge(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                             opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_bge, instr);
    instr = INSTR_CREATE_bltu(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_bltu, instr);
    instr = INSTR_CREATE_bgeu(dc, opnd_create_pc(pc), opnd_create_reg(DR_REG_A0),
                              opnd_create_reg(DR_REG_A1));
    test_instr_encoding(dc, OP_bgeu, instr);

    /* Compressed */
    instr = INSTR_CREATE_c_j(dc, opnd_create_pc(pc));
    test_instr_encoding(dc, OP_c_j, instr);
}

static void
test_xinst(void *dc)
{
    byte *pc;
    instr_t *instr;

    instr = INSTR_CREATE_lui(dc, opnd_create_reg(DR_REG_A0),
                             opnd_create_immed_int(42, OPSZ_20b));
    pc = test_instr_encoding(dc, OP_lui, instr);

    instr = XINST_CREATE_jump(dc, opnd_create_pc(pc));
    ASSERT(opnd_is_reg(instr_get_dst(instr, 0)) &&
           opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_ZERO);
    test_instr_encoding(dc, OP_jal, instr);

    instr = XINST_CREATE_jump_short(dc, opnd_create_pc(pc));
    ASSERT(opnd_is_reg(instr_get_dst(instr, 0)) &&
           opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_ZERO);
    test_instr_encoding(dc, OP_jal, instr);

    instr = XINST_CREATE_call(dc, opnd_create_pc(pc));
    ASSERT(opnd_is_reg(instr_get_dst(instr, 0)) &&
           opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_RA);
    test_instr_encoding(dc, OP_jal, instr);
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

    test_jump_and_branch(dcontext);
    print("test_jump_and_branch complete\n");

    test_xinst(dcontext);
    print("test_xinst complete\n");

    print("All tests complete\n");
    return 0;
}
