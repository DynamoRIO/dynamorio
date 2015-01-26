/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h" /* need this to include decode.h (uint, etc.) */
#include "arch.h"    /* need this to include decode.h (byte, etc. */
#include "decode.h"
#include "decode_private.h"
#include "table_private.h"

/****************************************************************************
 * T32.16 table for inside IT block
 */

/* top-level table */
/* indexed by bits 15:12*/
const instr_info_t T32_16_it_opc4[] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    /* 0 */
    {EXT_11,    0x0000, "(ext 11    0)", xx, xx,  xx, xx, xx, no, x, 0},
    {EXT_11,    0x1000, "(ext 11    1)", xx, xx,  xx, xx, xx, no, x, 1},
    {EXT_11,    0x2000, "(ext 11    2)", xx, xx,  xx, xx, xx, no, x, 2},
    {EXT_11,    0x3000, "(ext 11    3)", xx, xx,  xx, xx, xx, no, x, 3},
    {EXT_11_10, 0x4000, "(ext 11:10 0)", xx, xx,  xx, xx, xx, no, x, 0},
    {EXT_11_9,  0x5000, "(ext 11:9  0)", xx, xx,  xx, xx, xx, no, x, 0},
    {EXT_11,    0x6000, "(ext 11    4)", xx, xx,  xx, xx, xx, no, x, 4},
    {EXT_11,    0x7000, "(ext 11    5)", xx, xx,  xx, xx, xx, no, x, 5},
    /* 8 */
    {EXT_11,    0x8000, "(ext 11    6)", xx, xx,  xx, xx, xx, no, x, 6},
    {EXT_11,    0x9000, "(ext 11    7)", xx, xx,  xx, xx, xx, no, x, 7},
    {EXT_11,    0xa000, "(ext 11    8)", xx, xx,  xx, xx, xx, no, x, 8},
    {EXT_11_8,  0xb000, "(ext 11:8  0)", xx, xx,  xx, xx, xx, no, x, 0},
    {EXT_11,    0xc000, "(ext 11    9)", xx, xx,  xx, xx, xx, no, x, 9},
    {EXT_11_8,  0xb000, "(ext 11:8  1)", xx, xx,  xx, xx, xx, no, x, 1},
    {OP_b_short,0xe000, "b",             xx, xx, j11, xx, xx, no, x, END_LIST},
    {INVALID,   0xf000, "(bad)",         xx, xx,  xx, xx, xx, no, x, NA},
};

/* second-level table */
/* indexed by bit 11 */
const instr_info_t T32_16_it_ext_bit_11[][2] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_lsl,   0x0000, "lsl",        RZw, xx,    RYw, i5_6, xx, no, x, END_LIST},
      {OP_lsr ,  0x0800, "lsr",        RZw, xx,    RYw, i5_6, xx, no, x, END_LIST},
    }, { /* 1 */
      {OP_asr,   0x1000, "asr",        RZw, xx,    RYw, i5_6, xx, no, x, END_LIST},
      {EXT_10_9, 0x1800, "ext 10:9 0", xx,  xx,     xx,   xx, xx, no,     x, 0},
    }, { /* 2 */
      {OP_mov,   0x2000, "mov",        RWw, xx,     i8,   xx, xx, no, x, END_LIST},
      {OP_cmp,   0x2800, "cmp",        xx,  xx,    RWw,   i8, xx, no, fWNZCV, END_LIST},
    }, { /* 3 */
      {OP_add,  0x3000, "add",         RWw, xx,   RWDw,   i8, xx, no, x, END_LIST},
      {OP_sub,  0x3800, "sub",         RWw, xx,   RWDw,   i8, xx, no, x, END_LIST},
    }, { /* 4 */
      {OP_str,   0x6000, "str",       MP5w, xx,    RZw,   xx, xx, no,      x, END_LIST},
      {OP_ldr,   0x6800, "ldr",        RZw, xx,   MP5w,   xx, xx, no,      x, END_LIST},
    }, { /* 5 */
      {OP_strb,  0x7000, "strb",      MP5b, xx,    RZw,   xx, xx, no,      x, END_LIST},
      {OP_ldrb,  0x7800, "ldrb",       RZw, xx,   MP5b,   xx, xx, no,      x, END_LIST},
    }, { /* 6 */
      {OP_strh,  0x8000, "strh",      MP5h, xx,    RZw,   xx, xx, no,      x, END_LIST},
      {OP_ldrh,  0x8800, "ldrh",       RZw, xx,   MP5h,   xx, xx, no,      x, END_LIST},
    }, { /* 7 */
      {OP_str,   0x9000, "str",     MSPP8w, xx,    RWw,   xx, xx, no,      x, END_LIST},
      {OP_ldr,   0x9800, "ldr",        RWw, xx, MSPP8w,   xx, xx, no,      x, END_LIST},
    }, { /* 8 */
      {OP_add,   0xa000, "add",        RWw, xx,    PCw,   i8, xx, no,      x, END_LIST},
      {OP_add,   0xa800, "add",        RWw, xx,    SPw,   i8, xx, no,      x, END_LIST},
    }, { /* 9 */
      {OP_stm,   0xc000, "stm",        Ml, RWw,    L8w,  RWw, xx, no,      x, END_LIST},
      {OP_ldm,   0xc800, "ldm",        L8w, xx,     Ml,   xx, xx, no,      x, END_LIST},
    },
};

