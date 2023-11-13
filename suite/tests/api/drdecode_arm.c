/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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

/* Uses the static decoder library drdecode */

#include "configure.h"
#include "dr_api.h"
#include <stdio.h>
#include <stdlib.h>

#define GD GLOBAL_DCONTEXT

#define ASSERT(x)                                                                    \
    ((void)((!(x)) ? (printf("ASSERT FAILURE: %s:%d: %s\n", __FILE__, __LINE__, #x), \
                      abort(), 0)                                                    \
                   : 0))

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))

#define ORIG_PC ((byte *)0x10000)

static void
test_LSB(void)
{
    /* Test i#1688: LSB=1 decoding */
    const ushort b[] = {
        0xf300,
        0xe100,
        0x4668,
        0x0002,
    };
    byte *pc = (byte *)b;
    byte *start = pc;
    dr_set_isa_mode(GD, DR_ISA_ARM_A32, NULL);
    /* First decode w/ LSB=0 => ARM */
    while (pc < (byte *)b + sizeof(b)) {
        pc = disassemble_from_copy(GD, pc, ORIG_PC + (pc - start), STDOUT,
                                   false /*don't show pc*/, true);
    }
    /* Now decode w/ LSB=1 => Thumb */
    pc = dr_app_pc_as_jump_target(DR_ISA_ARM_THUMB, (byte *)b);
    while (pc < (byte *)b + sizeof(b)) {
        pc = disassemble_from_copy(GD, pc, ORIG_PC + (pc - start), STDOUT,
                                   false /*don't show pc*/, true);
    }
    /* Thread mode should not change */
    ASSERT(dr_get_isa_mode(GD) == DR_ISA_ARM_A32);
}

/* XXX: It would be nice to share some of this code w/ the other
 * platforms but we'd need cross-platform register references or keep
 * the encoded instr around and compare operands or sthg.
 */
static void
test_noalloc(void)
{
    byte buf[128];
    byte *pc, *end;

    instr_t *to_encode = XINST_CREATE_load(GD, opnd_create_reg(DR_REG_R0),
                                           OPND_CREATE_MEMPTR(DR_REG_R0, 0));
    end = instr_encode(GD, to_encode, buf);
    ASSERT(end - buf < BUFFER_SIZE_ELEMENTS(buf));
    instr_destroy(GD, to_encode);

    instr_noalloc_t noalloc;
    instr_noalloc_init(GD, &noalloc);
    instr_t *instr = instr_from_noalloc(&noalloc);
    pc = decode(GD, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_R0);

    instr_reset(GD, instr);
    pc = decode(GD, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_R0);

    /* There should be no leak reported even w/o a reset b/c there's no
     * extra heap.
     */
}

static void
test_store_source(void)
{
    instr_t *in = XINST_CREATE_store(GD, OPND_CREATE_MEMPTR(DR_REG_R0, 42),
                                     opnd_create_reg(DR_REG_R1));
    ASSERT(!instr_is_opnd_store_source(in, -1)); /* Out of bounds. */
    ASSERT(instr_is_opnd_store_source(in, 0));   /* r1. */
    ASSERT(!instr_is_opnd_store_source(in, 1));  /* Out of bounds. */
    instr_destroy(GD, in);

    in = INSTR_CREATE_push(GD, opnd_create_reg(DR_REG_R1));
    ASSERT(instr_is_opnd_store_source(in, 0));  /* r1. */
    ASSERT(!instr_is_opnd_store_source(in, 1)); /* immed. */
    ASSERT(!instr_is_opnd_store_source(in, 2)); /* sp. */
    instr_destroy(GD, in);

    in = INSTR_CREATE_str_wbimm(GD, OPND_CREATE_MEMPTR(DR_REG_R0, 42),
                                opnd_create_reg(DR_REG_R0), OPND_CREATE_INT(16));
    ASSERT(instr_is_opnd_store_source(in, 0));  /* r0. */
    ASSERT(!instr_is_opnd_store_source(in, 1)); /* immed. */
    ASSERT(!instr_is_opnd_store_source(in, 2)); /* r0 address. */
    instr_destroy(GD, in);

    in = INSTR_CREATE_stmdb_wb(GD, OPND_CREATE_MEMLIST(DR_REG_R3), 10,
                               opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
                               opnd_create_reg(DR_REG_R2), opnd_create_reg(DR_REG_R3),
                               opnd_create_reg(DR_REG_R4), opnd_create_reg(DR_REG_R5),
                               opnd_create_reg(DR_REG_R6), opnd_create_reg(DR_REG_R7),
                               opnd_create_reg(DR_REG_R8), opnd_create_reg(DR_REG_R9));
    ASSERT(instr_is_opnd_store_source(in, 0));   /* r0. */
    ASSERT(instr_is_opnd_store_source(in, 1));   /* r1. */
    ASSERT(instr_is_opnd_store_source(in, 2));   /* r2. */
    ASSERT(instr_is_opnd_store_source(in, 3));   /* r3. */
    ASSERT(instr_is_opnd_store_source(in, 4));   /* r4. */
    ASSERT(instr_is_opnd_store_source(in, 5));   /* r5. */
    ASSERT(instr_is_opnd_store_source(in, 6));   /* r6. */
    ASSERT(instr_is_opnd_store_source(in, 7));   /* r7. */
    ASSERT(instr_is_opnd_store_source(in, 8));   /* r8. */
    ASSERT(instr_is_opnd_store_source(in, 9));   /* r9. */
    ASSERT(!instr_is_opnd_store_source(in, 10)); /* r3 addr. */
    instr_destroy(GD, in);
}

int
main()
{
    test_LSB();

    test_noalloc();

    test_store_source();

    printf("done\n");

    return 0;
}
