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

#include "ir_riscv64.h"

byte buf[8192];

byte *
test_instr_encoding_copy(void *dc, uint opcode, app_pc instr_pc, instr_t *instr)
{
    instr_t *decin;
    byte *pc, *next_pc;

    ASSERT(instr_get_opcode(instr) == opcode);
    instr_disassemble(dc, instr, STDERR);
    print("\n");
    ASSERT(instr_is_encoding_possible(instr));
    pc = instr_encode_to_copy(dc, instr, buf, instr_pc);
    ASSERT(pc != NULL);
    decin = instr_create(dc);
    next_pc = decode_from_copy(dc, buf, instr_pc, decin);
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

byte *
test_instr_encoding(void *dc, uint opcode, instr_t *instr)
{
    return test_instr_encoding_copy(dc, opcode, (app_pc)&buf, instr);
}

void
test_instr_encoding_failure(void *dc, uint opcode, app_pc instr_pc, instr_t *instr)
{
    byte *pc;

    pc = instr_encode_to_copy(dc, instr, buf, instr_pc);
    ASSERT(pc == NULL);
    instr_destroy(dc, instr);
}

byte *
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
