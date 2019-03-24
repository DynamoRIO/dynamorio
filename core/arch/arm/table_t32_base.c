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
 * We refer to the first half-word as A and the second as B.
 */

// We skip auto-formatting for the entire file to keep our single-line table entries:
/* clang-format off */

/****************************************************************************
 * Top-level T32 table for non-coprocessor instructions starting with 0xe.
 * Indexed by bits A9:4.
 */
const instr_info_t T32_base_e[] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    /* 80 */
    {OP_srsdb,    0xe80dc000, "srsdb",  Mq, xx, i5, LRw, SPSR, no, x, xbase[0x02]},/*PUW=000*/
    {OP_rfedb,    0xe810c000, "rfedb",  CPSR, xx, Mq, xx, xx, no, fWNZCVQG, END_LIST},/*PUW=000*/
    {OP_srsdb,    0xe82dc000, "srsdb",  Mq, SPw, i5, SPw, LRw, xop, x, xexop[0x6]},/*PUW=001*/
    {OP_rfedb,    0xe830c000, "rfedb",  RAw, CPSR, Mq, RAw, xx, no, fWNZCVQG, xbase[0x01]},/*PUW=001*/
    {OP_strex,    0xe8400000, "strex",  MP8Xw, RCw, RBw, xx, xx, no, x, END_LIST},
    {OP_ldrex,    0xe8500f00, "ldrex",  RBw, xx, MP8Xw, xx, xx, no, x, END_LIST},
    {OP_strd,     0xe8600000, "strd",   Mq, RAw, RBw, RCw, n8, xop_wb, x, END_LIST},/*PUW=001*/
    {OP_ldrd,     0xe8700000, "ldrd",   RBw, RCw, RAw, Mq, n8, xop_wb|dstX3, x, END_LIST},/*PUW=001*/
    {OP_stm,      0xe8800000, "stm",    Ml, xx, L14w, xx, xx, no, x, END_LIST},/*PUW=010*/
    {OP_ldm,      0xe8900000, "ldm",    L15w, xx, Ml, xx, xx, no, x, END_LIST},/*PUW=010*/
    {OP_stm,      0xe8a00000, "stm",    Ml, RAw, L14w, RAw, xx, no, x, xbase[0x08]},/*PUW=011*/
    {OP_ldm,      0xe8b00000, "ldm",    L15w, RAw, Ml, RAw, xx, no, x, xbase[0x09]},/*PUW=011*//*"pop" for RA==SP*/
    {EXT_B7_4,    0xe8c00000, "(ext b7_4 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_B7_4,    0xe8d00000, "(ext b7_4 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_strd,     0xe8e00000, "strd",   Mq, RAw, RBw, RCw, i8, xop_wb, x, xbase[0x06]},/*PUW=011*/
    {OP_ldrd,     0xe8f00000, "ldrd",   RBw, RCw, RAw, Mq, i8, xop_wb|dstX3, x, xbase[0x07]},/*PUW=011*/
    /* 90 */
    {OP_stmdb,    0xe9000000, "stmdb",  MDBl, xx, L14w, xx, xx, no, x, xbase[0x12]},/*PUW=100*/
    {OP_ldmdb,    0xe9100000, "ldmdb",  L15w, xx, MDBl, xx, xx, no, x, xbase[0x13]},/*PUW=100*/
    {OP_stmdb,    0xe9200000, "stmdb",  MDBl, RAw, L14w, RAw, xx, no, x, END_LIST},/*PUW=101*//*"push" if RA==sp*/
    {OP_ldmdb,    0xe9300000, "ldmdb",  L15w, RAw, MDBl, RAw, xx, no, x, END_LIST},/*PUW=101*/
    {OP_strd,     0xe9400000, "strd",   MN8Xq, xx, RBw, RCw, xx, no, x, xbase[0x1e]},/*PUW=100*/
    {OP_ldrd,     0xe9500000, "ldrd",   RBw, RCw, MN8Xq, xx, xx, no, x, xbase[0x1f]},/*PUW=100*/
    {OP_strd,     0xe9600000, "strd",   MN8Xq, RAw, RBw, RCw, n8x4, xop_wb, x, xbase[0x0e]},/*PUW=101*/
    {OP_ldrd,     0xe9700000, "ldrd",   RBw, RCw, RAw, MN8Xq, n8x4, xop_wb|dstX3, x, xbase[0x0f]},/*PUW=101*/
    {OP_srs,      0xe98dc000, "srs",    Mq, xx, i5, LRw, SPSR, no, x, xbase[0x1a]},/*PUW=110*/
    {OP_rfe,      0xe990c000, "rfe",    CPSR, xx, Mq, xx, xx, no, fWNZCVQG, xbase[0x1b]},/*PUW=110*/
    {OP_srs,      0xe9adc000, "srs",    Mq, SPw, i5, SPw, LRw, xop, x, xexop[0x6]},/*PUW=111*/
    {OP_rfe,      0xe9b0c000, "rfe",    RAw, CPSR, Mq, RAw, xx, no, fWNZCVQG, END_LIST},/*PUW=111*/
    {OP_strd,     0xe9c00000, "strd",   MP8Xq, xx, RBw, RCw, xx, no, x, xbase[0x14]},/*PUW=110*/
    {OP_ldrd,     0xe9d00000, "ldrd",   RBw, RCw, MP8Xq, xx, xx, no, x, xbase[0x15]},/*PUW=110*/
    {OP_strd,     0xe9e00000, "strd",   MP8Xq, RAw, RBw, RCw, i8x4, xop_wb, x, xbase[0x16]},/*PUW=111*/
    {OP_ldrd,     0xe9f00000, "ldrd",   RBw, RCw, RAw, MP8Xq, i8x4, xop_wb|dstX3, x, xbase[0x17]},/*PUW=111*/
    /* a0 */
    {OP_and,      0xea000000, "and",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, x, END_LIST},
    {EXT_RCPC,    0xea100000, "(ext rcpc 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_bic,      0xea200000, "bic",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, x, END_LIST},
    {OP_bics,     0xea300000, "bics",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZC, END_LIST},
    {EXT_RAPC,    0xea400000, "(ext rapc 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_RAPC,    0xea500000, "(ext rapc 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_RAPC,    0xea600000, "(ext rapc 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_RAPC,    0xea700000, "(ext rapc 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_eor,      0xea800000, "eor",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, x, END_LIST},
    {EXT_RCPC,    0xea900000, "(ext rcpc 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,     0xeaa00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeab00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_B5_4,    0xeac00000, "(ext b5_4 2)", xx, xx, xx, xx, xx, no, x, 2},
    {INVALID,     0xead00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeae00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeaf00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* b0 */
    {OP_add,      0xeb000000, "add",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, x, END_LIST},
    {EXT_RCPC,    0xeb000000, "(ext rcpc 2)", xx, xx, xx, xx, xx, no, x, 2},
    {INVALID,     0xeb200000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeb300000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_adc,      0xeb400000, "adc",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fRC, END_LIST},
    {OP_adcs,     0xeb500000, "adcs",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fRC|fWNZCV, END_LIST},
    {OP_sbc,      0xeb600000, "sbc",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fRC, END_LIST},
    {OP_sbcs,     0xeb700000, "sbcs",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fRC|fWNZCV, END_LIST},
    {INVALID,     0xeb800000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xeb900000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_sub,      0xeba00000, "sub",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, x, END_LIST},
    {EXT_RCPC,    0xebb00000, "(ext rcpc 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_rsb,      0xebc00000, "rsb",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, x, END_LIST},
    {OP_rsbs,     0xebd00000, "rsbs",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZCV, END_LIST},
    {INVALID,     0xebe00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xebf00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
};

/****************************************************************************
 * Top-level T32 table for non-coprocessor instructions starting with 0xf.
 * Indexed by bits A11,B15:14,B12.
 */
const instr_info_t T32_base_f[] = {
    {EXT_FOPC8,   0xf0000000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf0001000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf0004000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf0005000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_A9_7_eq1,0xf0008000, "(ext a9_7_eq1 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_b,        0xf0009000, "b",      xx, xx, j24x26_13_11_16_0, xx, xx, no, x, xa97[0][0x01]},
    {OP_blx,      0xf000c000, "blx",    LRw, xx, j24x26_13_11_16_0, xx, xx, no, x, END_LIST},
    {OP_bl,       0xf000d000, "bl",     LRw, xx, j24x26_13_11_16_0, xx, xx, no, x, END_LIST},
    {EXT_FOPC8,   0xf8000000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf8001000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf8004000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf8005000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf8008000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf8009000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf800c000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FOPC8,   0xf800d000, "(ext fopc8 0)", xx, xx, xx, xx, xx, no, x, 0},
};

/* High--level T32 table for non-coprocessor instructions starting with 0xf
 * and either with bit B15 == 0 or bit A11 == 1.
 * Indexed by bits A11:4 but stop at 0xfb.
 */
const instr_info_t T32_ext_fopc8[][192] = {
  { /* 0 */
    /* 00 */
    {OP_and,      0xf0000000, "and",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xbase[0x20]},
    {EXT_RCPC,    0xf0100000, "(ext rcpc 4)", xx, xx, xx, xx, xx, no, x, 4},
    {OP_bic,      0xf0200000, "bic",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xbase[0x22]},
    {OP_bics,     0xf0300000, "bics",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, xbase[0x23]},
    {EXT_RAPC,    0xf0400000, "(ext rapc 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_RAPC,    0xf0500000, "(ext rapc 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_RAPC,    0xf0600000, "(ext rapc 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_RAPC,    0xf0700000, "(ext rapc 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_eor,      0xf0800000, "eor",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xbase[0x28]},
    {EXT_RCPC,    0xf0900000, "(ext rcpc 5)", xx, xx, xx, xx, xx, no, x, 5},
    {INVALID,     0xf0a00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf0b00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf0c00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf0d00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf0e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf0f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* 10 */
    {OP_add,      0xf1000000, "add",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xbase[0x30]},
    {EXT_RCPC,    0xf1100000, "(ext rcpc 6)", xx, xx, xx, xx, xx, no, x, 6},
    {INVALID,     0xf1200000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf1300000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_adc,      0xf1400000, "adc",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xbase[0x34]},
    {OP_adcs,     0xf1500000, "adcs",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, xbase[0x35]},
    {OP_sbc,      0xf1600000, "sbc",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xbase[0x36]},
    {OP_sbcs,     0xf1700000, "sbcs",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, xbase[0x37]},
    {INVALID,     0xf1800000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf1900000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_sub,      0xf1a00000, "sub",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xbase[0x3a]},
    {EXT_RCPC,    0xf1b00000, "(ext rcpc 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_rsb,      0xf1c00000, "rsb",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xbase[0x3c]},
    {OP_rsbs,     0xf1d00000, "rsbs",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, xbase[0x3d]},
    {INVALID,     0xf1e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf1f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* 20 */
    {OP_addw,     0xf2000000, "addw",   RCw, xx, RAw, i12x26_12_0_z, xx, no, x, END_LIST},
    {INVALID,     0xf2100000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2200000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2300000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_movw,     0xf2400000, "movw",   RCw, xx, i16x16_26_12_0, xx, xx, no, x, END_LIST},
    {INVALID,     0xf2500000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2600000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2700000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2800000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2900000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_subw,     0xf2a00000, "subw",   RCw, xx, RAw, i12x26_12_0_z, xx, no, x, END_LIST},
    {INVALID,     0xf2b00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_movt,     0xf2c00000, "movt",   RCt, xx, i16x16_26_12_0, xx, xx, no, x, END_LIST},
    {INVALID,     0xf2d00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* 30 */
    {OP_ssat,     0xf3000000, "ssat",   RCw, i5, RAw, sh1_21, i5x12_6, srcX4, fWQ, END_LIST},
    {INVALID,     0xf3100000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ssat16,   0xf3200000, "ssat16", RCw, xx, i4, RAw, xx, no, fWQ, END_LIST},
    {INVALID,     0xf3300000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_sbfx,     0xf3400000, "sbfx",   RCw, xx, RAw, i5x12_6, i5, no, x, END_LIST},
    {INVALID,     0xf3500000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_RAPC,    0xf3600000, "(ext rapc 8)", xx, xx, xx, xx, xx, no, x, 8},
    {INVALID,     0xf3700000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_usat,     0xf3800000, "usat",   RCw, i5, RAw, sh1_21, i5x12_6, srcX4, fWQ, END_LIST},
    {INVALID,     0xf3900000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_usat16,   0xf3a00000, "usat16", RCw, xx, i4, RAw, xx, no, fWQ, END_LIST},
    {INVALID,     0xf3b00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ubfx,     0xf3c00000, "ubfx",   RCw, xx, RAw, i5x12_6, i5, no, x, END_LIST},
    {INVALID,     0xf3d00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* 40 */
    {OP_and,      0xf4000000, "and",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {EXT_RCPC,    0xf4100000, "(ext rcpc 8)", xx, xx, xx, xx, xx, no, x, 8},
    {OP_bic,      0xf4200000, "bic",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {OP_bics,     0xf4300000, "bics",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, DUP_ENTRY},
    {EXT_RAPC,    0xf4400000, "(ext rapc 9)", xx, xx, xx, xx, xx, no, x, 9},
    {EXT_RAPC,    0xf4500000, "(ext rapc 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_RAPC,    0xf4600000, "(ext rapc 11)", xx, xx, xx, xx, xx, no, x, 11},
    {EXT_RAPC,    0xf4700000, "(ext rapc 12)", xx, xx, xx, xx, xx, no, x, 12},
    {OP_eor,      0xf4800000, "eor",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {EXT_RCPC,    0xf4900000, "(ext rcpc 9)", xx, xx, xx, xx, xx, no, x, 9},
    {INVALID,     0xf4a00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf4b00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf4c00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf4d00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf4e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf4f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* 50 */
    {OP_add,      0xf5000000, "add",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {EXT_RCPC,    0xf5100000, "(ext rcpc 10)", xx, xx, xx, xx, xx, no, x, 10},
    {INVALID,     0xf5200000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf5300000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_adc,      0xf5400000, "adc",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {OP_adcs,     0xf5500000, "adcs",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, DUP_ENTRY},
    {OP_sbc,      0xf5600000, "sbc",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {OP_sbcs,     0xf5700000, "sbcs",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, DUP_ENTRY},
    {INVALID,     0xf5800000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf5900000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_sub,      0xf5a00000, "sub",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {EXT_RCPC,    0xf5b00000, "(ext rcpc 11)", xx, xx, xx, xx, xx, no, x, 11},
    {OP_rsb,      0xf5c00000, "rsb",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {OP_rsbs,     0xf5d00000, "rsbs",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, DUP_ENTRY},
    {INVALID,     0xf5e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf5f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* 60 */
    {OP_addw,     0xf6000000, "addw",   RCw, xx, RAw, i12x26_12_0_z, xx, no, x, DUP_ENTRY},
    {INVALID,     0xf6100000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf6200000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf6300000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_movw,     0xf6400000, "movw",   RCw, xx, i16x16_26_12_0, xx, xx, no, x, DUP_ENTRY},
    {INVALID,     0xf6500000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf6600000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf6700000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf6800000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf6900000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_subw,     0xf6a00000, "subw",   RCw, xx, RAw, i12x26_12_0_z, xx, no, x, DUP_ENTRY},
    {INVALID,     0xf6b00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_movt,     0xf6c00000, "movt",   RCt, xx, i16x16_26_12_0, xx, xx, no, x, DUP_ENTRY},
    {INVALID,     0xf6d00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf6e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf6f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* 70 */
    {INVALID,     0xf7000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7100000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7200000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7300000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7400000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7500000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7600000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7700000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7800000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7900000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7a00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7b00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf4c00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf4d00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf4e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf4f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* 80 */
    {EXT_OPCBX,   0xf8000000, "(ext opcbx 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_RAPC,    0xf8100000, "(ext rapc 13)", xx, xx, xx, xx, xx, no, x, 13},
    {EXT_OPCBX,   0xf8200000, "(ext opcbx 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_RAPC,    0xf8300000, "(ext rapc 14)", xx, xx, xx, xx, xx, no, x, 14},
    {EXT_OPCBX,   0xf8400000, "(ext opcbx 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_RAPC,    0xf8500000, "(ext rapc 15)", xx, xx, xx, xx, xx, no, x, 15},
    {INVALID,     0xf8600000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf8700000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_strb,     0xf8800000, "strb",   MP12b, xx, RBb, xx, xx, no, x, END_LIST},
    {EXT_RAPC,    0xf8900000, "(ext rapc 16)", xx, xx, xx, xx, xx, no, x, 16},
    {OP_strh,     0xf8a00000, "strh",   MP12h, xx, RBh, xx, xx, no, x, END_LIST},
    {EXT_RAPC,    0xf8b00000, "(ext rapc 17)", xx, xx, xx, xx, xx, no, x, 17},
    {OP_str,      0xf8c00000, "str",    MP12w, xx, RBw, xx, xx, no, x, END_LIST},
    {EXT_RAPC,    0xf8d00000, "(ext rapc 18)", xx, xx, xx, xx, xx, no, x, 18},
    {INVALID,     0xf8e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf8f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* 90 */
    {EXT_VLDA,    0xf9000000, "(ext vldA  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_RAPC,    0xf9100000, "(ext rapc 19)", xx, xx, xx, xx, xx, no, x, 19},
    {EXT_VLDA,    0xf9200000, "(ext vldA  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_RAPC,    0xf9300000, "(ext rapc 20)", xx, xx, xx, xx, xx, no, x, 20},
    {EXT_VLDA,    0xf9400000, "(ext vldA  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,     0xf9500000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_VLDA,    0xf9600000, "(ext vldA  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,     0xf9700000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_VLDB,    0xf9800000, "(ext vldB  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_RAPC,    0xf9900000, "(ext rapc 21)", xx, xx, xx, xx, xx, no, x, 21},
    {EXT_VLDB,    0xf9a00000, "(ext vldB  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_RAPC,    0xf9b00000, "(ext rapc 28)", xx, xx, xx, xx, xx, no, x, 28},
    {EXT_VLDB,    0xf9c00000, "(ext vldB  0)",  xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,     0xf9d00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_VLDB,    0xf9e00000, "(ext vldB  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,     0xf9f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* a0 */
    {EXT_RAPC,    0xfa000000, "(ext rapc 22)", xx, xx, xx, xx, xx, no, x, 22},
    {EXT_RAPC,    0xfa100000, "(ext rapc 23)", xx, xx, xx, xx, xx, no, x, 23},
    {EXT_RAPC,    0xfa200000, "(ext rapc 24)", xx, xx, xx, xx, xx, no, x, 24},
    {EXT_RAPC,    0xfa300000, "(ext rapc 25)", xx, xx, xx, xx, xx, no, x, 25},
    {EXT_RAPC,    0xfa400000, "(ext rapc 26)", xx, xx, xx, xx, xx, no, x, 26},
    {EXT_RAPC,    0xfa500000, "(ext rapc 27)", xx, xx, xx, xx, xx, no, x, 27},
    {OP_ror,      0xfa60f000, "ror",    RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_rors,     0xfa70f000, "rors",   RCw, xx, RAw, RDw, xx, no, fWNZC, END_LIST},
    {EXT_B7_4,    0xfa800000, "(ext b7_4 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_B7_4,    0xfa900000, "(ext b7_4 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_B7_4,    0xfaa00000, "(ext b7_4 4)", xx, xx, xx, xx, xx, no, x, 4},
    {OP_clz,      0xfab0f080, "clz",    RCw, xx, RDw, RA_EQ_Dw, xx, no, x, END_LIST},
    {EXT_B7_4,    0xfac00000, "(ext b7_4 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_B7_4,    0xfad00000, "(ext b7_4 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_B6_4,    0xfae00000, "(ext b6_4 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,     0xfaf00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    /* b0 */
    {EXT_RBPC,    0xfb000000, "(ext rbpc 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_RBPC,    0xfb100000, "(ext rbpc 11)", xx, xx, xx, xx, xx, no, x, 11},
    {EXT_RBPC,    0xfb200000, "(ext rbpc 12)", xx, xx, xx, xx, xx, no, x, 12},
    {EXT_RBPC,    0xfb300000, "(ext rbpc 13)", xx, xx, xx, xx, xx, no, x, 13},
    {EXT_RBPC,    0xfb400000, "(ext rbpc 14)", xx, xx, xx, xx, xx, no, x, 14},
    {EXT_RBPC,    0xfb500000, "(ext rbpc 15)", xx, xx, xx, xx, xx, no, x, 15},
    {EXT_B4,      0xfb60f000, "(ext b4 9)", xx, xx, xx, xx, xx, no, x, 9},
    {EXT_RBPC,    0xfb700000, "(ext rbpc 16)", xx, xx, xx, xx, xx, no, x, 16},
    {OP_smull,    0xfb800000, "smull",  RCw, RBw, RAw, RDw, xx, no, x, END_LIST},
    {OP_sdiv,     0xfb90f0f0, "sdiv",   RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {EXT_B7_4,    0xfba00000, "(ext b7_4 8)", xx, xx, xx, xx, xx, no, x, 8},
    {OP_udiv,     0xfbb0f0f0, "udiv",   RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {EXT_B7_4,    0xfbc00000, "(ext b7_4 7)", xx, xx, xx, xx, xx, no, x, 7},
    {EXT_B4,      0xfbd00000, "(ext b4 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_B5,      0xfbe00000, "(ext b5 4)", xx, xx, xx, xx, xx, no, x, 4},
    {INVALID,     0xfbf00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by whether bits A9:7 are all 1's (==0x7) */
const instr_info_t T32_ext_A9_7_eq1[][2] = {
  { /* 0 */
    {EXT_A10_6_4, 0xf3808000, "(ext a10_6_4 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_b,        0xf0008000, "b",      xx, xx, j20x26_11_13_16_0, xx, xx, pred22, x, END_LIST},/*FIXME i#1551: not permitted in IT block*/
  },
};

/* Indexed by bits A10,6:4 */
const instr_info_t T32_ext_bits_A10_6_4[][16] = {
  { /* 0 */
    {EXT_B5,      0xf3808000, "(ext b5 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_B5,      0xf3908000, "(ext b5 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_B10_8,   0xf3af8000, "(ext b10_8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_B6_4,    0xf3bf8000, "(ext b6_4 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_bxj,      0xf3c08f00, "bxj",    xx, xx, RDw, xx, xx, no, x, END_LIST},
    {OP_eret,     0xf3de8f00, "eret",   xx, xx, LRw, i8, xx, no, fWNZCV, END_LIST},/*XXX: identical to "subs pc, lr, #0"*/
    {EXT_B5,      0xf3e08000, "(ext b5 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_B5,      0xf3f08000, "(ext b5 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_B2_0,    0xf78f8000, "(ext b2_0 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,     0xf7908000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7a08000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7b08000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7c08000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf7d08000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_hvc,      0xf7e08000, "hvc",    xx, xx, i16x16_0, xx, xx, no, x, END_LIST},
    {EXT_B13,     0xf7f08000, "(ext b13 0)", xx, xx, xx, xx, xx, no, x, 0},
  },
};

/* Indexed by bits B11:8 but in the following manner:
 * + If bit 11 == 0, take entry 0;
 * + Else, take entry 1 + bits 10:8
 */
const instr_info_t T32_ext_opcBX[][9] = {
  { /* 0 */
    {OP_strb,     0xf8000000, "strb",   MLSb, xx, RBb, xx, xx, no, x, xfop8[0][0x88]},
    {INVALID,     0xf8000800, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {OP_strb,     0xf8000900, "strb",   Mb, RAw, RBb, n8, RAw, no, x, xopbx[0][0x00]},/*PUW=001*/
    {INVALID,     0xf8000a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=010*/
    {OP_strb,     0xf8000b00, "strb",   Mb, RAw, RBb, i8, RAw, no, x, xopbx[0][0x02]},/*PUW=011*/
    {OP_strb,     0xf8000c00, "strb",   MN8b, xx, RBb, xx, xx, no, x, xopbx[0][0x08]},/*PUW=100*/
    {OP_strb,     0xf8000d00, "strb",   MN8b, RAw, RBb, n8, RAw, no, x, xopbx[0][0x04]},/*PUW=101*/
    {OP_strbt,    0xf8000e00, "strbt",  MP8b, xx, RBb, xx, xx, no, x, END_LIST},/*PUW=110*/
    {OP_strb,     0xf8000f00, "strb",   MP8b, RAw, RBb, i8, RAw, no, x, xopbx[0][0x06]},/*PUW=111*/
  }, { /* 1 */
    {EXT_RBPC,    0xf8100000, "(ext rbpc 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,     0xf8100800, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {OP_ldrb,     0xf8100900, "ldrb",   RBw, RAw, Mb, n8, RAw, no, x, END_LIST},/*PUW=001*/
    {INVALID,     0xf8100a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=010*/
    {OP_ldrb,     0xf8100b00, "ldrb",   RBw, RAw, Mb, i8, RAw, no, x, xopbx[1][0x02]},/*PUW=011*/
    {EXT_RBPC,    0xf8100c00, "(ext rbpc 18)", xx, xx, xx, xx, xx, no, x, 18},
    {OP_ldrb,     0xf8100d00, "ldrb",   RBw, RAw, MN8b, n8, RAw, no, x, xopbx[1][0x04]},/*PUW=101*/
    {OP_ldrbt,    0xf8100e00, "ldrbt",  RBw, xx, MP8b, xx, xx, no, x, END_LIST},/*PUW=110*/
    {OP_ldrb,     0xf8100f00, "ldrb",   RBw, RAw, MP8b, i8, RAw, no, x, xopbx[1][0x06]},/*PUW=111*/
  }, { /* 2 */
    {OP_strh,     0xf8200000, "strh",   MLSh, xx, RBh, xx, xx, no, x, xfop8[0][0x8a]},
    {INVALID,     0xf8200800, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {OP_strh,     0xf8200900, "strh",   Mh, RAw, RBh, n8, RAw, no, x, xopbx[2][0x00]},/*PUW=001*/
    {INVALID,     0xf8200a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=010*/
    {OP_strh,     0xf8200b00, "strh",   Mh, RAw, RBh, i8, RAw, no, x, xopbx[2][0x02]},/*PUW=011*/
    {OP_strh,     0xf8200c00, "strh",   MN8h, xx, RBh, xx, xx, no, x, xopbx[2][0x08]},/*PUW=100*/
    {OP_strh,     0xf8200d00, "strh",   MN8h, RAw, RBh, n8, RAw, no, x, xopbx[2][0x04]},/*PUW=101*/
    {OP_strht,    0xf8200e00, "strht",  MP8h, xx, RBh, xx, xx, no, x, END_LIST},/*PUW=110*/
    {OP_strh,     0xf8200f00, "strh",   MP8h, RAw, RBh, i8, RAw, no, x, xopbx[2][0x06]},/*PUW=111*/
  }, { /* 3 */
    {EXT_RBPC,    0xf8300000, "(ext rbpc 2)", xx, xx, xx, xx, xx, no, x, 2},
    {INVALID,     0xf8300800, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {OP_ldrh,     0xf8300900, "ldrh",   RBw, RAw, Mh, n8, RAw, no, x, END_LIST},/*PUW=001*/
    {INVALID,     0xf8300a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=010*/
    {OP_ldrh,     0xf8300b00, "ldrh",   RBw, RAw, Mh, i8, RAw, no, x, xopbx[3][0x02]},/*PUW=011*/
    {OP_ldrh,     0xf8300c00, "ldrh",   RBw, xx, MN8h, xx, xx, no, x, xopbx[3][0x08]},/*PUW=100*/
    {OP_ldrh,     0xf8300d00, "ldrh",   RBw, RAw, MN8h, n8, RAw, no, x, xopbx[3][0x04]},/*PUW=101*/
    {OP_ldrht,    0xf8300e00, "ldrht",  RBw, xx, MP8h, xx, xx, no, x, END_LIST},/*PUW=110*/
    {OP_ldrh,     0xf8300f00, "ldrh",   RBw, RAw, MP8h, i8, RAw, no, x, xopbx[3][0x06]},/*PUW=111*/
  }, { /* 4 */
    {OP_str,      0xf8400000, "str",    MLSw, xx, RBw, xx, xx, no, x, xfop8[0][0x8c]},
    {INVALID,     0xf8400800, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {OP_str,      0xf8400900, "str",    Mw, RAw, RBw, n8, RAw, no, x, xopbx[4][0x00]},/*PUW=001*/
    {INVALID,     0xf8400a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=010*/
    {OP_str,      0xf8400b00, "str",    Mw, RAw, RBw, i8, RAw, no, x, xopbx[4][0x02]},/*PUW=011*/
    {OP_str,      0xf8400c00, "str",    MN8w, xx, RBw, xx, xx, no, x, xopbx[4][0x08]},/*PUW=100*/
    {OP_str,      0xf8400d00, "str",    MN8w, RAw, RBw, n8, RAw, no, x, xopbx[4][0x04]},/*PUW=101*//*"push" if RA==SP,i8==4*/
    {OP_strt,     0xf8400e00, "strt",   MP8w, xx, RBw, xx, xx, no, x, END_LIST},/*PUW=110*/
    {OP_str,      0xf8400f00, "str",    MP8w, RAw, RBw, i8, RAw, no, x, xopbx[4][0x06]},/*PUW=111*/
  }, { /* 5 */
    {OP_ldr,      0xf8500000, "ldr",    RBw, xx, MLSw, xx, xx, no, x, END_LIST},
    {INVALID,     0xf8500800, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {OP_ldr,      0xf8500900, "ldr",    RBw, RAw, Mw, n8, RAw, no, x, xopbx[5][0x00]},/*PUW=001*/
    {INVALID,     0xf8500a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=010*/
    {OP_ldr,      0xf8500b00, "ldr",    RBw, RAw, Mw, i8, RAw, no, x, xopbx[5][0x02]},/*PUW=011*//*"pop" if RA==SP,i8==4*/
    {OP_ldr,      0xf8500c00, "ldr",    RBw, xx, MN8w, xx, xx, no, x, xopbx[5][0x08]},/*PUW=100*/
    {OP_ldr,      0xf8500d00, "ldr",    RBw, RAw, MN8w, n8, RAw, no, x, xopbx[5][0x04]},/*PUW=101*/
    {OP_ldrt,     0xf8500e00, "ldrt",   RBw, xx, MP8w, xx, xx, no, x, END_LIST},/*PUW=110*/
    {OP_ldr,      0xf8500f00, "ldr",    RBw, RAw, MP8w, i8, RAw, no, x, xopbx[5][0x06]},/*PUW=111*/
  }, { /* 6 */
    {OP_ldrsb,    0xf9100000, "ldrsb",  RBw, xx, MLSb, xx, xx, no, x, END_LIST},
    {INVALID,     0xf9100800, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {OP_ldrsb,    0xf9100900, "ldrsb",  RBw, RAw, Mb, n8, RAw, no, x, xopbx[6][0x00]},/*PUW=001*/
    {INVALID,     0xf9100a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=010*/
    {OP_ldrsb,    0xf9100b00, "ldrsb",  RBw, RAw, Mb, i8, RAw, no, x, xopbx[6][0x02]},/*PUW=011*/
    {OP_ldrsb,    0xf9100c00, "ldrsb",  RBw, xx, MN8b, xx, xx, no, x, xopbx[6][0x08]},/*PUW=100*/
    {OP_ldrsb,    0xf9100d00, "ldrsb",  RBw, RAw, MN8b, n8, RAw, no, x, xopbx[6][0x04]},/*PUW=101*/
    {OP_ldrsbt,   0xf9100e00, "ldrsbt", RBw, xx, MP8b, xx, xx, no, x, END_LIST},/*PUW=110*/
    {OP_ldrsb,    0xf9100f00, "ldrsb",  RBw, RAw, MP8b, i8, RAw, no, x, xopbx[6][0x06]},/*PUW=111*/
  }, { /* 7 */
    {OP_ldrsh,    0xf9300000, "ldrsh",  RBw, xx, MLSh, xx, xx, no, x, END_LIST},
    {INVALID,     0xf9300800, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=000*/
    {OP_ldrsh,    0xf9300900, "ldrsh",  RBw, RAw, Mh, n8, RAw, no, x, xopbx[7][0x00]},/*PUW=001*/
    {INVALID,     0xf9300a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=010*/
    {OP_ldrsh,    0xf9300b00, "ldrsh",  RBw, RAw, Mh, i8, RAw, no, x, xopbx[7][0x02]},/*PUW=011*/
    {OP_ldrsh,    0xf9300c00, "ldrsh",  RBw, xx, MN8h, xx, xx, no, x, xopbx[7][0x08]},/*PUW=100*/
    {OP_ldrsh,    0xf9300d00, "ldrsh",  RBw, RAw, MN8h, n8, RAw, no, x, xopbx[7][0x04]},/*PUW=101*/
    {OP_ldrsht,   0xf9300e00, "ldrsht", RBw, xx, MP8h, xx, xx, no, x, END_LIST},/*PUW=110*/
    {OP_ldrsh,    0xf9300f00, "ldrsh",  RBw, RAw, MP8h, i8, RAw, no, x, xopbx[7][0x06]},/*PUW=111*/
  },
};

/* Indexed by bits B10:8 */
const instr_info_t T32_ext_bits_B10_8[][8] = {
  { /* 0 */
    {EXT_B7_4_eq1,0xf3af8000, "(ext b7_4_eq1 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_cps,      0xf3af8100, "cps",    xx, xx, i5, xx, xx, no, x, END_LIST},
    {INVALID,     0xf3af8200, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3af8300, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_cpsie,    0xf3af8400, "cpsie",  xx, xx, i3_5, xx, xx, no, x, END_LIST},
    {OP_cpsie,    0xf3af8500, "cpsie",  xx, xx, i3_5, i5, xx, no, x, xb108[0][0x04]},
    {OP_cpsid,    0xf3af8600, "cpsid",  xx, xx, i3_5, xx, xx, no, x, END_LIST},
    {OP_cpsid,    0xf3af8700, "cpsid",  xx, xx, i3_5, i5, xx, no, x, xb108[0][0x06]},
  },
};

/* Indexed by bits B7:4 */
const instr_info_t T32_ext_bits_B7_4[][16] = {
  { /* 0 */
    {INVALID,     0xe8c00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xe8c00010, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xe8c00020, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xe8c00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_strexb,   0xe8c00f40, "strexb", Mb, RDw, RBb, xx, xx, no, x, END_LIST},
    {OP_strexh,   0xe8c00f50, "strexh", Mh, RDw, RBh, xx, xx, no, x, END_LIST},
    {INVALID,     0xe8c00060, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_strexd,   0xe8c00070, "strexd", Mq, RDw, RBw, RCw, xx, no, x, END_LIST},
    {OP_stlb,     0xe8c00f8f, "stlb",   Mb, xx, RBw, xx, xx, no, x, END_LIST},
    {OP_stlh,     0xe8c00f9f, "stlh",   Mh, xx, RBh, xx, xx, no, x, END_LIST},
    {OP_stl,      0xe8c00faf, "stl",    Mw, xx, RBw, xx, xx, no, x, END_LIST},
    {INVALID,     0xe8c000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_stlexb,   0xe8c00fc0, "stlexb", Mb, RDw, RBb, xx, xx, no, x, END_LIST},
    {OP_stlexh,   0xe8c00fd0, "stlexh", Mh, RDw, RBh, xx, xx, no, x, END_LIST},
    {OP_stlex,    0xe8c00fe0, "stlex",  Mw, RDw, RBw, xx, xx, no, x, END_LIST},
    {OP_stlexd,   0xe8c000f0, "stlexd", Mq, RDw, RBw, RCw, xx, no, x, END_LIST},
  }, { /* 1 */
    {OP_tbb,      0xe8d0f000, "tbb",    xx, xx, MPRb, no, x, END_LIST},
    {OP_tbh,      0xe8d0f010, "tbh",    xx, xx, MPLS1h, no, x, END_LIST},
    {INVALID,     0xe8d00020, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xe8d00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrexb,   0xe8d00f4f, "ldrexb", RBw, xx, Mb, xx, xx, no, x, END_LIST},
    {OP_ldrexh,   0xe8d00f5f, "ldrexh", RBw, xx, Mh, xx, xx, no, x, END_LIST},
    {INVALID,     0xe8d00060, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrexd,   0xe8d0007f, "ldrexd", RBw, RCw, Mq, xx, xx, no, x, END_LIST},
    {OP_ldab,     0xe8d00f8f, "ldab",   RBw, xx, Mb, xx, xx, no, x, END_LIST},
    {OP_ldah,     0xe8d00f9f, "ldah",   RBw, xx, Mh, xx, xx, no, x, END_LIST},
    {OP_lda,      0xe8d00faf, "lda",    RBw, xx, Mw, xx, xx, no, x, END_LIST},
    {INVALID,     0xe8d000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldaexb,   0xe8d00fcf, "ldaexb", RBw, xx, Mb, xx, xx, no, x, END_LIST},
    {OP_ldaexh,   0xe8d00fdf, "ldaexh", RBw, xx, Mh, xx, xx, no, x, END_LIST},
    {OP_ldaex,    0xe8d00fef, "ldaex",  RBw, xx, Mw, xx, xx, no, x, END_LIST},
    {OP_ldaexd,   0xe8d000ff, "ldaexd", RBEw, RB2w, Mq, xx, xx, no, x, END_LIST},
  }, { /* 2 */
    {OP_sadd8,    0xfa80f000, "sadd8",  RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_qadd8,    0xfa80f010, "qadd8",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_shadd8,   0xfa80f020, "shadd8", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfa800030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_uadd8,    0xfa80f040, "uadd8",  RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_uqadd8,   0xfa80f050, "uqadd8", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_uhadd8,   0xfa80f060, "uhadd8", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfa800070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_qadd,     0xfa80f080, "qadd",   RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_qdadd,    0xfa80f090, "qdadd",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_qsub,     0xfa80f0a0, "qsub",   RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_qdsub,    0xfa80f0b0, "qdsub",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfa8000c0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfa8000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfa8000e0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfa8000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 3 */
    {OP_sadd16,   0xfa90f000, "sadd16", RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_qadd16,   0xfa90f010, "qadd16", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_shadd16,  0xfa90f020, "shadd16", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfa900030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_uadd16,   0xfa90f040, "uadd16", RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_uqadd16,  0xfa90f050, "uqadd16", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_uhadd16,  0xfa90f060, "uhadd16", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfa900070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rev,      0xfa90f080, "rev",    RCw, xx, RDw, RA_EQ_Dw, xx, no, x, END_LIST},
    {OP_rev16,    0xfa90f090, "rev16",  RCw, xx, RDw, RA_EQ_Dw, xx, no, x, END_LIST},
    {OP_rbit,     0xfa90f0a0, "rbit",   RCw, xx, RDw, RA_EQ_Dw, xx, no, x, END_LIST},
    {OP_revsh,    0xfa90f0b0, "revsh",  RCw, xx, RDh, RA_EQ_Dh, xx, no, x, END_LIST},
    {INVALID,     0xfa9000c0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfa9000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfa9000e0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfa9000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 4 */
    {OP_sasx,     0xfaa0f000, "sasx",   RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_qasx,     0xfaa0f010, "qasx",   RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_shasx,    0xfaa0f020, "shasx",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfaa00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_uasx,     0xfaa0f040, "uasx",   RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_uqasx,    0xfaa0f050, "uqasx",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_uhasx,    0xfaa0f060, "uhasx",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfaa00070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_sel,      0xfaa0f080, "sel",    RCw, xx, RAw, RDw, xx, no, fRGE, END_LIST},
    {INVALID,     0xfaa00090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfaa000a0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfaa000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfaa000c0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfaa000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfaa000e0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfaa000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 5 */
    {OP_ssub8,    0xfac0f000, "ssub8",  RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_qsub8,    0xfac0f010, "qsub8",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_shsub8,   0xfac0f020, "shsub8", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfac00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_usub8,    0xfac0f040, "usub8",  RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_uqsub8,   0xfac0f050, "uqsub8", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_uhsub8,   0xfac0f060, "uhsub8", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfac00070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_crc32b,   0xfac0f080, "crc32b",  RCw, xx, RAw, RDb, xx, v8, x, END_LIST},
    {OP_crc32h,   0xfac0f090, "crc32h",  RCw, xx, RAw, RDh, xx, v8, x, END_LIST},
    {OP_crc32w,   0xfac0f0a0, "crc32w",  RCw, xx, RAw, RDw, xx, v8, x, END_LIST},
    {OP_crc32w,   0xfac0f0b0, "crc32w",  RCw, xx, RAw, RDw, xx, v8|unp, x, xb74[5][0x0a]},
    {INVALID,     0xfac000c0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfac000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfac000e0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfac000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 6 */
    {OP_ssub16,   0xfad0f000, "ssub16", RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_qsub16,   0xfad0f010, "qsub16", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_shsub16,  0xfad0f020, "shsub16", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfad00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_usub16,   0xfad0f040, "usub16", RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_uqsub16,  0xfad0f050, "uqsub16", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_uhsub16,  0xfad0f060, "uhsub16", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfad00070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_crc32cb,  0xfad0f080, "crc32cb", RCw, xx, RAw, RDb, xx, v8, x, END_LIST},
    {OP_crc32ch,  0xfad0f090, "crc32ch", RCw, xx, RAw, RDh, xx, v8, x, END_LIST},
    {OP_crc32cw,  0xfad0f0a0, "crc32cw", RCw, xx, RAw, RDw, xx, v8, x, END_LIST},
    {OP_crc32cw,  0xfad0f0b0, "crc32cw", RCw, xx, RAw, RDw, xx, v8|unp, x, xb74[6][0x0a]},
    {INVALID,     0xfad000c0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfad000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfad000e0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfad000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 7 */
    {OP_smlal,    0xfbc00000, "smlal",  RCw, RBw, RCw, RBw, RAw, xop, x, xexop[0x7]},
    {INVALID,     0xfbc00010, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfbc00020, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfbc00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfbc00040, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfbc00050, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfbc00060, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfbc00070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_smlalbb,  0xfbc00080, "smlalbb",RCw, RBw, RCw, RBw, RAh, xop, x, xexop[0x4]},
    {OP_smlalbt,  0xfbc00090, "smlalbt",RCw, RBw, RCw, RBw, RAh, xop, x, xexop[0x5]},
    {OP_smlaltb,  0xfbc000a0, "smlaltb",RCw, RBw, RCw, RBw, RAt, xop, x, xexop[0x4]},
    {OP_smlaltt,  0xfbc000b0, "smlaltt",RCw, RBw, RCw, RBw, RAt, xop, x, xexop[0x5]},
    {OP_smlald,   0xfbc000c0, "smlald", RCw, RBw, RCw, RBw, RAw, xop, x, xexop[0x7]},
    {OP_smlaldx,  0xfbc000d0, "smlaldx",RCw, RBw, RCw, RBw, RAw, xop, x, xexop[0x7]},
    {INVALID,     0xfbc000e0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfbc000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 8 */
    /* We need to ensure 7:4 are 0 for OP_umull */
    {OP_umull,    0xfba00000, "umull",  RCw, RBw, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfba00010, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba00020, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba00040, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba00050, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba00060, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba00070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba00080, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba00090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba000a0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba000c0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba000e0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xfba000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by whether bits B7:4 are 0xf or not */
const instr_info_t T32_ext_B7_4_eq1[][8] = {
  { /* 0 */
    {OP_dbg,      0xf3af80f0, "dbg",    xx, xx, i4, xx, xx, no, x, END_LIST},
    {EXT_B2_0,    0xf3af8000, "(ext b2_0 0)", xx, xx, xx, xx, xx, no, x, 0},
  },
};

/* Indexed by bits B6:4
 * XXX: merge B5:4 into here?  Merge this into B7:4?
 */
const instr_info_t T32_ext_bits_B6_4[][8] = {
  { /* 0 */
    {OP_leavex,   0xf3bf8f0f, "leavex", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_enterx,   0xf3bf8f1f, "enterx", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_clrex,    0xf3bf8f2f, "clrex",  xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf3bf8030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_dsb,      0xf3bf8f40, "dsb",    xx, xx, i4, xx, xx, no, x, END_LIST},
    {OP_dmb,      0xf3bf8f50, "dmb",    xx, xx, i4, xx, xx, no, x, END_LIST},
    {OP_isb,      0xf3bf8f60, "isb",    xx, xx, i4, xx, xx, no, x, END_LIST},
    {INVALID,     0xf3bf8070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 1 */
    {OP_ssax,     0xfae0f000, "ssax",   RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_qsax,     0xfae0f010, "qsax",   RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_shsax,    0xfae0f020, "shsax",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfae00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_usax,     0xfae0f040, "usax",   RCw, xx, RAw, RDw, xx, no, fWGE, END_LIST},
    {OP_uqsax,    0xfae0f050, "uqsax",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_uhsax,    0xfae0f060, "uhsax",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {INVALID,     0xfae00070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits B5:4 */
const instr_info_t T32_ext_bits_B5_4[][4] = {
  { /* 0 */
    {EXT_IMM126,  0xea4f0000, "(ext imm126 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_lsr,      0xea4f0010, "lsr",    RCw, xx, RDw, i5x12_6, xx, no, x, END_LIST},
    {OP_asr,      0xea4f0020, "asr",    RCw, xx, RDw, i5x12_6, xx, no, x, END_LIST},
    {EXT_IMM126,  0xea4f0030, "(ext imm126 1)", xx, xx, xx, xx, xx, no, x, 1},
  }, { /* 1 */
    {EXT_IMM126,  0xea5f0000, "(ext imm126 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_lsrs,     0xea5f0010, "lsrs",   RCw, xx, RDw, i5x12_6, xx, no, fRC|fWNZC, END_LIST},
    {OP_asrs,     0xea5f0020, "asrs",   RCw, xx, RDw, i5x12_6, xx, no, fRC|fWNZC, END_LIST},
    {EXT_IMM126,  0xea5f0030, "(ext imm126 3)", xx, xx, xx, xx, xx, no, x, 3},
  }, { /* 2 */
    {OP_pkhbt,    0xeac00000, "pkhbt",  RCw, RAh, RDt, LSL, i5x12_6, srcX4, x, END_LIST},
    {INVALID,     0xeac00010, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_pkhtb,    0xeac00020, "pkhtb",  RCw, RAt, RDh, ASR, i5x12_6, srcX4, x, END_LIST},
    {INVALID,     0xeac00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 3 */
    {OP_smlabb,   0xfb100000, "smlabb", RCw, xx, RAh, RDh, RBw, no, x, END_LIST},
    {OP_smlabt,   0xfb100010, "smlabt", RCw, xx, RAh, RDt, RBw, no, x, END_LIST},
    {OP_smlatb,   0xfb100020, "smlatb", RCw, xx, RAt, RDh, RBw, no, x, END_LIST},
    {OP_smlatt,   0xfb100030, "smlatt", RCw, xx, RAt, RDt, RBw, no, x, END_LIST},
  }, { /* 4 */
    {OP_smulbb,   0xfb10f000, "smulbb", RCw, xx, RAh, RDh, xx, no, x, END_LIST},
    {OP_smulbt,   0xfb10f010, "smulbt", RCw, xx, RAh, RDt, xx, no, x, END_LIST},
    {OP_smultb,   0xfb10f020, "smultb", RCw, xx, RAt, RDh, xx, no, x, END_LIST},
    {OP_smultt,   0xfb10f030, "smultt", RCw, xx, RAt, RDt, xx, no, x, END_LIST},
  },
};

/* Indexed by bits B2:0 */
const instr_info_t T32_ext_bits_B2_0[][8] = {
  { /* 0 */
    {OP_nop,      0xf3af8000, "nop",    xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_yield,    0xf3af8001, "yield",  xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_wfe,      0xf3af8002, "wfe",    xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_wfi,      0xf3af8003, "wfi",    xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_sev,      0xf3af8004, "sev",    xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_sevl,     0xf3af8005, "sevl",   xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf3af8006, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3af8007, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 1 */
    {INVALID,     0xf78f8000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_dcps1,    0xf78f8001, "dcps1",  xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_dcps2,    0xf78f8002, "dcps2",  xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_dcps3,    0xf78f8003, "dcps3",  xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf78f8004, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf78f8005, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf78f8006, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf78f8007, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bit B4 */
const instr_info_t T32_ext_bit_B4[][2] = {
  { /* 0 */
    {OP_mla,      0xfb000000, "mla",    RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
    {OP_mls,      0xfb000010, "mls",    RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
  }, { /* 1 */
    {OP_smlad,    0xfb200000, "smlad",  RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
    {OP_smladx,   0xfb200010, "smladx", RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
  }, { /* 2 */
    {OP_smuad,    0xfb20f000, "smuad",  RCw, xx, RAw, RDw, xx, no, fWQ, END_LIST},
    {OP_smuadx,   0xfb20f010, "smuadx", RCw, xx, RAw, RDw, xx, no, fWQ, END_LIST},
  }, { /* 3 */
    {OP_smlawb,   0xfb300000, "smlawb", RCw, xx, RAh, RDh, RBw, no, x, END_LIST},
    {OP_smlawt,   0xfb300010, "smlawt", RCw, xx, RAt, RDt, RBw, no, x, END_LIST},
  }, { /* 4 */
    {OP_smulwb,   0xfb30f000, "smulwb", RCw, xx, RAw, RDh, xx, no, x, END_LIST},
    {OP_smulwt,   0xfb30f010, "smulwt", RCw, xx, RAw, RDt, xx, no, x, END_LIST},
  }, { /* 5 */
    {OP_smlsd,    0xfb400000, "smlsd",  RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
    {OP_smlsdx,   0xfb400010, "smlsdx", RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
  }, { /* 6 */
    {OP_smusd,    0xfb40f000, "smusd",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_smusdx,   0xfb40f010, "smusdx", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
  }, { /* 7 */
    {OP_smmla,    0xfb500000, "smmla",  RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
    {OP_smmlar,   0xfb500010, "smmlar", RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
  }, { /* 8 */
    {OP_smmul,    0xfb50f000, "smmul",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_smmulr,   0xfb50f010, "smmulr", RCw, xx, RAw, RDw, xx, no, x, END_LIST},
  }, { /* 9 */
    {OP_smmls,    0xfb600000, "smmls",  RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
    {OP_smmlsr,   0xfb600010, "smmlsr", RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
  }, { /* 10 */
    {OP_smlsld,   0xfbd000c0, "smlsld", RCw, RBw, RCw, RBw, RAw, xop, x, xexop[0x7]},
    {OP_smlsldx,  0xfbd000d0, "smlsldx",RCw, RBw, RCw, RBw, RAw, xop, x, xexop[0x7]},
  }, { /* 11 */
    {OP_cdp,     0xee000000, "cdp",    CRBw, i4_8, i4_20, CRAw, CRDw, xop|srcX4, x, xexop[0x3]},/*XXX: disasm not in dst-src order*//*no chain nec.*/
    {OP_mcr,     0xee000010, "mcr",    CRAw, CRDw, i4_8, i3_21, RBw, xop, x, xexop[0x3]},/*XXX: disasm not in dst-src order*/
  }, { /* 12 */
    {OP_cdp,     0xee100000, "cdp",    CRBw, i4_8, i4_20, CRAw, CRDw, xop|srcX4, x, xexop[0x3]},/*XXX: disasm not in dst-src order*/
    {OP_mrc,     0xee100010, "mrc",    RBw, i4_8, i3_21, CRAw, CRDw, xop|srcX4, x, xexop[0x3]},/*XXX: disasm not in dst-src order*/
  }, { /* 13 */
    {OP_cdp2,     0xfe000000, "cdp2",           CRBw, i4_8, i4_20, CRAw, CRDw, xop|srcX4, x, END_LIST},/*XXX: disasm not in dst-src order*//*no chain nec.*/
    {OP_mcr2,     0xfe000010, "mcr2",           CRAw, CRDw, i4_8, i3_21, RBw, xop, x, END_LIST},/*XXX: disasm not in dst-src order*/
  }, { /* 14 */
    {OP_cdp2,     0xfe100000, "cdp2",           CRBw, i4_8, i4_20, CRAw, CRDw, xop|srcX4, x, DUP_ENTRY},/*XXX: disasm not in dst-src order*//*no chain nec.*/
    {OP_mrc2,     0xfe100010, "mrc2",          RBw, i4_8, i3_21, CRAw, CRDw, xop|srcX4, x, xexop[0x3]},/*XXX: disasm not in dst-src order*/
  }, { /* 15 */
    /* To handle the 21:16 immed instrs that vary in high bits we must first
     * sseparate those out: we do that via bit4 and then bit7 in the next 8 entries.
     */
    {EXT_SIMD8,  0xef800000, "(ext simd8  0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_B7,     0xef800010, "(ext bit7   6)", xx, xx, xx, xx, xx, no, x, 6},
  }, { /* 16 */
    {EXT_SIMD6,  0xef900000, "(ext simd6  4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_B7,     0xef900010, "(ext bit7   7)", xx, xx, xx, xx, xx, no, x, 7},
  }, { /* 17 */
    {EXT_SIMD6,  0xefa00000, "(ext simd6  5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_B7,     0xefa00010, "(ext bit7   8)", xx, xx, xx, xx, xx, no, x, 8},
  }, { /* 18 */
    {EXT_BIT6,   0xefb00000, "(ext bit6   0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_B7,     0xefb00010, "(ext bit7   8)", xx, xx, xx, xx, xx, no, x, 8},
  }, { /* 19 */
    {EXT_SIMD8,  0xff800000, "(ext simd8  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_B7,     0xff800010, "(ext bit7   9)", xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 20 */
    {EXT_SIMD6,  0xff900000, "(ext simd6 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_B7,     0xff900010, "(ext bit7  10)", xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 21 */
    {EXT_SIMD6,  0xffa00000, "(ext simd6 11)", xx, xx, xx, xx, xx, no, x, 11},
    {EXT_B7,     0xffa00010, "(ext bit7  11)", xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 22 */
    {EXT_VTB,    0xffb00000, "(ext vtb 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_B7,     0xffb00010, "(ext bit7  11)", xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 23 */
    {EXT_VLDC,   0xf9a00e00, "(ext vldC  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,    0xf9a00e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bit B5 */
const instr_info_t T32_ext_bit_B5[][2] = {
  { /* 0 */
    {OP_msr,      0xf3808000, "msr",    CPSR, xx, i4_8, RAw, xx, no, fWNZCVQG/*see decode_eflags_to_instr_eflags*/, END_LIST},
    {OP_msr_priv, 0xf3808020, "msr",    xx, xx, i5x4_8, RAw, xx, no, x, END_LIST},
  }, { /* 1 */
    {OP_msr,      0xf3908000, "msr",    SPSR, xx, i4_8, RAw, xx, no, fWNZCVQG/*see decode_eflags_to_instr_eflags*/, xb5[0][0x00]},
    {OP_msr_priv, 0xf3908020, "msr",    SPSR, xx, i5x4_8, RAw, xx, no, x, xb5[0][0x01]},
  }, { /* 2 */
    {OP_mrs,      0xf3ef8000, "mrs",    RCw, xx, CPSR, xx, xx, no, fRNZCVQG, END_LIST},
    {OP_mrs_priv, 0xf3e08020, "mrs",    RCw, xx, i5x4_16, xx, xx, no, x, END_LIST},
  }, { /* 3 */
    {OP_mrs,      0xf3ff8000, "mrs",    RCw, xx, SPSR, xx, xx, no, fRNZCVQG, xb5[2][0x00]},
    {OP_mrs_priv, 0xf3f08020, "mrs",    RCw, xx, SPSR, i5x4_16, xx, no, x, xb5[2][0x01]},
  }, { /* 4 */
    {OP_umlal,    0xfbe00000, "umlal",  RCw, RBw, RCw, RBw, RAw, xop, x, xexop[0x7]},
    {OP_umaal,    0xfbe00060, "umaal",  RCw, RBw, RCw, RBw, RAw, xop, x, xexop[0x7]},
  },
};

/* Indexed by bit B7 */
const instr_info_t T32_ext_bit_B7[][2] = {
  { /* 0 */
    {OP_lsl,      0xfa00f000, "lsl",    RCw, xx, RAw, RDw, xx, no, x, END_LIST},
    {OP_sxtah,    0xfa00f080, "sxtah",  RCw, xx, RAw, RDw, ro2_4, no, x, END_LIST},
  }, { /* 1 */
    {OP_lsls,     0xfa10f000, "lsls",   RCw, xx, RAw, RDw, xx, no, fWNZC, END_LIST},
    {OP_uxtah,    0xfa10f080, "uxtah",  RCw, xx, RAw, RDw, ro2_4, no, x, END_LIST},
  }, { /* 2 */
    {OP_lsr,      0xfa20f000, "lsr",    RCw, xx, RAw, RDw, xx, no, x, xb54[0][0x01]},
    {OP_sxtab16,  0xfa20f080, "sxtab16", RCw, xx, RAw, RDw, ro2_4, no, x, END_LIST},
  }, { /* 3 */
    {OP_lsrs,     0xfa30f000, "lsrs",   RCw, xx, RAw, RDw, xx, no, fWNZC, xb54[1][0x01]},
    {OP_uxtab16,  0xfa30f080, "uxtab16", RCw, xx, RAw, RDw, ro2_4, no, x, END_LIST},
  }, { /* 4 */
    {OP_asr,      0xfa40f000, "asr",    RCw, xx, RAw, RDw, xx, no, x, xb54[0][0x02]},
    {OP_sxtab,    0xfa40f080, "sxtab",  RCw, xx, RAw, RDw, ro2_4, no, x, END_LIST},
  }, { /* 5 */
    {OP_asrs,     0xfa50f000, "asrs",   RCw, xx, RAw, RDw, xx, no, fWNZC, xb54[1][0x02]},
    {OP_uxtab,    0xfa50f080, "uxtab",  RCw, xx, RAw, RDw, ro2_4, no, x, END_LIST},
  }, { /* 6 */
    {EXT_BIT19,  0xef800010, "(ext bit19  0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_IMM6L,  0xef800090, "(ext imm6L  0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 7 */
    {EXT_SIMD6,  0xef900010, "(ext simd6  4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_IMM6L,  0xef900090, "(ext imm6L  0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 8 */
    /* The .*32 versions of the high-immed instrs can be 0xefa or 0xefb so we
     * point at the same simd6[5], w/ bit4=1 ensuring we skip the right entries
     * that we should not hit if we went there w/o checking bit4 first.
     */
    {EXT_SIMD6,  0xefa00010, "(ext simd6  5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_IMM6L,  0xefa00090, "(ext imm6L  0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 9 */
    {EXT_BIT19,  0xff800010, "(ext bit19  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_IMM6L,  0xff800090, "(ext imm6L  1)", xx, xx, xx, xx, xx, no, x, 1},
  }, { /* 10 */
    {EXT_SIMD6,  0xff900010, "(ext simd6 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_IMM6L,  0xff900090, "(ext imm6L  1)", xx, xx, xx, xx, xx, no, x, 1},
  }, { /* 11 */
    /* Similarly, we need to share 0xffa with 0xffb when bit4 is set. */
    {EXT_SIMD6,  0xffa00010, "(ext simd6 11)", xx, xx, xx, xx, xx, no, x, 11},
    {EXT_IMM6L,  0xffa00090, "(ext imm6L  1)", xx, xx, xx, xx, xx, no, x, 1},
  },
};

/* Indexed by bit B11 */
const instr_info_t T32_ext_bit_B11[][2] = {
  { /* 0 */
    {OP_pld,      0xf810f000, "pld",    xx, xx, MLSz, xx, xx, no, x, END_LIST},/*PUW=000*/
    {OP_pld,      0xf810fc00, "pld",    xx, xx, MN8z, xx, xx, no, x, xb11[0][0x00]},/*PUW=000*/
  }, { /* 1 */
    {OP_pldw,     0xf830f000, "pldw",   xx, xx, MLSz, xx, xx, no, x, END_LIST},/*PUW=001*/
    {OP_pldw,     0xf830fc00, "pldw",   xx, xx, MN8z, xx, xx, no, x, xb11[1][0x00]},/*PUW=001*/
  }, { /* 2 */
    {OP_pli,      0xf910f000, "pli",    xx, xx, MLSz, xx, xx, no, x, END_LIST},
    {OP_pli,      0xf910fc00, "pli",    xx, xx, MN8z, xx, xx, no, x, xb11[2][0x00]},
  },
};

/* Indexed by bit B13 */
const instr_info_t T32_ext_bit_B13[][2] = {
  { /* 0 */
    {OP_smc,      0xf7f08000, "smc",    xx, xx, i4_16, xx, xx, no, x, END_LIST},
    {OP_udf,      0xf7f0a000, "udf",    xx, xx, i16x16_0, xx, xx, no, x, END_LIST},
  },
};

/* Indexed by whether RA != PC.
 * XXX: would it be worthwhile to try and combine this with A32_ext_RAPC?
 */
const instr_info_t T32_ext_RAPC[][2] = {
  { /* 0 */
    {OP_orr,      0xea400000, "orr",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, x, END_LIST},
    {EXT_B5_4,    0xea4f0000, "(ext b5_4 0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 1 */
    {OP_orrs,     0xea500000, "orrs",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZC, END_LIST},
    {EXT_B5_4,    0xea5f0000, "(ext b5_4 1)", xx, xx, xx, xx, xx, no, x, 1},
  }, { /* 2 */
    {OP_orn,      0xea600000, "orn",    RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, x, END_LIST},
    {OP_mvn,      0xea6f0000, "mvn",    RCw, xx, RDw, sh2_4, i5x12_6, no, x, END_LIST},
  }, { /* 3 */
    {OP_orns,     0xea700000, "orns",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZC, END_LIST},
    {OP_mvns,     0xea7f0000, "mvns",   RCw, xx, RDw, sh2_4, i5x12_6, no, fWNZC, END_LIST},
  }, { /* 4 */
    {OP_orr,      0xf0400000, "orr",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xrapc[0][0x00]},
    {OP_mov,      0xf04f0000, "mov",    RCw, xx, i12x26_12_0, xx, xx, no, x, xi126[0][0x00]},
  }, { /* 5 */
    {OP_orrs,     0xf0500000, "orrs",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, xrapc[1][0x00]},
    {OP_movs,     0xf05f0000, "movs",   RCw, xx, i12x26_12_0, xx, xx, no, fRC|fWNZC, xi126[2][0x00]},
  }, { /* 6 */
    {OP_orn,      0xf0600000, "orn",    RCw, xx, RAw, i12x26_12_0, xx, no, x, xrapc[2][0x00]},
    {OP_mvn,      0xf06f0000, "mvn",    RCw, xx, i12x26_12_0, xx, xx, no, x, xrapc[2][0x01]},
  }, { /* 7 */
    {OP_orns,     0xf0700000, "orns",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, xrapc[3][0x00]},
    {OP_mvns,     0xf07f0000, "mvns",   RCw, xx, i12x26_12_0, xx, xx, no, fRC|fWNZC, xrapc[3][0x01]},
  }, { /* 8 */
    {OP_bfi,      0xf3600000, "bfi",    RCw, RAw, i5x12_6, i5, RCw, srcX4, x, END_LIST},
    {OP_bfc,      0xf36f0000, "bfc",    RCw, xx, i5x12_6, i5, RCw, no, x, END_LIST},
  }, { /* 9 */
    {OP_orr,      0xf4400000, "orr",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {OP_mov,      0xf44f0000, "mov",    RCw, xx, i12x26_12_0, xx, xx, no, x, DUP_ENTRY},
  }, { /* 10 */
    {OP_orrs,     0xf4500000, "orrs",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, DUP_ENTRY},
    {OP_movs,     0xf45f0000, "movs",   RCw, xx, i12x26_12_0, xx, xx, no, fRC|fWNZC, DUP_ENTRY},
  }, { /* 11 */
    {OP_orn,      0xf4600000, "orn",    RCw, xx, RAw, i12x26_12_0, xx, no, x, DUP_ENTRY},
    {OP_mvn,      0xf46f0000, "mvn",    RCw, xx, i12x26_12_0, xx, xx, no, x, DUP_ENTRY},
  }, { /* 12 */
    {OP_orns,     0xf4700000, "orns",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, DUP_ENTRY},
    {OP_mvns,     0xf47f0000, "mvns",   RCw, xx, i12x26_12_0, xx, xx, no, fRC|fWNZC, DUP_ENTRY},
  }, { /* 13 */
    {EXT_OPCBX,   0xf8100000, "(ext opcbx 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_RBPC,    0xf81f0000, "(ext rbpc 0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 14 */
    {EXT_OPCBX,   0xf8300000, "(ext opcbx 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_ldrh,     0xf83f0000, "ldrh",   RBw, xx, MPCN12h, xx, xx, no, x, xrbpc[2][0x00]},/*PUW=000*/
  }, { /* 15 */
    {EXT_OPCBX,   0xf8500000, "(ext opcbx 5)", xx, xx, xx, xx, xx, no, x, 5},
    {OP_ldr,      0xf85f0000, "ldr",    RBw, xx, MPCN12w, xx, xx, no, x, xopbx[5][0x05]},/*PUW=000*/
  }, { /* 16 */
    {EXT_RBPC,    0xf8900000, "(ext rbpc 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_RBPC,    0xf89f0000, "(ext rbpc 4)", xx, xx, xx, xx, xx, no, x, 4},
  }, { /* 17 */
    {EXT_RBPC,    0xf8b00000, "(ext rbpc 5)", xx, xx, xx, xx, xx, no, x, 5},
    {OP_ldrh,     0xf8bf0000, "ldrh",   RBw, xx, MPCP12h, xx, xx, no, x, xrapc[14][0x01]},/*PUW=010*/
  }, { /* 18 */
    {OP_ldr,      0xf8d00000, "ldr",    RBw, xx, MP12w, xx, xx, no, x, xrapc[15][0x01]},
    {OP_ldr,      0xf8df0000, "ldr",    RBw, xx, MPCP12w, xx, xx, no, x, xrapc[18][0x00]},/*PUW=010*/
  }, { /* 19 */
    {EXT_RBPC,    0xf9100000, "(ext rbpc 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_RBPC,    0xf91f0000, "(ext rbpc 7)", xx, xx, xx, xx, xx, no, x, 7},
  }, { /* 20 */
    {EXT_OPCBX,   0xf9300000, "(ext opcbx 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_ldrsh,    0xf93f0000, "ldrsh",  RBw, xx, MPCN12h, xx, xx, no, x, xopbx[7][0x05]},/*PUW=000*/
  }, { /* 21 */
    {EXT_RBPC,    0xf9900000, "(ext rbpc 8)", xx, xx, xx, xx, xx, no, x, 8},
    {EXT_RBPC,    0xf99f0000, "(ext rbpc 9)", xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 22 */
    {EXT_B7,      0xfa000000, "(ext b7 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_sxth,     0xfa0ff080, "sxth",   RCw, xx, RDw, ro2_4, xx, no, x, END_LIST},
  }, { /* 23 */
    {EXT_B7,      0xfa100000, "(ext b7 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_uxth,     0xfa1ff080, "uxth",   RCw, xx, RDw, ro2_4, xx, no, x, END_LIST},
  }, { /* 24 */
    {EXT_B7,      0xfa200000, "(ext b7 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_sxtb16,   0xfa2ff080, "sxtb16", RCw, xx, RDw, ro2_4, xx, no, x, END_LIST},
  }, { /* 25 */
    {EXT_B7,      0xfa300000, "(ext b7 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_uxtb16,   0xfa3ff080, "uxtb16", RCw, xx, RDw, ro2_4, xx, no, x, END_LIST},
  }, { /* 26 */
    {EXT_B7,      0xfa400000, "(ext b7 4)", xx, xx, xx, xx, xx, no, x, 4},
    {OP_sxtb,     0xfa4ff080, "sxtb",   RCw, xx, RDw, ro2_4, xx, no, x, END_LIST},
  }, { /* 27 */
    {EXT_B7,      0xfa500000, "(ext b7 5)", xx, xx, xx, xx, xx, no, x, 5},
    {OP_uxtb,     0xfa5ff080, "uxtb",   RCw, xx, RDw, ro2_4, xx, no, x, END_LIST},
  }, { /* 28 */
    {OP_ldrsh,    0xf9b00000, "ldrsh",  RBw, xx, MP12h, xx, xx, no, x, xrapc[20][0x01]},
    {OP_ldrsh,    0xf9bf0000, "ldrsh",  RBw, xx, MPCP12h, xx, xx, no, x, xrapc[28][0x00]},/*PUW=010*/
  },
};

/* Indexed by whether RB != PC */
const instr_info_t T32_ext_RBPC[][2] = {
  { /* 0 */
    {OP_ldrb,     0xf81f0000, "ldrb",   RBw, xx, MPCN12b, xx, xx, no, x, xrbpc[1][0x00]},/*PUW=000*/
    {OP_pld,      0xf81ff000, "pld",    xx, xx, MPCN12z, xx, xx, no, x, xb11[0][0x01]},/*PUW=000*/
  }, { /* 1 */
    {OP_ldrb,     0xf8100000, "ldrb",   RBw, xx, MLSb, xx, xx, no, x, xopbx[1][0x08]},
    {EXT_B11,     0xf810f000, "(ext b11 0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 2 */
    {OP_ldrh,     0xf8300000, "ldrh",   RBw, xx, MLSh, xx, xx, no, x, xopbx[3][0x05]},
    {EXT_B11,     0xf830f000, "(ext b11 1)", xx, xx, xx, xx, xx, no, x, 1},
  }, { /* 3 */
    {OP_ldrb,     0xf8900000, "ldrb",   RBw, xx, MP12b, xx, xx, no, x, xrbpc[0][0x00]},
    {OP_pld,      0xf890f000, "pld",    xx, xx, MP12z, xx, xx, no, x, xrbpc[4][0x01]},/*PUW=010*/
  }, { /* 4 */
    {OP_ldrb,     0xf89f0000, "ldrb",   RBw, xx, MPCP12b, xx, xx, no, x, xrbpc[3][0x00]},/*PUW=010*/
    {OP_pld,      0xf89ff000,/*can delete: literal==general for MP*/"pld",    xx, xx, MPCP12z, xx, xx, no, x, xrbpc[0][0x01]},/*PUW=010*/
  }, { /* 5 */
    {OP_ldrh,     0xf8b00000, "ldrh",   RBw, xx, MP12h, xx, xx, no, x, xrapc[17][0x01]},
    {OP_pldw,     0xf8b0f000, "pldw",   xx, xx, MP12z, xx, xx, no, x, xb11[1][0x01]},/*PUW=011*/
  }, { /* 6 */
    {EXT_OPCBX,   0xf9100000, "(ext opcbx 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_B11,     0xf910f000, "(ext b11 2)", xx, xx, xx, xx, xx, no, x, 2},
  }, { /* 7 */
    {OP_ldrsb,    0xf91f0000, "ldrsb",  RBw, xx, MPCN12b, xx, xx, no, x, xopbx[6][0x05]},/*PUW=000*/
    {OP_pli,      0xf91ff000, "pli",    xx, xx, MPCN12z, xx, xx, no, x, xrbpc[8][0x01]},/*PUW=100*/
  }, { /* 8 */
    {OP_ldrsb,    0xf9900000, "ldrsb",  RBw, xx, MP12b, xx, xx, no, x, xrbpc[7][0x00]},
    {OP_pli,      0xf990f000, "pli",    xx, xx, MP12z, xx, xx, no, x, xb11[2][0x01]},
  }, { /* 9 */
    {OP_ldrsb,    0xf99f0000, "ldrsb",  RBw, xx, MPCP12b, xx, xx, no, x, xrbpc[8][0x00]},/*PUW=010*/
    {OP_pli,      0xf99ff000,/*can delete: literal==general for MP*/ "pli",    xx, xx, MPCP12z, xx, xx, no, x, xrbpc[7][0x01]},/*PUW=110*/
  }, { /* 10 */
    {EXT_B4,      0xfb000000, "(ext b4 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_mul,      0xfb00f000, "mul",    RCw, xx, RAw, RDw, xx, no, x, END_LIST},
  }, { /* 11 */
    {EXT_B5_4,    0xfb100000, "(ext b5_4 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_B5_4,    0xfb10f000, "(ext b5_4 4)", xx, xx, xx, xx, xx, no, x, 4},
  }, { /* 12 */
    {EXT_B4,      0xfb200000, "(ext b4 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_B4,      0xfb20f000, "(ext b4 2)", xx, xx, xx, xx, xx, no, x, 2},
  }, { /* 13 */
    {EXT_B4,      0xfb300000, "(ext b4 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_B4,      0xfb30f000, "(ext b4 4)", xx, xx, xx, xx, xx, no, x, 4},
  }, { /* 14 */
    {EXT_B4,      0xfb400000, "(ext b4 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_B4,      0xfb40f000, "(ext b4 6)", xx, xx, xx, xx, xx, no, x, 6},
  }, { /* 15 */
    {EXT_B4,      0xfb500000, "(ext b4 7)", xx, xx, xx, xx, xx, no, x, 7},
    {EXT_B4,      0xfb50f000, "(ext b4 8)", xx, xx, xx, xx, xx, no, x, 8},
  }, { /* 16 */
    {OP_usada8,   0xfb700000, "usada8", RCw, xx, RAw, RDw, RBw, no, x, END_LIST},
    {OP_usad8,    0xfb70f000, "usad8",  RCw, xx, RAw, RDw, xx, no, x, END_LIST},
  }, { /* 17 */
    {EXT_IMM1916, 0xeef00a10, "(ext imm1916 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_vmrs,     0xeef0fa10, "vmrs",   CPSR, xx, FPSCR, xx, xx, vfp, fWNZCV, xi19[3][0x00]},
  }, { /* 18 */
    {OP_ldrb,     0xf8100c00, "ldrb",   RBw, xx, MN8b, xx, xx, no, x, xrbpc[4][0x00]},/*PUW=100*/
    {OP_pld,      0xf810fc00, "pld",    xx, xx, MN8z, xx, xx, no, x, DUP_ENTRY},/*PUW=000*/
  },
};

/* Indexed by whether RC != PC */
const instr_info_t T32_ext_RCPC[][2] = {
  { /* 0 */
    {OP_ands,     0xea100000, "ands",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZC, END_LIST},
    {OP_tst,      0xea100f00, "tst",    xx, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZC, END_LIST},
  }, { /* 1 */
    {OP_eors,     0xea900000, "eors",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZC, END_LIST},
    {OP_teq,      0xea900f00, "teq",    xx, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZC, END_LIST},
  }, { /* 2 */
    {OP_adds,     0xeb100000, "adds",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZCV, END_LIST},
    {OP_cmn,      0xeb100f00, "cmn",    xx, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZCV, END_LIST},
  }, { /* 3 */
    {OP_subs,     0xebb00000, "subs",   RCw, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZCV, END_LIST},
    {OP_cmp,      0xebb00f00, "cmp",    xx, RAw, RDw, sh2_4, i5x12_6, srcX4, fWNZCV, END_LIST},
  }, { /* 4 */
    {OP_ands,     0xf0100000, "ands",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, xrcpc[0][0x00]},
    {OP_tst,      0xf0100f00, "tst",    xx, xx, RAw, i12x26_12_0, xx, no, fWNZC, xrcpc[0][0x01]},
  }, { /* 5 */
    {OP_eors,     0xf0900000, "eors",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, xrcpc[1][0x00]},
    {OP_teq,      0xf0900f00, "teq",    xx, xx, RAw, i12x26_12_0, xx, no, fWNZC, xrcpc[1][0x01]},
  }, { /* 6 */
    {OP_adds,     0xf1100000, "adds",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, xrcpc[2][0x00]},
    {OP_cmn,      0xf1100f00, "cmn",    xx, xx, RAw, i12x26_12_0, xx, no, fWNZCV, xrcpc[2][0x01]},
  }, { /* 7 */
    {OP_subs,     0xf1b00000, "subs",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, xrcpc[3][0x00]},
    {OP_cmp,      0xf1b00f00, "cmp",    xx, xx, RAw, i12x26_12_0, xx, no, fWNZCV, xrcpc[3][0x01]},
  }, { /* 8 */
    {OP_ands,     0xf4100000, "ands",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, DUP_ENTRY},
    {OP_tst,      0xf4100f00, "tst",    xx, xx, RAw, i12x26_12_0, xx, no, fWNZC, DUP_ENTRY},
  }, { /* 9 */
    {OP_eors,     0xf4900000, "eors",   RCw, xx, RAw, i12x26_12_0, xx, no, fRC|fWNZC, DUP_ENTRY},
    {OP_teq,      0xf4900f00, "teq",    xx, xx, RAw, i12x26_12_0, xx, no, fWNZC, DUP_ENTRY},
  }, { /* 10 */
    {OP_adds,     0xf5100000, "adds",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, DUP_ENTRY},
    {OP_cmn,      0xf5100f00, "cmn",    xx, xx, RAw, i12x26_12_0, xx, no, fWNZCV, DUP_ENTRY},
  }, { /* 11 */
    {OP_subs,     0xf5b00000, "subs",   RCw, xx, RAw, i12x26_12_0, xx, no, fWNZCV, DUP_ENTRY},
    {OP_cmp,      0xf5b00f00, "cmp",    xx, xx, RAw, i12x26_12_0, xx, no, fWNZCV, DUP_ENTRY},
  },
};

/* Indexed by whether imm5 in B14:12,7:6 is 0 or not */
const instr_info_t T32_ext_imm126[][2] = {
  { /* 0 */
    {OP_mov,      0xea4f0000, "mov",    RCw, xx, RDw, xx, xx, no, x, END_LIST},
    {OP_lsl,      0xea4f0000, "lsl",    RCw, xx, RDw, i5x12_6, xx, no, x, xb7[0][0x00]},
  }, { /* 1 */
    {OP_rrx,      0xea4f0030, "rrx",    RCw, xx, RDw, xx, xx, no, x, END_LIST},
    {OP_ror,      0xea4f0030, "ror",    RCw, xx, RDw, i5x12_6, xx, no, x, xfop8[0][0xa6]},
  }, { /* 2 */
    {OP_movs,     0xea5f0000, "movs",   RCw, xx, RDw, xx, xx, no, fWNZ, END_LIST},
    {OP_lsls,     0xea5f0000, "lsls",   RCw, xx, RDw, i5x12_6, xx, no, fRC|fWNZC, xb7[1][0x00]},
  }, { /* 3 */
    {OP_rrxs,     0xea5f0030, "rrxs",   RCw, xx, RDw, xx, xx, no, fWNZC, END_LIST},
    {OP_rors,     0xea5f0030, "rors",   RCw, xx, RDw, i5x12_6, xx, no, fRC|fWNZC, xfop8[0][0xa7]},
  },
};

/****************************************************************************
 * Extra operands beyond the ones that fit into instr_info_t.
 * All cases where we have extra operands are either single-encoding-only
 * instructions or the final instruction in an encoding chain.
 */
const instr_info_t T32_extra_operands[] =
{
    /* 0x00 */
    {OP_CONTD, 0x00000000, "writeback shift + base", xx, xx, i5_7, RAw, xx, no, x, END_LIST},/*xop_shift*/
    {OP_CONTD, 0x00000000, "writeback base", xx, xx, RAw, xx, xx, no, x, END_LIST},/*xop_wb*/
    {OP_CONTD, 0x00000000, "writeback index + base", xx, xx, RDw, RAw, xx, no, x, END_LIST},/*xop_wb2*/
    {OP_CONTD, 0x00000000, "<cdp/mcr/mrc cont'd>", xx, xx, i3_5, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<smlalxb cont'd>",  xx, xx, RDh, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<smlalxt cont'd>",  xx, xx, RDt, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<srs* cont'd>",  xx, xx, SPSR, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<{s,u}mlal{,d} cont'd>",  xx, xx, RDw, xx, xx, no, x, END_LIST},
};

/* clang-format on */