/* indexed by bit 11:10 */
const instr_info_t T32_16_it_ext_bits_11_10[][4] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {EXT_9_6, 0x4000, "ext 9:6 0",  xx, xx,     xx, xx, xx, no, x, 0},
      {EXT_9_6, 0x4400, "ext 9:6 1",  xx, xx,     xx, xx, xx, no, x, 1},
      {OP_ldr,  0x4800, "ldr",       RWw, xx, MPCP8w, xx, xx, no, x,  END_LIST},
      {OP_ldr,  0x4C00, "ldr",       RWw, xx, MPCP8w, xx, xx, no, x, DUP_ENTRY},
    },
};

/* indexed by bits 11:9 */
const instr_info_t T32_16_it_ext_bits_11_9[][8] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_str,    0x5000, "str",    MPRw, xx,  RZw,  xx,  xx, no, x, END_LIST},
      {OP_strh,   0x5200, "strh",   MPRh, xx,  RZh,  xx,  xx, no, x, END_LIST},
      {OP_strb,   0x5400, "strb",   MPRb, xx,  RZb,  xx,  xx, no, x, END_LIST},
      {OP_ldrsb,  0x5600, "ldrsb",   RZw, xx, MPRb,  xx,  xx, no, x, END_LIST},
      {OP_ldr,    0x5800, "ldr",     RZw, xx, MPRw,  xx,  xx, no, x, END_LIST},
      {OP_ldrh,   0x5a00, "ldrh",    RZw, xx, MPRh,  xx,  xx, no, x, END_LIST},
      {OP_ldrb,   0x5c00, "ldrb",    RZw, xx, MPRb,  xx,  xx, no, x, END_LIST},
      {OP_ldrsh,  0x5e00, "ldrsh",   RZw, xx, MPRh,  xx,  xx, no, x, END_LIST},
    },
};

