/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

static void
test_disasm_style(void)
{
    byte buf[128];
    byte *pc, *end;
    instrlist_t *ilist = instrlist_create(GD);
    instrlist_append(ilist,
                     /* With a negative displacement we stress signed type handling. */
                     INSTR_CREATE_mov_st(GD, OPND_CREATE_MEM32(REG_XCX, -3),
                                         opnd_create_reg(REG_EAX)));
    instrlist_append(
        ilist, INSTR_CREATE_mov_imm(GD, opnd_create_reg(REG_EDI), OPND_CREATE_INT32(17)));
    end = instrlist_encode(GD, ilist, buf, false);
    ASSERT(end - buf < BUFFER_SIZE_ELEMENTS(buf));

    pc = buf;
    while (pc < end)
        pc = disassemble_with_info(GD, pc, STDOUT, false /*no pc*/, true);

    disassemble_set_syntax(DR_DISASM_INTEL);
    pc = buf;
    while (pc < end)
        pc = disassemble_with_info(GD, pc, STDOUT, false /*no pc*/, true);

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
    instr = INSTR_CREATE_lss(GD, opnd_create_reg(REG_XAX),
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

static void
test_ptrsz_imm(void)
{
    /* We just ensure that these interfaces are available: we don't stress their
     * corner cases here.
     */
    instrlist_t *ilist = instrlist_create(GD);
    instr_t *callee = INSTR_CREATE_label(GD);
    instrlist_insert_mov_instr_addr(GD, callee, (byte *)ilist,
                                    opnd_create_reg(DR_REG_XAX), ilist, NULL, NULL, NULL);
    instrlist_append(ilist, INSTR_CREATE_call_ind(GD, opnd_create_reg(DR_REG_XAX)));
    instrlist_insert_push_instr_addr(GD, callee, (byte *)ilist, ilist, NULL, NULL, NULL);
    instrlist_append(ilist, callee);
    instrlist_insert_mov_immed_ptrsz(GD, (ptr_uint_t)ilist, opnd_create_reg(DR_REG_XAX),
                                     ilist, NULL, NULL, NULL);
    instrlist_insert_push_immed_ptrsz(GD, (ptr_uint_t)ilist, ilist, NULL, NULL, NULL);
    instrlist_clear_and_destroy(GD, ilist);
}

static void
test_noalloc(void)
{
    byte buf[128];
    byte *pc, *end;

    instr_t *to_encode = XINST_CREATE_load(GD, opnd_create_reg(DR_REG_XAX),
                                           OPND_CREATE_MEMPTR(DR_REG_XAX, 42));
    end = instr_encode(GD, to_encode, buf);
    ASSERT(end - buf < BUFFER_SIZE_ELEMENTS(buf));
    instr_destroy(GD, to_encode);

    instr_noalloc_t noalloc;
    instr_noalloc_init(GD, &noalloc);
    instr_t *instr = instr_from_noalloc(&noalloc);
    pc = decode(GD, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_XAX);

    instr_reset(GD, instr);
    pc = decode(GD, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_XAX);

    /* There should be no leak reported even w/o a reset b/c there's no
     * extra heap.  However, drdecode is used in a mode where DR does not check
     * for leaks!  So we repeat this test inside the api.ir test.
     */
}

int
main()
{
    test_disasm_style();

    test_vendor();

    test_ptrsz_imm();

    test_noalloc();

    printf("done\n");

    return 0;
}
