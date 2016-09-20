/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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

#define ASSERT(x) \
    ((void)((!(x)) ? \
        (printf("ASSERT FAILURE: %s:%d: %s\n", __FILE__,  __LINE__, #x),\
         abort(), 0) : 0))

#define BUFFER_SIZE_BYTES(buf)      sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf)   (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))

static void
test_disasm_style(void)
{
    byte buf[128];
    byte *pc, *end;
    instrlist_t *ilist = instrlist_create(GD);
    instrlist_append(ilist, INSTR_CREATE_mov_st
                     (GD, OPND_CREATE_MEM32(REG_XCX, 37),
                      opnd_create_reg(REG_EAX)));
    instrlist_append(ilist, INSTR_CREATE_mov_imm
                     (GD, opnd_create_reg(REG_EDI),
                      OPND_CREATE_INT32(17)));
    end = instrlist_encode(GD, ilist, buf, false);
    ASSERT(end - buf < BUFFER_SIZE_ELEMENTS(buf));

    pc = buf;
    while (pc < end)
        pc = disassemble_with_info(GD, pc, STDOUT, false/*no pc*/, true);

    disassemble_set_syntax(DR_DISASM_INTEL);
    pc = buf;
    while (pc < end)
        pc = disassemble_with_info(GD, pc, STDOUT, false/*no pc*/, true);

    instrlist_clear_and_destroy(GD, ilist);
}

static void
test_vendor(void)
{
#ifdef X64
    byte buf[128];
    byte *pc, *end;
    instr_t *instr;

    /* create 10-byte mem ref which for Intel requires rex prefix */
    proc_set_vendor(VENDOR_INTEL);
    instr = INSTR_CREATE_lss
        (GD, opnd_create_reg(REG_XAX),
         opnd_create_base_disp(REG_XDX, REG_NULL, 0, 42, OPSZ_10));
    end = instr_encode(GD, instr, buf);
    ASSERT(end - buf < BUFFER_SIZE_ELEMENTS(buf));

    /* read back in */
    instr_reset(GD, instr);
    pc = decode(GD, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_size(instr_get_src(instr, 0)) == OPSZ_10);

    /* now interpret as on AMD and rex should be ignored */
    proc_set_vendor(VENDOR_AMD);
    instr_reset(GD, instr);
    pc = decode(GD, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_size(instr_get_src(instr, 0)) == OPSZ_6);

    instr_destroy(GD, instr);
#endif
}

int
main()
{
    test_disasm_style();

    test_vendor();

    printf("done\n");

    return 0;
}
