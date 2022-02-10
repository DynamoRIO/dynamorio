/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
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
#include "arch.h"       /* need this to include decode.h (byte, etc. */
#include "decode.h"
#include "decode_private.h"
#include "table_private.h"

/****************************************************************************
 * T32.16 table for outside IT block
 */

// We skip auto-formatting for the entire file to keep our single-line table entries:
/* clang-format off */

/* top-level table */
/* Indexed by bits 15:12*/
const instr_info_t T32_16_opc4[] = {
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
    {OP_b_short,0xe000, "b",             xx, xx, j11, xx, xx, no, x, y118[1][0x00]},
    {INVALID,   0xf000, "(bad)",         xx, xx,  xx, xx, xx, no, x, NA},
};

/* second-level table */
/* Indexed by bit 11 */
const instr_info_t T32_16_ext_bit_11[][2] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {EXT_10_6, 0x0000, "(ext 10:6 0)", xx, xx,    xx,   xx, xx, no,      x,        0},
      {OP_lsrs,  0x0800, "lsrs",       RZw, xx,    RYw, i5_6, xx, no, fRC|fWNZC, xb7[3][0x00]},
    }, { /* 1 */
      {OP_asrs,  0x1000, "asrs",       RZw, xx,    RYw, i5_6, xx, no, fRC|fWNZC, xb7[5][0x00]},
      {EXT_10_9, 0x1800, "(ext 10:9 0)", xx, xx,    xx,   xx, xx, no,      x,        0},
    }, { /* 2 */
      {OP_movs,  0x2000, "movs",       RWw, xx,     i8,   xx, xx, no, fRC|fWNZC, xrapc[5][0x01]},
      {OP_cmp,   0x2800, "cmp",        xx,  xx,    RWw,   i8, xx, no, fWNZCV, xrcpc[7][0x01]},
    }, { /* 3 */
      {OP_adds,  0x3000, "adds",       RWw, xx,   RWDw,   i8, xx, no, fWNZCV, xrcpc[6][0x00]},
      {OP_subs,  0x3800, "subs",       RWw, xx,   RWDw,   i8, xx, no, fWNZCV, xrcpc[7][0x00]},
    }, { /* 4 */
      {OP_str,   0x6000, "str",       MP5w, xx,    RZw,   xx, xx, no,      x, xopbx[4][0x05]},
      {OP_ldr,   0x6800, "ldr",        RZw, xx,   MP5w,   xx, xx, no,      x, xrapc[18][0x01]},
    }, { /* 5 */
      {OP_strb,  0x7000, "strb",      MP5b, xx,    RZw,   xx, xx, no,      x, xopbx[0][0x05]},
      {OP_ldrb,  0x7800, "ldrb",       RZw, xx,   MP5b,   xx, xx, no,      x, xrbpc[18][0x00]},
    }, { /* 6 */
      {OP_strh,  0x8000, "strh",      MP5h, xx,    RZw,   xx, xx, no,      x, xopbx[2][0x05]},
      {OP_ldrh,  0x8800, "ldrh",       RZw, xx,   MP5h,   xx, xx, no,      x, xrbpc[5][0x00]},
    }, { /* 7 */
      {OP_str,   0x9000, "str",     MSPP8w, xx,    RWw,   xx, xx, no,      x, y119[0][0x00]},
      {OP_ldr,   0x9800, "ldr",        RWw, xx, MSPP8w,   xx, xx, no,      x, y119[0][0x04]},
    }, { /* 8 */
      {OP_add,   0xa000, "add",        RWw, xx,    PCw, i8x4, xx, no,      x, y96[1][0x00]},
      {OP_add,   0xa800, "add",        RWw, xx,    SPw, i8x4, xx, no,      x, y11[8][0x00]},
    }, { /* 9 */
      {OP_stm,   0xc000, "stm",        Ml, RWw,    L8w,  RWw, xx, no,      x, xbase[0x0a]},
      {EXT_10_8, 0xc800, "ext 10:8 0", xx,  xx,     xx,   xx, xx, no,      x, 0},
    },
};

