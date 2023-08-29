/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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
#include "tools.h"

#define GD GLOBAL_DCONTEXT

#define ASSERT(x)                                                                   \
    ((void)((!(x)) ? (print("ASSERT FAILURE: %s:%d: %s\n", __FILE__, __LINE__, #x), \
                      abort(), 0)                                                   \
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
#if !defined(DR_HOST_NOT_TARGET)
    instrlist_t *ilist = instrlist_create(GD);
    instr_t *callee = INSTR_CREATE_label(GD);
    instrlist_append(
        ilist,
        XINST_CREATE_move(GD, opnd_create_reg(DR_REG_X1), opnd_create_reg(DR_REG_LR)));
    instrlist_insert_mov_instr_addr(GD, callee, (byte *)ilist, opnd_create_reg(DR_REG_X0),
                                    ilist, NULL, NULL, NULL);
    instrlist_append(ilist, INSTR_CREATE_blr(GD, opnd_create_reg(DR_REG_X0)));
    instrlist_append(ilist, INSTR_CREATE_ret(GD, opnd_create_reg(DR_REG_X1)));
    instrlist_append(ilist, callee);
    instrlist_insert_mov_immed_ptrsz(GD, 0xdeadbeef, opnd_create_reg(DR_REG_X0), ilist,
                                     NULL, NULL, NULL);
    instrlist_append(ilist, XINST_CREATE_return(GD));

    uint gencode_max_size = 1024;
    byte *generated_code =
        (byte *)allocate_mem(gencode_max_size, ALLOW_EXEC | ALLOW_READ | ALLOW_WRITE);
    assert(generated_code != NULL);
    instrlist_encode(GD, ilist, generated_code, true);
    protect_mem(generated_code, gencode_max_size, ALLOW_EXEC | ALLOW_READ);

    /* Make sure to flush the cache to avoid stale icache values which
     * can lead to SEGFAULTs or SIGILLS on the subsequent attempted
     * execution (i#5033)
     */
    __builtin___clear_cache((char *)generated_code,
                            (char *)generated_code + gencode_max_size);

    uint written = ((uint(*)(void))generated_code)();
    ASSERT(written == 0xdeadbeef);

    instrlist_clear_and_destroy(GD, ilist);
    free_mem((char *)generated_code, gencode_max_size);
#endif
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

static void
test_categories(void)
{
    const uint raw[] = {
        0x00000000, /* no category, udf $0x0000 */
        0x12020000, /* int math, and %w0 $0x40000000 -> %w0 */
        0x0b010000, /* int math, add %w0 %w1 lsl $0x00 -> %w0 */
        0x1E680821, /* fp math, fmul %d1 %d8 -> %d1 */
        0xF8620621, /* load, ldraa -0x0f00(%x17)[8byte] -> %x1 */
        0x39000000, /* store, strb %w0 -> (%x0)[1byte] */
        0x3D800000, /* store, str %q0 -> (%x0)[16byte] */
        0x39C00000, /* load, ldrsb (%x0)[1byte] -> %w0 */
        0x28000911, /* store, stnp %w17 %w2 -> (%x8)[8byte]*/
        0x28401241, /* load, ldnp (%x18)[8byte] -> %w1 %w4 */
        0x2C402020, /* load simd, ldnp (%x1)[8byte] -> %s0 %s8 */
        0x1C000600, /* load, ldr <rel> 0x0000ffffffffe814[4byte] -> %s0 */
        0x19400128, /* load, ldapurb (%x9)[1byte] -> %w8 */
        0x59000144, /* store, stlurh %w4 -> (%x10)[2byte] */
        0xD9600148, /* load, ldg %x8 (%x10) -> %x8 */
        0xD9E00144, /* load, ldgm */
        0xD9600544, /* store, stzg %x4 %x10 $0x0000000000000000 -> (%x10)[16byte] %x10*/
        0xd65f03c0, /* branch, ret %x30 */
        0x80800002, /* sme, fmopa */
        0xc5d57c04, /* sve2, ldff1d (%x0,%z21.d,sxtw)[32byte] %p7/z -> %z4.d */
        0xc700c000, /* other */
        0x02000000  /* other */
    };

    size_t instr_count = sizeof(raw) / sizeof(uint);
    const uint categories[] = { DR_INSTR_CATEGORY_UNCATEGORIZED,
                                DR_INSTR_CATEGORY_INT_MATH,
                                DR_INSTR_CATEGORY_INT_MATH,
                                DR_INSTR_CATEGORY_FP_MATH,
                                DR_INSTR_CATEGORY_LOAD,
                                DR_INSTR_CATEGORY_STORE,
                                DR_INSTR_CATEGORY_STORE,
                                DR_INSTR_CATEGORY_LOAD,
                                DR_INSTR_CATEGORY_STORE,
                                DR_INSTR_CATEGORY_LOAD,
                                DR_INSTR_CATEGORY_LOAD | DR_INSTR_CATEGORY_SIMD,
                                DR_INSTR_CATEGORY_LOAD,
                                DR_INSTR_CATEGORY_LOAD,
                                DR_INSTR_CATEGORY_STORE,
                                DR_INSTR_CATEGORY_LOAD,
                                DR_INSTR_CATEGORY_LOAD,
                                DR_INSTR_CATEGORY_STORE,
                                DR_INSTR_CATEGORY_BRANCH,
                                DR_INSTR_CATEGORY_SIMD,
                                DR_INSTR_CATEGORY_SIMD,
                                DR_INSTR_CATEGORY_OTHER,
                                DR_INSTR_CATEGORY_OTHER };
    byte *pc = (byte *)raw;
    for (int i = 0; i < instr_count; i++) {
        instr_t instr;
        instr_init(GD, &instr);
        instr_set_raw_bits(&instr, pc, 4);
        uint cat = instr_get_category(&instr);
        ASSERT(cat == categories[i]);
        pc += 4;
    }

    /* Test for synthetic instruction */
    instr_t *load = INSTR_CREATE_ldr(GD, opnd_create_reg(DR_REG_R0),
                                     OPND_CREATE_ABSMEM((void *)(1024ULL), OPSZ_4));

    uint cat = instr_get_category(load);
    ASSERT(cat == DR_INSTR_CATEGORY_UNCATEGORIZED);
}

int
main()
{
    test_disasm();

    test_noalloc();

    test_mov_instr_addr();

    test_categories();

    print("done\n");

    return 0;
}
