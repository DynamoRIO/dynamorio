/* **********************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited.  All rights reserved.
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

#define MOVZ 0xd2800000
#define MOVK 0xf2800000

#define GD GLOBAL_DCONTEXT

#define ASSERT(x)                                                                    \
    ((void)((!(x)) ? (printf("ASSERT FAILURE: %s:%d: %s\n", __FILE__, __LINE__, #x), \
                      abort(), 0)                                                    \
                   : 0))

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))

static void
test_disasm(void)
{
    const uint b[] = { 0x0b010000, 0xd65f03c0 };
    byte *pc = (byte *)b;

    while (pc < (byte *)b + sizeof(b))
        pc = disassemble_with_info(GD, pc, STDOUT, false /*no pc*/, true);
}

static void
test_mov_instr_addr(void)
{
    instrlist_t *ilist = instrlist_create(GD);
    instr_t *callee = INSTR_CREATE_label(GD);
    instrlist_insert_mov_instr_addr(GD, callee, (byte *)ilist, opnd_create_reg(DR_REG_X0),
                                    ilist, NULL, NULL, NULL);
    instrlist_append(ilist, INSTR_CREATE_blr(GD, opnd_create_reg(DR_REG_X0)));
    instrlist_append(ilist, callee);

    uint buf[1024];
    ptr_uint_t end_pc = (ptr_uint_t)instrlist_encode(GD, ilist, (byte *)buf, true);

    // calle is at end_pc
    // movz $callee[0:16) lsl $0x00
    // movk $callee[16:32) lsl $0x10
    // movk $callee[32:48) lsl $0x20
    // movk $callee[48:64) lsl $0x30
    uint instr_addr_bits = end_pc & 0xffff;
    ASSERT(buf[0] == (MOVZ | (instr_addr_bits << 5)));
    for (uint i = 1; i < 4 && (end_pc >> i * 16) > 0; i++) {
        uint instr_addr_bits = (end_pc >> i * 16) & 0xffff;
        uint ls_16mul = i;
        ASSERT(buf[i] == (MOVK | (ls_16mul << 21) | (instr_addr_bits << 5)));
    }
    instrlist_clear_and_destroy(GD, ilist);
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

    instr_t *to_encode = XINST_CREATE_load(GD, opnd_create_reg(DR_REG_X0),
                                           OPND_CREATE_MEMPTR(DR_REG_X0, 0));
    end = instr_encode(GD, to_encode, buf);
    ASSERT(end - buf < BUFFER_SIZE_ELEMENTS(buf));
    instr_destroy(GD, to_encode);

    instr_noalloc_t noalloc;
    instr_noalloc_init(GD, &noalloc);
    instr_t *instr = instr_from_noalloc(&noalloc);
    pc = decode(GD, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_X0);

    instr_reset(GD, instr);
    pc = decode(GD, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_X0);

    /* There should be no leak reported even w/o a reset b/c there's no
     * extra heap.
     */
}

int
main()
{
    test_disasm();

    test_noalloc();

    test_mov_instr_addr();

    printf("done\n");

    return 0;
}