/* Indexed by bit 11:10 */
const instr_info_t T32_16_ext_bits_11_10[][4] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {EXT_9_6, 0x4000, "ext 9:6 0",  xx, xx,     xx, xx, xx, no, x, 0},
      {EXT_9_6, 0x4400, "ext 9:6 1",  xx, xx,     xx, xx, xx, no, x, 1},
      {OP_ldr,  0x4800, "ldr",       RWw, xx, MPCP8w, xx, xx, no, x, y11[7][0x01]},
      {OP_ldr,  0x4C00, "ldr",       RWw, xx, MPCP8w, xx, xx, no, x, DUP_ENTRY},
    },
};

/* Indexed by bits 11:9 */
const instr_info_t T32_16_ext_bits_11_9[][8] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_str,    0x5000, "str",    MPRw, xx,  RZw,  xx,  xx, no, x, y11[4][0x00]},
      {OP_strh,   0x5200, "strh",   MPRh, xx,  RZh,  xx,  xx, no, x, y11[6][0x00]},
      {OP_strb,   0x5400, "strb",   MPRb, xx,  RZb,  xx,  xx, no, x, y11[5][0x00]},
      {OP_ldrsb,  0x5600, "ldrsb",   RZw, xx, MPRb,  xx,  xx, no, x, xrbpc[9][0x00]},
      {OP_ldr,    0x5800, "ldr",     RZw, xx, MPRw,  xx,  xx, no, x, y11[4][0x01]},
      {OP_ldrh,   0x5a00, "ldrh",    RZw, xx, MPRh,  xx,  xx, no, x, y11[6][0x01]},
      {OP_ldrb,   0x5c00, "ldrb",    RZw, xx, MPRb,  xx,  xx, no, x, y11[5][0x01]},
      {OP_ldrsh,  0x5e00, "ldrsh",   RZw, xx, MPRh,  xx,  xx, no, x, xrapc[28][0x01]},
    },
};

/* Indexed by bits 11:8 */
const instr_info_t T32_16_ext_bits_11_8[][16] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {EXT_7,    0xb000, "(ext 7 0)",         xx,  xx,     xx,  xx, xx, no, x,        0},
      {OP_cbz,   0xb100, "cbz",               xx,  xx, j6x9_3, RZw, xx, no, x, END_LIST},/*i=0*//*XXX: disasm srcs reordered vs instr_get_target() */
      {EXT_7_6,  0xb200, "(ext 7:6 0)",       xx,  xx,     xx,  xx, xx, no, x,        0},
      {OP_cbz,   0xb300, "cbz",               xx,  xx, j6x9_3, RZw, xx, no, x, DUP_ENTRY},/*i=1*//*XXX: disasm srcs reordered vs instr_get_target() */
      {OP_stmdb, 0xb400, "stmdb",         MSPDBl, SPw,   L9Lw, SPw, xx, no, x, xbase[0x10]},/*"push"*/
      {OP_stmdb, 0xb500, "stmdb",         MSPDBl, SPw,   L9Lw, SPw, xx, no, x, DUP_ENTRY},/*M=1*//*"push"*/
      {EXT_5_4,  0xb600, "(ext 5:4 0)",       xx,  xx,     xx,  xx, xx, no, x,        0},
      {INVALID,  0xb700, "(bad)",             xx,  xx,     xx,  xx, xx, no, x,       NA},
      {INVALID,  0xb800, "(bad)",             xx,  xx,     xx,  xx, xx, no, x,       NA},
      {OP_cbnz,  0xb900, "cbnz",              xx,  xx, j6x9_3, RZw, xx, no, x, END_LIST},/*i=0*//*XXX: disasm srcs reordered vs instr_get_target() */
      {EXT_7_6,  0xba00, "(ext 7:6 1)",       xx,  xx,     xx,  xx, xx, no, x,        1},
      {OP_cbnz,  0xbb00, "cbnz",              xx,  xx, j6x9_3, RZw, xx, no, x, DUP_ENTRY},/*i=1*//*XXX: disasm srcs reordered vs instr_get_target() */
      {OP_ldm,   0xbc00, "ldm",             L9Pw, SPw,   MSPl, SPw, xx, no, x, y108[0][0x01]},/*"pop"*/
      {OP_ldm,   0xbd00, "ldm",             L9Pw, SPw,   MSPl, SPw, xx, no, x, DUP_ENTRY},/*P=1*//*"pop"*/
      {OP_bkpt,  0xbe00, "bkpt",              xx,  xx,     i8,  xx, xx, no, x, END_LIST},
      {EXT_3_0,  0xbf00, "(ext 3:0 0)",       xx,  xx,     xx,  xx, xx, no, x,        0},
    }, { /* 1 */
      {OP_b_short, 0xd000, "b",            xx, xx, j8, xx, xx, pred8, x, END_LIST},
      {OP_b_short, 0xd100, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xd200, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xd300, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xd400, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xd500, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xd600, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xd700, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xd800, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xd900, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xda00, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xdb00, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xdc00, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_b_short, 0xdd00, "b",            xx, xx, j8, xx, xx, pred8, x, DUP_ENTRY},
      {OP_udf,     0xde00, "udf",          xx, xx, i8, xx, xx,    no, x, xb13[0][0x01]},
      {OP_svc,     0xdf00, "svc",          xx, xx, i8, xx, xx,    no, x, END_LIST},
    },
};

