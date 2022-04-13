/* **********************************************************
 * Copyright (c) 2014-2018 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h"
#include "arch.h"
#include "decode.h"
#include "decode_private.h"
#include "table_private.h"

/* For T32 32-bit instruction opcodes we store the two half-words in big-endian
 * format for easier human readability.  Thus we store 0xf8df 0x1004 as 0xf8df1004.
 */

/* Encodings that contain i12x8_28_16_0 can vary in their top nibble between 0xe and 0xf.
 * However, all such encodings start 0xef8, so we only have to ensure that 0xff8
 * also maps to the same thing, which we do via the 2 simd8 entries containing duplicates.
 */

// We skip auto-formatting for the entire file to keep our single-line table entries:
/* clang-format off */

/****************************************************************************
 * Top-level T32 table for coprocessor instructions starting with 0xec.
 * Indexed by bits 25:20 (27:26 are both 1's).
 */
const instr_info_t T32_coproc_e[] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    /* ec0 */
    {INVALID,    0xec000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xec100000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_FP,     0xec200000, "(ext fp 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FP,     0xec300000, "(ext fp 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_FP,     0xec400000, "(ext fp 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_FP,     0xec500000, "(ext fp 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_FP,     0xec600000, "(ext fp 4)",  xx, xx, xx, xx, xx, no, x, 4},
    {EXT_FP,     0xec700000, "(ext fp 5)",  xx, xx, xx, xx, xx, no, x, 5},
    /* ec8 */
    {EXT_FP,     0xec800000, "(ext fp 6)",  xx, xx, xx, xx, xx, no, x, 6},
    {EXT_FP,     0xec900000, "(ext fp 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_FP,     0xeca00000, "(ext fp 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_FP,     0xecb00000, "(ext fp 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_FP,     0xecc00000, "(ext fp 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_FP,     0xecd00000, "(ext fp 11)",  xx, xx, xx, xx, xx, no, x, 11},
    {EXT_FP,     0xece00000, "(ext fp 12)",  xx, xx, xx, xx, xx, no, x, 12},
    {EXT_FP,     0xecf00000, "(ext fp 13)",  xx, xx, xx, xx, xx, no, x, 13},
    /* ed0 */
    {EXT_FP,     0xed000000, "(ext fp 14)",  xx, xx, xx, xx, xx, no, x, 14},
    {EXT_FP,     0xed100000, "(ext fp 15)",  xx, xx, xx, xx, xx, no, x, 15},
    {EXT_FP,     0xed200000, "(ext fp 16)",  xx, xx, xx, xx, xx, no, x, 16},
    {EXT_FP,     0xed300000, "(ext fp 17)",  xx, xx, xx, xx, xx, no, x, 17},
    {EXT_FP,     0xed400000, "(ext fp 18)",  xx, xx, xx, xx, xx, no, x, 18},
    {EXT_FP,     0xed500000, "(ext fp 19)",  xx, xx, xx, xx, xx, no, x, 19},
    {EXT_FP,     0xed600000, "(ext fp 20)",  xx, xx, xx, xx, xx, no, x, 20},
    {EXT_FP,     0xed700000, "(ext fp 21)",  xx, xx, xx, xx, xx, no, x, 21},
    /* ed8 */
    {EXT_FP,     0xed800000, "(ext fp 22)",  xx, xx, xx, xx, xx, no, x, 22},
    {EXT_FP,     0xed900000, "(ext fp 23)",  xx, xx, xx, xx, xx, no, x, 23},
    {EXT_FP,     0xeda00000, "(ext fp 24)",  xx, xx, xx, xx, xx, no, x, 24},
    {EXT_FP,     0xedb00000, "(ext fp 25)",  xx, xx, xx, xx, xx, no, x, 25},
    {EXT_FP,     0xedc00000, "(ext fp 26)",  xx, xx, xx, xx, xx, no, x, 26},
    {EXT_FP,     0xedd00000, "(ext fp 27)",  xx, xx, xx, xx, xx, no, x, 27},
    {EXT_FP,     0xede00000, "(ext fp 28)",  xx, xx, xx, xx, xx, no, x, 28},
    {EXT_FP,     0xedf00000, "(ext fp 29)",  xx, xx, xx, xx, xx, no, x, 29},
    /* ee0 */
    {EXT_FP,     0xee000000, "(ext fp 30)",  xx, xx, xx, xx, xx, no, x, 30},
    {EXT_FP,     0xee100000, "(ext fp 31)",  xx, xx, xx, xx, xx, no, x, 31},
    {EXT_FP,     0xee200000, "(ext fp 32)",  xx, xx, xx, xx, xx, no, x, 32},
    {EXT_FP,     0xee300000, "(ext fp 33)",  xx, xx, xx, xx, xx, no, x, 33},
    {EXT_FP,     0xee400000, "(ext fp 34)",  xx, xx, xx, xx, xx, no, x, 34},
    {EXT_FP,     0xee500000, "(ext fp 35)",  xx, xx, xx, xx, xx, no, x, 35},
    {EXT_FP,     0xee600000, "(ext fp 36)",  xx, xx, xx, xx, xx, no, x, 36},
    {EXT_FP,     0xee700000, "(ext fp 37)",  xx, xx, xx, xx, xx, no, x, 37},
    /* ee8 */
    {EXT_FP,     0xee800000, "(ext fp 38)",  xx, xx, xx, xx, xx, no, x, 38},
    {EXT_FP,     0xee900000, "(ext fp 39)",  xx, xx, xx, xx, xx, no, x, 39},
    {EXT_FP,     0xeea00000, "(ext fp 40)",  xx, xx, xx, xx, xx, no, x, 40},
    {EXT_FP,     0xeeb00000, "(ext fp 41)",  xx, xx, xx, xx, xx, no, x, 41},
    {EXT_FP,     0xeec00000, "(ext fp 42)",  xx, xx, xx, xx, xx, no, x, 42},
    {EXT_FP,     0xeed00000, "(ext fp 43)",  xx, xx, xx, xx, xx, no, x, 43},
    {EXT_FP,     0xeee00000, "(ext fp 44)",  xx, xx, xx, xx, xx, no, x, 44},
    {EXT_FP,     0xeef00000, "(ext fp 45)",  xx, xx, xx, xx, xx, no, x, 45},
    /* ef0 */
    {EXT_SIMD6,  0xef000000, "(ext simd6  0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_SIMD6,  0xef100000, "(ext simd6  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_SIMD6,  0xef200000, "(ext simd6  2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_SIMD6,  0xef300000, "(ext simd6  3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_SIMD6,  0xef400000, "(ext simd6  0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_SIMD6,  0xef500000, "(ext simd6  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_SIMD6,  0xef600000, "(ext simd6  2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_SIMD6,  0xef700000, "(ext simd6  3)", xx, xx, xx, xx, xx, no, x, 3},
    /* ef8 */
    {EXT_B4,     0xef800000, "(ext bit4  15)", xx, xx, xx, xx, xx, no, x, 15},
    {EXT_B4,     0xef900000, "(ext bit4  16)", xx, xx, xx, xx, xx, no, x, 16},
    {EXT_B4,     0xefa00000, "(ext bit4  17)", xx, xx, xx, xx, xx, no, x, 17},
    {EXT_B4,     0xefb00000, "(ext bit4  18)", xx, xx, xx, xx, xx, no, x, 18},
    {EXT_B4,     0xefc00000, "(ext bit4  15)", xx, xx, xx, xx, xx, no, x, 15},
    {EXT_B4,     0xefd00000, "(ext bit4  16)", xx, xx, xx, xx, xx, no, x, 16},
    {EXT_B4,     0xefe00000, "(ext bit4  17)", xx, xx, xx, xx, xx, no, x, 17},
    {EXT_B4,     0xeff00000, "(ext bit4  18)", xx, xx, xx, xx, xx, no, x, 18},
};

/****************************************************************************
 * Top-level T32 table for coprocessor instructions starting with 0xfc.
 * Indexed by bits 25:23,21:20 (27:26 are both 1's, and we removed the D bit 22).
 * We could fold this into T32_coproc_e via dup entries (and add bit 28 to indexing).
 */
const instr_info_t T32_coproc_f[] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    /* fc0 */
    {EXT_BITS20, 0xfc000000, "(ext bits20 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BITS20, 0xfc100000, "(ext bits20 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BITS20, 0xfc200000, "(ext bits20 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BITS20, 0xfc300000, "(ext bits20 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BITS20, 0xfc800000, "(ext bits20 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BITS20, 0xfc900000, "(ext bits20 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BITS20, 0xfca00000, "(ext bits20 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BITS20, 0xfcb00000, "(ext bits20 0)", xx, xx, xx, xx, xx, no, x, 0},
    /* fd0 */
    {EXT_BITS20, 0xfd000000, "(ext bits20 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BITS20, 0xfd100000, "(ext bits20 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BITS20, 0xfd200000, "(ext bits20 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BITS20, 0xfd300000, "(ext bits20 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BITS20, 0xfd800000, "(ext bits20 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BITS20, 0xfd900000, "(ext bits20 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BITS20, 0xfda00000, "(ext bits20 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BITS20, 0xfdb00000, "(ext bits20 1)", xx, xx, xx, xx, xx, no, x, 1},
    /* fe0 */
    {EXT_FP,     0xfe000000, "(ext fp 46)",    xx, xx, xx, xx, xx, no, x, 46},
    {EXT_FP,     0xfe100000, "(ext fp 47)",    xx, xx, xx, xx, xx, no, x, 47},
    {EXT_FP,     0xfe200000, "(ext fp 48)",    xx, xx, xx, xx, xx, no, x, 48},
    {EXT_FP,     0xfe300000, "(ext fp 49)",    xx, xx, xx, xx, xx, no, x, 49},
    {EXT_FP,     0xfe800000, "(ext fp 50)",    xx, xx, xx, xx, xx, no, x, 50},
    {EXT_FP,     0xfe900000, "(ext fp 51)",    xx, xx, xx, xx, xx, no, x, 51},
    {EXT_FP,     0xfea00000, "(ext fp 52)",    xx, xx, xx, xx, xx, no, x, 52},
    {EXT_FP,     0xfeb00000, "(ext fp 53)",    xx, xx, xx, xx, xx, no, x, 53},
    /* ff0 */
    {EXT_SIMD6,  0xff000000, "(ext simd6  6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_SIMD6,  0xff100000, "(ext simd6  7)", xx, xx, xx, xx, xx, no, x, 7},
    {EXT_SIMD6,  0xff200000, "(ext simd6  8)", xx, xx, xx, xx, xx, no, x, 8},
    {EXT_SIMD6,  0xff300000, "(ext simd6  9)", xx, xx, xx, xx, xx, no, x, 9},
    {EXT_B4,     0xff800000, "(ext bit4  19)", xx, xx, xx, xx, xx, no, x, 19},
    {EXT_B4,     0xff900000, "(ext bit4  20)", xx, xx, xx, xx, xx, no, x, 20},
    {EXT_B4,     0xffa00000, "(ext bit4  21)", xx, xx, xx, xx, xx, no, x, 21},
    {EXT_B4,     0xffb00000, "(ext bit4  22)", xx, xx, xx, xx, xx, no, x, 22},
};

/* Indexed by whether coprocessor (11:8) is:
 * + 0xa   => index 0
 * + 0xb   => index 1
 * + other => index 2
 */
const instr_info_t T32_ext_fp[][3] = {
  { /* 0 */
    {INVALID,    0xec200a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID,    0xec200b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_stc,     0xec200000, "stc",    Mw, RAw, i4_8, CRBw, n8x4, xop_wb, x, END_LIST},/*PUW=001*/
  }, { /* 1 */
    {INVALID,    0xec300a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID,    0xec300b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_ldc,     0xec300000, "ldc",    CRBw, RAw, Mw, i4_8, n8x4, xop_wb, x, END_LIST},/*PUW=001*/
  }, { /* 2 */
    {OP_vmov,    0xec400a10, "vmov",     WCd, WC2d, RBd, RAd, xx, vfp, x, END_LIST},
    {OP_vmov,    0xec400b10, "vmov",     VCq, xx, RBd, RAd, xx, vfp, x, xfpA[1][0x01]},
    {OP_mcrr,    0xec400000, "mcrr",   CRDw, RAw, RBw, i4_8, i4_7, srcX4, x, END_LIST},
  }, { /* 3 */
    {OP_vmov,    0xec500a10, "vmov",     RBd, RAd, WCd, WC2d, xx, vfp, x, xfp[2][0x00]},
    {OP_vmov,    0xec500b10, "vmov",     RBd, RAd, VCq,   xx, xx, vfp, x, xfp[2][0x01]},
    {OP_mrrc,    0xec500000, "mrrc",   RBw, RAw, i4_8, i4_7, CRDw, no, x, END_LIST},
  }, { /* 4 */
    {INVALID,    0xec600a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID,    0xec600b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_stcl,    0xec600000, "stcl",   Mw, RAw, i4_8, CRBw, n8x4, xop_wb, x, END_LIST},/*PUW=001*/
  }, { /* 5 */
    {INVALID,    0xec700a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID,    0xec700b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_ldcl,    0xec700000, "ldcl",   CRBw, RAw, Mw, i4_8, n8x4, xop_wb, x, END_LIST},/*PUW=001*/
  }, { /* 6 */
    {OP_vstm,    0xec800a00, "vstm",   Ml, xx, WBd, LCd, xx, vfp, x, END_LIST},/*PUW=010*/
    {OP_vstm,    0xec800b00, "vstm",   Ml, xx, VBq, LCq, xx, vfp, x, xfp[8][0x00]},/*PUW=010*/
    {OP_stc,     0xec800000, "stc",    Mw, xx, i4_8, CRBw, i8, no, x, xfp[0][0x02]},/*PUW=010*/
  }, { /* 7 */
    {OP_vldm,    0xec900a00, "vldm",   WBd, LCd, Ml, xx, xx, vfp, x, END_LIST},/*PUW=010*/
    {OP_vldm,    0xec900b00, "vldm",   VBq, LCq, Ml, xx, xx, vfp, x, xfp[9][0x00]},/*PUW=010*/
    {OP_ldc,     0xec900000, "ldc",    CRBw, xx, Mw, i4_8, i8, no, x, xfp[1][0x02]},/*PUW=010*/
  }, { /* 8 */
    {OP_vstm,    0xeca00a00, "vstm",   Ml, RAw, WBd, LCd, RAw, vfp, x, xfp[6][0x00]},/*PUW=011*/
    {OP_vstm,    0xeca00b00, "vstm",   Ml, RAw, VBq, LCq, RAw, vfp, x, xfp[6][0x01]},/*PUW=011*/
    {OP_stc,     0xeca00000, "stc",    Mw, RAw, i4_8, CRBw, i8x4, xop_wb, x, xfp[6][0x02]},/*PUW=011*/
  }, { /* 9 */
    {OP_vldm,    0xecb00a00, "vldm",   WBd, LCd, RAw, Ml, RAw, vfp|dstX3, x, xfp[7][0x00]},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_vldm,    0xecb00b00, "vldm",   VBq, LCq, RAw, Ml, RAw, vfp|dstX3, x, xfp[7][0x01]},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_ldc,     0xecb00000, "ldc",    CRBw, RAw, Mw, i4_8, i8x4, xop_wb, x, xfp[7][0x02]},/*PUW=011*/
  }, { /* 10 */
    {OP_vstm,    0xecc00a00, "vstm",   Ml, xx, WBd, LCd, xx, vfp, x, DUP_ENTRY},/*PUW=010*/
    {OP_vstm,    0xecc00b00, "vstm",   Ml, xx, VBq, LCq, xx, vfp, x, DUP_ENTRY},/*PUW=010*/
    {OP_stcl,    0xecc00000, "stcl",   Mw, xx, i4_8, CRBw, i8, no, x, xfp[4][0x02]},/*PUW=010*/
  }, { /* 11 */
    {OP_vldm,    0xecd00a00, "vldm",   WBd, LCd, Ml, xx, xx, vfp, x, DUP_ENTRY},/*PUW=010*/
    {OP_vldm,    0xecd00b00, "vldm",   VBq, LCq, Ml, xx, xx, vfp, x, DUP_ENTRY},/*PUW=010*/
    {OP_ldcl,    0xecd00000, "ldcl",   CRBw, xx, Mw, i4_8, i8, no, x, xfp[5][0x02]},/*PUW=010*/
  }, { /* 12 */
    {OP_vstm,    0xece00a00, "vstm",   Ml, RAw, WBd, LCd, RAw, vfp, x, DUP_ENTRY},/*PUW=011*/
    {OP_vstm,    0xece00b00, "vstm",   Ml, RAw, VBq, LCq, RAw, vfp, x, DUP_ENTRY},/*PUW=011*/
    {OP_stcl,    0xece00000, "stcl",   Mw, RAw, i4_8, CRBw, i8x4, xop_wb, x, xfp[10][0x02]},/*PUW=011*/
  }, { /* 13 */
    {OP_vldm,    0xecf00a00, "vldm",   WBd, LCd, RAw, Ml, RAw, vfp|dstX3, x, DUP_ENTRY},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_vldm,    0xecf00b00, "vldm",   VBq, LCq, RAw, Ml, RAw, vfp|dstX3, x, DUP_ENTRY},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_ldcl,    0xecf00000, "ldcl",   CRBw, RAw, Mw, i4_8, i8x4, xop_wb, x, xfp[11][0x02]},/*PUW=011*/
  }, { /* 14 */
    {OP_vstr,    0xed000a00, "vstr",   MN8Xd, xx, WBd, xx, xx, vfp, x, END_LIST},
    {OP_vstr,    0xed000b00, "vstr",   MN8Xq, xx, VBq, xx, xx, vfp, x, xfp[22][0x00]},
    {OP_stc,     0xed000000, "stc",    MN8Xw, xx, i4_8, CRBw, n8x4, no, x, xfp[24][0x02]},/*PUW=100*/
  }, { /* 15 */
    {OP_vldr,    0xed100a00, "vldr",   WBd, xx, MN8Xd, xx, xx, vfp, x, END_LIST},
    {OP_vldr,    0xed100b00, "vldr",   VBq, xx, MN8Xq, xx, xx, vfp, x, xfp[23][0x00]},
    {OP_ldc,     0xed100000, "ldc",    CRBw, xx, MN8Xw, i4_8, xx, no, x, xfp[25][0x02]},/*PUW=100*/
  }, { /* 16 */
    {OP_vstmdb,  0xed200a00, "vstmdb", Ml, RAw, WBd, LCd, RAw, vfp, x, END_LIST},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_vstmdb,  0xed200b00, "vstmdb", Ml, RAw, VBq, LCq, RAw, vfp, x, xfp[16][0x00]},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_stc,     0xed200000, "stc",    MN8Xw, RAw, i4_8, CRBw, n8x4, xop_wb, x, xfp[8][0x02]},/*PUW=101*/
  }, { /* 17 */
    {OP_vldmdb,  0xed300a00, "vldmdb", WBd, LCd, RAw, Ml, RAw, vfp|dstX3, x, END_LIST},/*PUW=101*/
    {OP_vldmdb,  0xed300b00, "vldmdb", VBq, LCq, RAw, Ml, RAw, vfp|dstX3, x, xfp[17][0x00]},/*PUW=101*/
    {OP_ldc,     0xed300000, "ldc",    CRBw, RAw, MN8Xw, i4_8, n8x4, xop_wb, x, xfp[9][0x02]},/*PUW=101*/
  }, { /* 18 */
    {OP_vstr,    0xed400a00, "vstr",   MN8Xd, xx, WBd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vstr,    0xed400b00, "vstr",   MN8Xq, xx, VBq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_stcl,    0xed400000, "stcl",   MN8Xw, xx, i4_8, CRBw, n8x4, no, x, xfp[28][0x02]},/*PUW=100*/
  }, { /* 19 */
    {OP_vldr,    0xed500a00, "vldr",   WBd, xx, MN8Xd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vldr,    0xed500b00, "vldr",   VBq, xx, MN8Xq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_ldcl,    0xed500000, "ldcl",   CRBw, xx, MN8Xw, i4_8, xx, no, x, xfp[29][0x02]},/*PUW=100*/
  }, { /* 20 */
    {OP_vstmdb,  0xed600a00, "vstmdb", Ml, RAw, WBd, LCd, RAw, vfp, x, DUP_ENTRY},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_vstmdb,  0xed600b00, "vstmdb", Ml, RAw, VBq, LCq, RAw, vfp, x, DUP_ENTRY},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_stcl,    0xed600000, "stcl",   MN8Xw, RAw, i4_8, CRBw, n8x4, xop_wb, x, xfp[12][0x02]},/*PUW=101*/
  }, { /* 21 */
    {OP_vldmdb,  0xed700a00, "vldmdb", WBd, LCd, RAw, Ml, RAw, vfp|dstX3, x, DUP_ENTRY},/*PUW=101*/
    {OP_vldmdb,  0xed700b00, "vldmdb", VBq, LCq, RAw, Ml, RAw, vfp|dstX3, x, DUP_ENTRY},/*PUW=101*/
    {OP_ldcl,    0xed700000, "ldcl",   CRBw, RAw, MN8Xw, i4_8, n8x4, xop_wb, x, xfp[13][0x02]},/*PUW=101*/
  }, { /* 22 */
    {OP_vstr,    0xed800a00, "vstr",   MP8Xd, xx, WBd, xx, xx, vfp, x, xfp[14][0x00]},
    {OP_vstr,    0xed800b00, "vstr",   MP8Xq, xx, VBq, xx, xx, vfp, x, xfp[14][0x01]},
    {OP_stc,     0xed800000, "stc",    MP8Xw, xx, i4_8, CRBw, i8x4, no, x, xfp[14][0x02]},/*PUW=110*/
  }, { /* 23 */
    {OP_vldr,    0xed900a00, "vldr",   WBd, xx, MP8Xd, xx, xx, vfp, x, xfp[15][0x00]},
    {OP_vldr,    0xed900b00, "vldr",   VBq, xx, MP8Xq, xx, xx, vfp, x, xfp[15][0x01]},
    {OP_ldc,     0xed900000, "ldc",    CRBw, xx, MP8Xw, i4_8, xx, no, x, xfp[15][0x02]},/*PUW=110*/
  }, { /* 24 */
    {INVALID,    0xeda00a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID,    0xeda00b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_stc,     0xeda00000, "stc",    MP8Xw, RAw, i4_8, CRBw, i8x4, xop_wb, x, xfp[16][0x02]},/*PUW=111*/
  }, { /* 25 */
    {INVALID,    0xedb00a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID,    0xedb00b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_ldc,     0xedb00000, "ldc",    CRBw, RAw, MP8Xw, i4_8, i8x4, xop_wb, x, xfp[17][0x02]},/*PUW=111*/
  }, { /* 26 */
    {OP_vstr,    0xedc00a00, "vstr",   MP8Xd, xx, WBd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vstr,    0xedc00b00, "vstr",   MP8Xq, xx, VBq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_stcl,    0xedc00000, "stcl",   MP8Xw, xx, i4_8, CRBw, i8x4, no, x, xfp[18][0x02]},/*PUW=110*/
  }, { /* 27 */
    {OP_vldr,    0xedd00a00, "vldr",   WBd, xx, MP8Xd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vldr,    0xedd00b00, "vldr",   VBq, xx, MP8Xq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_ldcl,    0xedd00000, "ldcl",   CRBw, xx, MP8Xw, i4_8, xx, no, x, xfp[19][0x02]},/*PUW=110*/
  }, { /* 28 */
    {INVALID,    0xede00a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID,    0xede00b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_stcl,    0xede00000, "stcl",   MP8Xw, RAw, i4_8, CRBw, i8x4, xop_wb, x, xfp[20][0x02]},/*PUW=111*/
  }, { /* 29 */
    {INVALID,    0xedf00a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID,    0xedf00b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_ldcl,    0xedf00000, "ldcl",   CRBw, RAw, MP8Xw, i4_8, i8x4, xop_wb, x, xfp[21][0x02]},/*PUW=111*/
  }, { /* 30 */
    {EXT_FPA,    0xee000a00, "(ext fpA 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FPB,    0xee000b00, "(ext fpB 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_B4,     0xee000000, "(ext bit4 11)",  xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 31 */
    {EXT_FPA,    0xee100a00, "(ext fpA 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_FPB,    0xee100b00, "(ext fpB 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_B4,     0xee100000, "(ext bit4 12)",  xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 32 */
    {EXT_FPA,    0xee200a00, "(ext fpA 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_FPB,    0xee200b00, "(ext fpB 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_B4,     0xee200000, "(ext bit4 11)",  xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 33 */
    {EXT_FPA,    0xee300a00, "(ext fpA 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_FPB,    0xee300b00, "(ext fpB 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_B4,     0xee300000, "(ext bit4 12)",  xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 34 */
    {EXT_FPA,    0xee400a00, "(ext fpA 4)",  xx, xx, xx, xx, xx, no, x, 4},
    {EXT_FPB,    0xee400b00, "(ext fpB 4)",  xx, xx, xx, xx, xx, no, x, 4},
    {EXT_B4,     0xee400000, "(ext bit4 11)",  xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 35 */
    {EXT_FPA,    0xee500a00, "(ext fpA 5)",  xx, xx, xx, xx, xx, no, x, 5},
    {EXT_FPB,    0xee500b00, "(ext fpB 5)",  xx, xx, xx, xx, xx, no, x, 5},
    {EXT_B4,     0xee500000, "(ext bit4 12)",  xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 36 */
    {EXT_FPA,    0xee600a00, "(ext fpA 6)",  xx, xx, xx, xx, xx, no, x, 6},
    {EXT_FPB,    0xee600b00, "(ext fpB 6)",  xx, xx, xx, xx, xx, no, x, 6},
    {EXT_B4,     0xee600000, "(ext bit4 11)",  xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 37 */
    {EXT_FPA,    0xee700a00, "(ext fpA 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_FPB,    0xee700b00, "(ext fpB 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_B4,     0xee700000, "(ext bit4 12)",  xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 38 */
    {EXT_FPA,    0xee800a00, "(ext fpA 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_FPB,    0xee800b00, "(ext fpB 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_B4,     0xee800000, "(ext bit4 11)",  xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 39 */
    {EXT_FPA,    0xee900a00, "(ext fpA 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_FPB,    0xee900b00, "(ext fpB 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_B4,     0xee900000, "(ext bit4 12)",  xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 40 */
    {EXT_FPA,    0xeea00a00, "(ext fpA 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_FPB,    0xeea00b00, "(ext fpB 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_B4,     0xeea00000, "(ext bit4 11)",  xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 41 */
    {EXT_OPC4,   0xeeb00a00, "(ext opc4 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_OPC4,   0xeeb00b00, "(ext opc4 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_B4,     0xeeb00000, "(ext bit4 12)",  xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 42 */
    {EXT_FPA,    0xeec00a00, "(ext fpA 11)",  xx, xx, xx, xx, xx, no, x, 11},
    {EXT_FPB,    0xeec00b00, "(ext fpB 11)",  xx, xx, xx, xx, xx, no, x, 11},
    {EXT_B4,     0xeec00000, "(ext bit4 11)",  xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 43 */
    {EXT_FPA,    0xeed00a00, "(ext fpB 12)",  xx, xx, xx, xx, xx, no, x, 12},
    {EXT_FPB,    0xeed00b00, "(ext fpB 12)",  xx, xx, xx, xx, xx, no, x, 12},
    {EXT_B4,     0xeed00000, "(ext bit4 12)",  xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 44 */
    {EXT_FPA,    0xeee00a00, "(ext fpA 13)",  xx, xx, xx, xx, xx, no, x, 13},
    {EXT_FPB,    0xeee00b00, "(ext fpB 13)",  xx, xx, xx, xx, xx, no, x, 13},
    {EXT_B4,     0xeee00000, "(ext bit4 11)",  xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 45 */
    {EXT_OPC4,   0xeef00a00, "(ext opc4 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_OPC4,   0xeef00b00, "(ext opc4 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_B4,     0xeef00000, "(ext bit4 12)",  xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 46 */
    {OP_vsel_eq_f32, 0xfe000a00, "vsel.eq.f32",   WBd, xx, i2_20, WAd, WCd, v8|vfp, fRZ, END_LIST},
    {OP_vsel_eq_f64, 0xfe000b00, "vsel.eq.f64",   VBq, xx, i2_20, VAq, VCq, v8|vfp, fRZ, END_LIST},
    {EXT_B4,         0xfe000000, "(ext bit4 13)", xx, xx, xx, xx, xx, no, x, 13},
  }, { /* 47 */
    {OP_vsel_vs_f32, 0xfe100a00, "vsel.vs.f32",   WBd, xx, i2_20, WAd, WCd, v8|vfp, fRV, END_LIST},
    {OP_vsel_vs_f64, 0xfe100b00, "vsel.vs.f64",   VBq, xx, i2_20, VAq, VCq, v8|vfp, fRV, END_LIST},
    {EXT_B4,         0xfe100000, "(ext bit4 14)", xx, xx, xx, xx, xx, no, x, 14},
  }, { /* 48 */
    {OP_vsel_ge_f32, 0xfe200a00, "vsel.ge.f32",   WBd, xx, i2_20, WAd, WCd, v8|vfp, fRNV, END_LIST},
    {OP_vsel_ge_f64, 0xfe200b00, "vsel.ge.f64",   VBq, xx, i2_20, VAq, VCq, v8|vfp, fRNV, END_LIST},
    {EXT_B4,         0xfe200000, "(ext bit4 13)", xx, xx, xx, xx, xx, no, x, 13},
  }, { /* 49 */
    {OP_vsel_gt_f32, 0xfe300a00, "vsel.gt.f32",   WBd, xx, i2_20, WAd, WCd, v8|vfp, fRNZV, END_LIST},
    {OP_vsel_gt_f64, 0xfe300b00, "vsel.gt.f64",   VBq, xx, i2_20, VAq, VCq, v8|vfp, fRNZV, END_LIST},
    {EXT_B4,         0xfe300000, "(ext bit4 14)", xx, xx, xx, xx, xx, no, x, 14},
  }, { /* 50 */
    {EXT_BIT6,       0xfe800a00, "(ext bit6  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BIT6,       0xfe800b00, "(ext bit6  2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_B4,         0xfe800000, "(ext bit4 13)", xx, xx, xx, xx, xx, no, x, 13},
  }, { /* 51 */
    {INVALID,        0xfe900a00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0xfe900b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {EXT_B4,         0xfe900000, "(ext bit4 14)", xx, xx, xx, xx, xx, no, x, 14},
  }, { /* 52 */
    {INVALID,        0xfea00a00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0xfea00b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {EXT_B4,         0xfea00000, "(ext bit4 13)", xx, xx, xx, xx, xx, no, x, 13},
  }, { /* 53 */
    {EXT_SIMD5B,     0xfeb00a00, "(ext simd5b 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_SIMD5B,     0xfeb00b00, "(ext simd5b 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_B4,         0xfeb00000, "(ext bit4 14)", xx, xx, xx, xx, xx, no, x, 14},
  },
};

/* Indexed by bits 7:4 */
const instr_info_t T32_ext_opc4[][16] = {
  { /* 0 */
    {OP_vmov_f32,0xeeb00a00, "vmov.f32", WBd, xx, i8x16_0, xx, xx, vfp, x, xbi16[0][0x00]},
    {INVALID,    0xeeb00a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeeb00a20, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeeb00a30, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeeb00a40, "(ext bits16 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,    0xeeb00a50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeeb00a60, "(ext bits16 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,    0xeeb00a70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeeb00a80, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeeb00a90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeeb00aa0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeeb00ab0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeeb00ac0, "(ext bits16 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,    0xeeb00ad0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeeb00ae0, "(ext bits16 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,    0xeeb00af0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 1 */
    {OP_vmov_f64,0xeeb00b00, "vmov.f64", VBq, xx, i8x16_0, xx, xx, vfp, x, xbi16[2][0x00]},
    {INVALID,    0xeeb00b10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeeb00b20, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u16,0xeeb00b30, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0xeeb00b40, "(ext bits16 2)", xx, xx, xx, xx, xx, no, x, 2},
    {INVALID,    0xeeb00b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeeb00b60, "(ext bits16 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_vmov_u16,0xeeb00b70, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, DUP_ENTRY},
    {INVALID,    0xeeb00b80, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeeb00b90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeeb00ba0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u16,0xeeb00bb0, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0xeeb00bc0, "(ext bits16 3)", xx, xx, xx, xx, xx, no, x, 3},
    {INVALID,    0xeeb00bd0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeeb00be0, "(ext bits16 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_vmov_u16,0xeeb00bf0, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, DUP_ENTRY},
  }, { /* 2 */
    {OP_vmov_f32,0xeef00a00, "vmov.f32", WBd, xx, i8x16_0, xx, xx, vfp, x, DUP_ENTRY},
    {EXT_RBPC,   0xeef00a10, "(ext rbpc 17)", xx, xx, xx, xx, xx, no, x, 17},
    {INVALID,    0xeef00a20, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeef00a30, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeef00a40, "(ext bits16 4)", xx, xx, xx, xx, xx, no, x, 4},
    {INVALID,    0xeef00a50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeef00a60, "(ext bits16 4)", xx, xx, xx, xx, xx, no, x, 4},
    {INVALID,    0xeef00a70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeef00a80, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeef00a90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeef00aa0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xeef00ab0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeef00ac0, "(ext bits16 5)", xx, xx, xx, xx, xx, no, x, 5},
    {INVALID,    0xeef00ad0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0xeef00ae0, "(ext bits16 5)", xx, xx, xx, xx, xx, no, x, 5},
    {INVALID,    0xeef00af0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 3 */
    {OP_vmov_f64,0xeef00b00, "vmov.f64", VBq, xx, i8x16_0, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_u8, 0xeef00b10, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {INVALID,    0xeef00b20, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8, 0xeef00b30, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0xeef00b40, "(ext bits16 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_vmov_u8, 0xeef00b50, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0xeef00b60, "(ext bits16 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_vmov_u8, 0xeef00b70, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {INVALID,    0xeef00b80, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8, 0xeef00b90, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {INVALID,    0xeef00ba0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8, 0xeef00bb0, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0xeef00bc0, "(ext bits16 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_vmov_u8, 0xeef00bd0, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0xeef00be0, "(ext bits16 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_vmov_u8, 0xeef00bf0, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
  },
};

/* Indexed by whether imm4 in 19:16 is 0, 1, or other */
const instr_info_t T32_ext_imm1916[][3] = {
  { /* 0 */
    {OP_vmovl_s16, 0xef900a10, "vmovl.s16", VBdq, xx, VCq, xx, xx, no, x, END_LIST},
    {OP_vshll_s16, 0xef900a10, "vshll.s16", VBdq, xx, VCq, i4_16, xx, no, x, END_LIST},/*19:16 cannot be 0*/
    {OP_vshll_s16, 0xef900a10, "vshll.s16", VBdq, xx, VCq, i4_16, xx, no, x, DUP_ENTRY},/*19:16 cannot be 0*/
  }, { /* 1 */
    {OP_vmovl_u16, 0xff900a10, "vmovl.u16", VBdq, xx, VCq, xx, xx, no, x, END_LIST},
    {OP_vshll_u16, 0xff900a10, "vshll.u16", VBdq, xx, VCq, i4_16, xx, no, x, END_LIST},/*19:16 cannot be 0*/
    {OP_vshll_u16, 0xff900a10, "vshll.u16", VBdq, xx, VCq, i4_16, xx, no, x, DUP_ENTRY},/*19:16 cannot be 0*/
  }, { /* 2 */
    {OP_vmsr,     0xeee00a10, "vmsr",   xx, xx, RBd, i4_16, xx, vfp, x, xi19[2][0x01]},
    {OP_vmsr,     0xeee10a10, "vmsr",   FPSCR, xx, RBd, xx, xx, vfp, x, END_LIST},
    {OP_vmsr,     0xeee00a10, "vmsr",   xx, xx, RBd, i4_16, xx, vfp, x, DUP_ENTRY},
  }, { /* 3 */
    {OP_vmrs,     0xeef00a10, "vmrs",   RBw, xx, i4_16, xx, xx, vfp, x, xi19[3][0x01]},
    {OP_vmrs,     0xeef10a10, "vmrs",   RBw, xx, FPSCR, xx, xx, vfp, x, END_LIST},
    {OP_vmrs,     0xeef00a10, "vmrs",   RBw, xx, i4_16, xx, xx, vfp, x, DUP_ENTRY},
  },
};

/* Indexed by bits 6,4 but if both are set it's invalid. */
const instr_info_t T32_ext_opc4fpA[][3] = {
  { /* 0 */
    {OP_vmla_f32, 0xee000a00, "vmla.f32",  WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
    {OP_vmov,     0xee000a10, "vmov",     WAd, xx, RBd, xx, xx, vfp, x, xfp[3][0x00]},
    {OP_vmls_f32, 0xee000a40, "vmls.f32",  WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
  }, { /* 1 */
    {OP_vnmls_f32,0xee100a00, "vnmls.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
    {OP_vmov,     0xee100a10, "vmov",     RBd, xx, WAd, xx, xx, vfp, x, xfpA[0][0x01]},
    {OP_vnmla_f32,0xee100a40, "vnmla.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
  }, { /* 2 */
    {OP_vmul_f32, 0xee200a00, "vmul.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
    {INVALID,     0xee200a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmul_f32,0xee200a40, "vnmul.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
  }, { /* 3 */
    {OP_vadd_f32, 0xee300a00, "vadd.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
    {INVALID,     0xee300a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsub_f32, 0xee300a40, "vsub.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
  }, { /* 4 */
    {OP_vmla_f32, 0xee400a00, "vmla.f32",  WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xee400a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmls_f32, 0xee400a40, "vmls.f32",  WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
  }, { /* 5 */
    {OP_vnmls_f32,0xee500a00, "vnmls.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xee500a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmla_f32,0xee500a40, "vnmla.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
  }, { /* 6 */
    {OP_vmul_f32, 0xee600a00, "vmul.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xee600a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmul_f32,0xee600a40, "vnmul.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
  }, { /* 7 */
    {OP_vadd_f32, 0xee700a00, "vadd.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xee700a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsub_f32, 0xee700a40, "vsub.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
  }, { /* 8 */
    {OP_vdiv_f32, 0xee800a00, "vdiv.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
    {INVALID,     0xee800a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xee800a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_vfnms_f32,0xee900a00, "vfnms.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
    {INVALID,     0xee900a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnma_f32,0xee900a40, "vfnma.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
  }, { /* 10 */
    {OP_vfma_f32, 0xeea00a00, "vfma.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
    {INVALID,     0xeea00a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f32, 0xeea00a40, "vfms.f32", WBd, xx, WAd, WCd, xx, vfp, x, END_LIST},
  }, { /* 11 */
    {OP_vdiv_f32, 0xeec00a00, "vdiv.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xeec00a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeec00a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 12 */
    {OP_vfnms_f32,0xeed00a00, "vfnms.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xeed00a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnma_f32,0xeed00a40, "vfnma.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
  }, { /* 13 */
    {OP_vfma_f32, 0xeee00a00, "vfma.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
    {EXT_IMM1916, 0xeee00a10, "(ext imm1916 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_vfms_f32, 0xeee00a40, "vfms.f32", WBd, xx, WAd, WCd, xx, vfp, x, DUP_ENTRY},
  },
};

/* Indexed by bits 6:4 */
const instr_info_t T32_ext_opc4fpB[][8] = {
  { /* 0 */
    {OP_vmla_f64, 0xee000b00, "vmla.f64",  VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {OP_vmov_32,  0xee000b10, "vmov.32",  VAd_q, xx, RBd, i1_21, xx, vfp, x, END_LIST},
    {OP_vmla_f64, 0xee000b20, "vmla.f64",  VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_16,  0xee000b30, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, vfp, x, END_LIST},
    {OP_vmls_f64, 0xee000b40, "vmls.f64",  VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {INVALID,     0xee000b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmls_f64, 0xee000b60, "vmls.f64",  VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_16,  0xee000b70, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, vfp, x, DUP_ENTRY},
  }, { /* 1 */
    {OP_vnmls_f64,0xee100b00, "vnmls.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {OP_vmov_32,  0xee100b10, "vmov.32",  RBd, xx, VAd_q, i1_21, xx, vfp, x, xfpB[0][0x01]},
    {OP_vnmls_f64,0xee100b20, "vnmls.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s16, 0xee100b30, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, END_LIST},
    {OP_vnmla_f64,0xee100b40, "vnmla.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {INVALID,     0xee100b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmla_f64,0xee100b60, "vnmla.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s16, 0xee100b70, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, DUP_ENTRY},
  }, { /* 2 */
    {OP_vmul_f64, 0xee200b00, "vmul.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {OP_vmov_32,  0xee200b10, "vmov.32",  VAd_q, xx, RBd, i1_21, xx, vfp, x, DUP_ENTRY},
    {OP_vmul_f64, 0xee200b20, "vmul.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_16,  0xee200b30, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, vfp, x, DUP_ENTRY},
    {OP_vnmul_f64,0xee200b40, "vnmul.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {INVALID,     0xee200b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmul_f64,0xee200b60, "vnmul.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_16,  0xee200b70, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, vfp, x, DUP_ENTRY},
  }, { /* 3 */
    {OP_vadd_f64, 0xee300b00, "vadd.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {OP_vmov_32,  0xee300b10, "vmov.32",  RBd, xx, VAd_q, i1_21, xx, vfp, x, DUP_ENTRY},
    {OP_vadd_f64, 0xee300b20, "vadd.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s16, 0xee300b30, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, DUP_ENTRY},
    {OP_vsub_f64, 0xee300b40, "vsub.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {INVALID,     0xee300b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsub_f64, 0xee300b60, "vsub.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s16, 0xee300b70, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, DUP_ENTRY},
  }, { /* 4 */
    {OP_vmla_f64, 0xee400b00, "vmla.f64",  VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0xee400b10, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, vfp, x, END_LIST},
    {OP_vmla_f64, 0xee400b20, "vmla.f64",  VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0xee400b30, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vmls_f64, 0xee400b40, "vmls.f64",  VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0xee400b50, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vmls_f64, 0xee400b60, "vmls.f64",  VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0xee400b70, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, vfp, x, DUP_ENTRY},
  }, { /* 5 */
    {OP_vnmls_f64,0xee500b00, "vnmls.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0xee500b10, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, END_LIST},
    {OP_vnmls_f64,0xee500b20, "vnmls.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0xee500b30, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vnmla_f64,0xee500b40, "vnmla.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0xee500b50, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vnmla_f64,0xee500b60, "vnmla.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0xee500b70, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
  }, { /* 6 */
    {OP_vmul_f64, 0xee600b00, "vmul.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0xee600b10, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vmul_f64, 0xee600b20, "vmul.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0xee600b30, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vnmul_f64,0xee600b40, "vnmul.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0xee600b50, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vnmul_f64,0xee600b60, "vnmul.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0xee600b70, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, vfp, x, DUP_ENTRY},
  }, { /* 7 */
    {OP_vadd_f64, 0xee700b00, "vadd.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0xee700b10, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vadd_f64, 0xee700b20, "vadd.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0xee700b30, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vsub_f64, 0xee700b40, "vsub.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0xee700b50, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vsub_f64, 0xee700b60, "vsub.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0xee700b70, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
  }, { /* 8 */
    {OP_vdiv_f64, 0xee800b00, "vdiv.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {OP_vdup_32,  0xee800b10, "vdup.32",  WAd, xx, RBd, xx, xx, vfp, x, END_LIST},
    {OP_vdiv_f64, 0xee800b20, "vdiv.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vdup_16,  0xee800b30, "vdup.16",  WAd, xx, RBh, xx, xx, vfp, x, END_LIST},
    {INVALID,     0xee800b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xee800b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xee800b60, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xee800b70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_vfnms_f64,0xee900b00, "vfnms.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {INVALID,     0xee900b10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnms_f64,0xee900b20, "vfnms.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_u16, 0xee900b30, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, END_LIST},
    {OP_vfnma_f64,0xee900b40, "vfnma.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {INVALID,     0xee900b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnma_f64,0xee900b60, "vfnma.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_u16, 0xee900b70, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, vfp, x, DUP_ENTRY},
  }, { /* 10 */
    {OP_vfma_f64, 0xeea00b00, "vfma.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {OP_vdup_32,  0xeea00b10, "vdup.32",  VAq, xx, RBd, xx, xx, vfp, x, xfpB[8][0x01]},
    {OP_vfma_f64, 0xeea00b20, "vfma.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vdup_16,  0xeea00b30, "vdup.16",  VAq, xx, RBh, xx, xx, vfp, x, xfpB[8][0x03]},
    {OP_vfms_f64, 0xeea00b40, "vfms.f64", VBq, xx, VAq, VCq, xx, vfp, x, END_LIST},
    {INVALID,     0xeea00b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f64, 0xeea00b60, "vfms.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xeea00b70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 11 */
    {OP_vdiv_f64, 0xeec00b00, "vdiv.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vdup_8,   0xeec00b10, "vdup.8",   WAd, xx, RBb, xx, xx, vfp, x, END_LIST},
    {OP_vdiv_f64, 0xeec00b20, "vdiv.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xeec00b30, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeec00b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeec00b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeec00b60, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeec00b70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 12 */
    {OP_vfnms_f64,0xeed00b00, "vfnms.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_u8,  0xeed00b10, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, END_LIST},
    {OP_vfnms_f64,0xeed00b20, "vfnms.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_u8,  0xeed00b30, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vfnma_f64,0xeed00b40, "vfnma.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_u8,  0xeed00b50, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
    {OP_vfnma_f64,0xeed00b60, "vfnma.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vmov_u8,  0xeed00b70, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, vfp, x, DUP_ENTRY},
  }, { /* 13 */
    {OP_vfma_f64, 0xeee00b00, "vfma.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vdup_8,   0xeee00b10, "vdup.8",   VAq, xx, RBb, xx, xx, vfp, x, xfpB[11][0x01]},
    {OP_vfma_f64, 0xeee00b20, "vfma.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xeee00b30, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f64, 0xeee00b40, "vfms.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xeee00b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f64, 0xeee00b60, "vfms.f64", VBq, xx, VAq, VCq, xx, vfp, x, DUP_ENTRY},
    {INVALID,     0xeee00b70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits 19:16 */
const instr_info_t T32_ext_bits16[][16] = {
  { /* 0 */
    {OP_vmov_f32,     0xeeb00a40, "vmov.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vneg_f32,     0xeeb10a40, "vneg.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvtb_f32_f16, 0xeeb20a40, "vcvtb.f32.f16", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvtb_f16_f32, 0xeeb30a40, "vcvtb.f16.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcmp_f32,     0xeeb40a40, "vcmp.f32", FPSCR, xx, WBd, WCd, xx, vfp, x, END_LIST},
    {OP_vcmp_f32,     0xeeb50a40, "vcmp.f32", FPSCR, xx, WBd, k0, xx, vfp, x, xbi16[0][0x04]},/*XXX: const is really fp, not int*/
    {OP_vrintr_f32,   0xeeb60a40, "vrintr.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vrintx_f32,   0xeeb70a40, "vrintx.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_f32_u32, 0xeeb80a40, "vcvt.f32.u32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {INVALID,         0xeeb90a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s16, 0xeeba0a40, "vcvt.f32.s16", WBd, xx, WCh, i5x0_5, xx, vfp, x, END_LIST},
    {OP_vcvt_f32_u16, 0xeebb0a40, "vcvt.f32.u16", WBd, xx, WCh, i5x0_5, xx, vfp, x, END_LIST},
    {OP_vcvtr_u32_f32,0xeebc0a40, "vcvtr.u32.f32",WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvtr_s32_f32,0xeebd0a40, "vcvtr.s32.f32",WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_s16_f32, 0xeebe0a40, "vcvt.s16.f32", WBh, xx, WCd, i5x0_5, xx, vfp, x, END_LIST},
    {OP_vcvt_u16_f32, 0xeebf0a40, "vcvt.u16.f32", WBh, xx, WCd, i5x0_5, xx, vfp, x, END_LIST},
  }, { /* 1 */
    {OP_vabs_f32,     0xeeb00ac0, "vabs.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vsqrt_f32,    0xeeb10ac0, "vsqrt.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvtt_f32_f16, 0xeeb20ac0, "vcvtt.f32.f16", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvtt_f16_f32, 0xeeb30ac0, "vcvtt.f16.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcmpe_f32,    0xeeb40ac0, "vcmpe.f32", FPSCR, xx, WBd, WCd, xx, vfp, x, END_LIST},
    {OP_vcmpe_f32,    0xeeb50ac0, "vcmpe.f32", FPSCR, xx, WBd, k0, xx, vfp, x, xbi16[1][0x04]},/*XXX: const is really fp, not int*/
    {OP_vrintz_f32,   0xeeb60ac0, "vrintz.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_f64_f32, 0xeeb70ac0, "vcvt.f64.f32", VBq, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_f32_s32, 0xeeb80ac0, "vcvt.f32.s32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {INVALID,         0xeeb90ac0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s32, 0xeeba0ac0, "vcvt.f32.s32", WBd, xx, WCd, i5x0_5, xx, vfp, x, xbi16[1][0x08]},
    {OP_vcvt_f32_u32, 0xeebb0ac0, "vcvt.f32.u32", WBd, xx, WCd, i5x0_5, xx, vfp, x, xbi16[0][0x08]},
    {OP_vcvt_u32_f32, 0xeebc0ac0, "vcvt.u32.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_s32_f32, 0xeebd0ac0, "vcvt.s32.f32", WBd, xx, WCd, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_s32_f32, 0xeebe0ac0, "vcvt.s32.f32", WBd, xx, WCd, i5x0_5, xx, vfp, x, xbi16[1][0x0d]},
    {OP_vcvt_u32_f32, 0xeebf0ac0, "vcvt.u32.f32", WBd, xx, WCd, i5x0_5, xx, vfp, x, xbi16[1][0x0c]},
  }, { /* 2 */
    {OP_vmov_f64,     0xeeb00b40, "vmov.f64", VBq, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vneg_f64,     0xeeb10b40, "vneg.f64", VBq, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vcvtb_f64_f16, 0xeeb20b40, "vcvtb.f64.f16", VBq, xx, WCd, xx, xx, vfp|v8, x, END_LIST},
    {OP_vcvtb_f16_f64, 0xeeb30b40, "vcvtb.f16.f64", WBd, xx, VCq, xx, xx, vfp|v8, x, END_LIST},
    {OP_vcmp_f64,     0xeeb40b40, "vcmp.f64", FPSCR, xx, VBq, VCq, xx, vfp, x, END_LIST},
    {OP_vcmp_f64,     0xeeb50b40, "vcmp.f64", FPSCR, xx, VBq, k0, xx, vfp, x, xbi16[2][0x04]},/*XXX: const is really fp, not int*/
    {OP_vrintr_f64,   0xeeb60b40, "vrintr.f64", VBq, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vrintx_f64,   0xeeb70b40, "vrintx.f64", VBq, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_f64_u32, 0xeeb80b40, "vcvt.f64.u32", VBq, xx, WCd, xx, xx, vfp, x, END_LIST},
    {INVALID,         0xeeb90b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s16, 0xeeba0b40, "vcvt.f64.s16", VBq, xx, WCh, i5x0_5, xx, vfp, x, END_LIST},
    {OP_vcvt_f64_u16, 0xeebb0b40, "vcvt.f64.u16", VBq, xx, WCh, i5x0_5, xx, vfp, x, END_LIST},
    {OP_vcvtr_u32_f64,0xeebc0b40, "vcvtr.u32.f64",WBd, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vcvtr_s32_f64,0xeebd0b40, "vcvtr.s32.f64",WBd, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_s16_f64, 0xeebe0b40, "vcvt.s16.f64", WBh, xx, VCq, i5x0_5, xx, vfp, x, END_LIST},
    {OP_vcvt_u16_f64, 0xeebf0b40, "vcvt.u16.f64", WBh, xx, VCq, i5x0_5, xx, vfp, x, END_LIST},
  }, { /* 3 */
    {OP_vabs_f64,     0xeeb00bc0, "vabs.f64", VBq, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vsqrt_f64,    0xeeb10bc0, "vsqrt.f64", VBq, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vcvtt_f64_f16, 0xeeb20bc0, "vcvtt.f64.f16", VBq, xx, WCd, xx, xx, vfp|v8, x, END_LIST},
    {OP_vcvtt_f16_f64, 0xeeb30bc0, "vcvtt.f16.f64", WBd, xx, VCq, xx, xx, vfp|v8, x, END_LIST},
    {OP_vcmpe_f64,    0xeeb40bc0, "vcmpe.f64", FPSCR, xx, VBq, VCq, xx, vfp, x, END_LIST},
    {OP_vcmpe_f64,    0xeeb50bc0, "vcmpe.f64", FPSCR, xx, VBq, k0, xx, vfp, x, xbi16[3][0x04]},/*XXX: const is really fp, not int*/
    {OP_vrintz_f64,   0xeeb60bc0, "vrintz.f64", VBq, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_f32_f64, 0xeeb70bc0, "vcvt.f32.f64", WBd, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_f64_s32, 0xeeb80bc0, "vcvt.f64.s32", VBq, xx, WCd, xx, xx, vfp, x, END_LIST},
    {INVALID,         0xeeb90bc0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s32, 0xeeba0bc0, "vcvt.f64.s32", VBq, xx, WCd, i5x0_5, xx, vfp, x, xbi16[3][0x08]},
    {OP_vcvt_f64_u32, 0xeebb0bc0, "vcvt.f64.u32", VBq, xx, WCd, i5x0_5, xx, vfp, x, xbi16[2][0x08]},
    {OP_vcvt_u32_f64, 0xeebc0bc0, "vcvt.u32.f64", WBd, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_s32_f64, 0xeebd0bc0, "vcvt.s32.f64", WBd, xx, VCq, xx, xx, vfp, x, END_LIST},
    {OP_vcvt_s32_f64, 0xeebe0bc0, "vcvt.s32.f64", WBd, xx, VCq, i5x0_5, xx, vfp, x, xbi16[3][0x0d]},
    {OP_vcvt_u32_f64, 0xeebf0bc0, "vcvt.u32.f64", WBd, xx, VCq, i5x0_5, xx, vfp, x, xbi16[3][0x0c]},
  }, { /* 4 */
    {OP_vmov_f32,     0xeef00a40, "vmov.f32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vneg_f32,     0xeef10a40, "vneg.f32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {INVALID,         0xeef20a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0xeef30a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmp_f32,     0xeef40a40, "vcmp.f32", FPSCR, xx, WBd, WCd, xx, vfp, x, DUP_ENTRY},
    {OP_vcmp_f32,     0xeef50a40, "vcmp.f32", FPSCR, xx, WBd, k0, xx, vfp, x, DUP_ENTRY},/*XXX: const is really fp, not int*/
    {OP_vrintr_f32,   0xeef60a40, "vrintr.f32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vrintx_f32,   0xeef70a40, "vrintx.f32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_u32, 0xeef80a40, "vcvt.f32.u32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {INVALID,         0xeef90a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s16, 0xeefa0a40, "vcvt.f32.s16", WBd, xx, WCh, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_u16, 0xeefb0a40, "vcvt.f32.u16", WBd, xx, WCh, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvtr_u32_f32,0xeefc0a40, "vcvtr.u32.f32",WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvtr_s32_f32,0xeefd0a40, "vcvtr.s32.f32",WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_s16_f32, 0xeefe0a40, "vcvt.s16.f32", WBh, xx, WCd, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_u16_f32, 0xeeff0a40, "vcvt.u16.f32", WBh, xx, WCd, i5x0_5, xx, vfp, x, DUP_ENTRY},
  }, { /* 5 */
    {OP_vabs_f32,     0xeef00ac0, "vabs.f32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vsqrt_f32,    0xeef10ac0, "vsqrt.f32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {INVALID,         0xeef20ac0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0xeef30ac0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmpe_f32,    0xeef40ac0, "vcmpe.f32", FPSCR, xx, WBd, WCd, xx, vfp, x, DUP_ENTRY},
    {OP_vcmpe_f32,    0xeef50ac0, "vcmpe.f32", FPSCR, xx, WBd, k0, xx, vfp, x, DUP_ENTRY},/*XXX: const is really fp, not int*/
    {OP_vrintz_f32,   0xeef60ac0, "vrintz.f32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_f32, 0xeef70ac0, "vcvt.f64.f32", VBq, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_s32, 0xeef80ac0, "vcvt.f32.s32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {INVALID,         0xeef90ac0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s32, 0xeefa0ac0, "vcvt.f32.s32", WBd, xx, WCd, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_u32, 0xeefb0ac0, "vcvt.f32.u32", WBd, xx, WCd, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_u32_f32, 0xeefc0ac0, "vcvt.u32.f32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_s32_f32, 0xeefd0ac0, "vcvt.s32.f32", WBd, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_s32_f32, 0xeefe0ac0, "vcvt.s32.f32", WBd, xx, WCd, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_u32_f32, 0xeeff0ac0, "vcvt.u32.f32", WBd, xx, WCd, i5x0_5, xx, vfp, x, DUP_ENTRY},
  }, { /* 6 */
    {OP_vmov_f64,     0xeef00b40, "vmov.f64", VBq, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vneg_f64,     0xeef10b40, "vneg.f64", VBq, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {INVALID,         0xeef20b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0xeef30b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmp_f64,     0xeef40b40, "vcmp.f64", FPSCR, xx, VBq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vcmp_f64,     0xeef50b40, "vcmp.f64", FPSCR, xx, VBq, k0, xx, vfp, x, DUP_ENTRY},/*XXX: const is really fp, not int*/
    {OP_vrintr_f64,   0xeef60b40, "vrintr.f64", VBq, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vrintx_f64,   0xeef70b40, "vrintx.f64", VBq, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_u32, 0xeef80b40, "vcvt.f64.u32", VBq, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {INVALID,         0xeef90b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s16, 0xeefa0b40, "vcvt.f64.s16", VBq, xx, WCh, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_u16, 0xeefb0b40, "vcvt.f64.u16", VBq, xx, WCh, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvtr_u32_f64,0xeefc0b40, "vcvtr.u32.f64",WBd, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvtr_s32_f64,0xeefd0b40, "vcvtr.s32.f64",WBd, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_s16_f64, 0xeefe0b40, "vcvt.s16.f64", WBh, xx, VCq, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_u16_f64, 0xeeff0b40, "vcvt.u16.f64", WBh, xx, VCq, i5x0_5, xx, vfp, x, DUP_ENTRY},
  }, { /* 7 */
    {OP_vabs_f64,     0xeef00bc0, "vabs.f64", VBq, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vsqrt_f64,    0xeef10bc0, "vsqrt.f64", VBq, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {INVALID,         0xeef20bc0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0xeef30bc0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmpe_f64,    0xeef40bc0, "vcmpe.f64", FPSCR, xx, VBq, VCq, xx, vfp, x, DUP_ENTRY},
    {OP_vcmpe_f64,    0xeef50bc0, "vcmpe.f64", FPSCR, xx, VBq, k0, xx, vfp, x, DUP_ENTRY},/*XXX: const is really fp, not int*/
    {OP_vrintz_f64,   0xeef60bc0, "vrintz.f64", VBq, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_f64, 0xeef70bc0, "vcvt.f32.f64", WBd, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_s32, 0xeef80bc0, "vcvt.f64.s32", VBq, xx, WCd, xx, xx, vfp, x, DUP_ENTRY},
    {INVALID,         0xeef90bc0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s32, 0xeefa0bc0, "vcvt.f64.s32", VBq, xx, WCd, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_u32, 0xeefb0bc0, "vcvt.f64.u32", VBq, xx, WCd, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_u32_f64, 0xeefc0bc0, "vcvt.u32.f64", WBd, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_s32_f64, 0xeefd0bc0, "vcvt.s32.f64", WBd, xx, VCq, xx, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_s32_f64, 0xeefe0bc0, "vcvt.s32.f64", WBd, xx, VCq, i5x0_5, xx, vfp, x, DUP_ENTRY},
    {OP_vcvt_u32_f64, 0xeeff0bc0, "vcvt.u32.f64", WBd, xx, VCq, i5x0_5, xx, vfp, x, DUP_ENTRY},
  }, { /* 8 */
    /* These assume bit4 is not set */
    {EXT_SIMD6B,      0xffb00000, "(ext simd6B 9)", xx, xx, xx, xx, xx, no, x, 9},
    {EXT_SIMD6B,      0xffb10000, "(ext simd6b 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_SIMD6B,      0xffb20000, "(ext simd6b 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_SIMD2,       0xffb30000, "(ext simd2  0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_SIMD6B,      0xffb40000, "(ext simd6b 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_SIMD6B,      0xffb50000, "(ext simd6b 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_SIMD6B,      0xffb60000, "(ext simd6b 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_SIMD2,       0xffb70000, "(ext simd2  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_SIMD6B,      0xffb80000, "(ext simd6b 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_SIMD6B,      0xffb90000, "(ext simd6b 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_SIMD6B,      0xffba0000, "(ext simd6b 7)", xx, xx, xx, xx, xx, no, x, 7},
    {EXT_SIMD6B,      0xffbb0000, "(ext simd6b 8)", xx, xx, xx, xx, xx, no, x, 8},
    {EXT_SIMD2,       0xffbc0000, "(ext simd2  2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_SIMD2,       0xffbd0000, "(ext simd2  3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_SIMD2,       0xffbe0000, "(ext simd2  4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_SIMD2,       0xffbf0000, "(ext simd2  5)", xx, xx, xx, xx, xx, no, x, 5},
  },
};

/* Indexed by bits 23:20 */
const instr_info_t T32_ext_bits20[][16] = {
  { /* 0 */
    {INVALID,    0xfc000000, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {INVALID,    0xfc100000, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {OP_stc2,    0xfc200000, "stc2",    Mw, RAw, i4_8, CRBw, n8x4, xop_wb, x, END_LIST},/*PUW=001*/
    {OP_ldc2,    0xfc300000, "ldc2",    CRBw, RAw, Mw, i4_8, n8x4, xop_wb, x, END_LIST},/*PUW=001*/
    {OP_mcrr2,   0xfc400000, "mcrr2",   CRDw, RAw, RBw, i4_8, i4_7, srcX4, x, END_LIST},
    {OP_mrrc2,   0xfc500000, "mrrc2",   RBw, RAw, i4_8, i4_7, CRDw, no, x, END_LIST},
    {OP_stc2l,   0xfc600000, "stc2l",   Mw, RAw, i4_8, CRBw, n8x4, xop_wb, x, END_LIST},/*PUW=001*/
    {OP_ldc2l,   0xfc700000, "ldc2l",   CRBw, RAw, Mw, i4_8, n8x4, xop_wb, x, END_LIST},/*PUW=001*/
    {OP_stc2,    0xfc800000, "stc2",    Mw, xx, i4_8, CRBw, i8, no, x, xbi20[0][0x02]},/*PUW=010*/
    {OP_ldc2,    0xfc900000, "ldc2",    CRBw, xx, Mw, i4_8, i8, no, x, xbi20[0][0x03]},/*PUW=010*/
    {OP_stc2,    0xfca00000, "stc2",    Mw, RAw, i4_8, CRBw, i8x4, xop_wb, x, xbi20[0][0x08]},/*PUW=011*/
    {OP_ldc2,    0xfcb00000, "ldc2",    CRBw, RAw, Mw, i4_8, i8x4, xop_wb, x, xbi20[0][0x09]},/*PUW=011*/
    {OP_stc2l,   0xfcc00000, "stc2l",   Mw, xx, i4_8, CRBw, i8, no, x, xbi20[0][0x06]},/*PUW=010*/
    {OP_ldc2l,   0xfcd00000, "ldc2l",   CRBw, xx, Mw, i4_8, i8, no, x, xbi20[0][0x07]},/*PUW=010*/
    {OP_stc2l,   0xfce00000, "stc2l",   Mw, RAw, i4_8, CRBw, i8x4, xop_wb, x, xbi20[0][0x0c]},/*PUW=011*/
    {OP_ldc2l,   0xfcf00000, "ldc2l",   CRBw, RAw, Mw, i4_8, i8x4, xop_wb, x, xbi20[0][0x0d]},/*PUW=011*/
  }, { /* 1 */
    {OP_stc2,    0xfd000000, "stc2",    MN8Xw, xx, i4_8, CRBw, n8x4, no, x, xbi20[1][0x0a]},/*PUW=100*/
    {OP_ldc2,    0xfd100000, "ldc2",    CRBw, xx, MN8Xw, i4_8, i8x4, no, x, xbi20[1][0x0b]},/*PUW=100*/
    {OP_stc2,    0xfd200000, "stc2",    MN8Xw, RAw, i4_8, CRBw, n8x4, xop_wb, x, xbi20[0][0x0a]},/*PUW=101*/
    {OP_ldc2,    0xfd300000, "ldc2",    CRBw, RAw, MN8Xw, i4_8, n8x4, xop_wb, x, xbi20[0][0x0b]},/*PUW=101*/
    {OP_stc2l,   0xfd400000, "stc2l",   MN8Xw, xx, i4_8, CRBw, n8x4, no, x, xbi20[1][0x0e]},/*PUW=100*/
    {OP_ldc2l,   0xfd500000, "ldc2l",   CRBw, xx, MN8Xw, i4_8, i8x4, no, x, xbi20[1][0x0f]},/*PUW=100*/
    {OP_stc2l,   0xfd600000, "stc2l",   MN8Xw, RAw, i4_8, CRBw, n8x4, xop_wb, x, xbi20[0][0x0e]},/*PUW=101*/
    {OP_ldc2l,   0xfd700000, "ldc2l",   CRBw, RAw, MN8Xw, i4_8, n8x4, xop_wb, x, xbi20[0][0x0f]},/*PUW=101*/
    {OP_stc2,    0xfd800000, "stc2",    MP8Xw, xx, i4_8, CRBw, i8x4, no, x, xbi20[1][0x00]},/*PUW=110*/
    {OP_ldc2,    0xfd900000, "ldc2",    CRBw, xx, MP8Xw, i4_8, i8x4, no, x, xbi20[1][0x01]},/*PUW=110*/
    {OP_stc2,    0xfda00000, "stc2",    MP8Xw, RAw, i4_8, CRBw, i8x4, xop_wb, x, xbi20[1][0x02]},/*PUW=111*/
    {OP_ldc2,    0xfdb00000, "ldc2",    CRBw, RAw, MP8Xw, i4_8, i8x4, xop_wb, x, xbi20[1][0x03]},/*PUW=111*/
    {OP_stc2l,   0xfdc00000, "stc2l",   MP8Xw, xx, i4_8, CRBw, i8x4, no, x, xbi20[1][0x04]},/*PUW=110*/
    {OP_ldc2l,   0xfdd00000, "ldc2l",   CRBw, xx, MP8Xw, i4_8, i8x4, no, x, xbi20[1][0x05]},/*PUW=110*/
    {OP_stc2l,   0xfde00000, "stc2l",   MP8Xw, RAw, i4_8, CRBw, i8x4, xop_wb, x, xbi20[1][0x06]},/*PUW=111*/
    {OP_ldc2l,   0xfdf00000, "ldc2l",   CRBw, RAw, MP8Xw, i4_8, i8x4, xop_wb, x, xbi20[1][0x07]},/*PUW=111*/
  },
};

/* Indexed by whether imm4 in 20:16 is zero or not */
const instr_info_t T32_ext_imm2016[][2] = {
  { /* 0 */
    {OP_vmovl_s32,      0xefa00a10, "vmovl.s32",      VBdq, xx, VCq, xx, xx, no, x, END_LIST},
    {OP_vshll_s32,      0xefa00a10, "vshll.s32",      VBdq, xx, VCq, i5_16, xx, no, x, END_LIST},/*20:16 cannot be 0*/
  }, { /* 1 */
    {OP_vmovl_u32,      0xffa00a10, "vmovl.u32",      VBdq, xx, VCq, xx, xx, no, x, END_LIST},
    {OP_vshll_u32,      0xffa00a10, "vshll.u32",      VBdq, xx, VCq, i5_16, xx, no, x, END_LIST},/*20:16 cannot be 0*/
  },
};

/* Indexed by whether imm4 in 18:16 is zero or not */
const instr_info_t T32_ext_imm1816[][2] = {
  { /* 0 */
    {OP_vmovl_s8,       0xef880a10, "vmovl.s8",       VBdq, xx, VCq, xx, xx, no, x, END_LIST},
    {OP_vshll_s8,       0xef880a10, "vshll.s8",       VBdq, xx, VCq, i3_16, xx, no, x, END_LIST},/*18:16 cannot be 0*/
  }, { /* 1 */
    {OP_vmovl_u8,       0xff880a10, "vmovl.u8",       VBdq, xx, VCq, xx, xx, no, x, END_LIST},
    {OP_vshll_u8,       0xff880a10, "vshll.u8",       VBdq, xx, VCq, i3_16, xx, no, x, END_LIST},/*18:16 cannot be 0*/
  },
};

/* Indexed by bit 6 */
const instr_info_t T32_ext_bit6[][2] = {
  { /* 0 */
    {OP_vext,           0xefb00000, "vext.8",         VBq, xx, VAq, VCq, i4_8, no, x, xb6[0][0x01]},/*XXX: reads from part of srcs, but complex which part*/
    {OP_vext,           0xefb00040, "vext.8",         VBdq, xx, VAdq, VCdq, i4_8, no, x, END_LIST},/*XXX: reads from part of srcs, but complex which part*/
  }, { /* 1 */
    {OP_vmaxnm_f32,     0xfe800a00, "vmaxnm.f32",     WBd, xx, WAd, WCd, xx, v8|vfp, x, END_LIST},
    {OP_vminnm_f32,     0xfe800a40, "vminnm.f32",     WBd, xx, WAd, WCd, xx, v8|vfp, x, END_LIST},
  }, { /* 2 */
    {OP_vmaxnm_f64,     0xfe800b00, "vmaxnm.f64",     VBq, xx, VAq, VCq, xx, v8|vfp, x, END_LIST},
    {OP_vminnm_f64,     0xfe800b40, "vminnm.f64",     VBq, xx, VAq, VCq, xx, v8|vfp, x, END_LIST},
  },
};

/* Indexed by bit 19.  This up-front split is simpler than having to split
 * 37+ entries inside T32_ext_simd5[] into 2-entry members of this table.
 */
const instr_info_t T32_ext_bit19[][2] = {
  { /* 0 */
    {EXT_SIMD8,         0xef800000, "(ext simd8  0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_SIMD5,         0xef880000, "(ext simd5  0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 1 */
    {EXT_SIMD8,         0xef800000, "(ext simd8  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_SIMD5,         0xff880000, "(ext simd5  1)", xx, xx, xx, xx, xx, no, x, 1},
  },
};

/* Indexed by 6 bits 11:8,6,4 (thus: a-f | 0,1,4,5) */
const instr_info_t T32_ext_simd6[][64] = {
  { /* 0 */
    {OP_vhadd_s8,       0xef000000, "vhadd.s8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x02]},
    {OP_vqadd_s8,       0xef000010, "vqadd.s8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x03]},
    {OP_vhadd_s8,       0xef000040, "vhadd.s8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqadd_s8,       0xef000050, "vqadd.s8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrhadd_s8,      0xef000100, "vrhadd.s8",      VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x06]},
    {OP_vand,           0xef000110, "vand",           VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x07]},
    {OP_vrhadd_s8,      0xef000140, "vrhadd.s8",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vand,           0xef000150, "vand",           VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vhsub_s8,       0xef000200, "vhsub.s8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x0a]},
    {OP_vqsub_s8,       0xef000210, "vqsub.s8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x0b]},
    {OP_vhsub_s8,       0xef000240, "vhsub.s8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqsub_s8,       0xef000250, "vqsub.s8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcgt_s8,        0xef000300, "vcgt.s8",        VBq, xx, VAq, VCq, xx, no, x, xsi6b[0][0x01]},
    {OP_vcge_s8,        0xef000310, "vcge.s8",        VBq, xx, VAq, VCq, xx, no, x, xsi6b[0][0x03]},
    {OP_vcgt_s8,        0xef000340, "vcgt.s8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcge_s8,        0xef000350, "vcge.s8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vshl_s8,        0xef000400, "vshl.s8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x12]},
    {OP_vqshl_s8,       0xef000410, "vqshl.s8",       VBq, xx, VAq, VCq, xx, no, x, xsi5[0][0x0f]},
    {OP_vshl_s8,        0xef000440, "vshl.s8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshl_s8,       0xef000450, "vqshl.s8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrshl_s8,       0xef000500, "vrshl.s8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x16]},
    {OP_vqrshl_s8,      0xef000510, "vqrshl.s8",      VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x17]},
    {OP_vrshl_s8,       0xef000540, "vrshl.s8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqrshl_s8,      0xef000550, "vqrshl.s8",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmax_s8,        0xef000600, "vmax.s8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x1a]},
    {OP_vmin_s8,        0xef000610, "vmin.s8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x1b]},
    {OP_vmax_s8,        0xef000640, "vmax.s8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmin_s8,        0xef000650, "vmin.s8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vabd_s8,        0xef000700, "vabd.s8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x1e]},
    {OP_vaba_s8,        0xef000710, "vaba.s8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x1f]},
    {OP_vabd_s8,        0xef000740, "vabd.s8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vaba_s8,        0xef000750, "vaba.s8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    /* 0x80 */
    {OP_vadd_i8,        0xef000800, "vadd.i8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x22]},
    {OP_vtst_8,         0xef000810, "vtst.8",         VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x23]},
    {OP_vadd_i8,        0xef000840, "vadd.i8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vtst_8,         0xef000850, "vtst.8",         VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmla_i8,        0xef000900, "vmla.i8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x26]},
    {OP_vmul_i8,        0xef000910, "vmul.i8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x27]},
    {OP_vmla_i8,        0xef000940, "vmla.i8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmul_i8,        0xef000950, "vmul.i8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vpmax_s8,       0xef000a00, "vpmax.s8",       VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vpmin_s8,       0xef000a10, "vpmin.s8",       VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {INVALID,           0xef000a40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef000a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef000b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpadd_i8,       0xef000b10, "vpadd.i8",       VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {INVALID,           0xef000b40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef000b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef000c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfma_f32,       0xef000c10, "vfma.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x33]},
    {OP_sha1c_32,       0xef000c40, "sha1c.32",       VBdq, xx, VAdq, VCdq, xx, v8, x, END_LIST},
    {OP_vfma_f32,       0xef000c50, "vfma.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, xfpA[10][0x00]},
    {OP_vadd_f32,       0xef000d00, "vadd.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x36]},
    {OP_vmla_f32,       0xef000d10, "vmla.f32",       VBq, xx, VAq, VCq, xx, v8, x, xsi6[0][0x37]},
    {OP_vadd_f32,       0xef000d40, "vadd.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, xfpA[3][0x00]},
    {OP_vmla_f32,       0xef000d50, "vmla.f32",       VBdq, xx, VAdq, VCdq, xx, v8, x, xfpA[0][0x00]},
    {OP_vceq_f32,       0xef000e00, "vceq.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6b[6][0x15]},
    {INVALID,           0xef000e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vceq_f32,       0xef000e40, "vceq.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef000e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmax_f32,       0xef000f00, "vmax.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x3e]},
    {OP_vrecps_f32,     0xef000f10, "vrecps.f32",     VBq, xx, VAq, VCq, xx, no, x, xsi6[0][0x3f]},
    {OP_vmax_f32,       0xef000f40, "vmax.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrecps_f32,     0xef000f50, "vrecps.f32",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
  }, { /* 1 */
    {OP_vhadd_s16,      0xef100000, "vhadd.s16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x02]},
    {OP_vqadd_s16,      0xef100010, "vqadd.s16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x03]},
    {OP_vhadd_s16,      0xef100040, "vhadd.s16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqadd_s16,      0xef100050, "vqadd.s16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrhadd_s16,     0xef100100, "vrhadd.s16",     VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x06]},
    {OP_vbic,           0xef100110, "vbic",           VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x07]},
    {OP_vrhadd_s16,     0xef100140, "vrhadd.s16",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vbic,           0xef100150, "vbic",           VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vhsub_s16,      0xef100200, "vhsub.s16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x0a]},
    {OP_vqsub_s16,      0xef100210, "vqsub.s16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x0b]},
    {OP_vhsub_s16,      0xef100240, "vhsub.s16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqsub_s16,      0xef100250, "vqsub.s16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcgt_s16,       0xef100300, "vcgt.s16",       VBq, xx, VAq, VCq, xx, no, x, xsi6b[3][0x01]},
    {OP_vcge_s16,       0xef100310, "vcge.s16",       VBq, xx, VAq, VCq, xx, no, x, xsi6b[3][0x03]},
    {OP_vcgt_s16,       0xef100340, "vcgt.s16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcge_s16,       0xef100350, "vcge.s16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vshl_s16,       0xef100400, "vshl.s16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x12]},
    {OP_vqshl_s16,      0xef100410, "vqshl.s16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[4][0x1f]},
    {OP_vshl_s16,       0xef100440, "vshl.s16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshl_s16,      0xef100450, "vqshl.s16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrshl_s16,      0xef100500, "vrshl.s16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x16]},
    {OP_vqrshl_s16,     0xef100510, "vqrshl.s16",     VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x17]},
    {OP_vrshl_s16,      0xef100540, "vrshl.s16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqrshl_s16,     0xef100550, "vqrshl.s16",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmax_s16,       0xef100600, "vmax.s16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x1a]},
    {OP_vmin_s16,       0xef100610, "vmin.s16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x1b]},
    {OP_vmax_s16,       0xef100640, "vmax.s16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmin_s16,       0xef100650, "vmin.s16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vabd_s16,       0xef100700, "vabd.s16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x1e]},
    {OP_vaba_s16,       0xef100710, "vaba.s16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x1f]},
    {OP_vabd_s16,       0xef100740, "vabd.s16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vaba_s16,       0xef100750, "vaba.s16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    /* 0x80 */
    {OP_vadd_i16,       0xef100800, "vadd.i16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x22]},
    {OP_vtst_16,        0xef100810, "vtst.16",        VBq, xx, VAq, VCq, xx, no, x, xsi6[1][0x23]},
    {OP_vadd_i16,       0xef100840, "vadd.i16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vtst_16,        0xef100850, "vtst.16",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmla_i16,       0xef100900, "vmla.i16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[10][0x02]},
    {OP_vmul_i16,       0xef100910, "vmul.i16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[10][0x22]},
    {OP_vmla_i16,       0xef100940, "vmla.i16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmul_i16,       0xef100950, "vmul.i16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vpmax_s16,      0xef100a00, "vpmax.s16",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vpmin_s16,      0xef100a10, "vpmin.s16",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {INVALID,           0xef100a40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmulh_s16,    0xef100b00, "vqdmulh.s16",    VBq, xx, VAq, VCq, xx, no, x, xsi6[10][0x32]},
    {OP_vpadd_i16,      0xef100b10, "vpadd.i16",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vqdmulh_s16,    0xef100b40, "vqdmulh.s16",    VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef100b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_sha1p_32,       0xef100c40, "sha1p.32",       VBdq, xx, VAdq, VCdq, xx, v8, x, END_LIST},
    {INVALID,           0xef100c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100d40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100f10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef100f50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 2 */
    {OP_vhadd_s32,      0xef200000, "vhadd.s32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x02]},
    {OP_vqadd_s32,      0xef200010, "vqadd.s32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x03]},
    {OP_vhadd_s32,      0xef200040, "vhadd.s32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqadd_s32,      0xef200050, "vqadd.s32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrhadd_s32,     0xef200100, "vrhadd.s32",     VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x06]},
    {OP_vorr,           0xef200110, "vorr",           VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x07]},/*XXX: if src1==src2 then "vmov"*/
    {OP_vrhadd_s32,     0xef200140, "vrhadd.s32",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vorr,           0xef200150, "vorr",           VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},/*XXX: if src1==src2 then "vmov"*/
    {OP_vhsub_s32,      0xef200200, "vhsub.s32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x0a]},
    {OP_vqsub_s32,      0xef200210, "vqsub.s32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x0b]},
    {OP_vhsub_s32,      0xef200240, "vhsub.s32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqsub_s32,      0xef200250, "vqsub.s32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcgt_s32,       0xef200300, "vcgt.s32",       VBq, xx, VAq, VCq, xx, no, x, xsi6b[6][0x01]},
    {OP_vcge_s32,       0xef200310, "vcge.s32",       VBq, xx, VAq, VCq, xx, no, x, xsi6b[6][0x03]},
    {OP_vcgt_s32,       0xef200340, "vcgt.s32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcge_s32,       0xef200350, "vcge.s32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vshl_s32,       0xef200400, "vshl.s32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x12]},
    {OP_vqshl_s32,      0xef200410, "vqshl.s32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[5][0x1f]},
    {OP_vshl_s32,       0xef200440, "vshl.s32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshl_s32,      0xef200450, "vqshl.s32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrshl_s32,      0xef200500, "vrshl.s32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x16]},
    {OP_vqrshl_s32,     0xef200510, "vqrshl.s32",     VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x17]},
    {OP_vrshl_s32,      0xef200540, "vrshl.s32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqrshl_s32,     0xef200550, "vqrshl.s32",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmax_s32,       0xef200600, "vmax.s32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x1a]},
    {OP_vmin_s32,       0xef200610, "vmin.s32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x1b]},
    {OP_vmax_s32,       0xef200640, "vmax.s32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmin_s32,       0xef200650, "vmin.s32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vabd_s32,       0xef200700, "vabd.s32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x1e]},
    {OP_vaba_s32,       0xef200710, "vaba.s32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x1f]},
    {OP_vabd_s32,       0xef200740, "vabd.s32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vaba_s32,       0xef200750, "vaba.s32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vadd_i32,       0xef200800, "vadd.i32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x22]},
    {OP_vtst_32,        0xef200810, "vtst.32",        VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x23]},
    {OP_vadd_i32,       0xef200840, "vadd.i32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vtst_32,        0xef200850, "vtst.32",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmla_i32,       0xef200900, "vmla.i32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[11][0x02]},
    {OP_vmul_i32,       0xef200910, "vmul.i32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[11][0x22]},
    {OP_vmla_i32,       0xef200940, "vmla.i32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmul_i32,       0xef200950, "vmul.i32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vpmax_s32,      0xef200a00, "vpmax.s32",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vpmin_s32,      0xef200a10, "vpmin.s32",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {INVALID,           0xef200a40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef200a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmulh_s32,    0xef200b00, "vqdmulh.s32",    VBq, xx, VAq, VCq, xx, no, x, xsi6[11][0x32]},
    {OP_vpadd_i32,      0xef200b10, "vpadd.i32",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vqdmulh_s32,    0xef200b40, "vqdmulh.s32",    VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef200b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef200c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f32,       0xef200c10, "vfms.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x33]},
    {OP_sha1m_32,       0xef200c40, "sha1m.32",       VBdq, xx, VAdq, VCdq, xx, v8, x, END_LIST},
    {OP_vfms_f32,       0xef200c50, "vfms.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, xfpA[10][0x02]},
    {OP_vsub_f32,       0xef200d00, "vsub.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x36]},
    {OP_vmls_f32,       0xef200d10, "vmls.f32",       VBq, xx, VAq, VCq, xx, v8, x, xsi6[2][0x37]},
    {OP_vsub_f32,       0xef200d40, "vsub.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, xfpA[3][0x02]},
    {OP_vmls_f32,       0xef200d50, "vmls.f32",       VBdq, xx, VAdq, VCdq, xx, v8, x, xfpA[0][0x02]},
    {INVALID,           0xef200e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef200e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef200e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef200e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmin_f32,       0xef200f00, "vmin.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x3e]},
    {OP_vrsqrts_f32,    0xef200f10, "vrsqrts.f32",    VBq, xx, VAq, VCq, xx, no, x, xsi6[2][0x3f]},
    {OP_vmin_f32,       0xef200f40, "vmin.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrsqrts_f32,    0xef200f50, "vrsqrts.f32",    VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
  }, { /* 3 */
    /* XXX: this entry is sparse: should we make a new table to somehow compress it? */
    {INVALID,           0xef300000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqadd_s64,      0xef300010, "vqadd.s64",      VBq, xx, VAq, VCq, xx, no, x, xsi6[3][0x03]},
    {INVALID,           0xef300040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqadd_s64,      0xef300050, "vqadd.s64",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef300100, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vorn,           0xef300110, "vorn",           VBq, xx, VAq, VCq, xx, no, x, xsi6[3][0x07]},
    {INVALID,           0xef300140, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vorn,           0xef300150, "vorn",           VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef300200, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqsub_s64,      0xef300210, "vqsub.s64",      VBq, xx, VAq, VCq, xx, no, x, xsi6[3][0x0b]},
    {INVALID,           0xef300240, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqsub_s64,      0xef300250, "vqsub.s64",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef300300, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300310, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300340, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300350, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vshl_s64,       0xef300400, "vshl.s64",       VBq, xx, VAq, VCq, xx, no, x, xsi6[3][0x12]},
    {OP_vqshl_s64,      0xef300410, "vqshl.s64",      VBq, xx, VAq, VCq, xx, no, x, xi6l[0][0x0f]},
    {OP_vshl_s64,       0xef300440, "vshl.s64",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshl_s64,      0xef300450, "vqshl.s64",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrshl_s64,      0xef300500, "vrshl.s64",      VBq, xx, VAq, VCq, xx, no, x, xsi6[3][0x16]},
    {OP_vqrshl_s64,     0xef300510, "vqrshl.s64",     VBq, xx, VAq, VCq, xx, no, x, xsi6[3][0x17]},
    {OP_vrshl_s64,      0xef300540, "vrshl.s64",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqrshl_s64,     0xef300550, "vqrshl.s64",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef300600, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300610, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300640, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300650, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300700, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300710, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300740, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300750, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    /* 0x80 */
    {OP_vadd_i64,       0xef300800, "vadd.i64",       VBq, xx, VAq, VCq, xx, no, x, xsi6[3][0x22]},
    {INVALID,           0xef300810, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vadd_i64,       0xef300840, "vadd.i64",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef300850, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300900, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300910, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300940, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300950, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300a00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300a10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300a40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300b40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_sha1su0_32,     0xef300c40, "sha1su0.32",     VBdq, xx, VAdq, VCdq, xx, v8, x, END_LIST},
    {INVALID,           0xef300c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300d40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300f10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef300f50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 4 */
    {OP_vaddl_s16,      0xef900000, "vaddl.s16",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vshr_s16,       0xef900010, "vshr.s16",       VBq, xx, VCq, i4_16, xx, no, x, xsi6[4][0x03]},/*XXX: imm = 16-imm*/
    {OP_vmla_i16,       0xef900040, "vmla.i16",       VBq, xx, VAq, VC3h_q, i2x5_3, no, x, xsi6[1][0x24]},
    {OP_vshr_s16,       0xef900050, "vshr.s16",       VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vaddw_s16,      0xef900100, "vaddw.s16",      VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vsra_s16,       0xef900110, "vsra.s16",       VBq, xx, VCq, i4_16, xx, no, x, xsi6[4][0x07]},/*XXX: imm = 16-imm*/
    {INVALID,           0xef900140, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsra_s16,       0xef900150, "vsra.s16",       VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vsubl_s16,      0xef900200, "vsubl.s16",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vrshr_s16,      0xef900210, "vrshr.s16",      VBq, xx, VCq, i4_16, xx, no, x, xsi6[4][0x0b]},/*XXX: imm = 16-imm*/
    {OP_vmlal_s16,      0xef900240, "vmlal.s16",      VBdq, xx, VAq, VC3h_q, i2x5_3, no, x, END_LIST},
    {OP_vrshr_s16,      0xef900250, "vrshr.s16",      VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vsubw_s16,      0xef900300, "vsubw.s16",      VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vrsra_s16,      0xef900310, "vrsra.s16",      VBq, xx, VCq, i4_16, xx, no, x, xsi6[4][0x0f]},/*XXX: imm = 16-imm*/
    {OP_vqdmlal_s16,    0xef900340, "vqdmlal.s16",    VBdq, xx, VAq, VC3h_q, i2x5_3, no, x, END_LIST},
    {OP_vrsra_s16,      0xef900350, "vrsra.s16",      VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vaddhn_i32,     0xef900400, "vaddhn.i32",     VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef900410, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmls_i16,       0xef900440, "vmls.i16",       VBq, xx, VAq, VC3h_q, i2x5_3, no, x, xsi6[10][0x12]},
    {INVALID,           0xef900450, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vabal_s16,      0xef900500, "vabal.s16",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vshl_i16,       0xef900510, "vshl.i16",       VBq, xx, VCq, i4_16, xx, no, x, xsi6[4][0x17]},/*XXX: imm = 16-imm?*/
    {INVALID,           0xef900540, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vshl_i16,       0xef900550, "vshl.i16",       VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm?*/
    {OP_vsubhn_i32,     0xef900600, "vsubhn.i32",     VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xef900610, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmlsl_s16,      0xef900640, "vmlsl.s16",      VBdq, xx, VAq, VC3h_q, i2x5_3, no, x, END_LIST},
    {INVALID,           0xef900650, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vabdl_s16,      0xef900700, "vabdl.s16",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vqshl_s16,      0xef900710, "vqshl.s16",      VBq, xx, VCq, i4_16, xx, no, x, xsi6[1][0x11]},/*XXX: imm = imm-16*/
    {OP_vqdmlsl_s16,    0xef900740, "vqdmlsl.s16",    VBdq, xx, VAq, VC3h_q, i2x5_3, no, x, END_LIST},
    {OP_vqshl_s16,      0xef900750, "vqshl.s16",      VBdq, xx, VCdq, i4_16, xx, no, x, xsi6[1][0x13]},/*XXX: imm = imm-16*/
    /* 0x80 */
    {OP_vmlal_s16,      0xef900800, "vmlal.s16",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[4][0x0a]},
    {OP_vshrn_i32,      0xef900810, "vshrn.i32",      VBq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vmul_i16,       0xef900840, "vmul.i16",       VBq, xx, VAq, VC3h_q, i2x5_3, no, x, xsi6[1][0x25]},
    {OP_vrshrn_i32,     0xef900850, "vrshrn.i32",     VBq, xx, VCq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vqdmlal_s16,    0xef900900, "vqdmlal.s16",    VBdq, xx, VAq, VCq, xx, no, x, xsi6[4][0x0e]},
    {OP_vqshrn_s32,     0xef900910, "vqshrn.s32",     VBq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {INVALID,           0xef900940, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrshrn_s32,    0xef900950, "vqrshrn.s32",    VBq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vmlsl_s16,      0xef900a00, "vmlsl.s16",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[4][0x1a]},
    {EXT_IMM1916,       0xef900a10, "(ext imm1916 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_vmull_s16,      0xef900a40, "vmull.s16",      VBdq, xx, VAq, VC3h_q, i2x5_3, no, x, END_LIST},
    {INVALID,           0xef900a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmlsl_s16,    0xef900b00, "vqdmlsl.s16",    VBdq, xx, VAq, VCq, xx, no, x, xsi6[4][0x1e]},
    {INVALID,           0xef900b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmull_s16,    0xef900b40, "vqdmull.s16",    VBdq, xx, VAq, VC3h_q, i2x5_3, no, x, END_LIST},
    {INVALID,           0xef900b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmull_s16,      0xef900c00, "vmull.s16",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[4][0x2a]},
    {INVALID,           0xef900c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmulh_s16,    0xef900c40, "vqdmulh.s16",    VBq, xx, VAq, VC3h_q, i2x5_3, no, x, xsi6[1][0x2c]},
    {INVALID,           0xef900c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmull_s16,    0xef900d00, "vqdmull.s16",    VBdq, xx, VAq, VCq, xx, no, x, xsi6[4][0x2e]},
    {INVALID,           0xef900d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrdmulh_s16,   0xef900d40, "vqrdmulh.s16",   VBq, xx, VAq, VC3h_q, i2x5_3, no, x, xsi6[10][0x36]},
    {INVALID,           0xef900d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef900e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef900e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef900e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef900e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef900f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef900f10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef900f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef900f50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 5 */
    {OP_vaddl_s32,      0xefa00000, "vaddl.s32",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vshr_s32,       0xefa00010, "vshr.s32",       VBq, xx, VCq, i5_16, xx, no, x, xsi6[5][0x03]},/*XXX: imm = 32-imm*/
    {OP_vmla_i32,       0xefa00040, "vmla.i32",       VBq, xx, VAq, VC4d_q, i1_5, no, x, xsi6[2][0x24]},
    {OP_vshr_s32,       0xefa00050, "vshr.s32",       VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vaddw_s32,      0xefa00100, "vaddw.s32",      VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vsra_s32,       0xefa00110, "vsra.s32",       VBq, xx, VCq, i5_16, xx, no, x, xsi6[5][0x07]},/*XXX: imm = 32-imm*/
    {OP_vmla_f32,       0xefa00140, "vmla.f32",       VBq, xx, VAq, VC4d_q, i1_5, no, x, xsi6[11][0x06]},
    {OP_vsra_s32,       0xefa00150, "vsra.s32",       VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vsubl_s32,      0xefa00200, "vsubl.s32",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vrshr_s32,      0xefa00210, "vrshr.s32",      VBq, xx, VCq, i5_16, xx, no, x, xsi6[5][0x0b]},/*XXX: imm = 32-imm*/
    {OP_vmlal_s32,      0xefa00240, "vmlal.s32",      VBdq, xx, VAq, VC4d_q, i1_5, no, x, END_LIST},
    {OP_vrshr_s32,      0xefa00250, "vrshr.s32",      VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vsubw_s32,      0xefa00300, "vsubw.s32",      VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vrsra_s32,      0xefa00310, "vrsra.s32",      VBq, xx, VCq, i5_16, xx, no, x, xsi6[5][0x0f]},/*XXX: imm = 32-imm*/
    {OP_vqdmlal_s32,    0xefa00340, "vqdmlal.s32",    VBdq, xx, VAq, VC4d_q, i1_5, no, x, END_LIST},
    {OP_vrsra_s32,      0xefa00350, "vrsra.s32",      VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vaddhn_i64,     0xefa00400, "vaddhn.i64",     VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xefa00410, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmls_i32,       0xefa00440, "vmls.i32",       VBq, xx, VAq, VC4d_q, i1_5, no, x, xsi6[11][0x12]},
    {INVALID,           0xefa00450, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vabal_s32,      0xefa00500, "vabal.s32",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vshl_i32,       0xefa00510, "vshl.i32",       VBq, xx, VCq, i5_16, xx, no, x, xsi6[5][0x17]},/*XXX: imm = 32-imm?*/
    {OP_vmls_f32,       0xefa00540, "vmls.f32",       VBq, xx, VAq, VC4d_q, i1_5, no, x, xsi6[11][0x16]},
    {OP_vshl_i32,       0xefa00550, "vshl.i32",       VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm?*/
    {OP_vsubhn_i64,     0xefa00600, "vsubhn.i64",     VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xefa00610, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmlsl_s32,      0xefa00640, "vmlsl.s32",      VBdq, xx, VAq, VC4d_q, i1_5, no, x, END_LIST},
    {INVALID,           0xefa00650, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vabdl_s32,      0xefa00700, "vabdl.s32",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vqshl_s32,      0xefa00710, "vqshl.s32",      VBq, xx, VCq, i5_16, xx, no, x, xsi6[2][0x11]},/*XXX: imm = imm-32*/
    {OP_vqdmlsl_s32,    0xefa00740, "vqdmlsl.s32",    VBdq, xx, VAq, VC4d_q, i1_5, no, x, END_LIST},
    {OP_vqshl_s32,      0xefa00750, "vqshl.s32",      VBdq, xx, VCdq, i5_16, xx, no, x, xsi6[2][0x13]},/*XXX: imm = imm-32*/
    /* 0x80 */
    {OP_vmlal_s32,      0xefa00800, "vmlal.s32",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[5][0x0a]},
    {OP_vshrn_i64,      0xefa00810, "vshrn.i64",      VBq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vmul_i32,       0xefa00840, "vmul.i32",       VBq, xx, VAq, VC4d_q, i1_5, no, x, xsi6[2][0x25]},
    {OP_vrshrn_i64,     0xefa00850, "vrshrn.i64",     VBq, xx, VCq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vqdmlal_s32,    0xefa00900, "vqdmlal.s32",    VBdq, xx, VAq, VCq, xx, no, x, xsi6[5][0x0e]},
    {OP_vqshrn_s64,     0xefa00910, "vqshrn.s64",     VBq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vmul_f32,       0xefa00940, "vmul.f32",       VBq, xx, VAq, VC4d_q, i1_5, no, x, xsi6[11][0x26]},
    {OP_vqrshrn_s64,    0xefa00950, "vqrshrn.s64",    VBq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vmlsl_s32,      0xefa00a00, "vmlsl.s32",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[5][0x1a]},
    {EXT_IMM2016,       0xefa00a10, "(ext imm2016 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_vmull_s32,      0xefa00a40, "vmull.s32",      VBdq, xx, VAq, VC4d_q, i1_5, no, x, END_LIST},
    {INVALID,           0xefa00a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmlsl_s32,    0xefa00b00, "vqdmlsl.s32",    VBdq, xx, VAq, VCq, xx, no, x, xsi6[5][0x1e]},
    {INVALID,           0xefa00b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmull_s32,    0xefa00b40, "vqdmull.s32",    VBdq, xx, VAq, VC4d_q, i1_5, no, x, END_LIST},
    {INVALID,           0xefa00b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmull_s32,      0xefa00c00, "vmull.s32",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[5][0x2a]},
    {INVALID,           0xefa00c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmulh_s32,    0xefa00c40, "vqdmulh.s32",    VBq, xx, VAq, VC4d_q, i1_5, no, x, xsi6[2][0x2c]},
    {INVALID,           0xefa00c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmull_s32,    0xefa00d00, "vqdmull.s32",    VBdq, xx, VAq, VCq, xx, no, x, xsi6[5][0x2e]},
    {INVALID,           0xefa00d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrdmulh_s32,   0xefa00d40, "vqrdmulh.s32",   VBq, xx, VAq, VC4d_q, i1_5, no, x, xsi6[11][0x36]},
    {INVALID,           0xefa00d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmull_p32,      0xefa00e00, "vmull.p32",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vcvt_f32_s32,   0xefa00e10, "vcvt.f32.s32",   VBq, xx, VCq, i6_16, xx, no, x, xsi6b[8][0x19]},
    {INVALID,           0xefa00e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s32,   0xefa00e50, "vcvt.f32.s32",   VBdq, xx, VCdq, i6_16, xx, no, x, xbi16[1][0x0a]},
    {OP_vcvt_s32_f32,   0xefa00f10, "vcvt.s32.f32",   VBq, xx, VCq, i6_16, xx, no, x, xsi6b[8][0x1d]},
    {INVALID,           0xefa00f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_s32_f32,   0xefa00f50, "vcvt.s32.f32",   VBdq, xx, VCdq, i6_16, xx, no, x, xbi16[1][0x0e]},
  }, { /* 6 */
    {OP_vhadd_u8,       0xff000000, "vhadd.u8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x02]},
    {OP_vqadd_u8,       0xff000010, "vqadd.u8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x03]},
    {OP_vhadd_u8,       0xff000040, "vhadd.u8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqadd_u8,       0xff000050, "vqadd.u8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrhadd_u8,      0xff000100, "vrhadd.u8",      VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x06]},
    {OP_veor,           0xff000110, "veor",           VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x07]},
    {OP_vrhadd_u8,      0xff000140, "vrhadd.u8",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_veor,           0xff000150, "veor",           VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vhsub_u8,       0xff000200, "vhsub.u8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x0a]},
    {OP_vqsub_u8,       0xef000210, "vqsub.u8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x0b]},
    {OP_vhsub_u8,       0xff000240, "vhsub.u8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqsub_u8,       0xef000250, "vqsub.u8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcgt_u8,        0xff000300, "vcgt.u8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x0e]},
    {OP_vcge_u8,        0xff000310, "vcge.u8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x0f]},
    {OP_vcgt_u8,        0xff000340, "vcgt.u8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcge_u8,        0xff000350, "vcge.u8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vshl_u8,        0xff000400, "vshl.u8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x12]},
    {OP_vqshl_u8,       0xff000410, "vqshl.u8",       VBq, xx, VAq, VCq, xx, no, x, xsi5[1][0x0f]},
    {OP_vshl_u8,        0xff000440, "vshl.u8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshl_u8,       0xff000450, "vqshl.u8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrshl_u8,       0xff000500, "vrshl.u8",       VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x16]},
    {OP_vqrshl_u8,      0xff000510, "vqrshl.u8",      VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x17]},
    {OP_vrshl_u8,       0xff000540, "vrshl.u8",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqrshl_u8,      0xff000550, "vqrshl.u8",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmax_u8,        0xff000600, "vmax.u8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x1a]},
    {OP_vmin_u8,        0xff000610, "vmin.u8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x1b]},
    {OP_vmax_u8,        0xff000640, "vmax.u8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmin_u8,        0xff000650, "vmin.u8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vabd_u8,        0xff000700, "vabd.u8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x1e]},
    {OP_vaba_u8,        0xff000710, "vaba.u8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x1f]},
    {OP_vabd_u8,        0xff000740, "vabd.u8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vaba_u8,        0xff000750, "vaba.u8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    /* 0x80 */
    {OP_vsub_i8,        0xff000800, "vsub.i8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x22]},
    {OP_vceq_i8,        0xff000810, "vceq.i8",        VBq, xx, VAq, VCq, xx, no, x, xsi6b[0][0x05]},
    {OP_vsub_i8,        0xff000840, "vsub.i8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vceq_i8,        0xff000850, "vceq.i8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmls_i8,        0xff000900, "vmls.i8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x26]},
    {OP_vmul_p8,        0xff000910, "vmul.p8",        VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x27]},
    {OP_vmls_i8,        0xff000940, "vmls.i8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmul_p8,        0xff000950, "vmul.p8",        VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vpmax_u8,       0xff000a00, "vpmax.u8",       VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vpmin_u8,       0xff000a10, "vpmin.u8",       VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {INVALID,           0xff000a40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff000a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff000b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff000b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff000b40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff000b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff000c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff000c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_sha256h_32,     0xff000c40, "sha256h.32",     VBdq, xx, VAdq, VCdq, xx, v8, x, END_LIST},
    {INVALID,           0xff000c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpadd_f32,      0xff000d00, "vpadd.f32",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmul_f32,       0xff000d10, "vmul.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[5][0x26]},
    {INVALID,           0xff000d40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmul_f32,       0xff000d50, "vmul.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, xfpA[2][0x00]},
    {OP_vcge_f32,       0xff000e00, "vcge.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6b[6][0x13]},
    {OP_vacge_f32,      0xff000e10, "vacge.f32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[6][0x3b]},
    {OP_vcge_f32,       0xff000e40, "vcge.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vacge_f32,      0xff000e50, "vacge.f32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vpmax_f32,      0xff000f00, "vpmax.f32",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmaxnm_f32,     0xff000f10, "vmaxnm.f32",     VBq, xx, VAq, VCq, xx, v8, x, xsi6[6][0x3f]},
    {INVALID,           0xff000f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaxnm_f32,     0xff000f50, "vmaxnm.f32",     VBdq, xx, VAdq, VCdq, xx, v8, x, xb6[1][0x00]},
  }, { /* 7 */
    {OP_vhadd_u16,      0xff100000, "vhadd.u16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x02]},
    {OP_vqadd_u16,      0xff100010, "vqadd.u16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x03]},
    {OP_vhadd_u16,      0xff100040, "vhadd.u16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqadd_u16,      0xff100050, "vqadd.u16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrhadd_u16,     0xff100100, "vrhadd.u16",     VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x06]},
    {OP_vbsl,           0xff100110, "vbsl",           VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x07]},
    {OP_vrhadd_u16,     0xff100140, "vrhadd.u16",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vbsl,           0xff100150, "vbsl",           VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vhsub_u16,      0xff100200, "vhsub.u16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x0a]},
    {OP_vqsub_u16,      0xff100210, "vqsub.u16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x0b]},
    {OP_vhsub_u16,      0xff100240, "vhsub.u16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqsub_u16,      0xff100250, "vqsub.u16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcgt_u16,       0xff100300, "vcgt.u16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x0e]},
    {OP_vcge_u16,       0xff100310, "vcge.u16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x0f]},
    {OP_vcgt_u16,       0xff100340, "vcgt.u16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcge_u16,       0xff100350, "vcge.u16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vshl_u16,       0xff100400, "vshl.u16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x12]},
    {OP_vqshl_u16,      0xff100410, "vqshl.u16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[10][0x1f]},
    {OP_vshl_u16,       0xff100440, "vshl.u16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshl_u16,      0xff100450, "vqshl.u16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrshl_u16,      0xff100500, "vrshl.u16",      VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x16]},
    {OP_vqrshl_u16,     0xff100510, "vqrshl.u16",     VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x17]},
    {OP_vrshl_u16,      0xff100540, "vrshl.u16",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqrshl_u16,     0xff100550, "vqrshl.u16",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmax_u16,       0xff100600, "vmax.u16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x1a]},
    {OP_vmin_u16,       0xff100610, "vmin.u16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x1b]},
    {OP_vmax_u16,       0xff100640, "vmax.u16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmin_u16,       0xff100650, "vmin.u16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vabd_u16,       0xff100700, "vabd.u16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x1e]},
    {OP_vaba_u16,       0xff100710, "vaba.u16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x1f]},
    {OP_vabd_u16,       0xff100740, "vabd.u16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vaba_u16,       0xff100750, "vaba.u16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vsub_i16,       0xff100800, "vsub.i16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[7][0x22]},
    {OP_vceq_i16,       0xff100810, "vceq.i16",       VBq, xx, VAq, VCq, xx, no, x, xsi6b[3][0x05]},
    {OP_vsub_i16,       0xff100840, "vsub.i16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vceq_i16,       0xff100850, "vceq.i16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmls_i16,       0xff100900, "vmls.i16",       VBq, xx, VAq, VCq, xx, no, x, xsi6[4][0x12]},
    {INVALID,           0xff100910, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmls_i16,       0xff100940, "vmls.i16",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xff100950, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmax_u16,      0xff100a00, "vpmax.u16",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vpmin_u16,      0xff100a10, "vpmin.u16",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {INVALID,           0xff100a40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrdmulh_s16,   0xff100b00, "vqrdmulh.s16",   VBq, xx, VAq, VCq, xx, no, x, xsi6[4][0x36]},
    {INVALID,           0xff100b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrdmulh_s16,   0xff100b40, "vqrdmulh.s16",   VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xff100b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_sha256h2_32,    0xff100c40, "sha256h2.32",    VBdq, xx, VAdq, VCdq, xx, v8, x, END_LIST},
    {INVALID,           0xff100c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100d40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100f10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff100f50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 8 */
    {OP_vhadd_u32,      0xff200000, "vhadd.u32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x02]},
    {OP_vqadd_u32,      0xff200010, "vqadd.u32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x03]},
    {OP_vhadd_u32,      0xff200040, "vhadd.u32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqadd_u32,      0xff200050, "vqadd.u32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrhadd_u32,     0xff200100, "vrhadd.u32",     VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x06]},
    {OP_vbit,           0xff200110, "vbit",           VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x07]},
    {OP_vrhadd_u32,     0xff200140, "vrhadd.u32",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vbit,           0xff200150, "vbit",           VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vhsub_u32,      0xff200200, "vhsub.u32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x0a]},
    {OP_vqsub_u32,      0xff200210, "vqsub.u32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x0b]},
    {OP_vhsub_u32,      0xff200240, "vhsub.u32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqsub_u32,      0xff200250, "vqsub.u32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcgt_u32,       0xff200300, "vcgt.u32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x0e]},
    {OP_vcge_u32,       0xff200310, "vcge.u32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x0f]},
    {OP_vcgt_u32,       0xff200340, "vcgt.u32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vcge_u32,       0xff200350, "vcge.u32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vshl_u32,       0xff200400, "vshl.u32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x12]},
    {OP_vqshl_u32,      0xff200410, "vqshl.u32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[11][0x1f]},
    {OP_vshl_u32,       0xff200440, "vshl.u32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshl_u32,      0xff200450, "vqshl.u32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrshl_u32,      0xff200500, "vrshl.u32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x16]},
    {OP_vqrshl_u32,     0xff200510, "vqrshl.u32",     VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x17]},
    {OP_vrshl_u32,      0xff200540, "vrshl.u32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqrshl_u32,     0xff200550, "vqrshl.u32",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmax_u32,       0xff200600, "vmax.u32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x1a]},
    {OP_vmin_u32,       0xff200610, "vmin.u32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x1b]},
    {OP_vmax_u32,       0xff200640, "vmax.u32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmin_u32,       0xff200650, "vmin.u32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vabd_u32,       0xff200700, "vabd.u32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x1e]},
    {OP_vaba_u32,       0xff200710, "vaba.u32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x1f]},
    {OP_vabd_u32,       0xff200740, "vabd.u32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vaba_u32,       0xff200750, "vaba.u32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    /* 0x80 */
    {OP_vsub_i32,       0xff200800, "vsub.i32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x22]},
    {OP_vceq_i32,       0xff200810, "vceq.i32",       VBq, xx, VAq, VCq, xx, no, x, xsi6b[6][0x05]},
    {OP_vsub_i32,       0xff200840, "vsub.i32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vceq_i32,       0xff200850, "vceq.i32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmls_i32,       0xff200900, "vmls.i32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[5][0x12]},
    {OP_vmul_p32,       0xff200910, "vmul.p32",       VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x27]},
    {OP_vmls_i32,       0xff200940, "vmls.i32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmul_p32,       0xff200950, "vmul.p32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vpmax_u32,      0xff200a00, "vpmax.u32",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vpmin_u32,      0xff200a10, "vpmin.u32",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {INVALID,           0xff200a40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff200a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrdmulh_s32,   0xff200b00, "vqrdmulh.s32",   VBq, xx, VAq, VCq, xx, no, x, xsi6[5][0x36]},
    {INVALID,           0xff200b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrdmulh_s32,   0xff200b40, "vqrdmulh.s32",   VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xff200b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff200c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff200c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_sha256su1_32,   0xff200c40, "sha256su1.32",   VBdq, xx, VAdq, VCdq, xx, v8, x, END_LIST},
    {INVALID,           0xff200c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff200d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff200d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff200d40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff200d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcgt_f32,       0xff200e00, "vcgt.f32",       VBq, xx, VAq, VCq, xx, no, x, xsi6b[6][0x11]},
    {OP_vacgt_f32,      0xff200e10, "vacgt.f32",      VBq, xx, VAq, VCq, xx, no, x, xsi6[8][0x3b]},
    {OP_vcgt_f32,       0xff200e40, "vcgt.f32",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vacgt_f32,      0xff200e50, "vacgt.f32",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vpmin_f32,      0xff200f00, "vpmin.f32",      VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vminnm_f32,     0xff200f10, "vminnm.f32",     VBq, xx, VAq, VCq, xx, v8, x, xsi6[8][0x3f]},
    {INVALID,           0xff200f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vminnm_f32,     0xff200f50, "vminnm.f32",     VBdq, xx, VAdq, VCdq, xx, v8, x, xb6[1][0x01]},
  }, { /* 9 */
    /* XXX: this entry is sparse: should we make a new table to somehow compress it? */
    {INVALID,           0xff300000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqadd_u64,      0xff300010, "vqadd.u64",      VBq, xx, VAq, VCq, xx, no, x, xsi6[9][0x03]},
    {INVALID,           0xff300040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqadd_u64,      0xff300050, "vqadd.u64",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xff300100, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbif,           0xff300110, "vbif",           VBq, xx, VAq, VCq, xx, no, x, xsi6[9][0x07]},
    {INVALID,           0xff300140, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbif,           0xff300150, "vbif",           VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xff300200, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqsub_u64,      0xff300210, "vqsub.u64",      VBq, xx, VAq, VCq, xx, no, x, xsi6[9][0x0b]},
    {INVALID,           0xff300240, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqsub_u64,      0xff300250, "vqsub.u64",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xff300300, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300310, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300340, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300350, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vshl_u64,       0xff300400, "vshl.u64",       VBq, xx, VAq, VCq, xx, no, x, xsi6[9][0x12]},
    {OP_vqshl_u64,      0xff300410, "vqshl.u64",      VBq, xx, VAq, VCq, xx, no, x, xi6l[1][0x0f]},
    {OP_vshl_u64,       0xff300440, "vshl.u64",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshl_u64,      0xff300450, "vqshl.u64",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vrshl_u64,      0xff300500, "vrshl.u64",      VBq, xx, VAq, VCq, xx, no, x, xsi6[9][0x16]},
    {OP_vqrshl_u64,     0xff300510, "vqrshl.u64",     VBq, xx, VAq, VCq, xx, no, x, xsi6[9][0x17]},
    {OP_vrshl_u64,      0xff300540, "vrshl.u64",      VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqrshl_u64,     0xff300550, "vqrshl.u64",     VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xff300600, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300610, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300640, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300650, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300700, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300710, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300740, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300750, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    /* 0x80 */
    {OP_vsub_i64,       0xff300800, "vsub.i64",       VBq, xx, VAq, VCq, xx, no, x, xsi6[9][0x22]},
    {INVALID,           0xff300810, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsub_i64,       0xff300840, "vsub.i64",       VBdq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {INVALID,           0xff300850, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300900, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300910, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300940, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300950, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300a00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300a10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300a40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300b40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300c40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300d40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300f10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff300f50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 10 */
    {OP_vaddl_u16,      0xff900000, "vaddl.u16",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vshr_u16,       0xff900010, "vshr.u16",       VBq, xx, VCq, i4_16, xx, no, x, xsi6[10][0x03]},/*XXX: imm = 16-imm*/
    {OP_vmla_i16,       0xff900040, "vmla.i16",       VBdq, xx, VAdq, VC3h_q, i2x5_3, no, x, xsi6[1][0x26]},
    {OP_vshr_u16,       0xff900050, "vshr.u16",       VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vaddw_u16,      0xff900100, "vaddw.u16",      VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vsra_u16,       0xff900110, "vsra.u16",       VBq, xx, VCq, i4_16, xx, no, x, xsi6[10][0x07]},/*XXX: imm = 16-imm*/
    {INVALID,           0xff900140, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsra_u16,       0xff900150, "vsra.u16",       VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vsubl_u16,      0xff900200, "vsubl.u16",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vrshr_u16,      0xff900210, "vrshr.u16",      VBq, xx, VCq, i4_16, xx, no, x, xsi6[10][0x0b]},/*XXX: imm = 16-imm*/
    {OP_vmlal_u16,      0xff900240, "vmlal.u16",      VBdq, xx, VAq, VC3h_q, i2x5_3, no, x, END_LIST},
    {OP_vrshr_u16,      0xff900250, "vrshr.u16",      VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vsubw_u16,      0xff900300, "vsubw.u16",      VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vrsra_u16,      0xff900310, "vrsra.u16",      VBq, xx, VCq, i4_16, xx, no, x, xsi6[10][0x0f]},/*XXX: imm = 16-imm*/
    {INVALID,           0xff900340, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrsra_u16,      0xff900350, "vrsra.u16",      VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vraddhn_i32,    0xff900400, "vraddhn.i32",    VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vsri_16,        0xff900410, "vsri.16",        VBq, xx, VCq, i4_16, xx, no, x, xsi6[10][0x13]},/*XXX: imm = 16-imm?*/
    {OP_vmls_i16,       0xff900440, "vmls.i16",       VBdq, xx, VAdq, VC3h_q, i2x5_3, no, x, xsi6[7][0x26]},
    {OP_vsri_16,        0xff900450, "vsri.16",        VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm?*/
    {OP_vabal_u16,      0xff900500, "vabal.u16",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vsli_16,        0xff900510, "vsli.16",        VBq, xx, VCq, i4_16, xx, no, x, xsi6[10][0x17]},/*XXX: imm = 16-imm?*/
    {INVALID,           0xff900540, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsli_16,        0xff900550, "vsli.16",        VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm?*/
    {OP_vrsubhn_i32,    0xff900600, "vrsubhn.i32",    VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshlu_s16,     0xff900610, "vqshlu.s16",     VBq, xx, VCq, i4_16, xx, no, x, xsi6[10][0x1b]},/*XXX: imm = imm-16*/
    {OP_vmlsl_u16,      0xff900640, "vmlsl.u16",      VBdq, xx, VAq, VC3h_q, i2x5_3, no, x, END_LIST},
    {OP_vqshlu_s16,     0xff900650, "vqshlu.s16",     VBdq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = imm-16*/
    {OP_vabdl_u16,      0xff900700, "vabdl.u16",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vqshl_u16,      0xff900710, "vqshl.u16",      VBq, xx, VCq, i4_16, xx, no, x, xsi6[7][0x11]},/*XXX: imm = imm-16*/
    {INVALID,           0xff900740, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqshl_u16,      0xff900750, "vqshl.u16",      VBdq, xx, VCdq, i4_16, xx, no, x, xsi6[7][0x13]},/*XXX: imm = imm-16*/
    /* 0x80 */
    {OP_vmlal_u16,      0xff900800, "vmlal.u16",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[10][0x0a]},
    {OP_vqshrun_s32,    0xff900810, "vqshrun.s32",    VBq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vmul_i16,       0xff900840, "vmul.i16",       VBdq, xx, VAdq, VC3h_q, i2x5_3, no, x, xsi6[1][0x27]},
    {OP_vqrshrun_s32,   0xff900850, "vqrshrun.s32",   VBq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {INVALID,           0xff900900, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqshrn_u32,     0xff900910, "vqshrn.u32",     VBq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {INVALID,           0xff900940, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrshrn_u32,    0xff900950, "vqrshrn.u32",    VBq, xx, VCdq, i4_16, xx, no, x, END_LIST},/*XXX: imm = 16-imm*/
    {OP_vmlsl_u16,      0xff900a00, "vmlsl.u16",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[10][0x1a]},
    {EXT_IMM1916,       0xff900a10, "(ext imm1916 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_vmull_u16,      0xff900a40, "vmull.u16",      VBdq, xx, VAq, VC3h_q, i2x5_3, no, x, END_LIST},
    {INVALID,           0xff900a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900b40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmull_u16,      0xff900c00, "vmull.u16",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[10][0x2a]},
    {INVALID,           0xff900c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmulh_s16,    0xff900c40, "vqdmulh.s16",    VBdq, xx, VAdq, VC3h_q, i2x5_3, no, x, xsi6[1][0x2e]},
    {INVALID,           0xff900c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrdmulh_s16,   0xff900d40, "vqrdmulh.s16",   VBdq, xx, VAdq, VC3h_q, i2x5_3, no, x, xsi6[7][0x2e]},
    {INVALID,           0xff900d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900f10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff900f50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 11 */
    /* 0xffb with bit 4 set will also come here */
    {OP_vaddl_u32,      0xffa00000, "vaddl.u32",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vshr_u32,       0xffa00010, "vshr.u32",       VBq, xx, VCq, i5_16, xx, no, x, xsi6[11][0x03]},/*XXX: imm = 32-imm*/
    {OP_vmla_i32,       0xffa00040, "vmla.i32",       VBdq, xx, VAdq, VC4d_q, i1_5, no, x, xsi6[2][0x26]},
    {OP_vshr_u32,       0xffa00050, "vshr.u32",       VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vaddw_u32,      0xffa00100, "vaddw.u32",      VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vsra_u32,       0xffa00110, "vsra.u32",       VBq, xx, VCq, i5_16, xx, no, x, xsi6[11][0x07]},/*XXX: imm = 32-imm*/
    {OP_vmla_f32,       0xffa00140, "vmla.f32",       VBdq, xx, VAdq, VC4d_q, i1_5, no, x, xsi6[0][0x35]},
    {OP_vsra_u32,       0xffa00150, "vsra.u32",       VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vsubl_u32,      0xffa00200, "vsubl.u32",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vrshr_u32,      0xffa00210, "vrshr.u32",      VBq, xx, VCq, i5_16, xx, no, x, xsi6[11][0x0b]},/*XXX: imm = 32-imm*/
    {OP_vmlal_u32,      0xffa00240, "vmlal.u32",      VBdq, xx, VAq, VC4d_q, i1_5, no, x, END_LIST},
    {OP_vrshr_u32,      0xffa00250, "vrshr.u32",      VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vsubw_u32,      0xffa00300, "vsubw.u32",      VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vrsra_u32,      0xffa00310, "vrsra.u32",      VBq, xx, VCq, i5_16, xx, no, x, xsi6[11][0x0f]},/*XXX: imm = 32-imm*/
    {INVALID,           0xffa00340, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrsra_u32,      0xffa00350, "vrsra.u32",      VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vraddhn_i64,    0xffa00400, "vraddhn.i64",    VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vsri_32,        0xffa00410, "vsri.32",        VBq, xx, VCq, i5_16, xx, no, x, xsi6[11][0x13]},/*XXX: imm = 32-imm?*/
    {OP_vmls_i32,       0xffa00440, "vmls.i32",       VBdq, xx, VAdq, VC4d_q, i1_5, no, x, xsi6[8][0x26]},
    {OP_vsri_32,        0xffa00450, "vsri.32",        VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm?*/
    {OP_vabal_u32,      0xffa00500, "vabal.u32",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vsli_32,        0xffa00510, "vsli.32",        VBq, xx, VCq, i5_16, xx, no, x, xsi6[11][0x17]},/*XXX: imm = 32-imm?*/
    {OP_vmls_f32,       0xffa00540, "vmls.f32",       VBdq, xx, VAdq, VC4d_q, i1_5, no, x, xsi6[2][0x35]},
    {OP_vsli_32,        0xffa00550, "vsli.32",        VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm?*/
    {OP_vrsubhn_i64,    0xffa00600, "vrsubhn.i64",    VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vqshlu_s32,     0xffa00610, "vqshlu.s32",     VBq, xx, VCq, i5_16, xx, no, x, xsi6[11][0x1b]},/*XXX: imm = imm-32*/
    {OP_vmlsl_u32,      0xffa00640, "vmlsl.u32",      VBdq, xx, VAq, VC4d_q, i1_5, no, x, END_LIST},
    {OP_vqshlu_s32,     0xffa00650, "vqshlu.s32",     VBdq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = imm-32*/
    {OP_vabdl_u32,      0xffa00700, "vabdl.u32",      VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vqshl_u32,      0xffa00710, "vqshl.u32",      VBq, xx, VCq, i5_16, xx, no, x, xsi6[8][0x11]},/*XXX: imm = imm-32*/
    {INVALID,           0xffa00740, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqshl_u32,      0xffa00750, "vqshl.u32",      VBdq, xx, VCdq, i5_16, xx, no, x, xsi6[8][0x13]},/*XXX: imm = imm-32*/
    /* 0x80 */
    {OP_vmlal_u32,      0xffa00800, "vmlal.u32",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[11][0x0a]},
    {OP_vqshrun_s64,    0xffa00810, "vqshrun.s64",    VBq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vmul_i32,       0xffa00840, "vmul.i32",       VBdq, xx, VAdq, VC4d_q, i1_5, no, x, xsi6[2][0x27]},
    {OP_vqrshrun_s64,   0xffa00850, "vqrshrun.s64",   VBq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {INVALID,           0xffa00900, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqshrn_u64,     0xffa00910, "vqshrn.u64",     VBq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vmul_f32,       0xffa00940, "vmul.f32",       VBdq, xx, VAdq, VC4d_q, i1_5, no, x, xsi6[6][0x37]},
    {OP_vqrshrn_u64,    0xffa00950, "vqrshrn.u64",    VBq, xx, VCdq, i5_16, xx, no, x, END_LIST},/*XXX: imm = 32-imm*/
    {OP_vmlsl_u32,      0xffa00a00, "vmlsl.u32",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[11][0x1a]},
    {EXT_IMM2016,       0xffa00a10, "(ext imm2016 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_vmull_u32,      0xffa00a40, "vmull.u32",      VBdq, xx, VAq, VC4d_q, i1_5, no, x, END_LIST},
    {INVALID,           0xffa00a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffa00b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffa00b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffa00b40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffa00b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmull_u32,      0xffa00c00, "vmull.u32",      VBdq, xx, VAq, VCq, xx, no, x, xsi6[11][0x2a]},
    {INVALID,           0xffa00c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqdmulh_s32,    0xffa00c40, "vqdmulh.s32",    VBdq, xx, VAdq, VC4d_q, i1_5, no, x, xsi6[2][0x2e]},
    {INVALID,           0xffa00c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffa00d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffa00d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqrdmulh_s32,   0xffa00d40, "vqrdmulh.s32",   VBdq, xx, VAdq, VC4d_q, i1_5, no, x, xsi6[8][0x2e]},
    {INVALID,           0xffa00d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffa00e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_u32,   0xffa00e10, "vcvt.f32.u32",   VBq, xx, VCq, i6_16, xx, no, x, xsi6b[8][0x1b]},
    {INVALID,           0xffa00e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_u32,   0xffa00e50, "vcvt.f32.u32",   VBdq, xx, VCdq, i6_16, xx, no, x, xbi16[1][0x0b]},
    {INVALID,           0xffa00f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_u32_f32,   0xffa00f10, "vcvt.u32.f32",   VBq, xx, VCq, i6_16, xx, no, x, xsi6b[8][0x1f]},
    {INVALID,           0xffa00f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_u32_f32,   0xffa00f50, "vcvt.u32.f32",   VBdq, xx, VCdq, i6_16, xx, no, x, xbi16[1][0x0f]},
  },
};

/* Indexed by bits 11:8,6 */
const instr_info_t T32_ext_simd5[][32] = {
  { /* 0 */
    {OP_vshr_s8,        0xef880010, "vshr.s8",        VBq, xx, VCq, i3_16, xx, no, x, xsi5[0][0x01]},/*XXX: imm = 8-imm*/
    {OP_vshr_s8,        0xef880050, "vshr.s8",        VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vsra_s8,        0xef880110, "vsra.s8",        VBq, xx, VCq, i3_16, xx, no, x, xsi5[0][0x03]},/*XXX: imm = 8-imm*/
    {OP_vsra_s8,        0xef880150, "vsra.s8",        VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vrshr_s8,       0xef880210, "vrshr.s8",       VBq, xx, VCq, i3_16, xx, no, x, xsi5[0][0x05]},/*XXX: imm = 8-imm*/
    {OP_vrshr_s8,       0xef880250, "vrshr.s8",       VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vrsra_s8,       0xef880310, "vrsra.s8",       VBq, xx, VCq, i3_16, xx, no, x, xsi5[0][0x07]},/*XXX: imm = 8-imm*/
    {OP_vrsra_s8,       0xef880350, "vrsra.s8",       VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {INVALID,           0xef880410, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880450, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vshl_i8,        0xef880510, "vshl.i8",        VBq, xx, VCq, i3_16, xx, no, x, xsi5[0][0x0b]},/*XXX: imm = 8-imm?*/
    {OP_vshl_i8,        0xef880550, "vshl.i8",        VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm?*/
    {INVALID,           0xef880610, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880650, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqshl_s8,       0xef880710, "vqshl.s8",       VBq, xx, VCq, i3_16, xx, no, x, xsi6[0][0x11]},/*XXX: imm = imm-8*/
    {OP_vqshl_s8,       0xef880750, "vqshl.s8",       VBdq, xx, VCdq, i3_16, xx, no, x, xsi6[0][0x13]},/*XXX: imm = imm-8*/
    {OP_vshrn_i16,      0xef880810, "vshrn.i16",      VBq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vrshrn_i16,     0xef880850, "vrshrn.i16",     VBq, xx, VCq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vqshrn_s16,     0xef880910, "vqshrn.s16",     VBq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vqrshrn_s16,    0xef880950, "vqrshrn.s16",    VBq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {EXT_IMM1816,       0xef880a10, "(ext imm1816 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,           0xef880a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880f10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef880f50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 1 */
    {OP_vshr_u8,        0xff880010, "vshr.u8",        VBq, xx, VCq, i3_16, xx, no, x, xsi5[1][0x01]},/*XXX: imm = 8-imm*/
    {OP_vshr_u8,        0xff880050, "vshr.u8",        VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vsra_u8,        0xff880110, "vsra.u8",        VBq, xx, VCq, i3_16, xx, no, x, xsi5[1][0x03]},/*XXX: imm = 8-imm*/
    {OP_vsra_u8,        0xff880150, "vsra.u8",        VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vrshr_u8,       0xff880210, "vrshr.u8",       VBq, xx, VCq, i3_16, xx, no, x, xsi5[1][0x05]},/*XXX: imm = 8-imm*/
    {OP_vrshr_u8,       0xff880250, "vrshr.u8",       VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vrsra_u8,       0xff880310, "vrsra.u8",       VBq, xx, VCq, i3_16, xx, no, x, xsi5[1][0x07]},/*XXX: imm = 8-imm*/
    {OP_vrsra_u8,       0xff880350, "vrsra.u8",       VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vsri_8,         0xff880410, "vsri.8",         VBq, xx, VCq, i3_16, xx, no, x, xsi5[1][0x09]},/*XXX: imm = 8-imm?*/
    {OP_vsri_8,         0xff880450, "vsri.8",         VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm?*/
    {OP_vsli_8,         0xff880510, "vsli.8",         VBq, xx, VCq, i3_16, xx, no, x, xsi5[1][0x0b]},/*XXX: imm = 8-imm?*/
    {OP_vsli_8,         0xff880550, "vsli.8",         VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm?*/
    {OP_vqshlu_s8,      0xff880610, "vqshlu.s8",      VBq, xx, VCq, i3_16, xx, no, x, xsi5[1][0x0d]},/*XXX: imm = imm-8*/
    {OP_vqshlu_s8,      0xff880650, "vqshlu.s8",      VBdq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = imm-8*/
    {OP_vqshl_u8,       0xff880710, "vqshl.u8",       VBq, xx, VCq, i3_16, xx, no, x, xsi6[6][0x11]},/*XXX: imm = imm-8*/
    {OP_vqshl_u8,       0xff880750, "vqshl.u8",       VBdq, xx, VCdq, i3_16, xx, no, x, xsi6[6][0x13]},/*XXX: imm = imm-8*/
    {OP_vqshrun_s16,    0xff880810, "vqshrun.s16",    VBq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vqrshrun_s16,   0xff880850, "vqrshrun.s16",   VBq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vqshrn_u16,     0xff880910, "vqshrn.u16",     VBq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {OP_vqrshrn_u16,    0xff880950, "vqrshrn.u16",    VBq, xx, VCdq, i3_16, xx, no, x, END_LIST},/*XXX: imm = 8-imm*/
    {EXT_IMM1816,       0xff880a10, "(ext imm1816 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,           0xff880a50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880b10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880b50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880c50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880d10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880d50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880e50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880f10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xff880f50, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits 18:16,8:7 */
const instr_info_t T32_ext_simd5b[][32] = {
  { /* 0 */
    {OP_vrinta_f32_f32, 0xfeb80a40, "vrinta.f32.f32", WBd, xx, WCd, xx, xx, v8, x, END_LIST},
    {INVALID,           0xfeb80ac0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrinta_f64_f64, 0xfeb80b40, "vrinta.f64.f64", VBq, xx, VCq, xx, xx, v8, x, END_LIST},
    {INVALID,           0xfeb80bc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintn_f32_f32, 0xfeb90a40, "vrintn.f32.f32", WBd, xx, WCd, xx, xx, v8, x, END_LIST},
    {INVALID,           0xfeb90ac0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintn_f64_f64, 0xfeb90b40, "vrintn.f64.f64", VBq, xx, VCq, xx, xx, v8, x, END_LIST},
    {INVALID,           0xfeb90bc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintp_f32_f32, 0xfeba0a40, "vrintp.f32.f32", WBd, xx, WCd, xx, xx, v8, x, END_LIST},
    {INVALID,           0xfeba0ac0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintp_f64_f64, 0xfeba0b40, "vrintp.f64.f64", VBq, xx, VCq, xx, xx, v8, x, END_LIST},
    {INVALID,           0xfeba0bc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintm_f32_f32, 0xfebb0a40, "vrintm.f32.f32", WBd, xx, WCd, xx, xx, v8, x, END_LIST},
    {INVALID,           0xfebb0ac0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintm_f64_f64, 0xfebb0b40, "vrintm.f64.f64", VBq, xx, VCq, xx, xx, v8, x, END_LIST},
    {INVALID,           0xfebb0bc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvta_u32_f32,  0xfebc0a40, "vcvta.u32.f32",  WBd, xx, WCd, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvta_s32_f32,  0xfebc0ac0, "vcvta.s32.f32",  WBd, xx, WCd, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvta_u32_f64,  0xfebc0b40, "vcvta.u32.f64",  WBd, xx, VCq, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvta_s32_f64,  0xfebc0bc0, "vcvta.s32.f64",  WBd, xx, VCq, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtn_u32_f32,  0xfebd0a40, "vcvtn.u32.f32",  WBd, xx, WCd, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtn_s32_f32,  0xfebd0ac0, "vcvtn.s32.f32",  WBd, xx, WCd, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtn_u32_f64,  0xfebd0b40, "vcvtn.u32.f64",  WBd, xx, VCq, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtn_s32_f64,  0xfebd0bc0, "vcvtn.s32.f64",  WBd, xx, VCq, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtp_u32_f32,  0xfebe0a40, "vcvtp.u32.f32",  WBd, xx, WCd, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtp_s32_f32,  0xfebe0ac0, "vcvtp.s32.f32",  WBd, xx, WCd, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtp_u32_f64,  0xfebe0b40, "vcvtp.u32.f64",  WBd, xx, VCq, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtp_s32_f64,  0xfebe0bc0, "vcvtp.s32.f64",  WBd, xx, VCq, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtm_u32_f32,  0xfebf0a40, "vcvtm.u32.f32",  WBd, xx, WCd, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtm_s32_f32,  0xfebf0ac0, "vcvtm.s32.f32",  WBd, xx, WCd, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtm_u32_f64,  0xfebf0b40, "vcvtm.u32.f64",  WBd, xx, VCq, xx, xx, v8|vfp, x, END_LIST},
    {OP_vcvtm_s32_f64,  0xfebf0bc0, "vcvtm.s32.f64",  WBd, xx, VCq, xx, xx, v8|vfp, x, END_LIST},
  },
};

/* Indexed by bits 11:8,6:4, but 6:4 are in the following manner:
 * + If bit 4 == 0, offset is 0;
 * + Else, offset is 1 + bits 6:5.
 * (Thus, 0 followed by odds < 8).
 */
const instr_info_t T32_ext_simd8[][80] = {
  { /* 0 */
    {OP_vaddl_s8,       0xef800000, "vaddl.s8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800010, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x03]},
    {OP_vmvn_i32,       0xef800030, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x04]},
    {OP_vmov_i32,       0xef800050, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    {OP_vmvn_i32,       0xef800070, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    /* 0x10 */
    {OP_vaddw_s8,       0xef800100, "vaddw.s8",       VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vorr_i32,       0xef800110, "vorr.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x08]},
    {OP_vbic_i32,       0xef800130, "vbic.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x09]},
    {OP_vorr_i32,       0xef800150, "vorr.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    {OP_vbic_i32,       0xef800170, "vbic.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    /* 0x20 */
    {OP_vsubl_s8,       0xef800200, "vsubl.s8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800210, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800230, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800250, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800270, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x30 */
    {OP_vsubw_s8,       0xef800300, "vsubw.s8",       VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vorr_i32,       0xef800310, "vorr.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800330, "vbic.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i32,       0xef800350, "vorr.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800370, "vbic.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x40 */
    {OP_vaddhn_i16,     0xef800400, "vaddhn.i16",     VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800410, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800430, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800450, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800470, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x50 */
    {OP_vabal_s8,       0xef800500, "vabal.s8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vorr_i32,       0xef800510, "vorr.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800530, "vbic.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i32,       0xef800550, "vorr.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800570, "vbic.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x60 */
    {OP_vsubhn_i16,     0xef800600, "vsubhn.i16",     VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800610, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800630, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800650, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800670, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x70 */
    {OP_vabdl_s8,       0xef800700, "vabdl.s8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vorr_i32,       0xef800710, "vorr.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800730, "vbic.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i32,       0xef800750, "vorr.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800770, "vbic.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x80 */
    {OP_vmlal_s8,       0xef800800, "vmlal.s8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i16,       0xef800810, "vmov.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x2b]},
    {OP_vmvn_i16,       0xef800830, "vmvn.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x2c]},
    {OP_vmov_i16,       0xef800850, "vmov.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    {OP_vmvn_i16,       0xef800870, "vmvn.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    /* 0x90 */
    {INVALID,           0xef800900, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vorr_i16,       0xef800910, "vorr.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x30]},
    {OP_vbic_i16,       0xef800930, "vbic.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x31]},
    {OP_vorr_i16,       0xef800950, "vorr.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    {OP_vbic_i16,       0xef800970, "vbic.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    /* 0xa0 */
    {OP_vmlsl_s8,       0xef800a00, "vmlsl.s8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i16,       0xef800a10, "vmov.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i16,       0xef800a30, "vmvn.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i16,       0xef800a50, "vmov.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i16,       0xef800a70, "vmvn.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xb0 */
    {INVALID,           0xef800b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vorr_i16,       0xef800b10, "vorr.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i16,       0xef800b30, "vbic.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i16,       0xef800b50, "vorr.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i16,       0xef800b70, "vbic.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xc0 */
    {OP_vmull_s8,       0xef800c00, "vmull.s8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800c10, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800c30, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800c50, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800c70, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xd0 */
    {INVALID,           0xef800d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_i32,       0xef800d10, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800d30, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800d50, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800d70, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xe0 */
    {OP_vmull_p8,       0xef800e00, "vmull.p8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i8,        0xef800e10, "vmov.i8",        VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x49]},
    {OP_vmov_i64,       0xef800e30, "vmov.i64",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x4a]},
    {OP_vmov_i8,        0xef800e50, "vmov.i8",        VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    {OP_vmov_i64,       0xef800e70, "vmov.i64",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, END_LIST},
    /* 0xf0 */
    {INVALID,           0xef800f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_f32,       0xef800f10, "vmov.f32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, xsi8[0][0x4e]},
    {INVALID,           0xef800f30, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_f32,       0xef800f50, "vmov.f32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, xopc4[0][0x00]},
    {INVALID,           0xef800f70, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 1 */
    {OP_vaddl_u8,       0xff800000, "vaddl.u8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800010, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800030, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800050, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800070, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x10 */
    {OP_vaddw_u8,       0xff800100, "vaddw.u8",       VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vorr_i32,       0xef800110, "vorr.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800130, "vbic.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i32,       0xef800150, "vorr.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800170, "vbic.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x20 */
    {OP_vsubl_u8,       0xff800200, "vsubl.u8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800210, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800230, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800250, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800270, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x30 */
    {OP_vsubw_u8,       0xff800300, "vsubw.u8",       VBdq, xx, VAdq, VCq, xx, no, x, END_LIST},
    {OP_vorr_i32,       0xef800310, "vorr.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800330, "vbic.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i32,       0xef800350, "vorr.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800370, "vbic.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x40 */
    {OP_vraddhn_i16,    0xff800400, "vraddhn.i16",    VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800410, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800430, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800450, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800470, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x50 */
    {OP_vabal_u8,       0xff800500, "vabal.u8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vorr_i32,       0xef800510, "vorr.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800530, "vbic.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i32,       0xef800550, "vorr.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800570, "vbic.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x60 */
    {OP_vrsubhn_i16,    0xff800600, "vrsubhn.i16",    VBq, xx, VAdq, VCdq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800610, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800630, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800650, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800670, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x70 */
    {OP_vabdl_u8,       0xff800700, "vabdl.u8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vorr_i32,       0xef800710, "vorr.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800770, "vbic.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i32,       0xef800750, "vorr.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i32,       0xef800770, "vbic.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x80 */
    {OP_vmlal_u8,       0xff800800, "vmlal.u8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i16,       0xef800810, "vmov.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i16,       0xef800830, "vmvn.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i16,       0xef800850, "vmov.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i16,       0xef800870, "vmvn.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0x90 */
    {INVALID,           0xff800900, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vorr_i16,       0xef800910, "vorr.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i16,       0xef800930, "vbic.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i16,       0xef800950, "vorr.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i16,       0xef800970, "vbic.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xa0 */
    {OP_vmlsl_u8,       0xff800a00, "vmlsl.u8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i16,       0xef800a10, "vmov.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i16,       0xef800a30, "vmvn.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i16,       0xef800a50, "vmov.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i16,       0xef800a70, "vmvn.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xb0 */
    {INVALID,           0xff800b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vorr_i16,       0xef800b10, "vorr.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i16,       0xef800b30, "vbic.i16",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vorr_i16,       0xef800b50, "vorr.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vbic_i16,       0xef800b70, "vbic.i16",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xc0 */
    {OP_vmull_u8,       0xff800c00, "vmull.u8",       VBdq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vmov_i32,       0xef800c10, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800c30, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800c50, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800c70, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xd0 */
    {INVALID,           0xff800d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_i32,       0xef800d10, "vmov.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800d30, "vmvn.i32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i32,       0xef800d50, "vmov.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmvn_i32,       0xef800d70, "vmvn.i32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xe0 */
    {INVALID,           0xff800e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_i8,        0xef800e10, "vmov.i8",        VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i64,       0xef800e30, "vmov.i64",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i8,        0xef800e50, "vmov.i8",        VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {OP_vmov_i64,       0xef800e70, "vmov.i64",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    /* 0xf0 */
    {INVALID,           0xff800f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_f32,       0xff800f10, "vmov.f32",       VBq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {INVALID,           0xff800f30, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_f32,       0xff800f50, "vmov.f32",       VBdq, xx, i12x8_28_16_0, xx, xx, no, x, DUP_ENTRY},
    {INVALID,           0xff800f70, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits 10:8,7:6 with one extra set of 7:6 for bit 11 being set */
const instr_info_t T32_ext_simd6b[][36] = {
  { /* 0 */
    {OP_vcgt_s8,        0xffb10000, "vcgt.s8",        VBq, xx, VCq, k0, xx, no, x, xsi6[0][0x0c]},
    {OP_vcgt_s8,        0xffb10040, "vcgt.s8",        VBdq, xx, VCdq, k0, xx, no, x, xsi6[0][0x0e]},
    {OP_vcge_s8,        0xffb10080, "vcge.s8",        VBq, xx, VCq, k0, xx, no, x, xsi6[0][0x0d]},
    {OP_vcge_s8,        0xffb100c0, "vcge.s8",        VBdq, xx, VCdq, k0, xx, no, x, xsi6[0][0x0f]},
    {OP_vceq_i8,        0xffb10100, "vceq.i8",        VBq, xx, VCq, k0, xx, no, x, xsi6[6][0x21]},
    {OP_vceq_i8,        0xffb10140, "vceq.i8",        VBdq, xx, VCdq, k0, xx, no, x, xsi6[6][0x23]},
    {OP_vcle_s8,        0xffb10180, "vcle.s8",        VBq, xx, VCq, k0, xx, no, x, xsi6b[0][0x07]},
    {OP_vcle_s8,        0xffb101c0, "vcle.s8",        VBdq, xx, VCdq, k0, xx, no, x, END_LIST},
    {OP_vclt_s8,        0xffb10200, "vclt.s8",        VBq, xx, VCq, k0, xx, no, x, xsi6b[0][0x09]},
    {OP_vclt_s8,        0xffb10240, "vclt.s8",        VBdq, xx, VCdq, k0, xx, no, x, END_LIST},
    {INVALID,           0xffb10280, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb102c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vabs_s8,        0xffb10300, "vabs.s8",        VBq, xx, VCq, xx, xx, no, x, xsi6b[0][0x0d]},
    {OP_vabs_s8,        0xffb10340, "vabs.s8",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vneg_s8,        0xffb10380, "vneg.s8",        VBq, xx, VCq, xx, xx, no, x, xsi6b[0][0x0f]},
    {OP_vneg_s8,        0xffb103c0, "vneg.s8",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb10400, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10440, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10480, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb104c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10500, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10540, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10580, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb105c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10600, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10640, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10680, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb106c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10700, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10740, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10780, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb107c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_8,         0xffb10c00, "vdup.8",         VBq, xx, VCb_q, i3_17, xx, no, x, xsi6b[0][0x21]},
    {OP_vdup_8,         0xffb10c40, "vdup.8",         VBdq, xx, VCb_q, i3_17, xx, no, x, xfpB[13][0x01]},
    {INVALID,           0xffb10c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb10cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 1 */
    {OP_vswp,           0xffb20000, "vswp",           VBq, xx, VCq, xx, xx, no, x, xsi6b[1][0x01]},
    {OP_vswp,           0xffb20040, "vswp",           VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vtrn_8,         0xffb20080, "vtrn.8",         VBq, xx, VCq, xx, xx, no, x, xsi6b[1][0x03]},
    {OP_vtrn_8,         0xffb200c0, "vtrn.8",         VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vuzp_8,         0xffb20100, "vuzp.8",         VBq, xx, VCq, xx, xx, no, x, xsi6b[1][0x05]},
    {OP_vuzp_8,         0xffb20140, "vuzp.8",         VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vzip_8,         0xffb20180, "vzip.8",         VBq, xx, VCq, xx, xx, no, x, xsi6b[1][0x07]},
    {OP_vzip_8,         0xffb201c0, "vzip.8",         VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vmovn_i16,      0xffb20200, "vmovn.i16",      VBd, xx, VCdq, xx, xx, no, x, END_LIST},/*XXX: doesn't read entire src*/
    {OP_vqmovun_s16,    0xffb20240, "vqmovun.s16",    VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqmovn_s16,     0xffb20280, "vqmovn.s16",     VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqmovn_u16,     0xffb202c0, "vqmovn.u16",     VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vshll_i8,       0xffb20300, "vshll.i8",       VBdq, xx, VCq, k8, xx, no, x, END_LIST},
    {INVALID,           0xffb20340, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20380, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb203c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20400, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20440, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20480, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb204c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20500, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20540, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20580, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb205c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20600, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20640, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20680, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb206c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20700, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20740, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20780, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb207c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_16,        0xffb20c00, "vdup.16",        VBq, xx, VCh_q, i2_18, xx, no, x, xsi6b[1][0x21]},
    {OP_vdup_16,        0xffb20c40, "vdup.16",        VBdq, xx, VCh_q, i2_18, xx, no, x, xfpB[10][0x03]},
    {INVALID,           0xffb20c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb20cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 2 */
    {OP_vrev64_16,      0xffb40000, "vrev64.16",      VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x01]},
    {OP_vrev64_16,      0xffb40040, "vrev64.16",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vrev32_16,      0xffb40080, "vrev32.16",      VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x03]},
    {OP_vrev32_16,      0xffb400c0, "vrev32.16",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vrev16_16,      0xffb40100, "vrev16.16",      VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x05]},
    {OP_vrev16_16,      0xffb40140, "vrev16.16",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb40180, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb401c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddl_s16,     0xffb40200, "vpaddl.s16",     VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x09]},
    {OP_vpaddl_s16,     0xffb40240, "vpaddl.s16",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vpaddl_u16,     0xffb40280, "vpaddl.u16",     VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x0b]},
    {OP_vpaddl_u16,     0xffb402c0, "vpaddl.u16",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb40300, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb40340, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb40380, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb403c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcls_s16,       0xffb40400, "vcls.s16",       VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x11]},
    {OP_vcls_s16,       0xffb40440, "vcls.s16",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vclz_i16,       0xffb40480, "vclz.i16",       VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x13]},
    {OP_vclz_i16,       0xffb404c0, "vclz.i16",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb40500, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb40540, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb40580, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb405c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpadal_s16,     0xffb40600, "vpadal.s16",     VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x19]},
    {OP_vpadal_s16,     0xffb40640, "vpadal.s16",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vpadal_u16,     0xffb40680, "vpadal.u16",     VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x1b]},
    {OP_vpadal_u16,     0xffb406c0, "vpadal.u16",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqabs_s16,      0xffb40700, "vqabs.s16",      VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x1d]},
    {OP_vqabs_s16,      0xffb40740, "vqabs.s16",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqneg_s16,      0xffb40780, "vqneg.s16",      VBq, xx, VCq, xx, xx, no, x, xsi6b[2][0x1f]},
    {OP_vqneg_s16,      0xffb407c0, "vqneg.s16",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vdup_32,        0xffb40c00, "vdup.32",        VBq, xx, VCd_q, i1_19, xx, no, x, xsi6b[2][0x21]},
    {OP_vdup_32,        0xffb40c40, "vdup.32",        VBdq, xx, VCd_q, i1_19, xx, no, x, xfpB[10][0x01]},
    {INVALID,           0xffb40c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb40cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 3 */
    {OP_vcgt_s16,       0xffb50000, "vcgt.s16",       VBq, xx, VCq, k0, xx, no, x, xsi6[1][0x0c]},
    {OP_vcgt_s16,       0xffb50040, "vcgt.s16",       VBdq, xx, VCdq, k0, xx, no, x, xsi6[1][0x0e]},
    {OP_vcge_s16,       0xffb50080, "vcge.s16",       VBq, xx, VCq, k0, xx, no, x, xsi6[1][0x0d]},
    {OP_vcge_s16,       0xffb500c0, "vcge.s16",       VBdq, xx, VCdq, k0, xx, no, x, xsi6[1][0x0f]},
    {OP_vceq_i16,       0xffb50100, "vceq.i16",       VBq, xx, VCq, k0, xx, no, x, xsi6[7][0x21]},
    {OP_vceq_i16,       0xffb50140, "vceq.i16",       VBdq, xx, VCdq, k0, xx, no, x, xsi6[7][0x23]},
    {OP_vcle_s16,       0xffb50180, "vcle.s16",       VBq, xx, VCq, k0, xx, no, x, xsi6b[3][0x07]},
    {OP_vcle_s16,       0xffb501c0, "vcle.s16",       VBdq, xx, VCdq, k0, xx, no, x, END_LIST},
    {OP_vclt_s16,       0xffb50200, "vclt.s16",       VBq, xx, VCq, k0, xx, no, x, xsi6b[3][0x09]},
    {OP_vclt_s16,       0xffb50240, "vclt.s16",       VBdq, xx, VCdq, k0, xx, no, x, END_LIST},
    {INVALID,           0xffb50280, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb502c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vabs_s16,       0xffb50300, "vabs.s16",       VBq, xx, VCq, xx, xx, no, x, xsi6b[3][0x0d]},
    {OP_vabs_s16,       0xffb50340, "vabs.s16",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vneg_s16,       0xffb50380, "vneg.s16",       VBq, xx, VCq, xx, xx, no, x, xsi6b[3][0x0f]},
    {OP_vneg_s16,       0xffb503c0, "vneg.s16",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb50400, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50440, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50480, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb504c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50500, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50540, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50580, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb505c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50600, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50640, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50680, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb506c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50700, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50740, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50780, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb507c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_8,         0xffb50c00, "vdup.8",         VBq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {OP_vdup_8,         0xffb50c40, "vdup.8",         VBdq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {INVALID,           0xffb50c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb50cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 4 */
    {INVALID,           0xffb60000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vtrn_16,        0xffb60080, "vtrn.16",        VBq, xx, VCq, xx, xx, no, x, xsi6b[4][0x03]},
    {OP_vtrn_16,        0xffb600c0, "vtrn.16",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vuzp_16,        0xffb60100, "vuzp.16",        VBq, xx, VCq, xx, xx, no, x, xsi6b[4][0x05]},
    {OP_vuzp_16,        0xffb60140, "vuzp.16",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vzip_16,        0xffb60180, "vzip.16",        VBq, xx, VCq, xx, xx, no, x, xsi6b[4][0x07]},
    {OP_vzip_16,        0xffb601c0, "vzip.16",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vmovn_i32,      0xffb60200, "vmovn.i32",      VBd, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqmovun_s32,    0xffb60240, "vqmovun.s32",    VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqmovn_s32,     0xffb60280, "vqmovn.s32",     VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqmovn_u32,     0xffb602c0, "vqmovn.u32",     VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vshll_i16,      0xffb60300, "vshll.i16",      VBdq, xx, VCq, k16, xx, no, x, END_LIST},
    {INVALID,           0xffb60340, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60380, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb603c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60400, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60440, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60480, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb604c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60500, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60540, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60580, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb605c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f16_f32,   0xffb60600, "vcvt.f16.f32",   VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb60640, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60680, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb606c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_f16,   0xffb60700, "vcvt.f32.f16",   VBdq, xx, VCq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb60740, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60780, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb607c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_16,        0xffb60c00, "vdup.16",        VBq, xx, VCh_q, i2_18, xx, no, x, DUP_ENTRY},
    {OP_vdup_16,        0xffb60c40, "vdup.16",        VBdq, xx, VCh_q, i2_18, xx, no, x, DUP_ENTRY},
    {INVALID,           0xffb60c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb60cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 5 */
    {OP_vrev64_32,      0xffb80000, "vrev64.32",      VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x01]},
    {OP_vrev64_32,      0xffb80040, "vrev64.32",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vrev32_32,      0xffb80080, "vrev32.32",      VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x03]},
    {OP_vrev32_32,      0xffb800c0, "vrev32.32",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb80100, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb80140, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb80180, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb801c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddl_s32,     0xffb80200, "vpaddl.s32",     VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x09]},
    {OP_vpaddl_s32,     0xffb80240, "vpaddl.s32",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vpaddl_u32,     0xffb80280, "vpaddl.u32",     VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x0b]},
    {OP_vpaddl_u32,     0xffb802c0, "vpaddl.u32",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb80300, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb80340, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb80380, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb803c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcls_s32,       0xffb80400, "vcls.s32",       VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x11]},
    {OP_vcls_s32,       0xffb80440, "vcls.s32",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vclz_i32,       0xffb80480, "vclz.i32",       VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x13]},
    {OP_vclz_i32,       0xffb804c0, "vclz.i32",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb80500, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb80540, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb80580, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb805c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpadal_s32,     0xffb80600, "vpadal.s32",     VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x19]},
    {OP_vpadal_s32,     0xffb80640, "vpadal.s32",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vpadal_u32,     0xffb80680, "vpadal.u32",     VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x1b]},
    {OP_vpadal_u32,     0xffb806c0, "vpadal.u32",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqabs_s32,      0xffb80700, "vqabs.s32",      VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x1d]},
    {OP_vqabs_s32,      0xffb80740, "vqabs.s32",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqneg_s32,      0xffb80780, "vqneg.s32",      VBq, xx, VCq, xx, xx, no, x, xsi6b[5][0x1f]},
    {OP_vqneg_s32,      0xffb807c0, "vqneg.s32",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb80c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb80c40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb80c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb80cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 6 */
    {OP_vcgt_s32,       0xffb90000, "vcgt.s32",       VBq, xx, VCq, k0, xx, no, x, xsi6[2][0x0c]},
    {OP_vcgt_s32,       0xffb90040, "vcgt.s32",       VBdq, xx, VCdq, k0, xx, no, x, xsi6[2][0x0e]},
    {OP_vcge_s32,       0xffb90080, "vcge.s32",       VBq, xx, VCq, k0, xx, no, x, xsi6[2][0x0d]},
    {OP_vcge_s32,       0xffb900c0, "vcge.s32",       VBdq, xx, VCdq, k0, xx, no, x, xsi6[2][0x0f]},
    {OP_vceq_i32,       0xffb90100, "vceq.i32",       VBq, xx, VCq, k0, xx, no, x, xsi6[8][0x21]},
    {OP_vceq_i32,       0xffb90140, "vceq.i32",       VBdq, xx, VCdq, k0, xx, no, x, xsi6[8][0x23]},
    {OP_vcle_s32,       0xffb90180, "vcle.s32",       VBq, xx, VCq, k0, xx, no, x, xsi6b[6][0x07]},
    {OP_vcle_s32,       0xffb901c0, "vcle.s32",       VBdq, xx, VCdq, k0, xx, no, x, END_LIST},
    {OP_vclt_s32,       0xffb90200, "vclt.s32",       VBq, xx, VCq, k0, xx, no, x, xsi6b[6][0x09]},
    {OP_vclt_s32,       0xffb90240, "vclt.s32",       VBdq, xx, VCdq, k0, xx, no, x, END_LIST},
    {INVALID,           0xffb90280, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_sha1h_32,       0xffb902c0, "sha1h.32",       VBdq, xx, VCdq, xx, xx, v8, x, END_LIST},
    {OP_vabs_s32,       0xffb90300, "vabs.s32",       VBq, xx, VCq, xx, xx, no, x, xsi6b[6][0x0d]},
    {OP_vabs_s32,       0xffb90340, "vabs.s32",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vneg_s32,       0xffb90380, "vneg.s32",       VBq, xx, VCq, xx, xx, no, x, xsi6b[6][0x0f]},
    {OP_vneg_s32,       0xffb903c0, "vneg.s32",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vcgt_f32,       0xffb90400, "vcgt.f32",       VBq, xx, VCq, k0, xx, no, x, xsi6[8][0x38]},
    {OP_vcgt_f32,       0xffb90440, "vcgt.f32",       VBdq, xx, VCdq, k0, xx, no, x, xsi6[8][0x3a]},
    {OP_vcge_f32,       0xffb90480, "vcge.f32",       VBq, xx, VCq, k0, xx, no, x, xsi6[6][0x38]},
    {OP_vcge_f32,       0xffb904c0, "vcge.f32",       VBdq, xx, VCdq, k0, xx, no, x, xsi6[6][0x3a]},
    {OP_vceq_f32,       0xffb90500, "vceq.f32",       VBq, xx, VCq, k0, xx, no, x, xsi6[0][0x38]},
    {OP_vceq_f32,       0xffb90540, "vceq.f32",       VBdq, xx, VCdq, k0, xx, no, x, xsi6[0][0x3a]},
    {OP_vcle_f32,       0xffb90580, "vcle.f32",       VBq, xx, VCq, k0, xx, no, x, xsi6b[6][0x17]},
    {OP_vcle_f32,       0xffb905c0, "vcle.f32",       VBdq, xx, VCdq, k0, xx, no, x, END_LIST},
    {OP_vclt_f32,       0xffb90600, "vclt.f32",       VBq, xx, VCq, k0, xx, no, x, xsi6b[6][0x19]},
    {OP_vclt_f32,       0xffb90640, "vclt.f32",       VBdq, xx, VCdq, k0, xx, no, x, END_LIST},
    {INVALID,           0xffb90680, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb906c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vabs_f32,       0xffb90700, "vabs.f32",       VBq, xx, VCq, xx, xx, no, x, xsi6b[6][0x1d]},
    {OP_vabs_f32,       0xffb90740, "vabs.f32",       VBdq, xx, VCdq, xx, xx, no, x, xbi16[1][0x00]},
    {OP_vneg_f32,       0xffb90780, "vneg.f32",       VBq, xx, VCq, xx, xx, no, x, xsi6b[6][0x1f]},
    {OP_vneg_f32,       0xffb907c0, "vneg.f32",       VBdq, xx, VCdq, xx, xx, no, x, xbi16[0][0x01]},
    {OP_vdup_8,         0xffb90c00, "vdup.8",         VBq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {OP_vdup_8,         0xffb90c40, "vdup.8",         VBdq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {INVALID,           0xffb90c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb90cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 7 */
    {INVALID,           0xffba0000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffba0040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vtrn_32,        0xffba0080, "vtrn.32",        VBq, xx, VCq, xx, xx, no, x, xsi6b[7][0x03]},
    {OP_vtrn_32,        0xffba00c0, "vtrn.32",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vuzp_32,        0xffba0100, "vuzp.32",        VBq, xx, VCq, xx, xx, no, x, xsi6b[7][0x05]},
    {OP_vuzp_32,        0xffba0140, "vuzp.32",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vzip_32,        0xffba0180, "vzip.32",        VBq, xx, VCq, xx, xx, no, x, xsi6b[7][0x07]},
    {OP_vzip_32,        0xffba01c0, "vzip.32",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vmovn_i64,      0xffba0200, "vmovn.i64",      VBd, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqmovun_s64,    0xffba0240, "vqmovun.s64",    VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqmovn_s64,     0xffba0280, "vqmovn.s64",     VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqmovn_u64,     0xffba02c0, "vqmovn.u64",     VBq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vshll_i32,      0xffba0300, "vshll.i32",      VBdq, xx, VCq, k32, xx, no, x, END_LIST},
    {INVALID,           0xffba0340, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_sha1su1_32,     0xffba0380, "sha1su1.32",     VBdq, xx, VCdq, xx, xx, v8, x, END_LIST},
    {OP_sha256su0_32,   0xffba03c0, "sha256su0.32",   VBdq, xx, VCdq, xx, xx, v8, x, END_LIST},
    {OP_vrintn_f32_f32, 0xffba0400, "vrintn.f32.f32", VBq, xx, VCq, xx, xx, v8, x, xsi6b[7][0x11]},
    {OP_vrintn_f32_f32, 0xffba0440, "vrintn.f32.f32", VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x04]},
    {OP_vrintx_f32_f32, 0xffba0480, "vrintx.f32.f32", VBq, xx, VCq, xx, xx, v8, x, xsi6b[7][0x13]},
    {OP_vrintx_f32_f32, 0xffba04c0, "vrintx.f32.f32", VBdq, xx, VCdq, xx, xx, v8, x, END_LIST},
    {OP_vrinta_f32_f32, 0xffba0500, "vrinta.f32.f32", VBq, xx, VCq, xx, xx, v8, x, xsi6b[7][0x15]},
    {OP_vrinta_f32_f32, 0xffba0540, "vrinta.f32.f32", VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x00]},
    {OP_vrintz_f32_f32, 0xffba0580, "vrintz.f32.f32", VBq, xx, VCq, xx, xx, v8, x, xsi6b[7][0x17]},
    {OP_vrintz_f32_f32, 0xffba05c0, "vrintz.f32.f32", VBdq, xx, VCdq, xx, xx, v8, x, END_LIST},
    {INVALID,           0xffba0600, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffba0640, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintm_f32_f32, 0xffba0680, "vrintm.f32.f32", VBq, xx, VCq, xx, xx, v8, x, xsi6b[7][0x1b]},
    {OP_vrintm_f32_f32, 0xffba06c0, "vrintm.f32.f32", VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x0c]},
    {INVALID,           0xffba0700, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffba0740, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintp_f32_f32, 0xffba0780, "vrintp.f32.f32", VBq, xx, VCq, xx, xx, v8, x, xsi6b[7][0x1f]},
    {OP_vrintp_f32_f32, 0xffba07c0, "vrintp.f32.f32", VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x08]},
    {OP_vdup_16,        0xffba0c00, "vdup.16",        VBq, xx, VCh_q, i2_18, xx, no, x, DUP_ENTRY},
    {OP_vdup_16,        0xffba0c40, "vdup.16",        VBdq, xx, VCh_q, i2_18, xx, no, x, DUP_ENTRY},
    {INVALID,           0xffba0c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffba0cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 8 */
    {OP_vcvta_s32_f32,  0xffbb0000, "vcvta.s32.f32",  VBq, xx, VCq, xx, xx, v8, x, xsi6b[8][0x01]},
    {OP_vcvta_s32_f32,  0xffbb0040, "vcvta.s32.f32",  VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x11]},
    {OP_vcvta_u32_f32,  0xffbb0080, "vcvta.u32.f32",  VBq, xx, VCq, xx, xx, v8, x, xsi6b[8][0x03]},
    {OP_vcvta_u32_f32,  0xffbb00c0, "vcvta.u32.f32",  VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x10]},
    {OP_vcvtn_s32_f32,  0xffbb0100, "vcvtn.s32.f32",  VBq, xx, VCq, xx, xx, v8, x, xsi6b[8][0x05]},
    {OP_vcvtn_s32_f32,  0xffbb0140, "vcvtn.s32.f32",  VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x15]},
    {OP_vcvtn_u32_f32,  0xffbb0180, "vcvtn.u32.f32",  VBq, xx, VCq, xx, xx, v8, x, xsi6b[8][0x07]},
    {OP_vcvtn_u32_f32,  0xffbb01c0, "vcvtn.u32.f32",  VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x14]},
    {OP_vcvtp_s32_f32,  0xffbb0200, "vcvtp.s32.f32",  VBq, xx, VCq, xx, xx, v8, x, xsi6b[8][0x09]},
    {OP_vcvtp_s32_f32,  0xffbb0240, "vcvtp.s32.f32",  VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x19]},
    {OP_vcvtp_u32_f32,  0xffbb0280, "vcvtp.u32.f32",  VBq, xx, VCq, xx, xx, v8, x, xsi6b[8][0x0b]},
    {OP_vcvtp_u32_f32,  0xffbb02c0, "vcvtp.u32.f32",  VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x18]},
    {OP_vcvtm_s32_f32,  0xffbb0300, "vcvtm.s32.f32",  VBq, xx, VCq, xx, xx, v8, x, xsi6b[8][0x0d]},
    {OP_vcvtm_s32_f32,  0xffbb0340, "vcvtm.s32.f32",  VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x1d]},
    {OP_vcvtm_u32_f32,  0xffbb0380, "vcvtm.u32.f32",  VBq, xx, VCq, xx, xx, v8, x, xsi6b[8][0x0f]},
    {OP_vcvtm_u32_f32,  0xffbb03c0, "vcvtm.u32.f32",  VBdq, xx, VCdq, xx, xx, v8, x, xsi5b[0][0x1c]},
    {OP_vrecpe_u32,     0xffbb0400, "vrecpe.u32",     VBq, xx, VCq, xx, xx, no, x, xsi6b[8][0x11]},
    {OP_vrecpe_u32,     0xffbb0440, "vrecpe.u32",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vrsqrte_u32,    0xffbb0480, "vrsqrte.u32",    VBq, xx, VCq, xx, xx, no, x, xsi6b[8][0x13]},
    {OP_vrsqrte_u32,    0xffbb04c0, "vrsqrte.u32",    VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vrecpe_f32,     0xffbb0500, "vrecpe.f32",     VBq, xx, VCq, xx, xx, no, x, xsi6b[8][0x15]},
    {OP_vrecpe_f32,     0xffbb0540, "vrecpe.f32",     VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vrsqrte_f32,    0xffbb0580, "vrsqrte.f32",    VBq, xx, VCq, xx, xx, no, x, xsi6b[8][0x17]},
    {OP_vrsqrte_f32,    0xffbb05c0, "vrsqrte.f32",    VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vcvt_f32_s32,   0xffbb0600, "vcvt.f32.s32",   VBq, xx, VCq, xx, xx, no, x, xsi6[5][0x39]},
    {OP_vcvt_f32_s32,   0xffbb0640, "vcvt.f32.s32",   VBdq, xx, VCdq, xx, xx, no, x, xsi6[5][0x3b]},
    {OP_vcvt_f32_u32,   0xffbb0680, "vcvt.f32.u32",   VBq, xx, VCq, xx, xx, no, x, xsi6[11][0x39]},
    {OP_vcvt_f32_u32,   0xffbb06c0, "vcvt.f32.u32",   VBdq, xx, VCdq, xx, xx, no, x, xsi6[11][0x3b]},
    {OP_vcvt_s32_f32,   0xffbb0700, "vcvt.s32.f32",   VBq, xx, VCq, xx, xx, no, x, xsi6[5][0x3c]},
    {OP_vcvt_s32_f32,   0xffbb0740, "vcvt.s32.f32",   VBdq, xx, VCdq, xx, xx, no, x, xsi6[5][0x3e]},
    {OP_vcvt_u32_f32,   0xffbb0780, "vcvt.u32.f32",   VBq, xx, VCq, xx, xx, no, x, xsi6[11][0x3d]},
    {OP_vcvt_u32_f32,   0xffbb07c0, "vcvt.u32.f32",   VBdq, xx, VCdq, xx, xx, no, x, xsi6[11][0x3f]},
    {OP_vdup_8,         0xffbb0c00, "vdup.8",         VBq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {OP_vdup_8,         0xffbb0c40, "vdup.8",         VBdq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {INVALID,           0xffbb0c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffbb0cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_vrev64_8,       0xffb00000, "vrev64.8",       VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x01]},
    {OP_vrev64_8,       0xffb00040, "vrev64.8",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vrev32_8,       0xffb00080, "vrev32.8",       VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x03]},
    {OP_vrev32_8,       0xffb000c0, "vrev32.8",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vrev16_8,       0xffb00100, "vrev16.8",       VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x05]},
    {OP_vrev16_8,       0xffb00140, "vrev16.8",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb00180, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb001c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddl_s8,      0xffb00200, "vpaddl.s8",      VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x09]},
    {OP_vpaddl_s8,      0xffb00240, "vpaddl.s8",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vpaddl_u8,      0xffb00280, "vpaddl.u8",      VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x0b]},
    {OP_vpaddl_u8,      0xffb002c0, "vpaddl.u8",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_aese_8,         0xffb00300, "aese.8",         VBdq, xx, VBdq, VCdq, xx, v8, x, END_LIST},
    {OP_aesd_8,         0xffb00340, "aesd.8",         VBdq, xx, VBdq, VCdq, xx, v8, x, END_LIST},
    {OP_aesmc_8,        0xffb00380, "aesmc.8",        VBdq, xx, VCdq, xx, xx, v8, x, END_LIST},
    {OP_aesimc_8,       0xffb003c0, "aesimc.8",       VBdq, xx, VCdq, xx, xx, v8, x, END_LIST},
    {OP_vcls_s8,        0xffb00400, "vcls.s8",        VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x11]},
    {OP_vcls_s8,        0xffb00440, "vcls.s8",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vclz_i8,        0xffb00480, "vclz.i8",        VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x13]},
    {OP_vclz_i8,        0xffb004c0, "vclz.i8",        VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vcnt_8,         0xffb00500, "vcnt.8",         VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x15]},
    {OP_vcnt_8,         0xffb00540, "vcnt.8",         VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vmvn,           0xffb00580, "vmvn",           VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x17]},
    {OP_vmvn,           0xffb005c0, "vmvn",           VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vpadal_s8,      0xffb00600, "vpadal.s8",      VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x19]},
    {OP_vpadal_s8,      0xffb00640, "vpadal.s8",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vpadal_u8,      0xffb00680, "vpadal.u8",      VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x1b]},
    {OP_vpadal_u8,      0xffb006c0, "vpadal.u8",      VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqabs_s8,       0xffb00700, "vqabs.s8",       VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x1d]},
    {OP_vqabs_s8,       0xffb00740, "vqabs.s8",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {OP_vqneg_s8,       0xffb00780, "vqneg.s8",       VBq, xx, VCq, xx, xx, no, x, xsi6b[9][0x1f]},
    {OP_vqneg_s8,       0xffb007c0, "vqneg.s8",       VBdq, xx, VCdq, xx, xx, no, x, END_LIST},
    {INVALID,           0xffb00c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb00c40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb00c80, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb00cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits 11,6 (0x......840) */
const instr_info_t T32_ext_simd2[][4] = {
  { /* 0 */
    {INVALID,           0xffb30000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb30040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_8,         0xffb30c00, "vdup.8",         VBq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {OP_vdup_8,         0xffb30c40, "vdup.8",         VBdq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
  }, { /* 1 */
    {INVALID,           0xffb70000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffb70040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_8,         0xffb70c00, "vdup.8",         VBq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {OP_vdup_8,         0xffb70c40, "vdup.8",         VBdq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
  }, { /* 2 */
    {INVALID,           0xffbc0000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffbc0040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_32,        0xffbc0c00, "vdup.32",        VBq, xx, VCd_q, i1_19, xx, no, x, DUP_ENTRY},
    {OP_vdup_32,        0xffbc0c40, "vdup.32",        VBdq, xx, VCd_q, i1_19, xx, no, x, DUP_ENTRY},
  }, { /* 3 */
    {INVALID,           0xffbd0000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffbd0040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_8,         0xffbd0c00, "vdup.8",         VBq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {OP_vdup_8,         0xffbd0c40, "vdup.8",         VBdq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
  }, { /* 4 */
    {INVALID,           0xffbe0000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffbe0040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_16,        0xffbe0c00, "vdup.16",        VBq, xx, VCh_q, i2_18, xx, no, x, DUP_ENTRY},
    {OP_vdup_16,        0xffbe0c40, "vdup.16",        VBdq, xx, VCh_q, i2_18, xx, no, x, DUP_ENTRY},
  }, { /* 5 */
    {INVALID,           0xffbf0000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xffbf0040, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_8,         0xffbf0c00, "vdup.8",         VBq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
    {OP_vdup_8,         0xffbf0c40, "vdup.8",         VBdq, xx, VCb_q, i3_17, xx, no, x, DUP_ENTRY},
  },
};

/* Indexed by bits 10:8,6.  Bits 4 and 7 are already set.  These have
 * i6_16 with the L bit which means their upper bits can vary quite a bit.
 */
const instr_info_t T32_ext_imm6L[][16] = {
  { /* 0 */
    {OP_vshr_s64,       0xef800090, "vshr.s64",       VBq, xx, VCq, i6_16, xx, no, x, xi6l[0][0x01]},/*XXX: imm = 64-imm*/
    {OP_vshr_s64,       0xef8000d0, "vshr.s64",       VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm*/
    {OP_vsra_s64,       0xef800190, "vsra.s64",       VBq, xx, VCq, i6_16, xx, no, x, xi6l[0][0x03]},/*XXX: imm = 64-imm*/
    {OP_vsra_s64,       0xef8001d0, "vsra.s64",       VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm*/
    {OP_vrshr_s64,      0xef800290, "vrshr.s64",      VBq, xx, VCq, i6_16, xx, no, x, xi6l[0][0x05]},/*XXX: imm = 64-imm*/
    {OP_vrshr_s64,      0xef8002d0, "vrshr.s64",      VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm*/
    {OP_vrsra_s64,      0xef800390, "vrsra.s64",      VBq, xx, VCq, i6_16, xx, no, x, xi6l[0][0x07]},/*XXX: imm = 64-imm*/
    {OP_vrsra_s64,      0xef8003d0, "vrsra.s64",      VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm*/
    {INVALID,           0xef800490, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef8004d0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vshl_i64,       0xef800590, "vshl.i64",       VBq, xx, VCq, i6_16, xx, no, x, xi6l[0][0x0b]},/*XXX: imm = 64-imm?*/
    {OP_vshl_i64,       0xef8005d0, "vshl.i64",       VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm?*/
    {INVALID,           0xef800690, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xef8006d0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vqshl_s64,      0xef800790, "vqshl.s64",      VBq, xx, VCq, i6_16, xx, no, x, xsi6[3][0x11]},
    {OP_vqshl_s64,      0xef8007d0, "vqshl.s64",      VBdq, xx, VCdq, i6_16, xx, no, x, xsi6[3][0x13]},
  }, { /* 1 */
    {OP_vshr_u64,       0xff800090, "vshr.u64",       VBq, xx, VCq, i6_16, xx, no, x, xi6l[1][0x01]},/*XXX: imm = 64-imm*/
    {OP_vshr_u64,       0xff8000d0, "vshr.u64",       VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm*/
    {OP_vsra_u64,       0xff800190, "vsra.u64",       VBq, xx, VCq, i6_16, xx, no, x, xi6l[1][0x03]},/*XXX: imm = 64-imm*/
    {OP_vsra_u64,       0xff8001d0, "vsra.u64",       VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm*/
    {OP_vrshr_u64,      0xff800290, "vrshr.u64",      VBq, xx, VCq, i6_16, xx, no, x, xi6l[1][0x05]},/*XXX: imm = 64-imm*/
    {OP_vrshr_u64,      0xff8002d0, "vrshr.u64",      VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm*/
    {OP_vrsra_u64,      0xff800390, "vrsra.u64",      VBq, xx, VCq, i6_16, xx, no, x, xi6l[1][0x07]},/*XXX: imm = 64-imm*/
    {OP_vrsra_u64,      0xff8003d0, "vrsra.u64",      VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm*/
    {OP_vsri_64,        0xff800490, "vsri.64",        VBq, xx, VCq, i6_16, xx, no, x, xi6l[1][0x09]},/*XXX: imm = 64-imm?*/
    {OP_vsri_64,        0xff8004d0, "vsri.64",        VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm?*/
    {OP_vsli_64,        0xff800590, "vsli.64",        VBq, xx, VCq, i6_16, xx, no, x, xi6l[1][0x0b]},/*XXX: imm = 64-imm?*/
    {OP_vsli_64,        0xff8005d0, "vsli.64",        VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},/*XXX: imm = 64-imm?*/
    {OP_vqshlu_s64,     0xff800690, "vqshlu.s64",     VBq, xx, VCq, i6_16, xx, no, x, xi6l[1][0x0d]},
    {OP_vqshlu_s64,     0xff8006d0, "vqshlu.s64",     VBdq, xx, VCdq, i6_16, xx, no, x, END_LIST},
    {OP_vqshl_u64,      0xff800790, "vqshl.u64",      VBq, xx, VCq, i6_16, xx, no, x, xsi6[9][0x11]},
    {OP_vqshl_u64,      0xff8007d0, "vqshl.u64",      VBdq, xx, VCdq, i6_16, xx, no, x, xsi6[9][0x13]},
  },
};

/* Indexed by bits (11:8,7:6)*3+X where X is based on the value of 3:0:
 * + 0xd => 0
 * + 0xf => 1
 * + else => 2
 * However, the top 11:8 stops at 0xa.
 */
const instr_info_t T32_ext_vldA[][132] = {
  { /* 0 */
    {OP_vst4_8,         0xf900000d, "vst4.8",         Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst4_8,         0xf900000f, "vst4.8",         Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x00]},
    {OP_vst4_8,         0xf9000000, "vst4.8",         Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x01]},
    {OP_vst4_16,        0xf900004d, "vst4.16",        Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst4_16,        0xf900004f, "vst4.16",        Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x03]},
    {OP_vst4_16,        0xf9000040, "vst4.16",        Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x04]},
    {OP_vst4_32,        0xf900008d, "vst4.32",        Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst4_32,        0xf900008f, "vst4.32",        Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x06]},
    {OP_vst4_32,        0xf9000080, "vst4.32",        Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x07]},
    {INVALID,           0xf90000cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90000cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90000c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vst4_8,         0xf900010d, "vst4.8",         Mqq, RAw, LX4Dq, i2_4, RAw, no, x, xvlA[0][0x02]},
    {OP_vst4_8,         0xf900010f, "vst4.8",         Mqq, xx, LX4Dq, i2_4, xx, no, x, xvlA[0][0x0c]},
    {OP_vst4_8,         0xf9000100, "vst4.8",         Mqq, RAw, LX4Dq, i2_4, RDw, xop_wb, x, xvlA[0][0x0d]},
    {OP_vst4_16,        0xf900014d, "vst4.16",        Mqq, RAw, LX4Dq, i2_4, RAw, no, x, xvlA[0][0x05]},
    {OP_vst4_16,        0xf900014f, "vst4.16",        Mqq, xx, LX4Dq, i2_4, xx, no, x, xvlA[0][0x0f]},
    {OP_vst4_16,        0xf9000140, "vst4.16",        Mqq, RAw, LX4Dq, i2_4, RDw, xop_wb, x, xvlA[0][0x10]},
    {OP_vst4_32,        0xf900018d, "vst4.32",        Mqq, RAw, LX4Dq, i2_4, RAw, no, x, xvlA[0][0x08]},
    {OP_vst4_32,        0xf900018f, "vst4.32",        Mqq, xx, LX4Dq, i2_4, xx, no, x, xvlA[0][0x12]},
    {OP_vst4_32,        0xf9000180, "vst4.32",        Mqq, RAw, LX4Dq, i2_4, RDw, xop_wb, x, xvlA[0][0x13]},
    {INVALID,           0xf90001cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90001cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90001c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vst1_8,         0xf900020d, "vst1.8",         Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst1_8,         0xf900020f, "vst1.8",         Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x18]},
    {OP_vst1_8,         0xf9000200, "vst1.8",         Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x19]},
    {OP_vst1_16,        0xf900024d, "vst1.16",        Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst1_16,        0xf900024f, "vst1.16",        Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x1b]},
    {OP_vst1_16,        0xf9000240, "vst1.16",        Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x1c]},
    {OP_vst1_32,        0xf900028d, "vst1.32",        Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst1_32,        0xf900028f, "vst1.32",        Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x1e]},
    {OP_vst1_32,        0xf9000280, "vst1.32",        Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x1f]},
    {OP_vst1_64,        0xf90002cd, "vst1.64",        Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst1_64,        0xf90002cf, "vst1.64",        Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x21]},
    {OP_vst1_64,        0xf90002c0, "vst1.64",        Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x22]},
    {OP_vst2_8,         0xf900030d, "vst2.8",         Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst2_8,         0xf900030f, "vst2.8",         Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x24]},
    {OP_vst2_8,         0xf9000300, "vst2.8",         Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x25]},
    {OP_vst2_16,        0xf900034d, "vst2.16",        Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst2_16,        0xf900034f, "vst2.16",        Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x27]},
    {OP_vst2_16,        0xf9000340, "vst2.16",        Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x28]},
    {OP_vst2_32,        0xf900038d, "vst2.32",        Mqq, RAw, LX4q, i2_4, RAw, no, x, END_LIST},
    {OP_vst2_32,        0xf900038f, "vst2.32",        Mqq, xx, LX4q, i2_4, xx, no, x, xvlA[0][0x2a]},
    {OP_vst2_32,        0xf9000380, "vst2.32",        Mqq, RAw, LX4q, i2_4, RDw, xop_wb, x, xvlA[0][0x2b]},
    {INVALID,           0xf90003cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90003cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90003c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vst3_8,         0xf900040d, "vst3.8",         M24, RAw, LX3q, i2_4, RAw, no, x, END_LIST},
    {OP_vst3_8,         0xf900040f, "vst3.8",         M24, xx, LX3q, i2_4, xx, no, x, xvlA[0][0x30]},
    {OP_vst3_8,         0xf9000400, "vst3.8",         M24, RAw, LX3q, i2_4, RDw, xop_wb, x, xvlA[0][0x31]},
    {OP_vst3_16,        0xf900044d, "vst3.16",        M24, RAw, LX3q, i2_4, RAw, no, x, END_LIST},
    {OP_vst3_16,        0xf900044f, "vst3.16",        M24, xx, LX3q, i2_4, xx, no, x, xvlA[0][0x33]},
    {OP_vst3_16,        0xf9000440, "vst3.16",        M24, RAw, LX3q, i2_4, RDw, xop_wb, x, xvlA[0][0x34]},
    {OP_vst3_32,        0xf900048d, "vst3.32",        M24, RAw, LX3q, i2_4, RAw, no, x, END_LIST},
    {OP_vst3_32,        0xf900048f, "vst3.32",        M24, xx, LX3q, i2_4, xx, no, x, xvlA[0][0x36]},
    {OP_vst3_32,        0xf9000480, "vst3.32",        M24, RAw, LX3q, i2_4, RDw, xop_wb, x, xvlA[0][0x37]},
    {INVALID,           0xf90004cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90004cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90004c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vst3_8,         0xf900050d, "vst3.8",         M24, RAw, LX3Dq, i2_4, RAw, no, x, xvlA[0][0x32]},
    {OP_vst3_8,         0xf900050f, "vst3.8",         M24, xx, LX3Dq, i2_4, xx, no, x, xvlA[0][0x3c]},
    {OP_vst3_8,         0xf9000500, "vst3.8",         M24, RAw, LX3Dq, i2_4, RDw, xop_wb, x, xvlA[0][0x3d]},
    {OP_vst3_16,        0xf900054d, "vst3.16",        M24, RAw, LX3Dq, i2_4, RAw, no, x, xvlA[0][0x35]},
    {OP_vst3_16,        0xf900054f, "vst3.16",        M24, xx, LX3Dq, i2_4, xx, no, x, xvlA[0][0x3f]},
    {OP_vst3_16,        0xf9000540, "vst3.16",        M24, RAw, LX3Dq, i2_4, RDw, xop_wb, x, xvlA[0][0x40]},
    {OP_vst3_32,        0xf900058d, "vst3.32",        M24, RAw, LX3Dq, i2_4, RAw, no, x, xvlA[0][0x38]},
    {OP_vst3_32,        0xf900058f, "vst3.32",        M24, xx, LX3Dq, i2_4, xx, no, x, xvlA[0][0x42]},
    {OP_vst3_32,        0xf9000580, "vst3.32",        M24, RAw, LX3Dq, i2_4, RDw, xop_wb, x, xvlA[0][0x43]},
    {INVALID,           0xf90005cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90005cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90005c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vst1_8,         0xf900060d, "vst1.8",         M24, RAw, LX3q, i2_4, RAw, no, x, xvlA[0][0x1a]},
    {OP_vst1_8,         0xf900060f, "vst1.8",         M24, xx, LX3q, i2_4, xx, no, x, xvlA[0][0x48]},
    {OP_vst1_8,         0xf9000600, "vst1.8",         M24, RAw, LX3q, i2_4, RDw, xop_wb, x, xvlA[0][0x49]},
    {OP_vst1_16,        0xf900064d, "vst1.16",        M24, RAw, LX3q, i2_4, RAw, no, x, xvlA[0][0x1d]},
    {OP_vst1_16,        0xf900064f, "vst1.16",        M24, xx, LX3q, i2_4, xx, no, x, xvlA[0][0x4b]},
    {OP_vst1_16,        0xf9000640, "vst1.16",        M24, RAw, LX3q, i2_4, RDw, xop_wb, x, xvlA[0][0x4c]},
    {OP_vst1_32,        0xf900068d, "vst1.32",        M24, RAw, LX3q, i2_4, RAw, no, x, xvlA[0][0x20]},
    {OP_vst1_32,        0xf900068f, "vst1.32",        M24, xx, LX3q, i2_4, xx, no, x, xvlA[0][0x4e]},
    {OP_vst1_32,        0xf9000680, "vst1.32",        M24, RAw, LX3q, i2_4, RDw, xop_wb, x, xvlA[0][0x4f]},
    {OP_vst1_64,        0xf90006cd, "vst1.64",        M24, RAw, LX3q, i2_4, RAw, no, x, xvlA[0][0x23]},
    {OP_vst1_64,        0xf90006cf, "vst1.64",        M24, xx, LX3q, i2_4, xx, no, x, xvlA[0][0x51]},
    {OP_vst1_64,        0xf90006c0, "vst1.64",        M24, RAw, LX3q, i2_4, RDw, xop_wb, x, xvlA[0][0x52]},
    {OP_vst1_8,         0xf900070d, "vst1.8",         Mq, RAw, VBq, i2_4, RAw, no, x, xvlA[0][0x7a]},/*XXX: some align values => undefined*/
    {OP_vst1_8,         0xf900070f, "vst1.8",         Mq, xx, VBq, i2_4, xx, no, x, xvlA[0][0x54]},/*XXX: combine align into memop?*/
    {OP_vst1_8,         0xf9000700, "vst1.8",         Mq, RAw, VBq, i2_4, RDw, xop_wb, x, xvlA[0][0x55]},
    {OP_vst1_16,        0xf900074d, "vst1.16",        Mq, RAw, VBq, i2_4, RAw, no, x, xvlA[0][0x7d]},
    {OP_vst1_16,        0xf900074f, "vst1.16",        Mq, xx, VBq, i2_4, xx, no, x, xvlA[0][0x57]},
    {OP_vst1_16,        0xf9000740, "vst1.16",        Mq, RAw, VBq, i2_4, RDw, xop_wb, x, xvlA[0][0x58]},
    {OP_vst1_32,        0xf900078d, "vst1.32",        Mq, RAw, VBq, i2_4, RAw, no, x, xvlA[0][0x80]},
    {OP_vst1_32,        0xf900078f, "vst1.32",        Mq, xx, VBq, i2_4, xx, no, x, xvlA[0][0x5a]},
    {OP_vst1_32,        0xf9000780, "vst1.32",        Mq, RAw, VBq, i2_4, RDw, xop_wb, x, xvlA[0][0x5b]},
    {OP_vst1_64,        0xf90007cd, "vst1.64",        Mq, RAw, VBq, i2_4, RAw, no, x, xvlA[0][0x83]},
    {OP_vst1_64,        0xf90007cf, "vst1.64",        Mq, xx, VBq, i2_4, xx, no, x, xvlA[0][0x5d]},
    {OP_vst1_64,        0xf90007c0, "vst1.64",        Mq, RAw, VBq, i2_4, RDw, xop_wb, x, xvlA[0][0x5e]},
    /* 0x80 */
    {OP_vst2_8,         0xf900080d, "vst2.8",         Mdq, RAw, LX2q, i2_4, RAw, no, x, xvlA[0][0x26]},
    {OP_vst2_8,         0xf900080f, "vst2.8",         Mdq, xx, LX2q, i2_4, xx, no, x, xvlA[0][0x60]},
    {OP_vst2_8,         0xf9000800, "vst2.8",         Mdq, RAw, LX2q, i2_4, RDw, xop_wb, x, xvlA[0][0x61]},
    {OP_vst2_16,        0xf900084d, "vst2.16",        Mdq, RAw, LX2q, i2_4, RAw, no, x, xvlA[0][0x29]},
    {OP_vst2_16,        0xf900084f, "vst2.16",        Mdq, xx, LX2q, i2_4, xx, no, x, xvlA[0][0x63]},
    {OP_vst2_16,        0xf9000840, "vst2.16",        Mdq, RAw, LX2q, i2_4, RDw, xop_wb, x, xvlA[0][0x64]},
    {OP_vst2_32,        0xf900088d, "vst2.32",        Mdq, RAw, LX2q, i2_4, RAw, no, x, xvlA[0][0x2c]},
    {OP_vst2_32,        0xf900088f, "vst2.32",        Mdq, xx, LX2q, i2_4, xx, no, x, xvlA[0][0x66]},
    {OP_vst2_32,        0xf9000880, "vst2.32",        Mdq, RAw, LX2q, i2_4, RDw, xop_wb, x, xvlA[0][0x67]},
    {INVALID,           0xf90008cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90008cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90008c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vst2_8,         0xf900090d, "vst2.8",         Mdq, RAw, LX2Dq, i2_4, RAw, no, x, xvlA[0][0x62]},
    {OP_vst2_8,         0xf900090f, "vst2.8",         Mdq, xx, LX2Dq, i2_4, xx, no, x, xvlA[0][0x6c]},
    {OP_vst2_8,         0xf9000900, "vst2.8",         Mdq, RAw, LX2Dq, i2_4, RDw, xop_wb, x, xvlA[0][0x6d]},
    {OP_vst2_16,        0xf900094d, "vst2.16",        Mdq, RAw, LX2Dq, i2_4, RAw, no, x, xvlA[0][0x65]},
    {OP_vst2_16,        0xf900094f, "vst2.16",        Mdq, xx, LX2Dq, i2_4, xx, no, x, xvlA[0][0x6f]},
    {OP_vst2_16,        0xf9000940, "vst2.16",        Mdq, RAw, LX2Dq, i2_4, RDw, xop_wb, x, xvlA[0][0x70]},
    {OP_vst2_32,        0xf900098d, "vst2.32",        Mdq, RAw, LX2Dq, i2_4, RAw, no, x, xvlA[0][0x68]},
    {OP_vst2_32,        0xf900098f, "vst2.32",        Mdq, xx, LX2Dq, i2_4, xx, no, x, xvlA[0][0x72]},
    {OP_vst2_32,        0xf9000980, "vst2.32",        Mdq, RAw, LX2Dq, i2_4, RDw, xop_wb, x, xvlA[0][0x73]},
    {INVALID,           0xf90009cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90009cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf90009c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vst1_8,         0xf9000a0d, "vst1.8",         Mdq, RAw, LX2q, i2_4, RAw, no, x, xvlA[0][0x4a]},
    {OP_vst1_8,         0xf9000a0f, "vst1.8",         Mdq, xx, LX2q, i2_4, xx, no, x, xvlA[0][0x78]},
    {OP_vst1_8,         0xf9000a00, "vst1.8",         Mdq, RAw, LX2q, i2_4, RDw, xop_wb, x, xvlA[0][0x79]},
    {OP_vst1_16,        0xf9000a4d, "vst1.16",        Mdq, RAw, LX2q, i2_4, RAw, no, x, xvlA[0][0x4d]},
    {OP_vst1_16,        0xf9000a4f, "vst1.16",        Mdq, xx, LX2q, i2_4, xx, no, x, xvlA[0][0x7b]},
    {OP_vst1_16,        0xf9000a40, "vst1.16",        Mdq, RAw, LX2q, i2_4, RDw, xop_wb, x, xvlA[0][0x7c]},
    {OP_vst1_32,        0xf9000a8d, "vst1.32",        Mdq, RAw, LX2q, i2_4, RAw, no, x, xvlA[0][0x50]},
    {OP_vst1_32,        0xf9000a8f, "vst1.32",        Mdq, xx, LX2q, i2_4, xx, no, x, xvlA[0][0x7e]},
    {OP_vst1_32,        0xf9000a80, "vst1.32",        Mdq, RAw, LX2q, i2_4, RDw, xop_wb, x, xvlA[0][0x7f]},
    {OP_vst1_64,        0xf9000acd, "vst1.64",        Mdq, RAw, LX2q, i2_4, RAw, no, x, xvlA[0][0x53]},
    {OP_vst1_64,        0xf9000acf, "vst1.64",        Mdq, xx, LX2q, i2_4, xx, no, x, xvlA[0][0x81]},
    {OP_vst1_64,        0xf9000ac0, "vst1.64",        Mdq, RAw, LX2q, i2_4, RDw, xop_wb, x, xvlA[0][0x82]},
  }, { /* 1 */
    {OP_vld4_8,         0xf920000d, "vld4.8",         LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld4_8,         0xf920000f, "vld4.8",         LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x00]},
    {OP_vld4_8,         0xf9200000, "vld4.8",         LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x01]},
    {OP_vld4_16,        0xf920004d, "vld4.16",        LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld4_16,        0xf920004f, "vld4.16",        LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x03]},
    {OP_vld4_16,        0xf9200040, "vld4.16",        LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x04]},
    {OP_vld4_32,        0xf920008d, "vld4.32",        LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld4_32,        0xf920008f, "vld4.32",        LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x06]},
    {OP_vld4_32,        0xf9200080, "vld4.32",        LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x07]},
    {INVALID,           0xf92000cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92000cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92000c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld4_8,         0xf920010d, "vld4.8",         LX4Dq, RAw, Mqq, i2_4, RAw, no, x, xvlA[1][0x02]},
    {OP_vld4_8,         0xf920010f, "vld4.8",         LX4Dq, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x0c]},
    {OP_vld4_8,         0xf9200100, "vld4.8",         LX4Dq, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x0d]},
    {OP_vld4_16,        0xf920014d, "vld4.16",        LX4Dq, RAw, Mqq, i2_4, RAw, no, x, xvlA[1][0x05]},
    {OP_vld4_16,        0xf920014f, "vld4.16",        LX4Dq, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x0f]},
    {OP_vld4_16,        0xf9200140, "vld4.16",        LX4Dq, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x10]},
    {OP_vld4_32,        0xf920018d, "vld4.32",        LX4Dq, RAw, Mqq, i2_4, RAw, no, x, xvlA[1][0x08]},
    {OP_vld4_32,        0xf920018f, "vld4.32",        LX4Dq, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x12]},
    {OP_vld4_32,        0xf9200180, "vld4.32",        LX4Dq, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x13]},
    {INVALID,           0xf92001cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92001cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92001c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld1_8,         0xf920020d, "vld1.8",         LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld1_8,         0xf920020f, "vld1.8",         LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x18]},
    {OP_vld1_8,         0xf9200200, "vld1.8",         LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x19]},
    {OP_vld1_16,        0xf920024d, "vld1.16",        LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld1_16,        0xf920024f, "vld1.16",        LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x1b]},
    {OP_vld1_16,        0xf9200240, "vld1.16",        LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x1c]},
    {OP_vld1_32,        0xf920028d, "vld1.32",        LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld1_32,        0xf920028f, "vld1.32",        LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x1e]},
    {OP_vld1_32,        0xf9200280, "vld1.32",        LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x1f]},
    {OP_vld1_64,        0xf92002cd, "vld1.64",        LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld1_64,        0xf92002cf, "vld1.64",        LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x21]},
    {OP_vld1_64,        0xf92002c0, "vld1.64",        LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x22]},
    {OP_vld2_8,         0xf920030d, "vld2.8",         LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld2_8,         0xf920030f, "vld2.8",         LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x24]},
    {OP_vld2_8,         0xf9200300, "vld2.8",         LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x25]},
    {OP_vld2_16,        0xf920034d, "vld2.16",        LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld2_16,        0xf920034f, "vld2.16",        LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x27]},
    {OP_vld2_16,        0xf9200340, "vld2.16",        LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x28]},
    {OP_vld2_32,        0xf920038d, "vld2.32",        LX4q, RAw, Mqq, i2_4, RAw, no, x, END_LIST},
    {OP_vld2_32,        0xf920038f, "vld2.32",        LX4q, xx, Mqq, i2_4, xx, no, x, xvlA[1][0x2a]},
    {OP_vld2_32,        0xf9200380, "vld2.32",        LX4q, RAw, Mqq, i2_4, RDw, xop_wb, x, xvlA[1][0x2b]},
    {INVALID,           0xf92003cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92003cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92003c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld3_8,         0xf920040d, "vld3.8",         LX3q, RAw, M24, i2_4, RAw, no, x, END_LIST},
    {OP_vld3_8,         0xf920040f, "vld3.8",         LX3q, xx, M24, i2_4, xx, no, x, xvlA[1][0x30]},
    {OP_vld3_8,         0xf9200400, "vld3.8",         LX3q, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x31]},
    {OP_vld3_16,        0xf920044d, "vld3.16",        LX3q, RAw, M24, i2_4, RAw, no, x, END_LIST},
    {OP_vld3_16,        0xf920044f, "vld3.16",        LX3q, xx, M24, i2_4, xx, no, x, xvlA[1][0x33]},
    {OP_vld3_16,        0xf9200440, "vld3.16",        LX3q, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x34]},
    {OP_vld3_32,        0xf920048d, "vld3.32",        LX3q, RAw, M24, i2_4, RAw, no, x, END_LIST},
    {OP_vld3_32,        0xf920048f, "vld3.32",        LX3q, xx, M24, i2_4, xx, no, x, xvlA[1][0x36]},
    {OP_vld3_32,        0xf9200480, "vld3.32",        LX3q, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x37]},
    {INVALID,           0xf92004cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92004cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92004c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld3_8,         0xf920050d, "vld3.8",         LX3Dq, RAw, M24, i2_4, RAw, no, x, xvlA[1][0x32]},
    {OP_vld3_8,         0xf920050f, "vld3.8",         LX3Dq, xx, M24, i2_4, xx, no, x, xvlA[1][0x3c]},
    {OP_vld3_8,         0xf9200500, "vld3.8",         LX3Dq, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x3d]},
    {OP_vld3_16,        0xf920054d, "vld3.16",        LX3Dq, RAw, M24, i2_4, RAw, no, x, xvlA[1][0x35]},
    {OP_vld3_16,        0xf920054f, "vld3.16",        LX3Dq, xx, M24, i2_4, xx, no, x, xvlA[1][0x3f]},
    {OP_vld3_16,        0xf9200540, "vld3.16",        LX3Dq, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x40]},
    {OP_vld3_32,        0xf920058d, "vld3.32",        LX3Dq, RAw, M24, i2_4, RAw, no, x, xvlA[1][0x38]},
    {OP_vld3_32,        0xf920058f, "vld3.32",        LX3Dq, xx, M24, i2_4, xx, no, x, xvlA[1][0x42]},
    {OP_vld3_32,        0xf9200580, "vld3.32",        LX3Dq, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x43]},
    {INVALID,           0xf92005cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92005cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92005c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld1_8,         0xf920060d, "vld1.8",         LX3q, RAw, M24, i2_4, RAw, no, x, xvlA[1][0x7a]},
    {OP_vld1_8,         0xf920060f, "vld1.8",         LX3q, xx, M24, i2_4, xx, no, x, xvlA[1][0x48]},
    {OP_vld1_8,         0xf9200600, "vld1.8",         LX3q, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x49]},
    {OP_vld1_16,        0xf920064d, "vld1.16",        LX3q, RAw, M24, i2_4, RAw, no, x, xvlA[1][0x7d]},
    {OP_vld1_16,        0xf920064f, "vld1.16",        LX3q, xx, M24, i2_4, xx, no, x, xvlA[1][0x4b]},
    {OP_vld1_16,        0xf9200640, "vld1.16",        LX3q, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x4c]},
    {OP_vld1_32,        0xf920068d, "vld1.32",        LX3q, RAw, M24, i2_4, RAw, no, x, xvlA[1][0x80]},
    {OP_vld1_32,        0xf920068f, "vld1.32",        LX3q, xx, M24, i2_4, xx, no, x, xvlA[1][0x4e]},
    {OP_vld1_32,        0xf9200680, "vld1.32",        LX3q, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x4f]},
    {OP_vld1_64,        0xf92006cd, "vld1.64",        LX3q, RAw, M24, i2_4, RAw, no, x, xvlA[1][0x83]},
    {OP_vld1_64,        0xf92006cf, "vld1.64",        LX3q, xx, M24, i2_4, xx, no, x, xvlA[1][0x51]},
    {OP_vld1_64,        0xf92006c0, "vld1.64",        LX3q, RAw, M24, i2_4, RDw, xop_wb, x, xvlA[1][0x52]},
    {OP_vld1_8,         0xf920070d, "vld1.8",         VBq, RAw, Mq, i2_4, RAw, no, x, xvlA[1][0x4a]},/*XXX: some align values => undefined*/
    {OP_vld1_8,         0xf920070f, "vld1.8",         VBq, xx, Mq, i2_4, xx, no, x, xvlA[1][0x54]},/*XXX: combine align into memop?*/
    {OP_vld1_8,         0xf9200700, "vld1.8",         VBq, RAw, Mq, i2_4, RDw, xop_wb, x, xvlA[1][0x55]},
    {OP_vld1_16,        0xf920074d, "vld1.16",        VBq, RAw, Mq, i2_4, RAw, no, x, xvlA[1][0x4d]},
    {OP_vld1_16,        0xf920074f, "vld1.16",        VBq, xx, Mq, i2_4, xx, no, x, xvlA[1][0x57]},
    {OP_vld1_16,        0xf9200740, "vld1.16",        VBq, RAw, Mq, i2_4, RDw, xop_wb, x, xvlA[1][0x58]},
    {OP_vld1_32,        0xf920078d, "vld1.32",        VBq, RAw, Mq, i2_4, RAw, no, x, xvlA[1][0x50]},
    {OP_vld1_32,        0xf920078f, "vld1.32",        VBq, xx, Mq, i2_4, xx, no, x, xvlA[1][0x5a]},
    {OP_vld1_32,        0xf9200780, "vld1.32",        VBq, RAw, Mq, i2_4, RDw, xop_wb, x, xvlA[1][0x5b]},
    {OP_vld1_64,        0xf92007cd, "vld1.64",        VBq, RAw, Mq, i2_4, RAw, no, x, xvlA[1][0x53]},
    {OP_vld1_64,        0xf92007cf, "vld1.64",        VBq, xx, Mq, i2_4, xx, no, x, xvlA[1][0x5d]},
    {OP_vld1_64,        0xf92007c0, "vld1.64",        VBq, RAw, Mq, i2_4, RDw, xop_wb, x, xvlA[1][0x5e]},
    /* 0x80 */
    {OP_vld2_8,         0xf920080d, "vld2.8",         LX2q, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x26]},
    {OP_vld2_8,         0xf920080f, "vld2.8",         LX2q, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x60]},
    {OP_vld2_8,         0xf9200800, "vld2.8",         LX2q, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x61]},
    {OP_vld2_16,        0xf920084d, "vld2.16",        LX2q, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x29]},
    {OP_vld2_16,        0xf920084f, "vld2.16",        LX2q, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x63]},
    {OP_vld2_16,        0xf9200840, "vld2.16",        LX2q, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x64]},
    {OP_vld2_32,        0xf920088d, "vld2.32",        LX2q, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x2c]},
    {OP_vld2_32,        0xf920088f, "vld2.32",        LX2q, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x66]},
    {OP_vld2_32,        0xf9200880, "vld2.32",        LX2q, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x67]},
    {INVALID,           0xf92008cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92008cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92008c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld2_8,         0xf920090d, "vld2.8",         LX2Dq, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x62]},
    {OP_vld2_8,         0xf920090f, "vld2.8",         LX2Dq, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x6c]},
    {OP_vld2_8,         0xf9200900, "vld2.8",         LX2Dq, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x6d]},
    {OP_vld2_16,        0xf920094d, "vld2.16",        LX2Dq, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x65]},
    {OP_vld2_16,        0xf920094f, "vld2.16",        LX2Dq, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x6f]},
    {OP_vld2_16,        0xf9200940, "vld2.16",        LX2Dq, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x70]},
    {OP_vld2_32,        0xf920098d, "vld2.32",        LX2Dq, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x68]},
    {OP_vld2_32,        0xf920098f, "vld2.32",        LX2Dq, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x72]},
    {OP_vld2_32,        0xf9200980, "vld2.32",        LX2Dq, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x73]},
    {INVALID,           0xf92009cd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92009cf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf92009c0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld1_8,         0xf9200a0d, "vld1.8",         LX2q, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x1a]},
    {OP_vld1_8,         0xf9200a0f, "vld1.8",         LX2q, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x78]},
    {OP_vld1_8,         0xf9200a00, "vld1.8",         LX2q, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x79]},
    {OP_vld1_16,        0xf9200a4d, "vld1.16",        LX2q, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x1d]},
    {OP_vld1_16,        0xf9200a4f, "vld1.16",        LX2q, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x7b]},
    {OP_vld1_16,        0xf9200a40, "vld1.16",        LX2q, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x7c]},
    {OP_vld1_32,        0xf9200a8d, "vld1.32",        LX2q, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x20]},
    {OP_vld1_32,        0xf9200a8f, "vld1.32",        LX2q, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x7e]},
    {OP_vld1_32,        0xf9200a80, "vld1.32",        LX2q, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x7f]},
    {OP_vld1_64,        0xf9200acd, "vld1.64",        LX2q, RAw, Mdq, i2_4, RAw, no, x, xvlA[1][0x23]},
    {OP_vld1_64,        0xf9200acf, "vld1.64",        LX2q, xx, Mdq, i2_4, xx, no, x, xvlA[1][0x81]},
    {OP_vld1_64,        0xf9200ac0, "vld1.64",        LX2q, RAw, Mdq, i2_4, RDw, xop_wb, x, xvlA[1][0x82]},
  },
};

/* Indexed by bits (11:8,Y)*3+X where X is based on the value of 3:0:
 * + 0xd => 0
 * + 0xf => 1
 * + else => 2
 * And Y is:
 * + If bit 11 (0x.....8..) is set, the value of bit 6 (0x......4.)
 * + Else, the value of bit 5 (0x......2.).
 *
 * This requires some duplicate entries, marked below to make it easier to
 * reconfigure the table if we want to try a different arrangement.
 * It's just easier to deal w/ dups than tons of separate 2-entry tables
 * with indexes.
 */
const instr_info_t T32_ext_vldB[][96] = {
  { /* 0 */
    {OP_vst1_lane_8,    0xf980000d, "vst1.8",         Mb, RAw, VBb_q, i3_5, RAw, no, x, END_LIST},
    {OP_vst1_lane_8,    0xf980000f, "vst1.8",         Mb, xx, VBb_q, i3_5, xx, no, x, xvlB[0][0x00]},/*XXX: combine align into memop?*/
    {OP_vst1_lane_8,    0xf9800000, "vst1.8",         Mb, RAw, VBb_q, i3_5, RDw, xop_wb, x, xvlB[0][0x01]},
    {OP_vst1_lane_8,    0xf980002d, "vst1.8",         Mb, RAw, VBb_q, i3_5, RAw, no, x, DUP_ENTRY},
    {OP_vst1_lane_8,    0xf980002f, "vst1.8",         Mb, xx, VBb_q, i3_5, xx, no, x, DUP_ENTRY},/*XXX: combine align into memop?*/
    {OP_vst1_lane_8,    0xf9800020, "vst1.8",         Mb, RAw, VBb_q, i3_5, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vst2_lane_8,    0xf980010d, "vst2.8",         Mh, RAw, LX2b_q, i3_5, i1_4, xop_wb, x, END_LIST},
    {OP_vst2_lane_8,    0xf980010f, "vst2.8",         Mh, xx, LX2b_q, i3_5, i1_4, no, x, xvlB[0][0x06]},
    {OP_vst2_lane_8,    0xf9800100, "vst2.8",         Mh, RAw, LX2b_q, i3_5, i1_4, xop_wb2, x, xvlB[0][0x07]},
    {OP_vst2_lane_8,    0xf980012d, "vst2.8",         Mh, RAw, LX2b_q, i3_5, i1_4, xop_wb, x, DUP_ENTRY},
    {OP_vst2_lane_8,    0xf980012f, "vst2.8",         Mh, xx, LX2b_q, i3_5, i1_4, no, x, DUP_ENTRY},
    {OP_vst2_lane_8,    0xf9800120, "vst2.8",         Mh, RAw, LX2b_q, i3_5, i1_4, xop_wb2, x, DUP_ENTRY},
    {OP_vst3_lane_8,    0xf980020d, "vst3.8",         M3, RAw, LX3b_q, i3_5, RAw, no, x, END_LIST},
    {OP_vst3_lane_8,    0xf980020f, "vst3.8",         M3, xx, LX3b_q, i3_5, xx, no, x, xvlB[0][0x0c]},
    {OP_vst3_lane_8,    0xf9800200, "vst3.8",         M3, RAw, LX3b_q, i3_5, RDw, xop_wb, x, xvlB[0][0x0d]},
    {OP_vst3_lane_8,    0xf980022d, "vst3.8",         M3, RAw, LX3b_q, i3_5, RAw, no, x, DUP_ENTRY},
    {OP_vst3_lane_8,    0xf980022f, "vst3.8",         M3, xx, LX3b_q, i3_5, xx, no, x, DUP_ENTRY},
    {OP_vst3_lane_8,    0xf9800220, "vst3.8",         M3, RAw, LX3b_q, i3_5, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vst4_lane_8,    0xf980030d, "vst4.8",         Md, RAw, LX4b_q, i3_5, i1_4, xop_wb, x, END_LIST},
    {OP_vst4_lane_8,    0xf980030f, "vst4.8",         Md, xx, LX4b_q, i3_5, i1_4, no, x, xvlB[0][0x12]},
    {OP_vst4_lane_8,    0xf9800300, "vst4.8",         Md, RAw, LX4b_q, i3_5, i1_4, xop_wb2, x, xvlB[0][0x13]},
    {OP_vst4_lane_8,    0xf980032d, "vst4.8",         Md, RAw, LX4b_q, i3_5, i1_4, xop_wb, x, DUP_ENTRY},
    {OP_vst4_lane_8,    0xf980032f, "vst4.8",         Md, xx, LX4b_q, i3_5, i1_4, no, x, DUP_ENTRY},
    {OP_vst4_lane_8,    0xf9800320, "vst4.8",         Md, RAw, LX4b_q, i3_5, i1_4, xop_wb2, x, DUP_ENTRY},
    {OP_vst1_lane_16,   0xf980040d, "vst1.16",        Mh, RAw, VBh_q, i2_6, i1_4, xop_wb, x, END_LIST},
    {OP_vst1_lane_16,   0xf980040f, "vst1.16",        Mh, xx, VBh_q, i2_6, i1_4, no, x, xvlB[0][0x18]},
    {OP_vst1_lane_16,   0xf9800400, "vst1.16",        Mh, RAw, VBh_q, i2_6, i1_4, xop_wb2, x, xvlB[0][0x19]},
    {OP_vst1_lane_16,   0xf980042d, "vst1.16",        Mh, RAw, VBh_q, i2_6, i1_4, xop_wb, x, DUP_ENTRY},
    {OP_vst1_lane_16,   0xf980042f, "vst1.16",        Mh, xx, VBh_q, i2_6, i1_4, no, x, DUP_ENTRY},
    {OP_vst1_lane_16,   0xf9800420, "vst1.16",        Mh, RAw, VBh_q, i2_6, i1_4, xop_wb2, x, DUP_ENTRY},
    {OP_vst2_lane_16,   0xf980050d, "vst2.16",        Md, RAw, LX2h_q, i2_6, i1_4, xop_wb, x, END_LIST},
    {OP_vst2_lane_16,   0xf980050f, "vst2.16",        Md, xx, LX2h_q, i2_6, i1_4, no, x, xvlB[0][0x1e]},
    {OP_vst2_lane_16,   0xf9800500, "vst2.16",        Md, RAw, LX2h_q, i2_6, i1_4, xop_wb2, x, xvlB[0][0x1f]},
    {OP_vst2_lane_16,   0xf980052d, "vst2.16",        Md, RAw, LX2Dh_q, i2_6, i1_4, xop_wb, x, xvlB[0][0x20]},
    {OP_vst2_lane_16,   0xf980052f, "vst2.16",        Md, xx, LX2Dh_q, i2_6, i1_4, no, x, xvlB[0][0x21]},
    {OP_vst2_lane_16,   0xf9800520, "vst2.16",        Md, RAw, LX2Dh_q, i2_6, i1_4, xop_wb2, x, xvlB[0][0x22]},
    {OP_vst3_lane_16,   0xf980060d, "vst3.16",        M6, RAw, LX3h_q, i2_6, RAw, no, x, END_LIST},
    {OP_vst3_lane_16,   0xf980060f, "vst3.16",        M6, xx, LX3h_q, i2_6, xx, no, x, xvlB[0][0x24]},
    {OP_vst3_lane_16,   0xf9800600, "vst3.16",        M6, RAw, LX3h_q, i2_6, RDw, xop_wb, x, xvlB[0][0x25]},
    {OP_vst3_lane_16,   0xf980062d, "vst3.16",        M6, RAw, LX3Dh_q, i2_6, RAw, no, x, xvlB[0][0x26]},
    {OP_vst3_lane_16,   0xf980062f, "vst3.16",        M6, xx, LX3Dh_q, i2_6, xx, no, x, xvlB[0][0x27]},
    {OP_vst3_lane_16,   0xf9800620, "vst3.16",        M6, RAw, LX3Dh_q, i2_6, RDw, xop_wb, x, xvlB[0][0x28]},
    {OP_vst4_lane_16,   0xf980070d, "vst4.16",        Mq, RAw, LX4h_q, i2_6, i1_4, xop_wb, x, END_LIST},
    {OP_vst4_lane_16,   0xf980070f, "vst4.16",        Mq, xx, LX4h_q, i2_6, i1_4, no, x, xvlB[0][0x2a]},
    {OP_vst4_lane_16,   0xf9800700, "vst4.16",        Mq, RAw, LX4h_q, i2_6, i1_4, xop_wb2, x, xvlB[0][0x2b]},
    {OP_vst4_lane_16,   0xf980072d, "vst4.16",        Mq, RAw, LX4Dh_q, i2_6, i1_4, xop_wb, x, xvlB[0][0x2c]},
    {OP_vst4_lane_16,   0xf980072f, "vst4.16",        Mq, xx, LX4Dh_q, i2_6, i1_4, no, x, xvlB[0][0x2d]},
    {OP_vst4_lane_16,   0xf9800720, "vst4.16",        Mq, RAw, LX4Dh_q, i2_6, i1_4, xop_wb2, x, xvlB[0][0x2e]},
    /* 0x80 */
    {OP_vst1_lane_32,   0xf980080d, "vst1.32",        Md, RAw, VBd_q, i1_7, i2_4, xop_wb, x, END_LIST},
    {OP_vst1_lane_32,   0xf980080f, "vst1.32",        Md, xx, VBd_q, i1_7, i2_4, no, x, xvlB[0][0x30]},
    {OP_vst1_lane_32,   0xf9800800, "vst1.32",        Md, RAw, VBd_q, i1_7, i2_4, xop_wb2, x, xvlB[0][0x31]},
    {OP_vst1_lane_32,   0xf980084d, "vst1.32",        Md, RAw, VBd_q, i1_7, i2_4, xop_wb, x, DUP_ENTRY},
    {OP_vst1_lane_32,   0xf980084f, "vst1.32",        Md, xx, VBd_q, i1_7, i2_4, no, x, DUP_ENTRY},
    {OP_vst1_lane_32,   0xf9800840, "vst1.32",        Md, RAw, VBd_q, i1_7, i2_4, xop_wb2, x, DUP_ENTRY},
    {OP_vst2_lane_32,   0xf980090d, "vst2.32",        Mq, RAw, LX2d_q, i1_7, i2_4, xop_wb, x, END_LIST},
    {OP_vst2_lane_32,   0xf980090f, "vst2.32",        Mq, xx, LX2d_q, i1_7, i2_4, no, x, xvlB[0][0x36]},
    {OP_vst2_lane_32,   0xf9800900, "vst2.32",        Mq, RAw, LX2d_q, i1_7, i2_4, xop_wb2, x, xvlB[0][0x37]},
    {OP_vst2_lane_32,   0xf980094d, "vst2.32",        Mq, RAw, LX2Dd_q, i1_7, i2_4, xop_wb, x, xvlB[0][0x38]},
    {OP_vst2_lane_32,   0xf980094f, "vst2.32",        Mq, xx, LX2Dd_q, i1_7, i2_4, no, x, xvlB[0][0x39]},
    {OP_vst2_lane_32,   0xf9800940, "vst2.32",        Mq, RAw, LX2Dd_q, i1_7, i2_4, xop_wb2, x, xvlB[0][0x3a]},
    {OP_vst3_lane_32,   0xf9800a0d, "vst3.32",        M12, RAw, LX3d_q, i1_7, RAw, no, x, END_LIST},
    {OP_vst3_lane_32,   0xf9800a0f, "vst3.32",        M12, xx, LX3d_q, i1_7, xx, no, x, xvlB[0][0x3c]},
    {OP_vst3_lane_32,   0xf9800a00, "vst3.32",        M12, RAw, LX3d_q, i1_7, RDw, xop_wb, x, xvlB[0][0x3d]},
    {OP_vst3_lane_32,   0xf9800a4d, "vst3.32",        M12, RAw, LX3Dd_q, i1_7, RAw, no, x, xvlB[0][0x3e]},
    {OP_vst3_lane_32,   0xf9800a4f, "vst3.32",        M12, xx, LX3Dd_q, i1_7, xx, no, x, xvlB[0][0x3f]},
    {OP_vst3_lane_32,   0xf9800a40, "vst3.32",        M12, RAw, LX3Dd_q, i1_7, RDw, xop_wb, x, xvlB[0][0x40]},
    {OP_vst4_lane_32,   0xf9800b0d, "vst4.32",        Mdq, RAw, LX4d_q, i1_7, i2_4, xop_wb, x, END_LIST},
    {OP_vst4_lane_32,   0xf9800b0f, "vst4.32",        Mdq, xx, LX4d_q, i1_7, i2_4, no, x, xvlB[0][0x42]},
    {OP_vst4_lane_32,   0xf9800b00, "vst4.32",        Mdq, RAw, LX4d_q, i1_7, i2_4, xop_wb2, x, xvlB[0][0x43]},
    {OP_vst4_lane_32,   0xf9800b4d, "vst4.32",        Mdq, RAw, LX4Dd_q, i1_7, i2_4, xop_wb, x, xvlB[0][0x44]},
    {OP_vst4_lane_32,   0xf9800b4f, "vst4.32",        Mdq, xx, LX4Dd_q, i1_7, i2_4, no, x, xvlB[0][0x45]},
    {OP_vst4_lane_32,   0xf9800b40, "vst4.32",        Mdq, RAw, LX4Dd_q, i1_7, i2_4, xop_wb2, x, xvlB[0][0x46]},
    {INVALID,           0xf9800c0d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800c0f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800c00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800c4d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800c4f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800c40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800d0d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800d0f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800d00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800d4d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800d4f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800d40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800e0d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800e0f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800e00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800e4d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800e4f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800e40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800f0d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800f0f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800f00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800f4d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800f4f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9800f40, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 1 */
    {OP_vld1_lane_8,    0xf9a0000d, "vld1.8",         VBb_q, RAw, Mb, i3_5, RAw, no, x, END_LIST},
    {OP_vld1_lane_8,    0xf9a0000f, "vld1.8",         VBb_q, xx, Mb, i3_5, xx, no, x, xvlB[1][0x00]},/*XXX: combine align into memop?*/
    {OP_vld1_lane_8,    0xf9a00000, "vld1.8",         VBb_q, RAw, Mb, i3_5, RDw, xop_wb, x, xvlB[1][0x01]},
    {OP_vld1_lane_8,    0xf9a0002d, "vld1.8",         VBb_q, RAw, Mb, i3_5, RAw, no, x, DUP_ENTRY},
    {OP_vld1_lane_8,    0xf9a0002f, "vld1.8",         VBb_q, xx, Mb, i3_5, xx, no, x, DUP_ENTRY},/*XXX: combine align into memop?*/
    {OP_vld1_lane_8,    0xf9a00020, "vld1.8",         VBb_q, RAw, Mb, i3_5, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld2_lane_8,    0xf9a0010d, "vld2.8",         LX2b_q, RAw, Mh, i3_5, RAw, no, x, END_LIST},
    {OP_vld2_lane_8,    0xf9a0010f, "vld2.8",         LX2b_q, xx, Mh, i3_5, xx, no, x, xvlB[1][0x06]},
    {OP_vld2_lane_8,    0xf9a00100, "vld2.8",         LX2b_q, RAw, Mh, i3_5, RDw, xop_wb, x, xvlB[1][0x07]},
    {OP_vld2_lane_8,    0xf9a0012d, "vld2.8",         LX2b_q, RAw, Mh, i3_5, RAw, no, x, DUP_ENTRY},
    {OP_vld2_lane_8,    0xf9a0012f, "vld2.8",         LX2b_q, xx, Mh, i3_5, xx, no, x, DUP_ENTRY},
    {OP_vld2_lane_8,    0xf9a00120, "vld2.8",         LX2b_q, RAw, Mh, i3_5, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld3_lane_8,    0xf9a0020d, "vld3.8",         LX3b_q, RAw, M3, i3_5, RAw, no, x, END_LIST},
    {OP_vld3_lane_8,    0xf9a0020f, "vld3.8",         LX3b_q, xx, M3, i3_5, xx, no, x, xvlB[1][0x0c]},
    {OP_vld3_lane_8,    0xf9a00200, "vld3.8",         LX3b_q, RAw, M3, i3_5, RDw, xop_wb, x, xvlB[1][0x0d]},
    {OP_vld3_lane_8,    0xf9a0022d, "vld3.8",         LX3b_q, RAw, M3, i3_5, RAw, no, x, DUP_ENTRY},
    {OP_vld3_lane_8,    0xf9a0022f, "vld3.8",         LX3b_q, xx, M3, i3_5, xx, no, x, DUP_ENTRY},
    {OP_vld3_lane_8,    0xf9a00220, "vld3.8",         LX3b_q, RAw, M3, i3_5, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld4_lane_8,    0xf9a0030d, "vld4.8",         LX4b_q, RAw, Md, i3_5, RAw, no, x, END_LIST},
    {OP_vld4_lane_8,    0xf9a0030f, "vld4.8",         LX4b_q, xx, Md, i3_5, xx, no, x, xvlB[1][0x12]},
    {OP_vld4_lane_8,    0xf9a00300, "vld4.8",         LX4b_q, RAw, Md, i3_5, RDw, xop_wb, x, xvlB[1][0x13]},
    {OP_vld4_lane_8,    0xf9a0032d, "vld4.8",         LX4b_q, RAw, Md, i3_5, RAw, no, x, DUP_ENTRY},
    {OP_vld4_lane_8,    0xf9a0032f, "vld4.8",         LX4b_q, xx, Md, i3_5, xx, no, x, DUP_ENTRY},
    {OP_vld4_lane_8,    0xf9a00320, "vld4.8",         LX4b_q, RAw, Md, i3_5, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld1_lane_16,   0xf9a0040d, "vld1.16",        VBh_q, RAw, Mh, i2_6, i1_4, xop_wb, x, END_LIST},
    {OP_vld1_lane_16,   0xf9a0040f, "vld1.16",        VBh_q, xx, Mh, i2_6, i1_4, no, x, xvlB[1][0x18]},
    {OP_vld1_lane_16,   0xf9a00400, "vld1.16",        VBh_q, RAw, Mh, i2_6, i1_4, xop_wb2, x, xvlB[1][0x19]},
    {OP_vld1_lane_16,   0xf9a0042d, "vld1.16",        VBh_q, RAw, Mh, i2_6, i1_4, xop_wb, x, DUP_ENTRY},
    {OP_vld1_lane_16,   0xf9a0042f, "vld1.16",        VBh_q, xx, Mh, i2_6, i1_4, no, x, DUP_ENTRY},
    {OP_vld1_lane_16,   0xf9a00420, "vld1.16",        VBh_q, RAw, Mh, i2_6, i1_4, xop_wb2, x, DUP_ENTRY},
    {OP_vld2_lane_16,   0xf9a0050d, "vld2.16",        LX2h_q, RAw, Md, i2_6, i1_4, xop_wb, x, END_LIST},
    {OP_vld2_lane_16,   0xf9a0050f, "vld2.16",        LX2h_q, xx, Md, i2_6, i1_4, no, x, xvlB[1][0x1e]},
    {OP_vld2_lane_16,   0xf9a00500, "vld2.16",        LX2h_q, RAw, Md, i2_6, i1_4, xop_wb2, x, xvlB[1][0x1f]},
    {OP_vld2_lane_16,   0xf9a0052d, "vld2.16",        LX2Dh_q, RAw, Md, i2_6, i1_4, xop_wb, x, xvlB[1][0x20]},
    {OP_vld2_lane_16,   0xf9a0052f, "vld2.16",        LX2Dh_q, xx, Md, i2_6, i1_4, no, x, xvlB[1][0x21]},
    {OP_vld2_lane_16,   0xf9a00520, "vld2.16",        LX2Dh_q, RAw, Md, i2_6, i1_4, xop_wb2, x, xvlB[1][0x22]},
    {OP_vld3_lane_16,   0xf9a0060d, "vld3.16",        LX3h_q, RAw, M6, i2_6, i1_4, xop_wb, x, END_LIST},
    {OP_vld3_lane_16,   0xf9a0060f, "vld3.16",        LX3h_q, xx, M6, i2_6, i1_4, no, x, xvlB[1][0x24]},
    {OP_vld3_lane_16,   0xf9a00600, "vld3.16",        LX3h_q, RAw, M6, i2_6, i1_4, xop_wb2, x, xvlB[1][0x25]},
    {OP_vld3_lane_16,   0xf9a0062d, "vld3.16",        LX3Dh_q, RAw, M6, i2_6, i1_4, xop_wb, x, xvlB[1][0x26]},
    {OP_vld3_lane_16,   0xf9a0062f, "vld3.16",        LX3Dh_q, xx, M6, i2_6, i1_4, no, x, xvlB[1][0x27]},
    {OP_vld3_lane_16,   0xf9a00620, "vld3.16",        LX3Dh_q, RAw, M6, i2_6, i1_4, xop_wb2, x, xvlB[1][0x28]},
    {OP_vld4_lane_16,   0xf9a0070d, "vld4.16",        LX4h_q, RAw, Mq, i2_6, i1_4, xop_wb, x, END_LIST},
    {OP_vld4_lane_16,   0xf9a0070f, "vld4.16",        LX4h_q, xx, Mq, i2_6, i1_4, no, x, xvlB[1][0x2a]},
    {OP_vld4_lane_16,   0xf9a00700, "vld4.16",        LX4h_q, RAw, Mq, i2_6, i1_4, xop_wb2, x, xvlB[1][0x2b]},
    {OP_vld4_lane_16,   0xf9a0072d, "vld4.16",        LX4Dh_q, RAw, Mq, i2_6, i1_4, xop_wb, x, xvlB[1][0x2c]},
    {OP_vld4_lane_16,   0xf9a0072f, "vld4.16",        LX4Dh_q, xx, Mq, i2_6, i1_4, no, x, xvlB[1][0x2d]},
    {OP_vld4_lane_16,   0xf9a00720, "vld4.16",        LX4Dh_q, RAw, Mq, i2_6, i1_4, xop_wb2, x, xvlB[1][0x2e]},
    {OP_vld1_lane_32,   0xf9a0080d, "vld1.32",        VBd_q, RAw, Md, i1_7, i2_4, xop_wb, x, END_LIST},
    {OP_vld1_lane_32,   0xf9a0080f, "vld1.32",        VBd_q, xx, Md, i1_7, i2_4, no, x, xvlB[1][0x30]},
    {OP_vld1_lane_32,   0xf9a00800, "vld1.32",        VBd_q, RAw, Md, i1_7, i2_4, xop_wb2, x, xvlB[1][0x31]},
    {OP_vld1_lane_32,   0xf9a0082d, "vld1.32",        VBd_q, RAw, Md, i1_7, i2_4, xop_wb, x, DUP_ENTRY},
    {OP_vld1_lane_32,   0xf9a0082f, "vld1.32",        VBd_q, xx, Md, i1_7, i2_4, no, x, DUP_ENTRY},
    {OP_vld1_lane_32,   0xf9a00820, "vld1.32",        VBd_q, RAw, Md, i1_7, i2_4, xop_wb2, x, DUP_ENTRY},
    {OP_vld2_lane_32,   0xf9a0090d, "vld2.32",        LX2d_q, RAw, Mq, i1_7, i2_4, xop_wb, x, END_LIST},
    {OP_vld2_lane_32,   0xf9a0090f, "vld2.32",        LX2d_q, xx, Mq, i1_7, i2_4, no, x, xvlB[1][0x36]},
    {OP_vld2_lane_32,   0xf9a00900, "vld2.32",        LX2d_q, RAw, Mq, i1_7, i2_4, xop_wb2, x, xvlB[1][0x37]},
    {OP_vld2_lane_32,   0xf9a0094d, "vld2.32",        LX2Dd_q, RAw, Mq, i1_7, i2_4, xop_wb, x, xvlB[1][0x38]},
    {OP_vld2_lane_32,   0xf9a0094f, "vld2.32",        LX2Dd_q, xx, Mq, i1_7, i2_4, no, x, xvlB[1][0x39]},
    {OP_vld2_lane_32,   0xf9a00940, "vld2.32",        LX2Dd_q, RAw, Mq, i1_7, i2_4, xop_wb2, x, xvlB[1][0x3a]},
    {OP_vld3_lane_32,   0xf9a00a0d, "vld3.32",        LX3d_q, RAw, M12, i1_7, i2_4, xop_wb, x, END_LIST},
    {OP_vld3_lane_32,   0xf9a00a0f, "vld3.32",        LX3d_q, xx, M12, i1_7, i2_4, no, x, xvlB[1][0x3c]},
    {OP_vld3_lane_32,   0xf9a00a00, "vld3.32",        LX3d_q, RAw, M12, i1_7, i2_4, xop_wb2, x, xvlB[1][0x3d]},
    {OP_vld3_lane_32,   0xf9a00a4d, "vld3.32",        LX3Dd_q, RAw, M12, i1_7, i2_4, xop_wb, x, xvlB[1][0x3e]},
    {OP_vld3_lane_32,   0xf9a00a4f, "vld3.32",        LX3Dd_q, xx, M12, i1_7, i2_4, no, x, xvlB[1][0x3f]},
    {OP_vld3_lane_32,   0xf9a00a40, "vld3.32",        LX3Dd_q, RAw, M12, i1_7, i2_4, xop_wb2, x, xvlB[1][0x40]},
    {OP_vld4_lane_32,   0xf9a00b0d, "vld4.32",        LX4d_q, RAw, Mdq, i1_7, i2_4, xop_wb, x, END_LIST},
    {OP_vld4_lane_32,   0xf9a00b0f, "vld4.32",        LX4d_q, xx, Mdq, i1_7, i2_4, no, x, xvlB[1][0x42]},
    {OP_vld4_lane_32,   0xf9a00b00, "vld4.32",        LX4d_q, RAw, Mdq, i1_7, i2_4, xop_wb2, x, xvlB[1][0x43]},
    {OP_vld4_lane_32,   0xf9a00b4d, "vld4.32",        LX4Dd_q, RAw, Mdq, i1_7, i2_4, xop_wb, x, xvlB[1][0x44]},
    {OP_vld4_lane_32,   0xf9a00b4f, "vld4.32",        LX4Dd_q, xx, Mdq, i1_7, i2_4, no, x, xvlB[1][0x45]},
    {OP_vld4_lane_32,   0xf9a00b40, "vld4.32",        LX4Dd_q, RAw, Mdq, i1_7, i2_4, xop_wb2, x, xvlB[1][0x46]},
    {EXT_VLDD,          0xf9a00c0d, "(ext vldD  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDD,          0xf9a00c0f, "(ext vldD  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDD,          0xf9a00c00, "(ext vldD  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDD,          0xf9a00c4d, "(ext vldD  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDD,          0xf9a00c4f, "(ext vldD  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDD,          0xf9a00c40, "(ext vldD  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDC,          0xf9a00d0d, "(ext vldC  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDC,          0xf9a00d0f, "(ext vldC  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDC,          0xf9a00d00, "(ext vldC  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDC,          0xf9a00d4d, "(ext vldC  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDC,          0xf9a00d4f, "(ext vldC  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_VLDC,          0xf9a00d40, "(ext vldC  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_B4,            0xf9a00e0d, "(ext b4 23)",    xx, xx, xx, xx, xx, no, x, 23},
    {EXT_B4,            0xf9a00e0f, "(ext b4 23)",    xx, xx, xx, xx, xx, no, x, 23},
    {EXT_B4,            0xf9a00e00, "(ext b4 23)",    xx, xx, xx, xx, xx, no, x, 23},
    {EXT_B4,            0xf9a00e4d, "(ext b4 23)",    xx, xx, xx, xx, xx, no, x, 23},
    {EXT_B4,            0xf9a00e4f, "(ext b4 23)",    xx, xx, xx, xx, xx, no, x, 23},
    {EXT_B4,            0xf9a00e40, "(ext b4 23)",    xx, xx, xx, xx, xx, no, x, 23},
    {EXT_VLDD,          0xf9a00f0d, "(ext vldD  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_VLDD,          0xf9a00f0f, "(ext vldD  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_VLDD,          0xf9a00f00, "(ext vldD  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_VLDD,          0xf9a00f4d, "(ext vldD  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_VLDD,          0xf9a00f4f, "(ext vldD  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_VLDD,          0xf9a00f40, "(ext vldD  1)",  xx, xx, xx, xx, xx, no, x, 1},
  },
};

/* Indexed by bits (7:5)*3+X where X is based on the value of 3:0:
 * + 0xd => 0
 * + 0xf => 1
 * + else => 2
 */
const instr_info_t T32_ext_vldC[][24] = {
  { /* 0 */
    {OP_vld2_dup_8,     0xf9a00d0d, "vld2.8",         LX2q, RAw, Mh, RAw, xx, no, x, END_LIST},
    {OP_vld2_dup_8,     0xf9a00d0f, "vld2.8",         LX2q, xx, Mh, xx, xx, no, x, xvlC[0][0x00]},
    {OP_vld2_dup_8,     0xf9a00d00, "vld2.8",         LX2q, RAw, Mh, RDw, RAw, no, x, xvlC[0][0x01]},
    {OP_vld2_dup_8,     0xf9a00d2d, "vld2.8",         LX2Dq, RAw, Mh, RAw, xx, no, x, xvlC[0][0x02]},
    {OP_vld2_dup_8,     0xf9a00d2f, "vld2.8",         LX2Dq, xx, Mh, xx, xx, no, x, xvlC[0][0x03]},
    {OP_vld2_dup_8,     0xf9a00d20, "vld2.8",         LX2Dq, RAw, Mh, RDw, RAw, no, x, xvlC[0][0x04]},
    {OP_vld2_dup_16,    0xf9a00d4d, "vld2.16",        LX2q, RAw, Md, i1_4, RAw, no, x, END_LIST},
    {OP_vld2_dup_16,    0xf9a00d4f, "vld2.16",        LX2q, xx, Md, i1_4, xx, no, x, xvlC[0][0x06]},
    {OP_vld2_dup_16,    0xf9a00d40, "vld2.16",        LX2q, RAw, Md, i1_4, RDw, xop_wb, x, xvlC[0][0x07]},
    {OP_vld2_dup_16,    0xf9a00d6d, "vld2.16",        LX2Dq, RAw, Md, i1_4, RAw, no, x, xvlC[0][0x08]},
    {OP_vld2_dup_16,    0xf9a00d6f, "vld2.16",        LX2Dq, xx, Md, i1_4, xx, no, x, xvlC[0][0x09]},
    {OP_vld2_dup_16,    0xf9a00d60, "vld2.16",        LX2Dq, RAw, Md, i1_4, RDw, xop_wb, x, xvlC[0][0x0a]},
    {OP_vld2_dup_32,    0xf9a00d8d, "vld2.32",        LX2q, RAw, Mq, i1_4, RAw, no, x, END_LIST},
    {OP_vld2_dup_32,    0xf9a00d8f, "vld2.32",        LX2q, xx, Mq, i1_4, xx, no, x, xvlC[0][0x0c]},
    {OP_vld2_dup_32,    0xf9a00d80, "vld2.32",        LX2q, RAw, Mq, i1_4, RDw, xop_wb, x, xvlC[0][0x0d]},
    {OP_vld2_dup_32,    0xf9a00dad, "vld2.32",        LX2Dq, RAw, Mq, i1_4, RAw, no, x, xvlC[0][0x0e]},
    {OP_vld2_dup_32,    0xf9a00daf, "vld2.32",        LX2Dq, xx, Mq, i1_4, xx, no, x, xvlC[0][0x0f]},
    {OP_vld2_dup_32,    0xf9a00da0, "vld2.32",        LX2Dq, RAw, Mq, i1_4, RDw, xop_wb, x, xvlC[0][0x10]},
    {INVALID,           0xf9a00dcd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00dcf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00dc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00ded, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00def, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00de0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 2 */
    /* We've already ruled out bit4==1 as invalid */
    {OP_vld3_dup_8,     0xf9a00e0d, "vld3.8",         LX3q, RAw, M3, RAw, xx, no, x, END_LIST},
    {OP_vld3_dup_8,     0xf9a00e0f, "vld3.8",         LX3q, xx, M3, xx, xx, no, x, xvlC[1][0x00]},
    {OP_vld3_dup_8,     0xf9a00e00, "vld3.8",         LX3q, RAw, M3, RDw, RAw, no, x, xvlC[1][0x01]},
    {OP_vld3_dup_8,     0xf9a00e2d, "vld3.8",         LX3Dq, RAw, M3, RAw, xx, no, x, xvlC[1][0x02]},
    {OP_vld3_dup_8,     0xf9a00e2f, "vld3.8",         LX3Dq, xx, M3, xx, xx, no, x, xvlC[1][0x03]},
    {OP_vld3_dup_8,     0xf9a00e20, "vld3.8",         LX3Dq, RAw, M3, RDw, RAw, no, x, xvlC[1][0x04]},
    {OP_vld3_dup_16,    0xf9a00e4d, "vld3.16",        LX3q, RAw, M6, i1_4, RAw, no, x, END_LIST},
    {OP_vld3_dup_16,    0xf9a00e4f, "vld3.16",        LX3q, xx, M6, i1_4, xx, no, x, xvlC[1][0x06]},
    {OP_vld3_dup_16,    0xf9a00e40, "vld3.16",        LX3q, RAw, M6, i1_4, RDw, xop_wb, x, xvlC[1][0x07]},
    {OP_vld3_dup_16,    0xf9a00e6d, "vld3.16",        LX3Dq, RAw, M6, i1_4, RAw, no, x, xvlC[1][0x08]},
    {OP_vld3_dup_16,    0xf9a00e6f, "vld3.16",        LX3Dq, xx, M6, i1_4, xx, no, x, xvlC[1][0x09]},
    {OP_vld3_dup_16,    0xf9a00e60, "vld3.16",        LX3Dq, RAw, M6, i1_4, RDw, xop_wb, x, xvlC[1][0x0a]},
    {OP_vld3_dup_32,    0xf9a00e8d, "vld3.32",        LX3q, RAw, M12, i1_4, RAw, no, x, END_LIST},
    {OP_vld3_dup_32,    0xf9a00e8f, "vld3.32",        LX3q, xx, M12, i1_4, xx, no, x, xvlC[1][0x0c]},
    {OP_vld3_dup_32,    0xf9a00e80, "vld3.32",        LX3q, RAw, M12, i1_4, RDw, xop_wb, x, xvlC[1][0x0d]},
    {OP_vld3_dup_32,    0xf9a00ead, "vld3.32",        LX3Dq, RAw, M12, i1_4, RAw, no, x, xvlC[1][0x0e]},
    {OP_vld3_dup_32,    0xf9a00eaf, "vld3.32",        LX3Dq, xx, M12, i1_4, xx, no, x, xvlC[1][0x0f]},
    {OP_vld3_dup_32,    0xf9a00ea0, "vld3.32",        LX3Dq, RAw, M12, i1_4, RDw, xop_wb, x, xvlC[1][0x10]},
    {INVALID,           0xf9a00ecd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00ecf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00ec0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00eed, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00eef, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00ee0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits (7:4)*3+X where X is based on the value of 3:0:
 * + 0xd => 0
 * + 0xf => 1
 * + else => 2
 */
const instr_info_t T32_ext_vldD[][48] = {
  { /* 0 */
    {OP_vld1_dup_8,     0xf9a00c0d, "vld1.8",         VBq, RAw, Mb, RAw, xx, no, x, xvlD[0][0x08]},
    {OP_vld1_dup_8,     0xf9a00c0f, "vld1.8",         VBq, xx, Mb, xx, xx, no, x, xvlD[0][0x00]},
    {OP_vld1_dup_8,     0xf9a00c00, "vld1.8",         VBq, RAw, Mb, RDw, RAw, no, x, xvlD[0][0x01]},
    {INVALID,           0xf9a00c1d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00c1f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00c10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld1_dup_8,     0xf9a00c2d, "vld1.8",         LX2q, RAw, Mb, RAw, xx, no, x, END_LIST},
    {OP_vld1_dup_8,     0xf9a00c2f, "vld1.8",         LX2q, xx, Mb, xx, xx, no, x, xvlD[0][0x06]},
    {OP_vld1_dup_8,     0xf9a00c20, "vld1.8",         LX2q, RAw, Mb, RDw, RAw, no, x, xvlD[0][0x07]},
    {INVALID,           0xf9a00c3d, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00c3f, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00c30, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld1_dup_16,    0xf9a00c4d, "vld1.16",        VBq, RAw, Mh, i1_4, RAw, no, x, xvlD[0][0x14]},
    {OP_vld1_dup_16,    0xf9a00c4f, "vld1.16",        VBq, xx, Mh, i1_4, xx, no, x, xvlD[0][0x0c]},
    {OP_vld1_dup_16,    0xf9a00c40, "vld1.16",        VBq, RAw, Mh, i1_4, RDw, xop_wb, x, xvlD[0][0x0d]},
    {OP_vld1_dup_16,    0xf9a00c5d, "vld1.16",        VBq, RAw, Mh, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld1_dup_16,    0xf9a00c5f, "vld1.16",        VBq, xx, Mh, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld1_dup_16,    0xf9a00c50, "vld1.16",        VBq, RAw, Mh, i1_4, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld1_dup_16,    0xf9a00c6d, "vld1.16",        LX2q, RAw, Mh, i1_4, RAw, no, x, END_LIST},
    {OP_vld1_dup_16,    0xf9a00c6f, "vld1.16",        LX2q, xx, Mh, i1_4, xx, no, x, xvlD[0][0x12]},
    {OP_vld1_dup_16,    0xf9a00c60, "vld1.16",        LX2q, RAw, Mh, i1_4, RDw, xop_wb, x, xvlD[0][0x13]},
    {OP_vld1_dup_16,    0xf9a00c7d, "vld1.16",        LX2q, RAw, Mh, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld1_dup_16,    0xf9a00c7f, "vld1.16",        LX2q, xx, Mh, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld1_dup_16,    0xf9a00c70, "vld1.16",        LX2q, RAw, Mh, i1_4, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld1_dup_32,    0xf9a00c8d, "vld1.32",        VBq, RAw, Md, i1_4, RAw, no, x, xvlD[0][0x20]},
    {OP_vld1_dup_32,    0xf9a00c8f, "vld1.32",        VBq, xx, Md, i1_4, xx, no, x, xvlD[0][0x18]},
    {OP_vld1_dup_32,    0xf9a00c80, "vld1.32",        VBq, RAw, Md, i1_4, RDw, xop_wb, x, xvlD[0][0x19]},
    {OP_vld1_dup_32,    0xf9a00c9d, "vld1.32",        VBq, RAw, Md, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld1_dup_32,    0xf9a00c9f, "vld1.32",        VBq, xx, Md, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld1_dup_32,    0xf9a00c90, "vld1.32",        VBq, RAw, Md, i1_4, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld1_dup_32,    0xf9a00cad, "vld1.32",        LX2q, RAw, Md, i1_4, RAw, no, x, END_LIST},
    {OP_vld1_dup_32,    0xf9a00caf, "vld1.32",        LX2q, xx, Md, i1_4, xx, no, x, xvlD[0][0x1e]},
    {OP_vld1_dup_32,    0xf9a00ca0, "vld1.32",        LX2q, RAw, Md, i1_4, RDw, xop_wb, x, xvlD[0][0x1f]},
    {OP_vld1_dup_32,    0xf9a00cbd, "vld1.32",        LX2q, RAw, Md, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld1_dup_32,    0xf9a00cbf, "vld1.32",        LX2q, xx, Md, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld1_dup_32,    0xf9a00cb0, "vld1.32",        LX2q, RAw, Md, i1_4, RDw, xop_wb, x, DUP_ENTRY},
    {INVALID,           0xf9a00ccd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00ccf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00cc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00cdd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00cdf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00cd0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00ced, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00cef, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00ce0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00cfd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00cff, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00cf0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 1 */
    {OP_vld4_dup_8,     0xf9a00f0d, "vld4.8",         LX4q, RAw, Md, RAw, xx, no, x, END_LIST},
    {OP_vld4_dup_8,     0xf9a00f0f, "vld4.8",         LX4q, xx, Md, xx, xx, no, x, xvlD[1][0x00]},
    {OP_vld4_dup_8,     0xf9a00f00, "vld4.8",         LX4q, RAw, Md, RDw, RAw, no, x, xvlD[1][0x01]},
    {OP_vld4_dup_8,     0xf9a00f1d, "vld4.8",         LX4q, RAw, Md, RAw, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_8,     0xf9a00f1f, "vld4.8",         LX4q, xx, Md, xx, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_8,     0xf9a00f10, "vld4.8",         LX4q, RAw, Md, RDw, RAw, no, x, DUP_ENTRY},
    {OP_vld4_dup_8,     0xf9a00f2d, "vld4.8",         LX4Dq, RAw, Md, RAw, xx, no, x, xvlD[1][0x02]},
    {OP_vld4_dup_8,     0xf9a00f2f, "vld4.8",         LX4Dq, xx, Md, xx, xx, no, x, xvlD[1][0x06]},
    {OP_vld4_dup_8,     0xf9a00f20, "vld4.8",         LX4Dq, RAw, Md, RDw, RAw, no, x, xvlD[1][0x07]},
    {OP_vld4_dup_8,     0xf9a00f3d, "vld4.8",         LX4Dq, RAw, Md, RAw, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_8,     0xf9a00f3f, "vld4.8",         LX4Dq, xx, Md, xx, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_8,     0xf9a00f30, "vld4.8",         LX4Dq, RAw, Md, RDw, RAw, no, x, DUP_ENTRY},
    {OP_vld4_dup_16,    0xf9a00f4d, "vld4.16",        LX4q, RAw, Mq, i1_4, RAw, no, x, END_LIST},
    {OP_vld4_dup_16,    0xf9a00f4f, "vld4.16",        LX4q, xx, Mq, i1_4, xx, no, x, xvlD[1][0x0c]},
    {OP_vld4_dup_16,    0xf9a00f40, "vld4.16",        LX4q, RAw, Mq, i1_4, RDw, xop_wb, x, xvlD[1][0x0d]},
    {OP_vld4_dup_16,    0xf9a00f5d, "vld4.16",        LX4q, RAw, Mq, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld4_dup_16,    0xf9a00f5f, "vld4.16",        LX4q, xx, Mq, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_16,    0xf9a00f50, "vld4.16",        LX4q, RAw, Mq, i1_4, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld4_dup_16,    0xf9a00f6d, "vld4.16",        LX4Dq, RAw, Mq, i1_4, RAw, no, x, xvlD[1][0x0e]},
    {OP_vld4_dup_16,    0xf9a00f6f, "vld4.16",        LX4Dq, xx, Mq, i1_4, xx, no, x, xvlD[1][0x12]},
    {OP_vld4_dup_16,    0xf9a00f60, "vld4.16",        LX4Dq, RAw, Mq, i1_4, RDw, xop_wb, x, xvlD[1][0x13]},
    {OP_vld4_dup_16,    0xf9a00f7d, "vld4.16",        LX4Dq, RAw, Mq, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld4_dup_16,    0xf9a00f7f, "vld4.16",        LX4Dq, xx, Mq, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_16,    0xf9a00f70, "vld4.16",        LX4Dq, RAw, Mq, i1_4, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00f8d, "vld4.32",        LX4q, RAw, Mdq, i1_4, RAw, no, x, END_LIST},
    {OP_vld4_dup_32,    0xf9a00f8f, "vld4.32",        LX4q, xx, Mdq, i1_4, xx, no, x, xvlD[1][0x18]},
    {OP_vld4_dup_32,    0xf9a00f80, "vld4.32",        LX4q, RAw, Mdq, i1_4, RDw, xop_wb, x, xvlD[1][0x19]},
    {OP_vld4_dup_32,    0xf9a00f9d, "vld4.32",        LX4q, RAw, Mdq, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00f9f, "vld4.32",        LX4q, xx, Mdq, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00f90, "vld4.32",        LX4q, RAw, Mdq, i1_4, RDw, xop_wb, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00fad, "vld4.32",        LX4Dq, RAw, Mdq, i1_4, RAw, no, x, xvlD[1][0x1a]},
    {OP_vld4_dup_32,    0xf9a00faf, "vld4.32",        LX4Dq, xx, Mdq, i1_4, xx, no, x, xvlD[1][0x1e]},
    {OP_vld4_dup_32,    0xf9a00fa0, "vld4.32",        LX4Dq, RAw, Mdq, i1_4, RDw, xop_wb, x, xvlD[1][0x1f]},
    {OP_vld4_dup_32,    0xf9a00fbd, "vld4.32",        LX4Dq, RAw, Mdq, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00fbf, "vld4.32",        LX4Dq, xx, Mdq, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00fb0, "vld4.32",        LX4Dq, RAw, Mdq, i1_4, RDw, xop_wb, x, DUP_ENTRY},
    {INVALID,           0xf9a00fcd, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00fcf, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00fc0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld4_dup_32,    0xf9a00fdd, "vld4.32",        LX4Dq, RAw, Mdq, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00fdf, "vld4.32",        LX4Dq, xx, Mdq, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00fd0, "vld4.32",        LX4Dq, RAw, Mdq, i1_4, RDw, xop_wb, x, DUP_ENTRY},
    {INVALID,           0xf9a00fed, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00fef, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,           0xf9a00fe0, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_vld4_dup_32,    0xf9a00fdd, "vld4.32",        LX4Dq, RAw, Mdq, i1_4, RAw, no, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00fdf, "vld4.32",        LX4Dq, xx, Mdq, i1_4, xx, no, x, DUP_ENTRY},
    {OP_vld4_dup_32,    0xf9a00fd0, "vld4.32",        LX4Dq, RAw, Mdq, i1_4, RDw, xop_wb, x, DUP_ENTRY},
  },
};

/* Indexed by:
 * + if 11:10 != 2, then index 0;
 * + else, 9:8,6
 * XXX: this is to handle OP_vtb{l,x} only and it adds an extra step
 * for a lot of other opcodes -- can we do better?
 */
const instr_info_t T32_ext_vtb[][9] = {
  { /* 0 */
    {EXT_BITS16,        0xffb00000, "(ext bits16 8)", xx, xx, xx, xx, xx, no, x, 8},
    {OP_vtbl_8,         0xffb00800, "vtbl.8",         VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vtbx_8,         0xffb00840, "vtbx.8",         VBq, xx, VAq, VCq, xx, no, x, END_LIST},
    {OP_vtbl_8,         0xffb00900, "vtbl.8",         VBq, xx, LXA2q, VCq, xx, no, x, xvtb[0][0x01]},
    {OP_vtbx_8,         0xffb00940, "vtbx.8",         VBq, xx, LXA2q, VCq, xx, no, x, xvtb[0][0x02]},
    {OP_vtbl_8,         0xffb00a00, "vtbl.8",         VBq, xx, LXA3q, VCq, xx, no, x, xvtb[0][0x03]},
    {OP_vtbx_8,         0xffb00a40, "vtbx.8",         VBq, xx, LXA3q, VCq, xx, no, x, xvtb[0][0x04]},
    {OP_vtbl_8,         0xffb00b00, "vtbl.8",         VBq, xx, LXA4q, VCq, xx, no, x, xvtb[0][0x05]},
    {OP_vtbx_8,         0xffb00b40, "vtbx.8",         VBq, xx, LXA4q, VCq, xx, no, x, xvtb[0][0x06]},
  },
};

/* clang-format on */