/* indexed by bits 11:8 */
const instr_info_t T32_16_it_ext_bits_11_8[][16] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {EXT_7,    0xb000, "(ext 7 0)",         xx,  xx,     xx,  xx, xx, no, x,        0},
      {INVALID,  0xb100, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {EXT_7_6,  0xb200, "(ext 7:6 0)",       xx,  xx,     xx,  xx, xx, no, x,        0},
      {INVALID,  0xb300, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {OP_stmdb, 0xb400, "stmdb",         MSPDBl, SPw,   L9Lw, SPw, xx, no, x, END_LIST},
      {OP_stmdb, 0xb500, "stmdb",         MSPDBl, SPw,   L9Lw, SPw, xx, no, x, DUP_ENTRY},/*M=1*//*"push"*/
      {INVALID,  0xb600, "(bad)",             xx, xx,      xx,  xx, xx, no, x, NA},
      {INVALID,  0xb700, "(bad)",             xx,  xx,     xx,  xx, xx, no, x,       NA},
      {INVALID,  0xb800, "(bad)",             xx,  xx,     xx,  xx, xx, no, x,       NA},
      {INVALID,  0xb900, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {EXT_7_6,  0xba00, "(ext 7:6 1)",       xx,  xx,     xx,  xx, xx, no, x,        1},
      {INVALID,  0xbb00, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {OP_ldm,   0xbc00, "ldm",             L9Pw, SPw,   MSPl, SPw, xx, no, x, END_LIST},
      {OP_ldm,   0xbd00, "ldm",             L9Pw, SPw,   MSPl, SPw, xx, no, x, DUP_ENTRY},/*P=1*//*"pop"*/
      {OP_bkpt,  0xbe00, "bkpt",              xx,  xx,     i8,  xx, xx, no, x, END_LIST},/*FIXME: unconditional*/
      {EXT_6_4,  0xbf00, "(ext 6:4 0)",       xx,  xx,     xx,  xx, xx, no, x,        0},
    }, { /* 1 */
      {INVALID,  0xd000, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xd100, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xd200, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xd300, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xd400, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xd500, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xd600, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xd700, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xd800, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xd900, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xda00, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xdb00, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xdc00, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {INVALID,  0xdd00, "(bad)",             xx,  xx, xx,  xx, xx,     no, x,       NA},
      {OP_udf,   0xde00, "udf",               xx,  xx, i8,  xx, xx, no, x,  END_LIST},/*depreciated*/
      {OP_svc,   0xdf00, "svc",               xx,  xx, i8,  xx, xx, no, x,  END_LIST},/*UNKNOWN*/
    },
};

/* third level table */
/* indexed by bits 9:6 */
const instr_info_t T32_16_it_ext_bits_9_6[][16] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_and,    0x4000, "and",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_eor,    0x4040, "eor",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_lsl,    0x4080, "lsl",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_lsr,    0x40c0, "lsr",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_asr,    0x4100, "asr",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_adc,    0x4140, "adc",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_sbc,    0x4180, "sbc",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_ror,    0x41c0, "ror",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_tst,    0x4200, "tst",    xx, xx,  RZw,  RYw, xx, no, fWNZCV, END_LIST},
      {OP_rsb,    0x4240, "rsb",   RZw, xx,  RYw,   k0, xx, no, x, END_LIST},
      {OP_cmp,    0x4280, "cmp",    xx, xx,  RZw,  RYw, xx, no, fWNZCV, END_LIST},
      {OP_cmn,    0x42c0, "cmn",    xx, xx,  RZw,  RYw, xx, no, fWNZCV, END_LIST},
      {OP_orr,    0x4300, "orr",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_mul,    0x4340, "mul",   RZw, xx,  RYw, RZDw, xx, no, x, END_LIST},
      {OP_bic,    0x4380, "bic",   RZw, xx, RZDw,  RYw, xx, no, x, END_LIST},
      {OP_mvn,    0x43c0, "mvn",   RZw, xx,  RYw,   xx, xx, no, x, END_LIST},
    }, { /* 1 */
      {OP_add,     0x4400, "add",    RVw, xx, RVDw, RUw, xx, no,      x, END_LIST},
      {OP_add,     0x4440, "add",    RVw, xx, RVDw, RUw, xx, no,      x, DUP_ENTRY},
      {OP_add,     0x4480, "add",    RVw, xx, RVDw, RUw, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_add,     0x44c0, "add",    RVw, xx, RVDw, RUw, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_cmp,     0x4500, "cmp",     xx, xx,  RVw, RUw, xx, no, fWNZCV, END_LIST},
      {OP_cmp,     0x4540, "cmp",     xx, xx,  RVw, RUw, xx, no, fWNZCV, DUP_ENTRY},/*high*/
      {OP_cmp,     0x4580, "cmp",     xx, xx,  RVw, RUw, xx, no, fWNZCV, DUP_ENTRY},/*high*/
      {OP_cmp,     0x45c0, "cmp",     xx, xx,  RVw, RUw, xx, no, fWNZCV, DUP_ENTRY},/*high*/
      {OP_mov,     0x4600, "mov",    RVw, xx,  RUw,  xx, xx, no,      x, END_LIST},
      {OP_mov,     0x4640, "mov",    RVw, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_mov,     0x4680, "mov",    RVw, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_mov,     0x46c0, "mov",    RVw, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_bx,      0x4700, "bx",      xx, xx,  RUw,  xx, xx, no,      x, END_LIST},
      {OP_bx,      0x4740, "bx",      xx, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*reg-var*/
      {OP_blx_ind, 0x4780, "blx",    LRw, xx,  RUw,  xx, xx, no,      x, END_LIST},
      {OP_blx_ind, 0x47c0, "blx",    LRw, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*reg-var*/
    },
};

const instr_info_t T32_16_it_ext_bit_7[][2] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_add,     0xb000, "add",    SPw, xx, SPw, i7x4, xx, no, x, END_LIST},
      {OP_sub,     0xb080, "sub",    SPw, xx, SPw, i7x4, xx, no, x, END_LIST},
    },
};