/* third level table */
/* Indexed by bits 9:6 */
const instr_info_t T32_16_ext_bits_9_6[][16] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_ands,    0x4000, "ands",   RZw, xx, RZDw,  RYw, xx, no, fWNZC, xrcpc[4][0x00]},
      {OP_eors,    0x4040, "eors",   RZw, xx, RZDw,  RYw, xx, no, fWNZC, xrcpc[5][0x00]},
      {OP_lsls,    0x4080, "lsls",   RZw, xx, RZDw,  RYw, xx, no, fWNZC, xi126[2][0x01]},
      {OP_lsrs,    0x40c0, "lsrs",   RZw, xx, RZDw,  RYw, xx, no, fWNZC, y11[0][0x01]},
      {OP_asrs,    0x4100, "asrs",   RZw, xx, RZDw,  RYw, xx, no, fWNZC, y11[1][0x00]},
      {OP_adcs,    0x4140, "adcs",   RZw, xx, RZDw,  RYw, xx, no, fRC|fWNZCV, xfop8[0][0x15]},
      {OP_sbcs,    0x4180, "sbcs",   RZw, xx, RZDw,  RYw, xx, no, fRC|fWNZCV, xfop8[0][0x17]},
      {OP_rors,    0x41c0, "rors",   RZw, xx, RZDw,  RYw, xx, no, fWNZC, xi126[3][0x01]},
      {OP_tst,     0x4200, "tst",     xx, xx,  RZw,  RYw, xx, no, fWNZCV, xrcpc[4][0x01]},
      {OP_rsbs,    0x4240, "rsbs",   RZw, xx,  RYw,   k0, xx, no, fWNZCV, xfop8[0][0x1d]},
      {OP_cmp,     0x4280, "cmp",     xx, xx,  RZw,  RYw, xx, no, fWNZCV, y11[2][0x01]},
      {OP_cmn,     0x42c0, "cmn",     xx, xx,  RZw,  RYw, xx, no, fWNZCV, xrcpc[6][0x01]},
      {OP_orrs,    0x4300, "orrs",   RZw, xx, RZDw,  RYw, xx, no, fWNZC, xrapc[5][0x00]},
      {OP_muls,    0x4340, "muls",   RZw, xx,  RYw, RZDw, xx, no, fWNZ, END_LIST},
      {OP_bics,    0x4380, "bics",   RZw, xx, RZDw,  RYw, xx, no, fWNZC, xfop8[0][0x03]},
      {OP_mvns,    0x43c0, "mvns",   RZw, xx,  RYw,  xx, xx, no, fWNZC, xrapc[7][0x01]},
    }, { /* 1 */
      {OP_add,     0x4400, "add",    RVw, xx, RVDw, RUw, xx, no,      x, xfop8[0][0x10]},
      {OP_add,     0x4440, "add",    RVw, xx, RVDw, RUw, xx, no,      x, DUP_ENTRY},
      {OP_add,     0x4480, "add",    RVw, xx, RVDw, RUw, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_add,     0x44c0, "add",    RVw, xx, RVDw, RUw, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_cmp,     0x4500, "cmp",     xx, xx,  RVw, RUw, xx, no, fWNZCV, y96[0][0x0a]},
      {OP_cmp,     0x4540, "cmp",     xx, xx,  RVw, RUw, xx, no, fWNZCV, DUP_ENTRY},/*high*/
      {OP_cmp,     0x4580, "cmp",     xx, xx,  RVw, RUw, xx, no, fWNZCV, DUP_ENTRY},/*high*/
      {OP_cmp,     0x45c0, "cmp",     xx, xx,  RVw, RUw, xx, no, fWNZCV, DUP_ENTRY},/*high*/
      {OP_mov,     0x4600, "mov",    RVw, xx,  RUw,  xx, xx, no,      x, xrapc[4][0x01]},
      {OP_mov,     0x4640, "mov",    RVw, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_mov,     0x4680, "mov",    RVw, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_mov,     0x46c0, "mov",    RVw, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*high*/
      {OP_bx,      0x4700, "bx",      xx, xx,  RUw,  xx, xx, no,      x, END_LIST},
      {OP_bx,      0x4740, "bx",      xx, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*reg-var*/
      {OP_blx_ind, 0x4780, "blx",    LRw, xx,  RUw,  xx, xx, no,      x, END_LIST},
      {OP_blx_ind, 0x47c0, "blx",    LRw, xx,  RUw,  xx, xx, no,      x, DUP_ENTRY},/*reg-var*/
    },
};

const instr_info_t T32_16_ext_bit_7[][2] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_add,     0xb000, "add",    SPw, xx, SPw, i7x4, xx, no, x, y11[8][0x01]},
      {OP_sub,     0xb080, "sub",    SPw, xx, SPw, i7x4, xx, no, x, xfop8[0][0x1a]},
    },
};

