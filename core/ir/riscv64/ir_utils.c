/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h"
#include "instr_create_shared.h"
#include "instrument.h"

/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define PRE instrlist_meta_preinsert

byte *
remangle_short_rewrite(dcontext_t *dcontext, instr_t *instr, byte *pc, app_pc target)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

instr_t *
convert_to_near_rel_arch(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

static int
trailing_zeros_64(uint64 x)
{
    // See http://supertech.csail.mit.edu/papers/debruijn.pdf
    static const byte debruijn64tab[64] = {
        0,  1,  56, 2,  57, 49, 28, 3,  61, 58, 42, 50, 38, 29, 17, 4,
        62, 47, 59, 36, 45, 43, 51, 22, 53, 39, 33, 30, 24, 18, 12, 5,
        63, 55, 48, 27, 60, 41, 37, 16, 46, 35, 44, 21, 52, 32, 23, 11,
        54, 26, 40, 15, 34, 20, 31, 10, 25, 14, 19, 9,  13, 8,  7,  6,
    };

    static const uint64 debruijn64 = 0x03f79d71b4ca8b09ULL;
    if (x == 0) {
        return 64;
    }

    return (int)debruijn64tab[(x & -x) * debruijn64 >> (64 - 6)];
}

static void
mov32(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, opnd_t dst, int32_t val,
      OUT instr_t **first, OUT instr_t **last, OUT bool *first_set)
{

    /* `ADDIW rd, rs, imm12` encodes a 12-bit signed extended number;
     * `LUI   rd, uimm20` places the 20-bit signed value at [31, 12], sign-extends the top
     * 32 bits, and zeroes the bottom 12 bits.
     * Combined with these two instructions, we can load an arbitray 32-bit value into a
     * register.
     * Depending on val, the following instructions are emitted.
     * hi20 == 0              -> ADDIW
     * lo12 == 0 && hi20 != 0 -> LUI
     * else                   -> LUI+ADDIW
     */

    /* Add 0x800 to cancel out the signed extension of ADDIW. */
    int32_t hi20 = (val + 0x800) >> 12 & 0xfffff;
    int32_t lo12 = val & 0xfff;

    instr_t *instr_lui, *instr_addiw;
    opnd_t src;
    if (hi20 != 0) {
        instr_lui =
            INSTR_CREATE_lui(dcontext, dst, opnd_create_immed_int(hi20, OPSZ_20b));
        PRE(ilist, instr, instr_lui);
        if (first != NULL && !*first_set) {
            *first = instr_lui;
            *first_set = true;
        }
        if (last != NULL)
            *last = instr_lui;
    }
    if (lo12 != 0 || hi20 == 0) {
        src = hi20 != 0 ? dst : opnd_create_reg(DR_REG_X0);
        instr_addiw =
            INSTR_CREATE_addiw(dcontext, dst, src,
                               opnd_add_flags(opnd_create_immed_int(lo12, OPSZ_12b),
                                              DR_OPND_IMM_PRINT_DECIMAL));
        PRE(ilist, instr, instr_addiw);
        if (first != NULL && !*first_set) {
            *first = instr_addiw;
            *first_set = true;
        }
        if (last != NULL)
            *last = instr_addiw;
    }
}

static void
mov64(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, opnd_t dst, ptr_int_t val,
      OUT instr_t **first, OUT instr_t **last, OUT bool *first_set)
{
    instr_t *tmp;
    if (((val << 32) >> 32) == val) {
        mov32(dcontext, ilist, instr, dst, val, first, last, first_set);
        return;
    }

    /*
     * For 64-bit val, a sequence of up to 8 instructions (i.e.,
     * LUI+ADDIW+SLLI+ADDI+SLLI+ADDI+SLLI+ADDI) is emitted.
     *
     * In the following, val is processed from LSB to MSB while instruction emission is
     * performed from MSB to LSB by calling mov64 recursively. In each recursion,
     * the lowest 12 bits are removed from val and the optimal shift amount is calculated.
     * Then, the remaining part of val is processed recursively and mov32 get called as
     * soon as it fits into 32 bits.
     */
    int64_t lo12 = (val << 52) >> 52;
    /* Add 0x800 to cancel out the signed extension of ADDI. */
    int64_t hi52 = (val + 0x800) >> 12;
    int shift = 12 + trailing_zeros_64((uint64)hi52);
    hi52 = ((hi52 >> (shift - 12)) << shift) >> shift;

    mov64(dcontext, ilist, instr, dst, hi52, first, last, first_set);
    tmp = INSTR_CREATE_slli(
        dcontext, dst, dst,
        opnd_add_flags(opnd_create_immed_int(shift, OPSZ_6b), DR_OPND_IMM_PRINT_DECIMAL));
    PRE(ilist, instr, tmp);
    if (last != NULL)
        *last = tmp;

    if (lo12) {
        tmp = INSTR_CREATE_addi(dcontext, dst, dst,
                                opnd_add_flags(opnd_create_immed_int(lo12, OPSZ_12b),
                                               DR_OPND_IMM_PRINT_DECIMAL));
        PRE(ilist, instr, tmp);
        if (last != NULL)
            *last = tmp;
    }
}

/* FIXME i#3544: Keep this in sync with patch_mov_immed_arch(), which is not implemented
 * yet. */
void
insert_mov_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                      ptr_int_t val, opnd_t dst, instrlist_t *ilist, instr_t *instr,
                      OUT instr_t **first, OUT instr_t **last)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(src_inst == NULL && encode_estimate == NULL);

    CLIENT_ASSERT(opnd_is_reg(dst), "RISC-V cannot store an immediate direct to memory");

    if (opnd_get_reg(dst) == DR_REG_X0) {
        /* Moving a value to the zero register is a no-op. We insert nothing,
         * so *first and *last are set to NULL. Caller beware!
         */
        if (first != NULL)
            *first = NULL;
        if (last != NULL)
            *last = NULL;
        return;
    }

    ASSERT(reg_is_gpr(opnd_get_reg(dst)));

    bool first_set = false;
    mov64(dcontext, ilist, instr, dst, val, first, last, &first_set);
}

void
insert_push_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                       ptr_int_t val, instrlist_t *ilist, instr_t *instr,
                       OUT instr_t **first, OUT instr_t **last)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}