/* indexed by bits 10:9 */
const instr_info_t T32_16_it_ext_bits_10_9[][4] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_add,    0x1800, "add",   RZw, xx, RYw,  RXw, xx, no, x, END_LIST},
      {OP_sub,    0x1a00, "sub",   RZw, xx, RYw,  RXw, xx, no, x, END_LIST},
      {OP_add,    0x1c00, "add",   RZw, xx, RYw, i3_6, xx, no, x, END_LIST},
      {OP_sub,    0x1e00, "sub",   RZw, xx, RYw, i3_6, xx, no, x, END_LIST},
    },
};

/* indexed by bits 7:6 */
const instr_info_t T32_16_it_ext_bits_7_6[][4] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_sxth,    0xb200, "sxth",  RZw, xx, RYh, xx, xx, no, x, END_LIST},
      {OP_sxtb,    0xb240, "sxtb",  RZw, xx, RYb, xx, xx, no, x, END_LIST},
      {OP_uxth,    0xb280, "uxth",  RZw, xx, RYh, xx, xx, no, x, END_LIST},
      {OP_uxtb,    0xb2c0, "uxtb",  RZw, xx, RYb, xx, xx, no, x, END_LIST},
    }, { /* 1 */
      {OP_rev,     0xba00, "rev",   RZw, xx, RYw, xx, xx, no, x, END_LIST},
      {OP_rev16,   0xba40, "rev16", RZw, xx, RYw, xx, xx, no, x, END_LIST},
      {OP_hlt,     0xba80, "hlt",    xx, xx,  i6, xx, xx, v8, x, END_LIST},
      {OP_revsh,   0xbac0, "revsh", RZw, xx, RYh, xx, xx, no, x, END_LIST},
    },
};

/* indexed by bits 6:4 */
const instr_info_t T32_16_it_ext_bits_6_4[][8] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {INVALID,    0xbf00, "(bad)",     xx, xx, xx, xx, xx, no, x, NA},
      {OP_yield,   0xbf10, "yield",     xx, xx, xx, xx, xx, no, x, END_LIST},
      {OP_wfe,     0xbf20, "wfe",       xx, xx, xx, xx, xx, no, x, END_LIST},
      {OP_wfi,     0xbf30, "wfi",       xx, xx, xx, xx, xx, no, x, END_LIST},
      {OP_sev,     0xbf40, "sev",       xx, xx, xx, xx, xx, no, x, END_LIST},
      {OP_sevl,    0xbf50, "sevl",      xx, xx, xx, xx, xx, v8, x, END_LIST},
      {INVALID,    0xbf60, "(bad)",     xx, xx, xx, xx, xx, no, x, NA},
      {INVALID,    0xbf70, "(bad)",     xx, xx, xx, xx, xx, no, x, NA},
    },
};