/* Indexed by bits 5 and 4 */
const instr_info_t T32_16_ext_bits_5_4[][4] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_setend,  0xb640, "setend",  xx, xx, i1_3, xx, xx, no, x, END_LIST},
      {OP_setend,  0xb650, "setend",  xx, xx, i1_3, xx, xx, no, x, DUP_ENTRY},
      {OP_cpsie,   0xb660,  "cpsie",  xx, xx,   i3, xx, xx, no, x, xb108[0][0x05]},
      {OP_cpsid,   0xb670,  "cpsid",  xx, xx,   i3, xx, xx, no, x, xb108[0][0x07]},
    },
};

/* Indexed by bits 10:9 */
const instr_info_t T32_16_ext_bits_10_9[][4] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_adds,    0x1800, "adds",   RZw, xx, RYw,  RXw, xx, no, fWNZCV, y11[3][0x00]},
      {OP_subs,    0x1a00, "subs",   RZw, xx, RYw,  RXw, xx, no, fWNZCV, y11[3][0x01]},
      {OP_adds,    0x1c00, "adds",   RZw, xx, RYw, i3_6, xx, no, fWNZCV, y109[0][0x00]},
      {OP_subs,    0x1e00, "subs",   RZw, xx, RYw, i3_6, xx, no, fWNZCV, y109[0][0x01]},
    },
};

/* Indexed by if Rn is listed in reglist:
 * + whether TEST(1 << Rn, reglistbits), take entry 0
 * + else, take entry 1
 */
const instr_info_t T32_16_ext_bits_10_8[][2] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_ldm,   0xc800, "ldm",  L8w, xx,  Ml,  xx, xx, no, x, xbase[0x0b]},
      {OP_ldm,   0xc800, "ldm",  L8w, RWw, Ml, RWw, xx, no, x, y108[0][0x00]},
    },
};

/* Indexed by bits 7:6 */
const instr_info_t T32_16_ext_bits_7_6[][4] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_sxth,    0xb200, "sxth",  RZw, xx, RYh, xx, xx, no, x, xrapc[22][0x01]},
      {OP_sxtb,    0xb240, "sxtb",  RZw, xx, RYb, xx, xx, no, x, xrapc[26][0x01]},
      {OP_uxth,    0xb280, "uxth",  RZw, xx, RYh, xx, xx, no, x, xrapc[23][0x01]},
      {OP_uxtb,    0xb2c0, "uxtb",  RZw, xx, RYb, xx, xx, no, x, xrapc[27][0x01]},
    }, { /* 1 */
      {OP_rev,     0xba00, "rev",   RZw, xx, RYw, xx, xx, no, x, xb74[3][0x08]},
      {OP_rev16,   0xba40, "rev16", RZw, xx, RYw, xx, xx, no, x, xb74[3][0x09]},
      {OP_hlt,     0xba80, "hlt",    xx, xx,  i6, xx, xx, v8, x, END_LIST},
      {OP_revsh,   0xbac0, "revsh", RZw, xx, RYh, xx, xx, no, x, xb74[3][0x0b]},
    },
};

/* Indexed by whether bits 3:0 are zero or not */
const instr_info_t T32_16_ext_imm_3_0[][2] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {EXT_6_4,    0xbf00, "ext 6:4 0", xx, xx,   xx, xx, xx, no, x,        0},
      {OP_it,      0xbf00, "it",        xx, xx, i4_4, i4, xx, no, x, END_LIST},
    },
};

/* Indexed by whether bits 10:6 are zero or not */
const instr_info_t T32_16_ext_imm_10_6[][2] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_movs,  0x0000, "movs",       RZw, xx, RYw,   xx, xx, no, fWNZ, y11[2][0x00]},
      {OP_lsls,  0x0000, "lsls",       RZw, xx, RYw, i5_6, xx, no, fRC|fWNZC, y96[0][0x02]},
    },
};

/* fourth-level table */
/* Indexed by bits 6:4 */
const instr_info_t T32_16_ext_bits_6_4[][8] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    { /* 0 */
      {OP_nop,     0xbf00, "nop",      xx, xx,  xx, xx, xx, no, x, xb20[0][0x00]},
      {OP_yield,   0xbf10, "yield",     xx, xx, xx, xx, xx, no, x, xb20[0][0x01]},
      {OP_wfe,     0xbf20, "wfe",       xx, xx, xx, xx, xx, no, x, xb20[0][0x02]},
      {OP_wfi,     0xbf30, "wfi",       xx, xx, xx, xx, xx, no, x, xb20[0][0x03]},
      {OP_sev,     0xbf40, "sev",       xx, xx, xx, xx, xx, no, x, xb20[0][0x04]},
      {OP_sevl,    0xbf50, "sevl",      xx, xx, xx, xx, xx, v8, x, xb20[0][0x05]},
      {INVALID,    0xbf60, "(bad)",     xx, xx, xx, xx, xx, no, x,       NA},
      {INVALID,    0xbf70, "(bad)",     xx, xx, xx, xx, xx, no, x,       NA},
    },
};

/* clang-format on */
