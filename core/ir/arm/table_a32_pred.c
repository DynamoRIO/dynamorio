/* **********************************************************
 * Copyright (c) 2014-2021 Google, Inc.  All rights reserved.
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

#include "../globals.h" /* need this to include decode.h (uint, etc.) */
#include "arch.h"       /* need this to include decode.h (byte, etc. */
#include "decode.h"
#include "decode_private.h"
#include "table_private.h"

/* XXX i#1685:
 * + Do we want to try and model all of the unpredictable conditions in
 *   each instruction (typically when pc or lr is used but it varies
 *   quite a bit)?  For core DR we don't care as much b/c w/ the fixed-width
 *   we can keep decoding and wait for a fault.
 * + See comments in arm/decode.c: similarly we don't model the reserved
 *   "(0)" bits as we'd need a separate mask and it seems very low priority
 *   and maybe even undesirable to mark an instr invalid with a "(0)" set to 1.
 */

// We skip auto-formatting for the entire file to keep our single-line table entries:
/* clang-format off */

/****************************************************************************
 * Top-level A32 table for predicate != 1111, indexed by bits 27:20
 */
const instr_info_t A32_pred_opc8[] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, flags, eflags, code} */
    /* 00 */
    {EXT_OPC4X,  0x00000000, "(ext opc4x 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_OPC4X,  0x00100000, "(ext opc4x 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_OPC4X,  0x00200000, "(ext opc4x 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_OPC4X,  0x00300000, "(ext opc4x 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_OPC4X,  0x00400000, "(ext opc4x 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_OPC4X,  0x00500000, "(ext opc4x 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_OPC4X,  0x00600000, "(ext opc4x 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_OPC4X,  0x00700000, "(ext opc4x 7)", xx, xx, xx, xx, xx, no, x, 7},
    /* 08 */
    {EXT_OPC4X,  0x00800000, "(ext opc4x 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_OPC4X,  0x00900000, "(ext opc4x 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_OPC4X,  0x00a00000, "(ext opc4x 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_OPC4X,  0x00b00000, "(ext opc4x 11)", xx, xx, xx, xx, xx, no, x, 11},
    {EXT_OPC4X,  0x00c00000, "(ext opc4x 12)", xx, xx, xx, xx, xx, no, x, 12},
    {EXT_OPC4X,  0x00d00000, "(ext opc4x 13)", xx, xx, xx, xx, xx, no, x, 13},
    {EXT_OPC4X,  0x00e00000, "(ext opc4x 14)", xx, xx, xx, xx, xx, no, x, 14},
    {EXT_OPC4X,  0x00f00000, "(ext opc4x 15)", xx, xx, xx, xx, xx, no, x, 15},
    /* 10 */
    {EXT_OPC4,   0x01000000, "(ext opc4 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_OPC4X,  0x01100000, "(ext opc4x 16)", xx, xx, xx, xx, xx, no, x, 16},
    {EXT_OPC4,   0x01200000, "(ext opc4 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_OPC4X,  0x01300000, "(ext opc4x 17)", xx, xx, xx, xx, xx, no, x, 17},
    {EXT_OPC4,   0x01400000, "(ext opc4 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_OPC4X,  0x01500000, "(ext opc4x 18)", xx, xx, xx, xx, xx, no, x, 18},
    {EXT_OPC4,   0x01600000, "(ext opc4 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_OPC4X,  0x01700000, "(ext opc4x 19)", xx, xx, xx, xx, xx, no, x, 19},
    /* 18 */
    {EXT_OPC4X,  0x01800000, "(ext opc4x 20)", xx, xx, xx, xx, xx, no, x, 20},
    {EXT_OPC4X,  0x01900000, "(ext opc4x 21)", xx, xx, xx, xx, xx, no, x, 21},
    {EXT_OPC4,   0x01a00000, "(ext opc4 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_OPC4,   0x01b00000, "(ext opc4 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_OPC4X,  0x01c00000, "(ext opc4x 22)", xx, xx, xx, xx, xx, no, x, 22},
    {EXT_OPC4X,  0x01d00000, "(ext opc4x 23)", xx, xx, xx, xx, xx, no, x, 23},
    {EXT_OPC4X,  0x01e00000, "(ext opc4x 24)", xx, xx, xx, xx, xx, no, x, 24},
    {EXT_OPC4X,  0x01f00000, "(ext opc4x 25)", xx, xx, xx, xx, xx, no, x, 25},
    /* 20 */
    {OP_and,     0x02000000, "and",    RBw, xx, RAw, i12sh, xx, pred, x, top4x[0][0x00]},
    {OP_ands,    0x02100000, "ands",   RBw, xx, RAw, i12sh, xx, pred, fRC|fWNZC, top4x[1][0x00]},
    {OP_eor,     0x02200000, "eor",    RBw, xx, RAw, i12sh, xx, pred, x, top4x[2][0x00]},
    {OP_eors,    0x02300000, "eors",   RBw, xx, RAw, i12sh, xx, pred, fRC|fWNZC, top4x[3][0x00]},
    {OP_sub,     0x02400000, "sub",    RBw, xx, RAw, i12sh, xx, pred, x, top4x[4][0x00]},/* XXX disasm: RA=r15 => "adr" */
    {OP_subs,    0x02500000, "subs",   RBw, xx, RAw, i12sh, xx, pred, fWNZCV, top4x[5][0x00]},
    {OP_rsb,     0x02600000, "rsb",    RBw, xx, RAw, i12sh, xx, pred, x, top4x[6][0x00]},
    {OP_rsbs,    0x02700000, "rsbs",   RBw, xx, RAw, i12sh, xx, pred, fWNZCV, top4x[7][0x00]},
    /* 28 */
    {OP_add,     0x02800000, "add",    RBw, xx, RAw, i12sh, xx, pred, x, top4x[8][0x00]}, /* XXX disasm: RA=r15 => "adr" */
    {OP_adds,    0x02900000, "adds",   RBw, xx, RAw, i12sh, xx, pred, fWNZCV, top4x[9][0x00]},
    {OP_adc,     0x02a00000, "adc",    RBw, xx, RAw, i12sh, xx, pred, fRC, top4x[10][0x00]},
    {OP_adcs,    0x02b00000, "adcs",   RBw, xx, RAw, i12sh, xx, pred, fRC|fWNZCV, top4x[11][0x00]},
    {OP_sbc,     0x02c00000, "sbc",    RBw, xx, RAw, i12sh, xx, pred, fRC, top4x[12][0x00]},
    {OP_sbcs,    0x02d00000, "sbcs",   RBw, xx, RAw, i12sh, xx, pred, fRC|fWNZCV, top4x[13][0x00]},
    {OP_rsc,     0x02e00000, "rsc",    RBw, xx, RAw, i12sh, xx, pred, fRC, top4x[14][0x00]},
    {OP_rscs,    0x02f00000, "rscs",   RBw, xx, RAw, i12sh, xx, pred, fRC|fWNZCV, top4x[15][0x00]},
    /* 30 */
    {OP_movw,    0x03000000, "movw",   RBw, xx, i16x16_0, xx, xx, pred, x, END_LIST},
    {OP_tst,     0x03100000, "tst",    xx, xx, RAw, i12sh, xx, pred, fWNZC, top4x[16][0x00]},
    {EXT_IMM1916,0x03200000, "(ext imm1916 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_teq,     0x03300000, "teq",    xx, xx, RAw, i12sh, xx, pred, fWNZC, top4x[17][0x00]},
    {OP_movt,    0x03400000, "movt",   RBt, xx, i16x16_0, xx, xx, pred, x, END_LIST},
    {OP_cmp,     0x03500000, "cmp",    xx, xx, RAw, i12sh, xx, pred, fWNZCV, top4x[18][0x00]},
    {OP_msr,     0x0360f000, "msr",    SPSR, xx, i4_16, i12sh, xx, pred, fWNZCVQG/*see decode_eflags_to_instr_eflags*/, END_LIST},
    {OP_cmn,     0x03700000, "cmn",    xx, xx, RAw, i12sh, xx, pred, fWNZCV, top4x[19][0x00]},
    /* 38 */
    {OP_orr,     0x03800000, "orr",    RBw, xx, RAw, i12sh, xx, pred, x, top4x[20][0x00]},
    {OP_orrs,    0x03900000, "orrs",   RBw, xx, RAw, i12sh, xx, pred, fRC|fWNZC, top4x[21][0x00]},
    {OP_mov,     0x03a00000, "mov",    RBw, xx, i12sh, xx, xx, pred, x, ti5[0][0x00]},
    {OP_movs,    0x03b00000, "movs",   RBw, xx, i12sh, xx, xx, pred, fRC|fWNZC, ti5[2][0x00]},
    {OP_bic,     0x03c00000, "bic",    RBw, xx, RAw, i12sh, xx, pred, x, top4x[22][0x00]},
    {OP_bics,    0x03d00000, "bics",   RBw, xx, RAw, i12sh, xx, pred, fRC|fWNZC, top4x[23][0x00]},
    {OP_mvn,     0x03e00000, "mvn",    RBw, xx, i12sh, xx, xx, pred, x, top4x[24][0x00]},
    {OP_mvns,    0x03f00000, "mvns",   RBw, xx, i12sh, xx, xx, pred, fRC|fWNZC, top4x[25][0x00]},
    /* 40 */
    {OP_str,     0x04000000, "str",    Mw, RAw, RBw, n12, RAw, pred, x, top4y[6][0x00]},/*PUW=000*/
    {OP_ldr,     0x04100000, "ldr",    RBw, RAw, Mw, n12, RAw, pred, x, top8[0x69]},/*PUW=000*/
    {OP_strt,    0x04200000, "strt",   Mw, RAw, RBw, n12, RAw, pred, x, top4y[7][0x00]},/*PUW=001*/
    {OP_ldrt,    0x04300000, "ldrt",   RBw, RAw, Mw, n12, RAw, pred, x, top4y[8][0x00]},/*PUW=001*/
    {OP_strb,    0x04400000, "strb",   Mb, RAw, RBb, n12, RAw, pred, x, top4y[9][0x00]},/*PUW=000*/
    {OP_ldrb,    0x04500000, "ldrb",   RBw, RAw, Mb, n12, RAw, pred, x, top8[0x6d]},/*PUW=000*/
    {OP_strbt,   0x04600000, "strbt",  Mb, RAw, RBb, n12, RAw, pred, x, top4y[10][0x00]},/*PUW=001*/
    {OP_ldrbt,   0x04700000, "ldrbt",  RBw, RAw, Mb, n12, RAw, pred, x, top4y[11][0x00]},/*PUW=001*/
    /* 48 */
    {OP_str,     0x04800000, "str",    Mw, RAw, RBw, i12, RAw, pred, x, top8[0x40]},/*PUW=010*/
    {OP_ldr,     0x04900000, "ldr",    RBw, RAw, Mw, i12, RAw, pred, x, top8[0x41]},/*PUW=010*//* XXX: RA=SP + imm12=4, then "pop RBw" */
    {OP_strt,    0x04a00000, "strt",   Mw, RAw, RBw, i12, RAw, pred, x, top8[0x42]},/*PUW=011*/
    {OP_ldrt,    0x04b00000, "ldrt",   RBw, RAw, Mw, i12, RAw, pred, x, top8[0x43]},/*PUW=011*/
    {OP_strb,    0x04c00000, "strb",   Mb, RAw, RBb, i12, RAw, pred, x, top8[0x44]},/*PUW=010*/
    {OP_ldrb,    0x04d00000, "ldrb",   RBw, RAw, Mb, i12, RAw, pred, x, top8[0x45]},/*PUW=010*/
    {OP_strbt,   0x04e00000, "strbt",  Mb, RAw, RBb, i12, RAw, pred, x, top8[0x46]},/*PUW=011*/
    {OP_ldrbt,   0x04f00000, "ldrbt",  RBw, RAw, Mb, i12, RAw, pred, x, top8[0x47]},/*PUW=011*/
    /* 50 */
    {OP_str,     0x05000000, "str",    MN12w, xx, RBw, xx, xx, pred, x, top8[0x5a]},/*PUW=100*/
    {OP_ldr,     0x05100000, "ldr",    RBw, xx, MN12w, xx, xx, pred, x, top8[0x79]},/*PUW=100*/
    {OP_str,     0x05200000, "str",    MN12w, RAw, RBw, n12, RAw, pred, x, tb4[3][0x00]},/*PUW=101*//* XXX: RA=SP + imm12=4, then "push RBw" */
    {OP_ldr,     0x05300000, "ldr",    RBw, RAw, MN12w, n12, RAw, pred, x, tb4[4][0x00]},/*PUW=101*/
    {OP_strb,    0x05400000, "strb",   MN12b, xx, RBb, xx, xx, pred, x, top8[0x5e]},/*PUW=100*/
    {OP_ldrb,    0x05500000, "ldrb",   RBw, xx, MN12b, xx, xx, pred, x, tb4[6][0x00]},/*PUW=100*/
    {OP_strb,    0x05600000, "strb",   MN12b, RAw, RBb, n12, RAw, pred, x, tb4[7][0x00]},/*PUW=101*/
    {OP_ldrb,    0x05700000, "ldrb",   RBw, RAw, MN12b, n12, RAw, pred, x, tb4[8][0x00]},/*PUW=101*/
    /* 58 */
    {OP_str,     0x05800000, "str",    MP12w, xx, RBw, xx, xx, pred, x, top4y[12][0x00]},/*PUW=110*/
    {OP_ldr,     0x05900000, "ldr",    RBw, xx, MP12w, xx, xx, pred, x, top8[0x51]},/*PUW=110*/
    {OP_str,     0x05a00000, "str",    MP12w, RAw, RBw, i12, RAw, pred, x, top8[0x52]},/*PUW=111*/
    {OP_ldr,     0x05b00000, "ldr",    RBw, RAw, MP12w, i12, RAw, pred, x, top8[0x53]},/*PUW=111*/
    {OP_strb,    0x05c00000, "strb",   MP12b, xx, RBb, xx, xx, pred, x, top4y[13][0x00]},/*PUW=110*/
    {OP_ldrb,    0x05d00000, "ldrb",   RBw, xx, MP12b, xx, xx, pred, x, top8[0x55]},/*PUW=110*/
    {OP_strb,    0x05e00000, "strb",   MP12b, RAw, RBb, i12, RAw, pred, x, top8[0x56]},/*PUW=111*/
    {OP_ldrb,    0x05f00000, "ldrb",   RBw, RAw, MP12b, i12, RAw, pred, x, top8[0x57]},/*PUW=111*/
    /* 60 */
    {OP_str,     0x06000000, "str",    Mw, RAw, RBw, RDNw, sh2, xop_shift|pred, x, END_LIST},/*PUW=000*/
    {EXT_OPC4Y,  0x06100000, "(ext opc4y 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_OPC4Y,  0x06200000, "(ext opc4y 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_OPC4Y,  0x06300000, "(ext opc4y 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_strb,    0x06400000, "strb",   Mb, RAw, RBb, RDNw, sh2, xop_shift|pred, x, END_LIST},/*PUW=000*/
    {EXT_OPC4Y,  0x06500000, "(ext opc4y 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_OPC4Y,  0x06600000, "(ext opc4y 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_OPC4Y,  0x06700000, "(ext opc4y 5)", xx, xx, xx, xx, xx, no, x, 5},
    /* 68 */
    {EXT_OPC4Y,  0x06800000, "(ext opc4y 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_ldr,     0x06900000, "ldr",    RBw, RAw, Mw, RDw, sh2, xop_shift|pred, x, top4y[0][0x00]},/*PUW=010*/
    {EXT_OPC4Y,  0x06a00000, "(ext opc4y 7)", xx, xx, xx, xx, xx, no, x, 7},
    {EXT_OPC4Y,  0x06b00000, "(ext opc4y 8)", xx, xx, xx, xx, xx, no, x, 8},
    {EXT_OPC4Y,  0x06c00000, "(ext opc4y 9)", xx, xx, xx, xx, xx, no, x, 9},
    {OP_ldrb,    0x06d00000, "ldrb",   RBw, RAw, Mb, RDw, sh2, xop_shift|pred, x, top4y[3][0x00]},/*PUW=010*/
    {EXT_OPC4Y,  0x06e00000, "(ext opc4y 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_OPC4Y,  0x06f00000, "(ext opc4y 11)", xx, xx, xx, xx, xx, no, x, 11},
    /* 70 */
    {EXT_OPC4Y,  0x07000000, "(ext opc4y 12)", xx, xx, xx, xx, xx, no, x, 12},
    {EXT_BIT4,   0x07100000, "(ext bit4 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_str,     0x07200000, "str",    MNSw, RAw, RBw, RDw, sh2, xop_shift|pred, x, top8[0x48]},/*PUW=101*/
    {EXT_BIT4,   0x07300000, "(ext bit4 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_OPC4Y,  0x07400000, "(ext opc4y 13)", xx, xx, xx, xx, xx, no, x, 13},
    {EXT_OPC4Y,  0x07500000, "(ext opc4y 14)", xx, xx, xx, xx, xx, no, x, 14},
    {OP_strb,    0x07600000, "strb",   MNSb, RAw, RBb, RDw, sh2, xop_shift|pred, x, top8[0x4c]},/*PUW=101*/
    {OP_ldrb,    0x07700000, "ldrb",   RBw, RAw, MNSb, RDw, sh2, xop_shift|pred, x, top8[0x4d]},/*PUW=101*/
    /* 78 */
    {EXT_BIT4,   0x07800000, "(ext bit4 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_ldr,     0x07900000, "ldr",    RBw, xx, MPSw, xx, xx, pred, x, tb4[0][0x00]},/*PUW=110*/
    {EXT_BIT4,   0x07a00000, "(ext bit4 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_BIT4,   0x07b00000, "(ext bit4 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_BIT4,   0x07c00000, "(ext bit4 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_BIT4,   0x07d00000, "(ext bit4 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_BIT4,   0x07e00000, "(ext bit4 7)", xx, xx, xx, xx, xx, no, x, 7},
    {EXT_BIT4,   0x07f00000, "(ext bit4 8)", xx, xx, xx, xx, xx, no, x, 8},
    /* 80 */
    {OP_stmda,   0x08000000, "stmda",  MDAl, xx, L16w, xx, xx, pred, x, END_LIST},/*PUW=000*/
    {OP_ldmda,   0x08100000, "ldmda",  L16w, xx, MDAl, xx, xx, pred, x, END_LIST},/*PUW=000*/
    {OP_stmda,   0x08200000, "stmda",  MDAl, RAw, L16w, RAw, xx, pred, x, top8[0x80]},/*PUW=001*/
    {OP_ldmda,   0x08300000, "ldmda",  L16w, RAw, MDAl, RAw, xx, pred, x, top8[0x81]},/*PUW=001*/
    {OP_stmda_priv,0x08400000,"stmda", MDAl, xx, L16w, xx, xx, pred, x, END_LIST},/*PUW=000*/
    {OP_ldmda_priv,0x08500000,"ldmda", L16w, xx, MDAl, xx, xx, pred, x, END_LIST},/*PUW=000*/
    {INVALID,    0x08600000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldmda_priv,0x08700000,"ldmda", L16w, RAw, MDAl, RAw, xx, pred, x, top8[0x85]},/*PUW=001*/
    /* 88 */
    {OP_stm,     0x08800000, "stm",    Ml, xx, L16w, xx, xx, pred, x, END_LIST},/*PUW=010*//* XXX: "stmia" alias (used inconsistently by gdb) */
    {OP_ldm,     0x08900000, "ldm",    L16w, xx, Ml, xx, xx, pred, x, END_LIST},/*PUW=010*//* XXX: "ldmia" and "ldmfb" aliases */
    {OP_stm,     0x08a00000, "stm",    Ml, RAw, L16w, RAw, xx, pred, x, top8[0x88]},/*PUW=011*/
    {OP_ldm,     0x08b00000, "ldm",    L16w, RAw, Ml, RAw, xx, pred, x, top8[0x89]},/*PUW=011*//* XXX: RA=SP, then "pop <list>" */
    {OP_stm_priv,0x08c00000, "stm",    Ml, xx, L16w, xx, xx, pred, x, END_LIST},/*PUW=010*/
    {OP_ldm_priv,0x08d00000, "ldm",    L16w, xx, Ml, xx, xx, pred, x, END_LIST},/*PUW=010*/
    {INVALID,    0x08e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldm_priv,0x08f00000, "ldm",    L16w, RAw, Ml, RAw, xx, pred, x, top8[0x8d]},/*PUW=011*/
    /* 90 */
    {OP_stmdb,   0x09000000, "stmdb",  MDBl, xx, L16w, xx, xx, pred, x, top8[0x92]},/*PUW=100*/
    {OP_ldmdb,   0x09100000, "ldmdb",  L16w, xx, MDBl, xx, xx, pred, x, top8[0x93]},/*PUW=100*/
    {OP_stmdb,   0x09200000, "stmdb",  MDBl, RAw, L16w, RAw, xx, pred, x, END_LIST},/*PUW=101*//* XXX: if RA=SP, then "push" */
    {OP_ldmdb,   0x09300000, "ldmdb",  L16w, RAw, MDBl, RAw, xx, pred, x, END_LIST},/*PUW=101*/
    {OP_stmdb_priv,0x09400000,"stmdb", MDBl, xx, L16w, xx, xx, pred, x, END_LIST},/*PUW=100*/
    {OP_ldmdb_priv,0x09500000,"ldmdb", L16w, xx, MDBl, xx, xx, pred, x, top8[0x97]},/*PUW=100*/
    {INVALID,    0x09600000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldmdb_priv,0x09700000,"ldmdb", L16w, RAw, MDBl, RAw, xx, pred, x, END_LIST},/*PUW=101*/
    /* 98 */
    {OP_stmib,   0x09800000, "stmib",  MUBl, xx, L16w, xx, xx, pred, x, top8[0x9a]},/*PUW=110*//* XXX: "stmia" or "stmea" alias */
    {OP_ldmib,   0x09900000, "ldmib",  L16w, xx, MUBl, xx, xx, pred, x, top8[0x9b]},/*PUW=110*//* XXX: "ldmia" alias */
    {OP_stmib,   0x09a00000, "stmib",  MUBl, RAw, L16w, RAw, xx, pred, x, END_LIST},/*PUW=111*/
    {OP_ldmib,   0x09b00000, "ldmib",  L16w, RAw, MUBl, RAw, xx, pred, x, END_LIST},/*PUW=111*/
    {OP_stmib_priv, 0x09c00000, "stmib", MUBl, xx, L16w, xx, xx, pred, x, END_LIST},/*PUW=110*/
    {OP_ldmib_priv, 0x09d00000, "ldmib", L16w, xx, MUBl, xx, xx, pred, x, END_LIST},/*PUW=110*/
    {INVALID,    0x09e00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA}, /* stm_priv w/ writeback */
    {INVALID,    0x09f00000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA}, /* ldm_priv w/ writeback */
    /* a0 */
    {OP_b,       0x0a000000, "b",      xx, xx, j24_x4, xx, xx, pred, x, END_LIST},/*no chain nec.*/
    {OP_b,       0x0a100000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0a200000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0a300000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0a400000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0a500000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0a600000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0a700000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    /* a8 */
    {OP_b,       0x0a800000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0a900000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0aa00000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0ab00000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0ac00000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0ad00000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0ae00000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_b,       0x0af00000, "b",      xx, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    /* b0 */
    {OP_bl,      0x0b000000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, END_LIST},/*no chain nec.*/
    {OP_bl,      0x0b100000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0b200000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0b300000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0b400000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0b500000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0b600000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0b700000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    /* b8 */
    {OP_bl,      0x0b800000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0b900000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0ba00000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0bb00000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0bc00000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0bd00000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0be00000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    {OP_bl,      0x0bf00000, "bl",     LRw, xx, j24_x4, xx, xx, pred, x, DUP_ENTRY},
    /* c0 */
    {INVALID,    0x0c000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0c100000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_FP,     0x0c200000, "(ext fp 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FP,     0x0c300000, "(ext fp 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_FP,     0x0c400000, "(ext fp 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_FP,     0x0c500000, "(ext fp 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_FP,     0x0c600000, "(ext fp 4)",  xx, xx, xx, xx, xx, no, x, 4},
    {EXT_FP,     0x0c700000, "(ext fp 5)",  xx, xx, xx, xx, xx, no, x, 5},
    /* c8 */
    {EXT_FP,     0x0c800000, "(ext fp 6)",  xx, xx, xx, xx, xx, no, x, 6},
    {EXT_FP,     0x0c900000, "(ext fp 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_FP,     0x0ca00000, "(ext fp 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_FP,     0x0cb00000, "(ext fp 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_FP,     0x0cc00000, "(ext fp 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_FP,     0x0cd00000, "(ext fp 11)",  xx, xx, xx, xx, xx, no, x, 11},
    {EXT_FP,     0x0ce00000, "(ext fp 12)",  xx, xx, xx, xx, xx, no, x, 12},
    {EXT_FP,     0x0cf00000, "(ext fp 13)",  xx, xx, xx, xx, xx, no, x, 13},
    /* d0 */
    {EXT_FP,     0x0d000000, "(ext fp 14)",  xx, xx, xx, xx, xx, no, x, 14},
    {EXT_FP,     0x0d100000, "(ext fp 15)",  xx, xx, xx, xx, xx, no, x, 15},
    {EXT_FP,     0x0d200000, "(ext fp 16)",  xx, xx, xx, xx, xx, no, x, 16},
    {EXT_FP,     0x0d300000, "(ext fp 17)",  xx, xx, xx, xx, xx, no, x, 17},
    {EXT_FP,     0x0d400000, "(ext fp 18)",  xx, xx, xx, xx, xx, no, x, 18},
    {EXT_FP,     0x0d500000, "(ext fp 19)",  xx, xx, xx, xx, xx, no, x, 19},
    {EXT_FP,     0x0d600000, "(ext fp 20)",  xx, xx, xx, xx, xx, no, x, 20},
    {EXT_FP,     0x0d700000, "(ext fp 21)",  xx, xx, xx, xx, xx, no, x, 21},
    /* d8 */
    {EXT_FP,     0x0d800000, "(ext fp 22)",  xx, xx, xx, xx, xx, no, x, 22},
    {EXT_FP,     0x0d900000, "(ext fp 23)",  xx, xx, xx, xx, xx, no, x, 23},
    {EXT_FP,     0x0da00000, "(ext fp 24)",  xx, xx, xx, xx, xx, no, x, 24},
    {EXT_FP,     0x0db00000, "(ext fp 25)",  xx, xx, xx, xx, xx, no, x, 25},
    {EXT_FP,     0x0dc00000, "(ext fp 26)",  xx, xx, xx, xx, xx, no, x, 26},
    {EXT_FP,     0x0dd00000, "(ext fp 27)",  xx, xx, xx, xx, xx, no, x, 27},
    {EXT_FP,     0x0de00000, "(ext fp 28)",  xx, xx, xx, xx, xx, no, x, 28},
    {EXT_FP,     0x0df00000, "(ext fp 29)",  xx, xx, xx, xx, xx, no, x, 29},
    /* e0 */
    {EXT_FP,     0x0e000000, "(ext fp 30)",  xx, xx, xx, xx, xx, no, x, 30},
    {EXT_FP,     0x0e100000, "(ext fp 31)",  xx, xx, xx, xx, xx, no, x, 31},
    {EXT_FP,     0x0e200000, "(ext fp 32)",  xx, xx, xx, xx, xx, no, x, 32},
    {EXT_FP,     0x0e300000, "(ext fp 33)",  xx, xx, xx, xx, xx, no, x, 33},
    {EXT_FP,     0x0e400000, "(ext fp 34)",  xx, xx, xx, xx, xx, no, x, 34},
    {EXT_FP,     0x0e500000, "(ext fp 35)",  xx, xx, xx, xx, xx, no, x, 35},
    {EXT_FP,     0x0e600000, "(ext fp 36)",  xx, xx, xx, xx, xx, no, x, 36},
    {EXT_FP,     0x0e700000, "(ext fp 37)",  xx, xx, xx, xx, xx, no, x, 37},
    /* e8 */
    {EXT_FP,     0x0e800000, "(ext fp 38)",  xx, xx, xx, xx, xx, no, x, 38},
    {EXT_FP,     0x0e900000, "(ext fp 39)",  xx, xx, xx, xx, xx, no, x, 39},
    {EXT_FP,     0x0ea00000, "(ext fp 40)",  xx, xx, xx, xx, xx, no, x, 40},
    {EXT_FP,     0x0eb00000, "(ext fp 41)",  xx, xx, xx, xx, xx, no, x, 41},
    {EXT_FP,     0x0ec00000, "(ext fp 42)",  xx, xx, xx, xx, xx, no, x, 42},
    {EXT_FP,     0x0ed00000, "(ext fp 43)",  xx, xx, xx, xx, xx, no, x, 43},
    {EXT_FP,     0x0ee00000, "(ext fp 44)",  xx, xx, xx, xx, xx, no, x, 44},
    {EXT_FP,     0x0ef00000, "(ext fp 45)",  xx, xx, xx, xx, xx, no, x, 45},
    /* f0 */
    {OP_svc,     0x0f000000, "svc",    xx, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_svc,     0x0f100000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0f200000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0f300000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0f400000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0f500000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0f600000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0f700000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    /* f8 */
    {OP_svc,     0x0f800000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0f900000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0fa00000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0fb00000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0fc00000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0fd00000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0fe00000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
    {OP_svc,     0x0ff00000, "svc",    xx, xx, i24, xx, xx, pred, x, DUP_ENTRY},
};

/* Indexed by bits 7:4 but in the following manner:
 * + If bit 4 == 0, take entry 0;
 * + If bit 4 == 1 and bit 7 == 0, take entry 1;
 * + Else, take entry 2 + bits 6:5
 */
const instr_info_t A32_ext_opc4x[][6] = {
  { /* 0 */
    {OP_and,     0x00000000, "and",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, x, top4x[0][0x01]},
    {OP_and,     0x00000010, "and",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, x, END_LIST},
    {OP_mul,     0x00000090, "mul",    RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
    {OP_strh,    0x000000b0, "strh",   Mh, RAw, RBh, RDNw, RAw, pred, x, END_LIST},/*PUW=000*/
    {OP_ldrd,    0x000000d0, "ldrd",   RBEw, RB2w, RAw, Mq, RDNw, xop_wb|pred|dstX3, x, END_LIST},/*PUW=000*/
    {OP_strd,    0x000000f0, "strd",   Mq, RAw, RBEw, RB2w, RDNw, xop_wb|pred, x, END_LIST},/*PUW=000*/
  }, { /* 1 */
    {OP_ands,    0x00100000, "ands",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZC, top4x[1][0x01]},
    {OP_ands,    0x00100010, "ands",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fWNZC, END_LIST},
    {OP_muls,    0x00100090, "muls",   RAw, xx, RDw, RCw, xx, pred, fWNZ, END_LIST},
    {OP_ldrh,    0x001000b0, "ldrh",   RBw, RAw, Mw, RDNw, RAw, pred, x, END_LIST},/*PUW=000*/
    {OP_ldrsb,   0x001000d0, "ldrsb",  RBw, RAw, Mb, RDNw, RAw, pred, x, END_LIST},/*PUW=000*/
    {OP_ldrsh,   0x001000f0, "ldrsh",  RBw, RAw, Mh, RDNw, RAw, pred, x, END_LIST},/*PUW=000*/
  }, { /* 2 */
    {OP_eor,     0x00200000, "eor",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, x, top4x[2][0x01]},
    {OP_eor,     0x00200010, "eor",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, x, END_LIST},
    {OP_mla,     0x00200090, "mla",    RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_strht,   0x002000b0, "strht",  Mh, RAw, RBh, RDNw, RAw, pred, x, END_LIST},/*PUW=001*/
    {OP_ldrd,    0x002000d0, "ldrd",   RBEw, RB2w, RAw, Mq, RDNw, xop_wb|pred|dstX3|unp, x, top4x[0][0x04]},/*PUW=001*/
    {OP_strd,    0x002000f0, "strd",   Mq, RAw, RBEw, RB2w, RDNw, xop_wb|pred|unp, x, top4x[0][0x05]},/*PUW=001*/
  }, { /* 3 */
    {OP_eors,    0x00300000, "eors",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZC, top4x[3][0x01]},
    {OP_eors,    0x00300010, "eors",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fWNZC, END_LIST},
    {OP_mlas,    0x00300090, "mlas",   RAw, xx, RDw, RCw, RBw, pred, fWNZ, END_LIST},
    {OP_ldrht,   0x003000b0, "ldrht",  RBw, RAw, Mh, RDNw, RAw, pred, x, END_LIST},/*PUW=001*/
    {OP_ldrsbt,  0x003000d0, "ldrsbt", RBw, RAw, Mb, RDNw, RAw, pred, x, END_LIST},/*PUW=001*/
    {OP_ldrsht,  0x003000f0, "ldrsht", RBw, RAw, Mh, RDNw, RAw, pred, x, END_LIST},/*PUW=001*/
  }, { /* 4 */
    {OP_sub,     0x00400000, "sub",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, x, top4x[4][0x01]},
    {OP_sub,     0x00400010, "sub",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, x, END_LIST},
    {OP_umaal,   0x00400090, "umaal",  RAw, RBw, RAw, RBw, RDw, pred|xop, x, exop[0x7]},
    {OP_strh,    0x004000b0, "strh",   Mh, RAw, RBh, n8x8_0, RAw, pred, x, top4x[0][0x03]},/*PUW=000*/
    {OP_ldrd,    0x004000d0, "ldrd",   RBEw, RB2w, RAw, Mq, n8x8_0, xop_wb|pred|dstX3, x, top4x[2][0x04]},/*PUW=000*/
    {OP_strd,    0x004000f0, "strd",   Mq, RAw, RBEw, RB2w, n8x8_0, xop_wb|pred, x, top4x[2][0x05]},/*PUW=000*/
  }, { /* 5 */
    {OP_subs,    0x00500000, "subs",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZCV, top4x[5][0x01]},
    {OP_subs,    0x00500010, "subs",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fWNZCV, END_LIST},
    {INVALID,    0x00500090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh,    0x005000b0, "ldrh",   RBw, RAw, Mh, n8x8_0, RAw, pred, x, top4x[1][0x03]},/*PUW=000*/
    {OP_ldrsb,   0x005000d0, "ldrsb",  RBw, RAw, Mb, n8x8_0, RAw, pred, x, top4x[1][0x04]},/*PUW=000*/
    {OP_ldrsh,   0x005000f0, "ldrsh",  RBw, RAw, Mh, n8x8_0, RAw, pred, x, top4x[1][0x05]},/*PUW=000*/
  }, { /* 6 */
    {OP_rsb,     0x00600000, "rsb",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, x, top4x[6][0x01]},
    {OP_rsb,     0x00600010, "rsb",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, x, END_LIST},
    {OP_mls,     0x00600090, "mls",    RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_strht,   0x006000b0, "strht",  Mh, RAw, RBh, n8x8_0, RAw, pred, x, top4x[2][0x03]},/*PUW=001*/
    {OP_ldrd,    0x006000d0, "ldrd",   RBEw, RB2w, RAw, Mq, n8x8_0, xop_wb|dstX3|pred|unp, x, top4x[4][0x04]},/*PUW=001*/
    {OP_strd,    0x006000f0, "strd",   Mq, RAw, RBEw, RB2w, n8x8_0, xop_wb|pred|unp, x, top4x[4][0x05]},/*PUW=001*/
  }, { /* 7 */
    {OP_rsbs,    0x00700000, "rsbs",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZCV, top4x[7][0x01]},
    {OP_rsbs,    0x00700010, "rsbs",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fWNZCV, END_LIST},
    {INVALID,    0x00700090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrht,   0x007000b0, "ldrht",  RBw, RAw, Mh, n8x8_0, RAw, pred, x, top4x[3][0x03]},/*PUW=001*/
    {OP_ldrsbt,  0x007000d0, "ldrsbt", RBw, RAw, Mb, n8x8_0, RAw, pred, x, top4x[3][0x04]},/*PUW=001*/
    {OP_ldrsht,  0x007000f0, "ldrsht", RBw, RAw, Mh, n8x8_0, RAw, pred, x, top4x[3][0x05]},/*PUW=001*/
  }, { /* 8 */
    {OP_add,     0x00800000, "add",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, x, top4x[8][0x01]},
    {OP_add,     0x00800010, "add",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, x, END_LIST},
    {OP_umull,   0x00800090, "umull",  RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {OP_strh,    0x008000b0, "strh",   MPRh, RAw, RBh, RDw, RAw, pred, x, top4x[4][0x03]},/*PUW=010*/
    {OP_ldrd,    0x008000d0, "ldrd",   RBEw, RB2w, RAw, MPRq, RDw, xop_wb|pred|dstX3, x, top4x[6][0x04]},/*PUW=010*/
    {OP_strd,    0x008000f0, "strd",   MPRq, RAw, RBEw, RB2w, RDw, xop_wb|pred, x, top4x[6][0x05]},/*PUW=010*/
  }, { /* 9 */
    {OP_adds,    0x00900000, "adds",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZCV, top4x[9][0x01]},
    {OP_adds,    0x00900010, "adds",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fWNZCV, END_LIST},
    {OP_umulls,  0x00900090, "umulls", RAw, RBw, RDw, RCw, xx, pred, fWNZ, END_LIST},
    {OP_ldrh,    0x009000b0, "ldrh",   RBw, RAw, MPRw, RDw, RAw, pred, x, top4x[5][0x03]},/*PUW=010*/
    {OP_ldrsb,   0x009000d0, "ldrsb",  RBw, RAw, MPRb, RDw, RAw, pred, x, top4x[5][0x04]},/*PUW=010*/
    {OP_ldrsh,   0x009000f0, "ldrsh",  RBw, RAw, MPRh, RDw, RAw, pred, x, top4x[5][0x05]},/*PUW=010*/
  }, { /* 10 */
    {OP_adc,     0x00a00000, "adc",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fRC, top4x[10][0x01]},
    {OP_adc,     0x00a00010, "adc",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, fRC, END_LIST},
    {OP_umlal,   0x00a00090, "umlal",  RAw, RBw, RAw, RBw, RDw, pred|xop, x, exop[0x7]},
    {OP_strht,   0x00a000b0, "strht",  MPRh, RAw, RBh, RDw, RAw, pred, x, top4x[6][0x03]},/*PUW=011*/
    {OP_ldrd,    0x00a000d0, "ldrd",   RBEw, RB2w, RAw, MPRq, RDw, xop_wb|pred|dstX3|unp, x, top4x[8][0x04]},/*PUW=011*/
    {OP_strd,    0x00a000f0, "strd",   MPRq, RAw, RBEw, RB2w, RDw, xop_wb|pred|unp, x, top4x[8][0x05]},/*PUW=011*/
  }, { /* 11 */
    {OP_adcs,    0x00b00000, "adcs",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fRC|fWNZCV, top4x[11][0x01]},
    {OP_adcs,    0x00b00010, "adcs",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fRC|fWNZCV, END_LIST},
    {OP_umlals,  0x00b00090, "umlals", RAw, RBw, RAw, RBw, RDw, pred|xop, fWNZ, exop[0x7]},
    {OP_ldrht,   0x00b000b0, "ldrht",  RBw, RAw, MPRh, RDw, RAw, pred, x, top4x[7][0x03]},/*PUW=011*/
    {OP_ldrsbt,  0x00b000d0, "ldrsbt", RBw, RAw, MPRb, RDw, RAw, pred, x, top4x[7][0x04]},/*PUW=011*/
    {OP_ldrsht,  0x00b000f0, "ldrsht", RBw, RAw, MPRh, RDw, RAw, pred, x, top4x[7][0x05]},/*PUW=011*/
  }, { /* 12 */
    {OP_sbc,     0x00c00000, "sbc",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fRC, top4x[12][0x01]},
    {OP_sbc,     0x00c00010, "sbc",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, fRC, END_LIST},
    {OP_smull,   0x00c00090, "smull",  RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {OP_strh,    0x00c000b0, "strh",   MP44h, RAw, RBh, i8x8_0, RAw, pred, x, top4x[8][0x03]},/*PUW=010*/
    {OP_ldrd,    0x00c000d0, "ldrd",   RBEw, RB2w, RAw, MP44q, i8x8_0, xop_wb|pred|dstX3, x, top4x[10][0x04]},/*PUW=010*/
    {OP_strd,    0x00c000f0, "strd",   MP44q, RAw, RBEw, RB2w, i8x8_0, xop_wb|pred, x, top4x[10][0x05]},/*PUW=010*/
  }, { /* 13 */
    {OP_sbcs,    0x00d00000, "sbcs",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fRC|fWNZCV, top4x[13][0x01]},
    {OP_sbcs,    0x00d00010, "sbcs",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fRC|fWNZCV, END_LIST},
    {OP_smulls,  0x00d00090, "smulls", RAw, RBw, RDw, RCw, xx, pred, fWNZ, END_LIST},
    {OP_ldrh,    0x00d000b0, "ldrh",   RBw, RAw, Mh, i8x8_0, RAw, pred, x, top4x[9][0x03]},/*PUW=010*/
    {OP_ldrsb,   0x00d000d0, "ldrsb",  RBw, RAw, Mb, i8x8_0, RAw, pred, x, top4x[9][0x04]},/*PUW=010*/
    {OP_ldrsh,   0x00d000f0, "ldrsh",  RBw, RAw, Mh, i8x8_0, RAw, pred, x, top4x[9][0x05]},/*PUW=010*/
  }, { /* 14 */
    {OP_rsc,     0x00e00000, "rsc",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fRC, top4x[14][0x01]},
    {OP_rsc,     0x00e00010, "rsc",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, fRC, END_LIST},
    {OP_smlal,   0x00e00090, "smlal",  RAw, RBw, RAw, RBw, RDw, pred|xop, x, exop[0x7]},
    {OP_strht,   0x00e000b0, "strht",  MP44h, RAw, RBh, i8x8_0, RAw, pred, x, top4x[10][0x03]},/*PUW=011*/
    {OP_ldrd,    0x00e000d0, "ldrd",   RBEw, RB2w, RAw, MP44q, i8x8_0, xop_wb|dstX3|pred|unp, x, top4x[12][0x04]},/*PUW=011*/
    {OP_strd,    0x00e000f0, "strd",   MP44q, RAw, RBEw, RB2w, i8x8_0, xop_wb|pred|unp, x, top4x[12][0x05]},/*PUW=011*/
  }, { /* 15 */
    {OP_rscs,    0x00f00000, "rscs",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fRC|fWNZCV, top4x[15][0x01]},
    {OP_rscs,    0x00f00010, "rscs",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fRC|fWNZCV, END_LIST},
    {OP_smlals,  0x00f00090, "smlals", RAw, RBw, RAw, RBw, RDw, pred|xop, fWNZ, exop[0x7]},
    {OP_ldrht,   0x00f000b0, "ldrht",  RBw, RAw, MP44h, i8x8_0, RAw, pred, x, top4x[11][0x03]},/*PUW=011*/
    {OP_ldrsbt,  0x00f000d0, "ldrsbt", RBw, RAw, MP44b, i8x8_0, RAw, pred, x, top4x[11][0x04]},/*PUW=011*/
    {OP_ldrsht,  0x00f000f0, "ldrsht", RBw, RAw, MP44h, i8x8_0, RAw, pred, x, top4x[11][0x05]},/*PUW=011*/
  }, { /* 16 */
    {OP_tst,     0x01100000, "tst",    xx, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZC, top4x[16][0x01]},
    {OP_tst,     0x01100010, "tst",    xx, RAw, RDw, sh2, RCb, pred|srcX4, fWNZC, END_LIST},
    {INVALID,    0x01100090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh,    0x011000b0, "ldrh",   RBw, xx, MNRh, xx, xx, pred, x, top4x[25][0x03]},/*PUW=100*/
    {OP_ldrsb,   0x011000d0, "ldrsb",  RBw, xx, MNRb, xx, xx, pred, x, top4x[25][0x04]},/*PUW=100*/
    {OP_ldrsh,   0x011000f0, "ldrsh",  RBw, xx, MNRh, xx, xx, pred, x, top4x[25][0x05]},/*PUW=100*/
  }, { /* 17 */
    {OP_teq,     0x01300000, "teq",    xx, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZC, top4x[17][0x01]},
    {OP_teq,     0x01300010, "teq",    xx, RAw, RDw, sh2, RCb, pred|srcX4, fWNZC, END_LIST},
    {INVALID,    0x01300090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh,    0x013000b0, "ldrh",   RBw, RAw, MNRw, RDNw, RAw, pred, x, top4x[13][0x03]},/*PUW=101*/
    {OP_ldrsb,   0x013000d0, "ldrsb",  RBw, RAw, MNRb, RDNw, RAw, pred, x, top4x[13][0x04]},/*PUW=101*/
    {OP_ldrsh,   0x013000f0, "ldrsh",  RBw, RAw, MNRh, RDNw, RAw, pred, x, top4x[13][0x05]},/*PUW=101*/
  }, { /* 18 */
    {OP_cmp,     0x01500000, "cmp",    xx, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZCV, top4x[18][0x01]},
    {OP_cmp,     0x01500010, "cmp",    xx, RAw, RDw, sh2, RCb, pred|srcX4, fWNZCV, END_LIST},
    {INVALID,    0x01500090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh,    0x015000b0, "ldrh",   RBw, xx, MN44h, xx, xx, pred, x, top4x[16][0x03]},/*PUW=100*/
    {OP_ldrsb,   0x015000d0, "ldrsb",  RBw, xx, MN44b, xx, xx, pred, x, top4x[16][0x04]},/*PUW=100*/
    {OP_ldrsh,   0x015000f0, "ldrsh",  RBw, xx, MN44h, xx, xx, pred, x, top4x[16][0x05]},/*PUW=100*/
  }, { /* 19 */
    {OP_cmn,     0x01700000, "cmn",    xx, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZCV, top4x[19][0x01]},
    {OP_cmn,     0x01700010, "cmn",    xx, RAw, RDw, sh2, RCb, pred|srcX4, fWNZCV, END_LIST},
    {INVALID,    0x01700090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh,    0x017000b0, "ldrh",   RBw, RAw, MN44h, n8x8_0, RAw, pred, x, top4[5][0x0b]},/*PUW=101*/
    {OP_ldrsb,   0x017000d0, "ldrsb",  RBw, RAw, MN44b, n8x8_0, RAw, pred, x, top4x[17][0x04]},/*PUW=101*/
    {OP_ldrsh,   0x017000f0, "ldrsh",  RBw, RAw, MN44h, n8x8_0, RAw, pred, x, top4x[17][0x05]},/*PUW=101*/
  }, { /* 20 */
    {OP_orr,     0x01800000, "orr",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, x, top4x[20][0x01]},
    {OP_orr,     0x01800010, "orr",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, x, END_LIST},
    {EXT_BITS8,  0x01800090, "(ext bits8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_strh,    0x018000b0, "strh",   MPRh, xx, RBh, xx, xx, pred, x, top4[2][0x0b]},/*PUW=110*/
    {OP_ldrd,    0x018000d0, "ldrd",   RBEw, RB2w, MPRq, xx, xx, pred, x, top4[0][0x0d]},/*PUW=110*/
    {OP_strd,    0x018000f0, "strd",   MPRq, xx, RBEw, RB2w, xx, pred, x, top4[2][0x0f]},/*PUW=110*/
  }, { /* 21 */
    {OP_orrs,    0x01900000, "orrs",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZC, top4x[21][0x01]},
    {OP_orrs,    0x01900010, "orrs",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fWNZC, END_LIST},
    {EXT_BITS8,  0x01900090, "(ext bits8 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_ldrh,    0x019000b0, "ldrh",   RBw, xx, MPRh, xx, xx, pred, x, top4x[18][0x03]},/*PUW=110*/
    {OP_ldrsb,   0x019000d0, "ldrsb",  RBw, xx, MPRb, xx, xx, pred, x, top4x[18][0x04]},/*PUW=110*/
    {OP_ldrsh,   0x019000f0, "ldrsh",  RBw, xx, MPRh, xx, xx, pred, x, top4x[18][0x05]},/*PUW=110*/
  }, { /* 22 */
    {OP_bic,     0x01c00000, "bic",    RBw, RAw, RDw, sh2, i5_7, pred|srcX4, x, top4x[22][0x01]},
    {OP_bic,     0x01c00010, "bic",    RBw, RAw, RDw, sh2, RCb, pred|srcX4, x, END_LIST},
    {EXT_BITS8,  0x01b00090, "(ext bits8 4)", xx, xx, xx, xx, xx, no, x, 4},
    {OP_strh,    0x01c000b0, "strh",   MP44h, xx, RBh, xx, xx, pred, x, top4x[20][0x03]},/*PUW=110*/
    {OP_ldrd,    0x01c000d0, "ldrd",   RBEw, RB2w, MP44q, xx, xx, pred, x, top4[2][0x0d]},/*PUW=110*/
    {OP_strd,    0x01c000f0, "strd",   MP44q, xx, RBEw, RB2w, xx, pred, x, top4x[20][0x05]},/*PUW=110*/
  }, { /* 23 */
    {OP_bics,    0x01d00000, "bics",   RBw, RAw, RDw, sh2, i5_7, pred|srcX4, fWNZC, top4x[23][0x01]},
    {OP_bics,    0x01d00010, "bics",   RBw, RAw, RDw, sh2, RCb, pred|srcX4, fWNZC, END_LIST},
    {EXT_BITS8,  0x01d00090, "(ext bits8 5)", xx, xx, xx, xx, xx, no, x, 5},
    {OP_ldrh,    0x01d000b0, "ldrh",   RBw, xx, MP44h, xx, xx, pred, x, top4x[21][0x03]},/*PUW=110*/
    {OP_ldrsb,   0x01d000d0, "ldrsb",  RBw, xx, MP44b, xx, xx, pred, x, top4x[21][0x04]},/*PUW=110*/
    {OP_ldrsh,   0x01d000f0, "ldrsh",  RBw, xx, MP44h, xx, xx, pred, x, top4x[21][0x05]},/*PUW=110*/
  }, { /* 24 */
    {OP_mvn,     0x01e00000, "mvn",    RBw, xx, RDw, sh2, i5_7, pred, x, top4x[24][0x01]},
    {OP_mvn,     0x01e00010, "mvn",    RBw, xx, RDw, sh2, RCb, pred, x, END_LIST},
    {EXT_BITS8,  0x01e00090, "(ext bits8 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_strh,    0x01e000b0, "strh",   MP44h, RAw, RBh, i8x8_0, RAw, pred, x, DUP_ENTRY},/*PUW=111*/
    {OP_ldrd,    0x01e000d0, "ldrd",   RBEw, RB2w, RAw, MP44q, i8x8_0, xop_wb|pred|dstX3, x, DUP_ENTRY},/*PUW=111*/
    {OP_strd,    0x01e000f0, "strd",   MP44q, RAw, RBEw, RB2w, i8x8_0, xop_wb|pred, x, DUP_ENTRY},/*PUW=111*/
  }, { /* 25 */
    {OP_mvns,    0x01f00000, "mvns",   RBw, xx, RDw, sh2, i5_7, pred, fWNZC, top4x[25][0x01]},
    {OP_mvns,    0x01f00010, "mvns",   RBw, xx, RDw, sh2, RCb, pred, fWNZC, END_LIST},
    {EXT_BITS8,  0x01f00090, "(ext bits8 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_ldrh,    0x01f000b0, "ldrh",   RBw, RAw, MP44h, i8x8_0, RAw, pred, x, top4x[19][0x03]},/*PUW=111*/
    {OP_ldrsb,   0x01f000d0, "ldrsb",  RBw, RAw, MP44b, i8x8_0, RAw, pred, x, top4x[19][0x04]},/*PUW=111*/
    {OP_ldrsh,   0x01f000f0, "ldrsh",  RBw, RAw, MP44h, i8x8_0, RAw, pred, x, top4x[19][0x05]},/*PUW=111*/
  },
};

/* Indexed by bits 7:4 but in the following manner:
 * + If bit 4 == 0, take entry 0;
 * + Else, take entry 1 + bits 7:5
 */
const instr_info_t A32_ext_opc4y[][9] = {
  { /* 0 */
    {OP_ldr,     0x06100000, "ldr",    RBw, RAw, Mw, RDNw, sh2, xop_shift|pred, x, END_LIST},/*PUW=000*/
    {OP_sadd16,  0x06100f10, "sadd16", RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
    {OP_sasx,    0x06100f30, "sasx",   RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
    {OP_ssax,    0x06100f50, "ssax",   RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
    {OP_ssub16,  0x06100f70, "ssub16", RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
    {OP_sadd8,   0x06100f90, "sadd8",  RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
    {INVALID,    0x061000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x061000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ssub8,   0x06100ff0, "ssub8",  RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
  }, { /* 1 */
    {OP_strt,    0x06200000, "strt",   Mw, RAw, RBw, RDNw, sh2, xop_shift|pred, x, END_LIST},/*PUW=001*/
    {OP_qadd16,  0x06200f10, "qadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_qasx,    0x06200f30, "qasx",   RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_qsax,    0x06200f50, "qsax",   RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_qsub16,  0x06200f70, "qsub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_qadd8,   0x06200f90, "qadd8",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID,    0x062000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x062000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_qsub8,   0x06200ff0, "qsub8",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 2 */
    {OP_ldrt,    0x06300000, "ldrt",   RBw, RAw, Mw, RDNw, sh2, xop_shift|pred, x, END_LIST},/*PUW=001*/
    {OP_shadd16, 0x06300f10, "shadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_shasx,   0x06300f30, "shasx",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_shsax,   0x06300f50, "shsax",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_shsub16, 0x06300f70, "shsub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_shadd8,  0x06300f90, "shadd8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID,    0x063000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x063000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_shsub8,  0x06300ff0, "shsub8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 3 */
    {OP_ldrb,    0x06500000, "ldrb",   RBw, RAw, Mb, RDNw, sh2, xop_shift|pred, x, END_LIST},/*PUW=000*/
    {OP_uadd16,  0x06500f10, "uadd16", RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
    {OP_uasx,    0x06500f30, "uasx",   RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_usax,    0x06500f50, "usax",   RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
    {OP_usub16,  0x06500f70, "usub16", RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
    {OP_uadd8,   0x06500f90, "uadd8",  RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
    {INVALID,    0x065000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x065000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_usub8,   0x06500ff0, "usub8",  RBw, xx, RAw, RDw, xx, pred, fWGE, END_LIST},
  }, { /* 4 */
    {OP_strbt,   0x06600000, "strbt",  Mb, RAw, RBb, RDNw, sh2, xop_shift|pred, x, END_LIST},/*PUW=001*/
    {OP_uqadd16, 0x06600f10, "uqadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uqasx,   0x06600f30, "uqasx",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uqsax,   0x06600f50, "uqsax",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uqsub16, 0x06600f70, "uqsub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uqadd8,  0x06600f90, "uqadd8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID,    0x066000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x066000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_uqsub8,  0x06600ff0, "uqsub8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 5 */
    {OP_ldrbt,   0x06700000, "ldrbt",  RBw, RAw, Mb, RDNw, sh2, xop_shift|pred, x, END_LIST},/*PUW=001*/
    {OP_uhadd16, 0x06700f10, "uhadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uhasx,   0x06700f30, "uhasx",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uhsax,   0x06700f50, "uhsax",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uhsub16, 0x06700f70, "uhsub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uhadd8,  0x06700f90, "uhadd8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID,    0x067000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x067000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_uhsub8,  0x06700ff0, "uhsub8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 6 */
    {OP_str,     0x06800000, "str",    Mw, RAw, RBw, RDw, sh2, xop_shift|pred, x, top8[0x60]},/*PUW=010*/
    {OP_pkhbt,   0x06800010, "pkhbt",  RBw, RAh, RDt, LSL, i5_7, pred|srcX4, x, END_LIST},
    {INVALID,    0x06800030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_pkhtb,   0x06800050, "pkhtb",  RBw, RAt, RDh, ASR, i5_7, pred|srcX4, x, END_LIST},
    {EXT_RAPC,   0x06800070, "(ext rapc 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_pkhbt,   0x06800010, "pkhbt",  RBw, RAh, RDt, LSL, i5_7, pred|srcX4, x, DUP_ENTRY},
    {OP_sel,     0x06800fb0, "sel",    RBw, xx, RAw, RDw, xx, pred, fRGE, END_LIST},
    {OP_pkhtb,   0x06800050, "pkhtb",  RBw, RAt, RDh, ASR, i5_7, pred|srcX4, x, DUP_ENTRY},
    {INVALID,    0x068000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 7 */
    {OP_strt,    0x06a00000, "strt",   Mw, RAw, RBw, RDw, sh2, xop_shift|pred, x, top4y[1][0x00]},/*PUW=011*/
    {OP_ssat,    0x06a00010, "ssat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, END_LIST},
    {OP_ssat16,  0x06a00f30, "ssat16", RBw, xx, i4_16, RDw, xx, pred, fWQ, END_LIST},
    {OP_ssat,    0x06a00050, "ssat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {EXT_RAPC,   0x06a00070, "(ext rapc 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_ssat,    0x06a00010, "ssat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {INVALID,    0x06a000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ssat,    0x06a00050, "ssat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {INVALID,    0x06a000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 8 */
    {OP_ldrt,    0x06b00000, "ldrt",   RBw, RAw, Mw, RDw, sh2, xop_shift|pred, x, top4y[2][0x00]},/*PUW=011*/
    {OP_ssat,    0x06b00010, "ssat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {OP_rev,     0x06bf0f30, "rev",    RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_ssat,    0x06b00050, "ssat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {EXT_RAPC,   0x06b00070, "(ext rapc 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_ssat,    0x06b00010, "ssat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {OP_rev16,   0x06bf0fb0, "rev16",  RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_ssat,    0x06b00050, "ssat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {INVALID,    0x06b000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_strb,    0x06c00000, "strb",   Mb, RAw, RBb, RDw, sh2, xop_shift|pred, x, top8[0x64]},/*PUW=010*/
    {INVALID,    0x06c00010, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x06c00030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x06c00050, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_RAPC,   0x06c00070, "(ext rapc 3)", xx, xx, xx, xx, xx, no, x, 3},
    {INVALID,    0x06c00090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x06c000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x06c000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x06c000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 10 */
    {OP_strbt,   0x06e00000, "strbt",  Mb, RAw, RBb, RDw, sh2, xop_shift|pred, x, top4y[4][0x00]},/*PUW=011*/
    {OP_usat,    0x06e00010, "usat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, END_LIST},
    {OP_usat16,  0x06e00f30, "usat16", RBw, xx, i4_16, RDw, xx, pred, fWQ, END_LIST},
    {OP_usat,    0x06e00050, "usat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {EXT_RAPC,   0x06e00070, "(ext rapc 4)", xx, xx, xx, xx, xx, no, x, 4},
    {OP_usat,    0x06e00010, "usat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {INVALID,    0x06e000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_usat,    0x06e00050, "usat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {INVALID,    0x06e000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 11 */
    {OP_ldrbt,   0x06f00000, "ldrbt",  RBw, RAw, Mb, RDw, sh2, xop_shift|pred, x, top4y[5][0x00]},/*PUW=011*/
    {OP_usat,    0x06f00010, "usat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {OP_rbit,    0x06ff0f30, "rbit",   RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_usat,    0x06f00050, "usat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {EXT_RAPC,   0x06f00070, "(ext rapc 5)", xx, xx, xx, xx, xx, no, x, 5},
    {OP_usat,    0x06f00010, "usat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {OP_revsh,   0x06ff0fb0, "revsh",  RBw, xx, RDh, xx, xx, pred, x, END_LIST},
    {OP_usat,    0x06f00050, "usat",   RBw, i5_16, RDw, sh1, i5_7, pred|srcX4, fWQ, DUP_ENTRY},
    {INVALID,    0x06f000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 12 */
    {OP_str,     0x07000000, "str",    MNSw, xx, RBw, xx, xx, pred, x, top8[0x50]},/*PUW=100*/
    {EXT_RBPC,   0x07000010, "(ext rbpc 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_RBPC,   0x07000030, "(ext rbpc 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_RBPC,   0x07000050, "(ext rbpc 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_RBPC,   0x07000070, "(ext rbpc 4)", xx, xx, xx, xx, xx, no, x, 4},
    {INVALID,    0x07000090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x070000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x070000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x070000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 13 */
    {OP_strb,    0x07400000, "strb",   MNSb, xx, RBb, xx, xx, pred, x, top8[0x54]},/*PUW=100*/
    {OP_smlald,  0x07400010, "smlald", RAw, RBw, RAw, RBw, RDw, pred|xop, x, exop[0x7]},
    {OP_smlaldx, 0x07400030, "smlaldx",RAw, RBw, RAw, RBw, RDw, pred|xop, x, exop[0x7]},
    {OP_smlsld,  0x07400050, "smlsld", RAw, RBw, RAw, RBw, RDw, pred|xop, x, exop[0x7]},
    {OP_smlsldx, 0x07400070, "smlsldx",RAw, RBw, RAw, RBw, RDw, pred|xop, x, exop[0x7]},
    {INVALID,    0x07400090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x074000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x074000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x074000f0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 14 */
    {OP_ldrb,    0x07500000, "ldrb",   RBw, xx, MNSb, xx, xx, pred, x, top8[0x5f]},/*PUW=100*/
    {EXT_RBPC,   0x07500010, "(ext rbpc 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_RBPC,   0x07500030, "(ext rbpc 6)", xx, xx, xx, xx, xx, no, x, 6},
    {INVALID,    0x07500050, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x07500070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x07500090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x075000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_smmls,   0x075000d0, "smmls",  RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_smmlsr,  0x075000f0, "smmlsr", RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
  },
};

/* Indexed by bits 7:4 */
const instr_info_t A32_ext_opc4[][16] = {
  { /* 0 */
    {EXT_BIT9,   0x01000000, "(ext bit9 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,    0x01000010, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x01000020, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x01000030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BIT9,   0x01000040, "(ext bit9 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_qadd,    0x01000050, "qadd",   RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID,    0x01000060, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_hlt,     0xe1000070, "hlt",    xx, xx, i16x8_0, xx, xx, predAL, x, END_LIST},
    {OP_smlabb,  0x01000080, "smlabb", RAw, xx, RDh, RCh, RBw, pred, x, END_LIST},
    {OP_swp,     0x01000090, "swp",    Mw, RBw, Mw, RDw, xx, pred, x, END_LIST},
    {OP_smlatb,  0x010000a0, "smlatb", RAw, xx, RDt, RCh, RBw, pred, x, END_LIST},
    {OP_strh,    0x010000b0, "strh",   MNRh, xx, RBh, xx, xx, pred, x, top4[3][0x0b]},/*PUW=100*/
    {OP_smlabt,  0x010000c0, "smlabt", RAw, xx, RDh, RCt, RBw, pred, x, END_LIST},
    {OP_ldrd,    0x010000d0, "ldrd",   RBEw, RB2w, MNRq, xx, xx, pred, x, top4[3][0x0d]},/*PUW=100*/
    {OP_smlatt,  0x010000e0, "smlatt", RAw, xx, RDt, RCt, RBw, pred, x, END_LIST},
    {OP_strd,    0x010000f0, "strd",   MNRq, xx, RBEw, RB2w, xx, pred, x, top4[3][0x0f]},/*PUW=100*/
  }, { /* 1 */
    {EXT_BIT9,   0x01200000, "(ext bit9 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_bx,      0x012fff10, "bx",     xx, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_bxj,     0x012fff20, "bxj",    xx, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_blx_ind, 0x012fff30, "blx",    LRw, xx, RDw, xx, xx, pred, x, END_LIST},
    {EXT_BIT9,   0x01200040, "(ext bit9 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_qsub,    0x01200050, "qsub",   RBw, xx, RDw, RAw, xx, pred, x, END_LIST},
    {INVALID,    0x01200060, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_bkpt,    0xe1200070, "bkpt",   xx, xx, i16x8_0, xx, xx, predAL, x, END_LIST},
    {OP_smlawb,  0x01200080, "smlawb", RAw, xx, RDh, RCh, RBw, pred, x, END_LIST},
    {INVALID,    0x01200090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_smulwb,  0x012000a0, "smulwb", RAw, xx, RDw, RCh, xx, pred, x, END_LIST},
    {OP_strh,    0x012000b0, "strh",   MNRh, RAw, RBh, RDNw, RAw, pred, x, top4x[12][0x03]},/*PUW=101*/
    {OP_smlawt,  0x012000c0, "smlawt", RAw, xx, RDt, RCt, RBw, pred, x, END_LIST},
    {OP_ldrd,    0x012000d0, "ldrd",   RBEw, RB2w, RAw, MNRq, RDNw, xop_wb|pred|dstX3, x, top4x[14][0x04]},/*PUW=101*/
    {OP_smulwt,  0x012000e0, "smulwt", RAw, xx, RDw, RCt, xx, pred, x, END_LIST},
    {OP_strd,    0x012000f0, "strd",   MNRq, RAw, RBEw, RB2w, RDNw, xop_wb|pred, x, top4x[14][0x05]},/*PUW=101*/
  }, { /* 2 */
    {EXT_BIT9,   0x01400000, "(ext bit9 4)", xx, xx, xx, xx, xx, no, x, 4},
    {INVALID,    0x01400010, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x01400020, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x01400030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BIT9,   0x01400040, "(ext bit9 5)", xx, xx, xx, xx, xx, no, x, 5},
    {OP_qdadd,   0x01400050, "qdadd",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID,    0x01400060, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_hvc,     0x01400070, "hvc",    xx, xx, i16x16_0, xx, xx, pred, x, END_LIST},
    {OP_smlalbb, 0x01400080, "smlalbb", RAw, RBw, RAw, RBw, RDh, pred|xop, x, exop[0x5]},
    {OP_swpb,    0x01400090, "swpb",   Mb, RBw, Mb, RDb, xx, pred, x, END_LIST},
    {OP_smlaltb, 0x014000a0, "smlaltb", RAw, RBw, RAw, RBw, RDt, pred|xop, x, exop[0x5]},
    {OP_strh,    0x014000b0, "strh",   MN44h, xx, RBh, xx, xx, pred, x, top4[0][0x0b]},/*PUW=100*/
    {OP_smlalbt, 0x014000c0, "smlalbt", RAw, RBw, RAw, RBw, RDh, pred|xop, x, exop[0x4]},
    {OP_ldrd,    0x014000d0, "ldrd",   RBEw, RB2w, MN44q, xx, xx, pred, x, top4x[20][0x04]},/*PUW=100*/
    {OP_smlaltt, 0x014000e0, "smlaltt", RAw, RBw, RAw, RBw, RDt, pred|xop, x, exop[0x4]},
    {OP_strd,    0x014000f0, "strd",   MN44q, xx, RBEw, RB2w, xx, pred, x, top4[0][0x0f]},/*PUW=100*/
  }, { /* 3 */
    {EXT_BIT9,   0x01600000, "(ext bit9 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_clz,     0x016f0f10, "clz",    RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {INVALID,    0x01600020, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x01600030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BIT9,   0x01600040, "(ext bit9 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_qdsub,   0x01600050, "qdsub",  RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_eret,    0x0160006e, "eret",   xx, xx, LRw, xx, xx, pred, fWNZCV, END_LIST},
    {OP_smc,     0x01600070, "smc",    xx, xx, i4, xx, xx, pred, x, END_LIST},
    {OP_smulbb,  0x01600080, "smulbb", RAw, xx, RDh, RCh, xx, pred, x, END_LIST},
    {INVALID,    0x01600090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_smultb,  0x016000a0, "smultb", RAw, xx, RDt, RCh, xx, pred, x, END_LIST},
    {OP_strh,    0x016000b0, "strh",   MN44h, RAw, RBh, n8x8_0, RAw, pred, x, top4[1][0x0b]},/*PUW=101*/
    {OP_smulbt,  0x016000c0, "smulbt", RAw, xx, RDh, RCt, xx, pred, x, END_LIST},
    {OP_ldrd,    0x016000d0, "ldrd",   RBEw, RB2w, RAw, MN44q, n8x8_0, xop_wb|pred|dstX3, x, top4[1][0x0d]},/*PUW=101*/
    {OP_smultt,  0x016000e0, "smultt", RAw, xx, RDt, RCt, xx, pred, x, END_LIST},
    {OP_strd,    0x016000f0, "strd",   MN44q, RAw, RBEw, RB2w, n8x8_0, xop_wb|pred, x, top4[1][0x0f]},/*PUW=101*/
  }, { /* 4 */
    {EXT_IMM5,   0x01a00000, "(ext imm5 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_lsl,     0x01a00010, "lsl",    RBw, xx, RDw, RCw, xx, pred, x, END_LIST},
    {OP_lsr,     0x01a00020, "lsr",    RBw, xx, RDw, i5_7, xx, pred, x, top4[4][0x03]},
    {OP_lsr,     0x01a00030, "lsr",    RBw, xx, RDw, RCw, xx, pred, x, END_LIST},
    {OP_asr,     0x01a00040, "asr",    RBw, xx, RDw, i5_7, xx, pred, x, top4[4][0x05]},
    {OP_asr,     0x01a00050, "asr",    RBw, xx, RDw, RCw, xx, pred, x, END_LIST},
    {EXT_IMM5,   0x01a00060, "(ext imm5 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_ror,     0x01a00070, "ror",    RBw, xx, RDw, RCw, xx, pred, x, END_LIST},
    {OP_lsl,     0x01a00000, "lsl",    RBw, xx, RDw, i5_7, xx, pred, x, top4[4][0x01]},
    {EXT_BITS8,  0x01a00090, "(ext bits8 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_lsr,     0x01a00020, "lsr",    RBw, xx, RDw, i5_7, xx, pred, x, DUP_ENTRY},
    {OP_strh,    0x01a000b0, "strh",   MPRh, RAw, RBh, RDw, RAw, pred, x, DUP_ENTRY},/*PUW=111*/
    {OP_asr,     0x01a00040, "asr",    RBw, xx, RDw, i5_7, xx, pred, x, DUP_ENTRY},
    {OP_ldrd,    0x01a000d0, "ldrd",   RBEw, RB2w, RAw, MPRq, RDw, xop_wb|pred|dstX3, x, DUP_ENTRY},/*PUW=111*/
    {OP_ror,     0x01a00060, "ror",    RBw, xx, RDw, i5_7, xx, pred, x, top4[4][0x07]},
    {OP_strd,    0x01a000f0, "strd",   MPRq, RAw, RBEw, RB2w, RDw, xop_wb|pred, x, DUP_ENTRY},/*PUW=111*/
  }, { /* 5 */
    {EXT_IMM5,   0x01b00000, "(ext imm5 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_lsls,    0x01b00010, "lsls",   RBw, xx, RDw, RCw, xx, pred, fWNZC, END_LIST},
    {OP_lsrs,    0x01b00020, "lsrs",   RBw, xx, RDw, i5_7, xx, pred, fRC|fWNZC, top4[5][0x03]},
    {OP_lsrs,    0x01b00030, "lsrs",   RBw, xx, RDw, RCw, xx, pred, fWNZC, END_LIST},
    {OP_asrs,    0x01b00040, "asrs",   RBw, xx, RDw, i5_7, xx, pred, fRC|fWNZC, top4[5][0x05]},
    {OP_asrs,    0x01b00050, "asrs",   RBw, xx, RDw, RCw, xx, pred, fWNZC, END_LIST},
    {EXT_IMM5,   0x01b00060, "(ext imm5 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_rors,    0x01b00070, "rors",   RBw, xx, RDw, RCw, xx, pred, fWNZC, END_LIST},
    {OP_lsls,    0x01b00000, "lsls",   RBw, xx, RDw, i5_7, xx, pred, fRC|fWNZC, top4[5][0x01]},
    {EXT_BITS8,  0x01b00090, "(ext bits8 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_lsrs,    0x01b00020, "lsrs",   RBw, xx, RDw, i5_7, xx, pred, fRC|fWNZC, DUP_ENTRY},
    {OP_ldrh,    0x01b000b0, "ldrh",   RBw, RAw, MPRh, RDw, RAw, pred, x, top4x[17][0x03]},/*PUW=111*/
    {OP_asrs,    0x01b00040, "asrs",   RBw, xx, RDw, i5_7, xx, pred, fRC|fWNZC, DUP_ENTRY},
    {OP_ldrsb,   0x01b000d0, "ldrsb",  RBw, RAw, MPRb, RDw, RAw, pred, x, DUP_ENTRY},/*PUW=111*/
    {OP_rors,    0x01b00060, "rors",   RBw, xx, RDw, i5_7, xx, pred, fRC|fWNZC, top4[5][0x07]},
    {OP_ldrsh,   0x01b000f0, "ldrsh",  RBw, RAw, MPRh, RDw, RAw, pred, x, DUP_ENTRY},/*PUW=111*/
  }, { /* 6 */
    {EXT_BITS0,  0x03200000, "(ext bits0 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,    0x03200010, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x03200020, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x03200030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x03200040, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x03200050, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x03200060, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x03200070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x03200080, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x03200090, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x032000a0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x032000b0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x032000c0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x032000d0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x032000e0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_dbg,     0x0320f0f0, "dbg",    xx, xx, i4, xx, xx, pred, x, END_LIST},
  }, { /* 7 */
    {OP_vmov_f32,0x0eb00a00, "vmov.f32", WBd, xx, i8x16_0, xx, xx, pred|vfp, x, t16[0][0x00]},
    {INVALID,    0x0eb00a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0eb00a20, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0eb00a30, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0eb00a40, "(ext bits16 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,    0x0eb00a50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0eb00a60, "(ext bits16 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,    0x0eb00a70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0eb00a80, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0eb00a90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0eb00aa0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0eb00ab0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0eb00ac0, "(ext bits16 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,    0x0eb00ad0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0eb00ae0, "(ext bits16 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,    0x0eb00af0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 8 */
    {OP_vmov_f64,0x0eb00b00, "vmov.f64", VBq, xx, i8x16_0, xx, xx, pred|vfp, x, t16[2][0x00]},
    {INVALID,    0x0eb00b10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0eb00b20, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u16,0x0eb00b30, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0x0eb00b40, "(ext bits16 2)", xx, xx, xx, xx, xx, no, x, 2},
    {INVALID,    0x0eb00b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0eb00b60, "(ext bits16 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_vmov_u16,0x0eb00b70, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,    0x0eb00b80, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0eb00b90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0eb00ba0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u16,0x0eb00bb0, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0x0eb00bc0, "(ext bits16 3)", xx, xx, xx, xx, xx, no, x, 3},
    {INVALID,    0x0eb00bd0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0eb00be0, "(ext bits16 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_vmov_u16,0x0eb00bf0, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 9 */
    {OP_vmov_f32,0x0ef00a00, "vmov.f32", WBd, xx, i8x16_0, xx, xx, pred|vfp, x, DUP_ENTRY},
    {EXT_RBPC,   0x0ef00a10, "(ext rbpc 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,    0x0ef00a20, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0ef00a30, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0ef00a40, "(ext bits16 4)", xx, xx, xx, xx, xx, no, x, 4},
    {INVALID,    0x0ef00a50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0ef00a60, "(ext bits16 4)", xx, xx, xx, xx, xx, no, x, 4},
    {INVALID,    0x0ef00a70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0ef00a80, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0ef00a90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0ef00aa0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0ef00ab0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0ef00ac0, "(ext bits16 5)", xx, xx, xx, xx, xx, no, x, 5},
    {INVALID,    0x0ef00ad0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0ef00ae0, "(ext bits16 5)", xx, xx, xx, xx, xx, no, x, 5},
    {INVALID,    0x0ef00af0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 10 */
    {OP_vmov_f64,0x0ef00b00, "vmov.f64", VBq, xx, i8x16_0, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_u8, 0x0ef00b10, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,    0x0ef00b20, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8, 0x0ef00b30, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0x0ef00b40, "(ext bits16 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_vmov_u8, 0x0ef00b50, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0x0ef00b60, "(ext bits16 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_vmov_u8, 0x0ef00b70, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,    0x0ef00b80, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8, 0x0ef00b90, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,    0x0ef00ba0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8, 0x0ef00bb0, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0x0ef00bc0, "(ext bits16 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_vmov_u8, 0x0ef00bd0, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {EXT_BITS16, 0x0ef00be0, "(ext bits16 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_vmov_u8, 0x0ef00bf0, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
  },
};

/* Indexed by whether imm4 in 19:16 is 0, 1, or other */
const instr_info_t A32_ext_imm1916[][3] = {
  { /* 0 */
    {EXT_OPC4,   0x03200000, "(ext opc4 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_msr,     0x0320f000, "msr",    CPSR, xx, i4_16, i12sh, xx, pred, fWNZCVQG/*see decode_eflags_to_instr_eflags*/, top8[0x36]},
    {OP_msr,     0x0320f000, "msr",    CPSR, xx, i4_16, i12sh, xx, pred, fWNZCVQG/*see decode_eflags_to_instr_eflags*/, DUP_ENTRY},
  }, { /* 1 */
    {OP_vmovl_s16, 0xf2900a10, "vmovl.s16", VBdq, xx, VCq, xx, xx, no, x, END_LIST},
    {OP_vshll_s16, 0xf2900a10, "vshll.s16", VBdq, xx, VCq, i4_16, xx, no, x, END_LIST},/*19:16 cannot be 0*/
    {OP_vshll_s16, 0xf2900a10, "vshll.s16", VBdq, xx, VCq, i4_16, xx, no, x, DUP_ENTRY},/*19:16 cannot be 0*/
  }, { /* 2 */
    {OP_vmovl_u16, 0xf3900a10, "vmovl.u16", VBdq, xx, VCq, xx, xx, no, x, END_LIST},
    {OP_vshll_u16, 0xf3900a10, "vshll.u16", VBdq, xx, VCq, i4_16, xx, no, x, END_LIST},/*19:16 cannot be 0*/
    {OP_vshll_u16, 0xf3900a10, "vshll.u16", VBdq, xx, VCq, i4_16, xx, no, x, DUP_ENTRY},/*19:16 cannot be 0*/
  }, { /* 3 */
    {OP_vmsr,     0x0ee00a10, "vmsr",   xx, xx, RBd, i4_16, xx, pred|vfp, x, ti19[3][0x01]},
    {OP_vmsr,     0x0ee10a10, "vmsr",   FPSCR, xx, RBd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vmsr,     0x0ee00a10, "vmsr",   xx, xx, RBd, i4_16, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 4 */
    {OP_vmrs,    0x0ef00a10, "vmrs",   RBd, xx, i4_16, xx, xx, pred|vfp, x, ti19[4][0x01]},
    {OP_vmrs,    0x0ef10a10, "vmrs",   RBd, xx, FPSCR, xx, xx, pred|vfp, x, END_LIST},
    {OP_vmrs,    0x0ef00a10, "vmrs",   RBd, xx, i4_16, xx, xx, pred|vfp, x, DUP_ENTRY},
  },
};

/* Indexed by bits 2:0 */
const instr_info_t A32_ext_bits0[][8] = {
  { /* 0 */
    {OP_nop,     0x0320f000, "nop",    xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_yield,   0x0320f001, "yield",  xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_wfe,     0x0320f002, "wfe",    xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_wfi,     0x0320f003, "wfi",    xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_sev,     0x0320f004, "sev",    xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_sevl,    0x0320f005, "sevl",   xx, xx, xx, xx, xx, pred, x, END_LIST},
    {INVALID,    0x03200006, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x03200007, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits 9:8 */
const instr_info_t A32_ext_bits8[][4] = {
  { /* 0 */
    {OP_stl,     0x0180fc90, "stl",    Mw, xx, RDw, xx, xx, pred, x, END_LIST},
    {INVALID,    0x01800d90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_stlex,   0x01800e90, "stlex",  Mw, RBw, RDw, xx, xx, pred, x, END_LIST},
    {OP_strex,   0x01800f90, "strex",  Mw, RBw, RDw, xx, xx, pred, x, END_LIST},
  }, { /* 1 */
    {OP_lda,     0x01900c9f, "lda",    RBw, xx, Mw, xx, xx, pred, x, END_LIST},
    {INVALID,    0x01900d90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldaex,   0x01900e9f, "ldaex",  RBw, xx, Mw, xx, xx, pred, x, END_LIST},
    {OP_ldrex,   0x01900f9f, "ldrex",  RBw, xx, Mw, xx, xx, pred, x, END_LIST},
  }, { /* 2 */
    {INVALID,    0x01a00c90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x01a00d90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_stlexd,  0x01a00e90, "stlexd", Mq, RBw, RDEw, RD2w, xx, pred, x, END_LIST},
    {OP_strexd,  0x01a00f90, "strexd", Mq, RBw, RDEw, RD2w, xx, pred, x, END_LIST},
  }, { /* 3 */
    {INVALID,    0x01b00c90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x01b00d90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldaexd,  0x01b00e9f, "ldaexd", RBEw, RB2w, Mq, xx, xx, pred, x, END_LIST},
    {OP_ldrexd,  0x01b00f9f, "ldrexd", RBEw, RB2w, Mq, xx, xx, pred, x, END_LIST},
  }, { /* 4 */
    {OP_stlb,    0x01c00c90, "stlb",   Mb, xx, RDb, xx, xx, pred, x, END_LIST},
    {INVALID,    0x01c00d90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_stlexb,  0x01c00e90, "stlexb", Mb, RBw, RDb, xx, xx, pred, x, END_LIST},
    {OP_strexb,  0x01c00f90, "strexb", Mb, RBw, RDb, xx, xx, pred, x, END_LIST},
  }, { /* 5 */
    {OP_ldab,    0x01d00c9f, "ldab",   RBw, xx, Mb, xx, xx, pred, x, END_LIST},
    {INVALID,    0x01d00d90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldaexb,  0x01d00e9f, "ldaexb", RBw, xx, Mb, xx, xx, pred, x, END_LIST},
    {OP_ldrexb,  0x01d00f9f, "ldrexb", RBb, xx, Mb, xx, xx, pred, x, END_LIST},
  }, { /* 6 */
    {OP_stlh,    0x01e0fc90, "stlh",   Mh, xx, RDh, xx, xx, pred, x, END_LIST},
    {INVALID,    0x01e00d90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_stlexh,  0x01e00e90, "stlexh", Mh, RBw, RDh, xx, xx, pred, x, END_LIST},
    {OP_strexh,  0x01e00f90, "strexh", Mh, RBw, RDh, xx, xx, pred, x, END_LIST},
  }, { /* 7 */
    {OP_ldah,    0x01f00c9f, "ldah",   RBw, xx, Mh, xx, xx, pred, x, END_LIST},
    {INVALID,    0x01f00d90, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldaexh,  0x01f00e9f, "ldaexh", RBw, xx, Mh, xx, xx, pred, x, END_LIST},
    {OP_ldrexh,  0x01f00f9f, "ldrexh", RBw, xx, Mh, xx, xx, pred, x, END_LIST},
  },
};

/* Indexed by bit 9 */
const instr_info_t A32_ext_bit9[][2] = {
  { /* 0 */
    {OP_mrs,      0x010f0000, "mrs",    RBw, xx, CPSR, xx, xx, pred, fRNZCVQG, END_LIST},
    {OP_mrs_priv, 0x01000200, "mrs",    RBw, xx, i5x8_16, xx, xx, pred, x, END_LIST},
  }, { /* 1 */
    {OP_crc32b,  0xe1000040, "crc32b",  RBw, xx, RAw, RDb, xx, predAL|v8, x, END_LIST},
    {OP_crc32cb, 0xe1000240, "crc32cb", RBw, xx, RAw, RDb, xx, predAL|v8, x, END_LIST},
  }, { /* 2 */
    {OP_msr,      0x0120f000, "msr",    CPSR, xx, i4_16, RDw, xx, pred, fWNZCVQG/*see decode_eflags_to_instr_eflags*/, ti19[0][0x01]},
    {OP_msr_priv, 0x0120f000, "msr",    xx, xx, i5x8_16, RDw, xx, pred, x, END_LIST},
  }, { /* 3 */
    {OP_crc32h,   0xe1200040, "crc32h",  RBw, xx, RAw, RDh, xx, predAL|v8, x, END_LIST},
    {OP_crc32ch, 0xe1200240, "crc32ch", RBw, xx, RAw, RDh, xx, predAL|v8, x, END_LIST},
  }, { /* 4 */
    {OP_mrs,      0x014f0000, "mrs",    RBw, xx, SPSR, xx, xx, pred, fRNZCVQG, tb9[0][0x00]},
    {OP_mrs_priv, 0x01400200, "mrs",    RBw, xx, SPSR, i5x8_16, xx, pred, x, tb9[0][0x01]},
  }, { /* 5 */
    {OP_crc32w,  0xe1400040, "crc32w",  RBw, xx, RAw, RDw, xx, predAL|v8, x, END_LIST},
    {OP_crc32cw, 0xe1400240, "crc32cw", RBw, xx, RAw, RDw, xx, predAL|v8, x, END_LIST},
  }, { /* 6 */
    {OP_msr,      0x0160f000, "msr",    SPSR, xx, i4_16, RDw, xx, pred, fWNZCVQG/*see decode_eflags_to_instr_eflags*/, tb9[2][0x00]},
    {OP_msr_priv, 0x0160f000, "msr",    SPSR, xx, i5x8_16, RDw, xx, pred, x, tb9[2][0x01]},
  }, { /* 7 */
    /* XXX: or should we just mark INVALID? */
    {OP_crc32w,  0xe1600040, "crc32w",  RBw, xx, RAw, RDw, xx, predAL|v8|unp, x, tb9[5][0x00]},
    {OP_crc32cw, 0xe1600240, "crc32cw", RBw, xx, RAw, RDw, xx, predAL|v8|unp, x, tb9[5][0x01]},
  },
};

/* Indexed by bit 5 */
const instr_info_t A32_ext_bit5[][2] = {
  { /* 0 */
    {OP_ubfx,    0x07f00050, "ubfx",   RBw, xx, RDw, i5_7, i5_16, pred, x, DUP_ENTRY},
    {OP_udf,     0xe7f000f0, "udf",    xx, xx, i16x8_0, xx, xx, predAL, x, END_LIST},
  },
};

/* Indexed by bit 4 */
const instr_info_t A32_ext_bit4[][2] = {
  { /* 0 */
    {OP_ldr,     0x07100000, "ldr",    RBw, xx, MNSw, xx, xx, pred, x, top8[0x5b]},/*PUW=100*/
    {OP_sdiv,    0x0710f010, "sdiv",   RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
  }, { /* 1 */
    {OP_ldr,     0x07300000, "ldr",    RBw, RAw, MNSw, RDNw, sh2, xop_shift|pred, x, top8[0x49]},/*PUW=101*/
    {OP_udiv,    0x0730f010, "udiv",   RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
  }, { /* 2 */
    {OP_str,     0x07800000, "str",    MPSw, xx, RBw, xx, xx, pred, x, top8[0x58]},/*PUW=110*/
    {EXT_RBPC,   0x07800010, "(ext rbpc 7)", xx, xx, xx, xx, xx, no, x, 7},
  }, { /* 3 */
    {OP_str,     0x07a00000, "str",    MPSw, RAw, RBw, RDw, sh2, xop_shift|pred, x, top8[0x72]},/*PUW=111*/
    {OP_sbfx,    0x07a00050, "sbfx",   RBw, xx, RDw, i5_7, i5_16, pred, x, END_LIST},
  }, { /* 4 */
    {OP_ldr,     0x07b00000, "ldr",    RBw, RAw, MPSw, RDw, sh2, xop_shift|pred, x, tb4[1][0x00]},/*PUW=111*/
    {OP_sbfx,    0x07b00050, "sbfx",   RBw, xx, RDw, i5_7, i5_16, pred, x, DUP_ENTRY},
  }, { /* 5 */
    {OP_strb,    0x07c00000, "strb",   MPSb, xx, RBb, xx, xx, pred, x, top8[0x5c]},/*PUW=110*/
    {EXT_RDPC,   0x07c00000, "(ext RDPC 0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 6 */
    {OP_ldrb,    0x07d00000, "ldrb",   RBw, xx, MPSb, xx, xx, pred, x, top4y[14][0x00]},/*PUW=110*/
    {EXT_RDPC,   0x07d00000, "(ext RDPC 1)", xx, xx, xx, xx, xx, no, x, 1},
  }, { /* 7 */
    {OP_strb,    0x07e00000, "strb",   MPSb, RAw, RBb, RDw, sh2, xop_shift|pred, x, top8[0x76]},/*PUW=111*/
    {OP_ubfx,    0x07e00050, "ubfx",   RBw, xx, RDw, i5_7, i5_16, pred, x, END_LIST},
  }, { /* 8 */
    {OP_ldrb,    0x07f00000, "ldrb",   RBw, RAw, MPSb, RDw, sh2, xop_shift|pred, x, top8[0x77]},/*PUW=111*/
    {EXT_BIT5,   0x07f00010, "(ext bit5 0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 9 */
    {OP_cdp,     0x0e000000, "cdp",    CRBw, i4_8, i4_20, CRAw, CRDw, pred|xop|srcX4, x, exop[0x3]},/*XXX: disasm not in dst-src order*//*no chain nec.*/
    {OP_mcr,     0x0e000010, "mcr",    CRAw, CRDw, i4_8, i3_21, RBw, pred|xop, x, exop[0x3]},/*XXX: disasm not in dst-src order*/
  }, { /* 10*/
    {OP_cdp,     0x0e100000, "cdp",    CRBw, i4_8, i4_20, CRAw, CRDw, pred|xop|srcX4, x, exop[0x3]},/*XXX: disasm not in dst-src order*/
    {OP_mrc,     0x0e100010, "mrc",    RBw, i4_8, i3_21, CRAw, CRDw, pred|xop|srcX4, x, exop[0x3]},/*XXX: disasm not in dst-src order*/
  }, { /* 11 */
    {OP_cdp2,     0xfe000000, "cdp2",           CRBw, i4_8, i4_20, CRAw, CRDw, xop|srcX4, x, END_LIST},/*XXX: disasm not in dst-src order*//*no chain nec.*/
    {OP_mcr2,     0xfe000010, "mcr2",           CRAw, CRDw, i4_8, i3_21, RBw, xop, x, END_LIST},/*XXX: disasm not in dst-src order*/
  }, { /* 12 */
    {OP_cdp2,     0xfe100000, "cdp2",           CRBw, i4_8, i4_20, CRAw, CRDw, xop|srcX4, x, DUP_ENTRY},/*XXX: disasm not in dst-src order*//*no chain nec.*/
    {OP_mrc2,     0xfe100010, "mrc2",          RBw, i4_8, i3_21, CRAw, CRDw, xop|srcX4, x, exop[0x3]},/*XXX: disasm not in dst-src order*/
  }, { /* 13 */
    /* To handle the 21:16 immed instrs that vary in high bits we must first
     * sseparate those out: we do that via bit4 and then bit7 in the next 8 entries.
     */
    {EXT_SIMD8,  0xf2800000, "(ext simd8  0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BIT7,   0xf2800010, "(ext bit7   0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 14 */
    {EXT_SIMD6,  0xf2900000, "(ext simd6  4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_BIT7,   0xf2900010, "(ext bit7   1)", xx, xx, xx, xx, xx, no, x, 1},
  }, { /* 15 */
    {EXT_SIMD6,  0xf2a00000, "(ext simd6  5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_BIT7,   0xf2a00010, "(ext bit7   2)", xx, xx, xx, xx, xx, no, x, 2},
  }, { /* 16 */
    {EXT_BIT6,   0xf2b00000, "(ext bit6   0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BIT7,   0xf2b00010, "(ext bit7   2)", xx, xx, xx, xx, xx, no, x, 2},
  }, { /* 17 */
    {EXT_SIMD8,  0xf3800000, "(ext simd8  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BIT7,   0xf3800010, "(ext bit7   3)", xx, xx, xx, xx, xx, no, x, 3},
  }, { /* 18 */
    {EXT_SIMD6,  0xf3900000, "(ext simd6 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_BIT7,   0xf3900010, "(ext bit7   4)", xx, xx, xx, xx, xx, no, x, 4},
  }, { /* 19 */
    {EXT_SIMD6,  0xf3a00000, "(ext simd6 11)", xx, xx, xx, xx, xx, no, x, 11},
    {EXT_BIT7,   0xf3a00010, "(ext bit7   5)", xx, xx, xx, xx, xx, no, x, 5},
  }, { /* 20 */
    {EXT_VTB,    0xf3b00000, "(ext vtb 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BIT7,   0xf3b00010, "(ext bit7   5)", xx, xx, xx, xx, xx, no, x, 5},
  }, { /* 21 */
    {EXT_VLDC,   0xf4a00e00, "(ext vldC  1)",  xx, xx, xx, xx, xx, no, x, 1},
    {INVALID,    0xf4a00e10, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by whether coprocessor (11:8) is:
 * + 0xa   => index 0
 * + 0xb   => index 1
 * + other => index 2
 */
const instr_info_t A32_ext_fp[][3] = {
  { /* 0 */
    {INVALID,    0x0c200a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID,    0x0c200b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_stc,     0x0c200000, "stc",    Mw, RAw, i4_8, CRBw, n8x4, xop_wb|pred, x, END_LIST},/*PUW=001*/
  }, { /* 1 */
    {INVALID,    0x0c300a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID,    0x0c300b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_ldc,     0x0c300000, "ldc",    CRBw, RAw, Mw, i4_8, n8x4, xop_wb|pred, x, END_LIST},/*PUW=001*/
  }, { /* 2 */
    {OP_vmov,    0x0c400a10, "vmov",     WCd, WC2d, RBd, RAd, xx, pred|vfp, x, END_LIST},
    {OP_vmov,    0x0c400b10, "vmov",     VCq, xx, RBd, RAd, xx, pred|vfp, x, tfpA[1][0x01]},
    {OP_mcrr,    0x0c400000, "mcrr",   CRDw, RAw, RBw, i4_8, i4_7, pred|srcX4, x, END_LIST},
  }, { /* 3 */
    {OP_vmov,    0x0c500a10, "vmov",     RBd, RAd, WCd, WC2d, xx, pred|vfp, x, tfp[2][0x00]},
    {OP_vmov,    0x0c500b10, "vmov",     RBd, RAd, VCq, xx, xx, pred|vfp, x, tfp[2][0x01]},
    {OP_mrrc,    0x0c500000, "mrrc",   RBw, RAw, i4_8, i4_7, CRDw, pred, x, END_LIST},
  }, { /* 4 */
    {INVALID,    0x0c600a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID,    0x0c600b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_stcl,    0x0c600000, "stcl",   Mw, RAw, i4_8, CRBw, n8x4, xop_wb|pred, x, END_LIST},/*PUW=001*/
  }, { /* 5 */
    {INVALID,    0x0c700a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID,    0x0c700b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_ldcl,    0x0c700000, "ldcl",   CRBw, RAw, Mw, i4_8, n8x4, xop_wb|pred, x, END_LIST},/*PUW=001*/
  }, { /* 6 */
    {OP_vstm,    0x0c800a00, "vstm",   Ml, xx, WBd, LCd, xx, pred|vfp, x, END_LIST},/*PUW=010*/
    {OP_vstm,    0x0c800b00, "vstm",   Ml, xx, VBq, LCq, xx, pred|vfp, x, tfp[8][0x00]},/*PUW=010*/
    {OP_stc,     0x0c800000, "stc",    Mw, xx, i4_8, CRBw, i8, pred, x, tfp[0][0x02]},/*PUW=010*/
  }, { /* 7 */
    {OP_vldm,    0x0c900a00, "vldm",   WBd, LCd, Ml, xx, xx, pred|vfp, x, END_LIST},/*PUW=010*/
    {OP_vldm,    0x0c900b00, "vldm",   VBq, LCq, Ml, xx, xx, pred|vfp, x, tfp[9][0x00]},/*PUW=010*/
    {OP_ldc,     0x0c900000, "ldc",    CRBw, xx, Mw, i4_8, i8, pred, x, tfp[1][0x02]},/*PUW=010*/
  }, { /* 8 */
    {OP_vstm,    0x0ca00a00, "vstm",   Ml, RAw, WBd, LCd, RAw, pred|vfp, x, tfp[6][0x00]},/*PUW=011*/
    {OP_vstm,    0x0ca00b00, "vstm",   Ml, RAw, VBq, LCq, RAw, pred|vfp, x, tfp[6][0x01]},/*PUW=011*/
    {OP_stc,     0x0ca00000, "stc",    Mw, RAw, i4_8, CRBw, i8x4, xop_wb|pred, x, tfp[6][0x02]},/*PUW=011*/
  }, { /* 9 */
    {OP_vldm,    0x0cb00a00, "vldm",   WBd, LCd, RAw, Ml, RAw, pred|vfp|dstX3, x, tfp[7][0x00]},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_vldm,    0x0cb00b00, "vldm",   VBq, LCq, RAw, Ml, RAw, pred|vfp|dstX3, x, tfp[7][0x01]},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_ldc,     0x0cb00000, "ldc",    CRBw, RAw, Mw, i4_8, i8x4, xop_wb|pred, x, tfp[7][0x02]},/*PUW=011*/
  }, { /* 10 */
    {OP_vstm,    0x0cc00a00, "vstm",   Ml, xx, WBd, LCd, xx, pred|vfp, x, DUP_ENTRY},/*PUW=010*/
    {OP_vstm,    0x0cc00b00, "vstm",   Ml, xx, VBq, LCq, xx, pred|vfp, x, DUP_ENTRY},/*PUW=010*/
    {OP_stcl,    0x0cc00000, "stcl",   Mw, xx, i4_8, CRBw, i8, pred, x, tfp[4][0x02]},/*PUW=010*/
  }, { /* 11 */
    {OP_vldm,    0x0cd00a00, "vldm",   WBd, LCd, Ml, xx, xx, pred|vfp, x, DUP_ENTRY},/*PUW=010*/
    {OP_vldm,    0x0cd00b00, "vldm",   VBq, LCq, Ml, xx, xx, pred|vfp, x, DUP_ENTRY},/*PUW=010*/
    {OP_ldcl,    0x0cd00000, "ldcl",   CRBw, xx, Mw, i4_8, i8, pred, x, tfp[5][0x02]},/*PUW=010*/
  }, { /* 12 */
    {OP_vstm,    0x0ce00a00, "vstm",   Ml, RAw, WBd, LCd, RAw, pred|vfp, x, DUP_ENTRY},/*PUW=011*/
    {OP_vstm,    0x0ce00b00, "vstm",   Ml, RAw, VBq, LCq, RAw, pred|vfp, x, DUP_ENTRY},/*PUW=011*/
    {OP_stcl,    0x0ce00000, "stcl",   Mw, RAw, i4_8, CRBw, i8x4, xop_wb|pred, x, tfp[10][0x02]},/*PUW=011*/
  }, { /* 13 */
    {OP_vldm,    0x0cf00a00, "vldm",   WBd, LCd, RAw, Ml, RAw, pred|vfp|dstX3, x, DUP_ENTRY},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_vldm,    0x0cf00b00, "vldm",   VBq, LCq, RAw, Ml, RAw, pred|vfp|dstX3, x, DUP_ENTRY},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_ldcl,    0x0cf00000, "ldcl",   CRBw, RAw, Mw, i4_8, i8x4, xop_wb|pred, x, tfp[11][0x02]},/*PUW=011*/
  }, { /* 14 */
    {OP_vstr,    0x0d000a00, "vstr",   MN8Xd, xx, WBd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vstr,    0x0d000b00, "vstr",   MN8Xq, xx, VBq, xx, xx, pred|vfp, x, tfp[22][0x00]},
    {OP_stc,     0x0d000000, "stc",    MN8Xw, xx, i4_8, CRBw, n8x4, pred, x, tfp[24][0x02]},/*PUW=100*/
  }, { /* 15 */
    {OP_vldr,    0x0d100a00, "vldr",   WBd, xx, MN8Xd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vldr,    0x0d100b00, "vldr",   VBq, xx, MN8Xq, xx, xx, pred|vfp, x, tfp[23][0x00]},
    {OP_ldc,     0x0d100000, "ldc",    CRBw, xx, MN8Xw, i4_8, xx, pred, x, tfp[25][0x02]},/*PUW=100*/
  }, { /* 16 */
    {OP_vstmdb,  0x0d200a00, "vstmdb", Ml, RAw, WBd, LCd, RAw, pred|vfp, x, END_LIST},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_vstmdb,  0x0d200b00, "vstmdb", Ml, RAw, VBq, LCq, RAw, pred|vfp, x, tfp[16][0x00]},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_stc,     0x0d200000, "stc",    MN8Xw, RAw, i4_8, CRBw, n8x4, xop_wb|pred, x, tfp[8][0x02]},/*PUW=101*/
  }, { /* 17 */
    {OP_vldmdb,  0x0d300a00, "vldmdb", WBd, LCd, RAw, Ml, RAw, pred|vfp|dstX3, x, END_LIST},/*PUW=101*/
    {OP_vldmdb,  0x0d300b00, "vldmdb", VBq, LCq, RAw, Ml, RAw, pred|vfp|dstX3, x, tfp[17][0x00]},/*PUW=101*/
    {OP_ldc,     0x0d300000, "ldc",    CRBw, RAw, MN8Xw, i4_8, n8x4, xop_wb|pred, x, tfp[9][0x02]},/*PUW=101*/
  }, { /* 18 */
    {OP_vstr,    0x0d400a00, "vstr",   MN8Xd, xx, WBd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vstr,    0x0d400b00, "vstr",   MN8Xq, xx, VBq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_stcl,    0x0d400000, "stcl",   MN8Xw, xx, i4_8, CRBw, n8x4, pred, x, tfp[28][0x02]},/*PUW=100*/
  }, { /* 19 */
    {OP_vldr,    0x0d500a00, "vldr",   WBd, xx, MN8Xd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vldr,    0x0d500b00, "vldr",   VBq, xx, MN8Xq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_ldcl,    0x0d500000, "ldcl",   CRBw, xx, MN8Xw, i4_8, xx, pred, x, tfp[29][0x02]},/*PUW=100*/
  }, { /* 20 */
    {OP_vstmdb,  0x0d600a00, "vstmdb", Ml, RAw, WBd, LCd, RAw, pred|vfp, x, DUP_ENTRY},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_vstmdb,  0x0d600b00, "vstmdb", Ml, RAw, VBq, LCq, RAw, pred|vfp, x, DUP_ENTRY},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_stcl,    0x0d600000, "stcl",   MN8Xw, RAw, i4_8, CRBw, n8x4, xop_wb|pred, x, tfp[12][0x02]},/*PUW=101*/
  }, { /* 21 */
    {OP_vldmdb,  0x0d700a00, "vldmdb", WBd, LCd, RAw, Ml, RAw, pred|vfp|dstX3, x, DUP_ENTRY},/*PUW=101*/
    {OP_vldmdb,  0x0d700b00, "vldmdb", VBq, LCq, RAw, Ml, RAw, pred|vfp|dstX3, x, DUP_ENTRY},/*PUW=101*/
    {OP_ldcl,    0x0d700000, "ldcl",   CRBw, RAw, MN8Xw, i4_8, n8x4, xop_wb|pred, x, tfp[13][0x02]},/*PUW=101*/
  }, { /* 22 */
    {OP_vstr,    0x0d800a00, "vstr",   MP8Xd, xx, WBd, xx, xx, pred|vfp, x, tfp[14][0x00]},
    {OP_vstr,    0x0d800b00, "vstr",   MP8Xq, xx, VBq, xx, xx, pred|vfp, x, tfp[14][0x01]},
    {OP_stc,     0x0d800000, "stc",    MP8Xw, xx, i4_8, CRBw, i8x4, pred, x, tfp[14][0x02]},/*PUW=110*/
  }, { /* 23 */
    {OP_vldr,    0x0d900a00, "vldr",   WBd, xx, MP8Xd, xx, xx, pred|vfp, x, tfp[15][0x00]},
    {OP_vldr,    0x0d900b00, "vldr",   VBq, xx, MP8Xq, xx, xx, pred|vfp, x, tfp[15][0x01]},
    {OP_ldc,     0x0d900000, "ldc",    CRBw, xx, MP8Xw, i4_8, xx, pred, x, tfp[15][0x02]},/*PUW=110*/
  }, { /* 24 */
    {INVALID,    0x0da00a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID,    0x0da00b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_stc,     0x0da00000, "stc",    MP8Xw, RAw, i4_8, CRBw, i8x4, xop_wb|pred, x, tfp[16][0x02]},/*PUW=111*/
  }, { /* 25 */
    {INVALID,    0x0db00a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID,    0x0db00b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_ldc,     0x0db00000, "ldc",    CRBw, RAw, MP8Xw, i4_8, i8x4, xop_wb|pred, x, tfp[17][0x02]},/*PUW=111*/
  }, { /* 26 */
    {OP_vstr,    0x0dc00a00, "vstr",   MP8Xd, xx, WBd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vstr,    0x0dc00b00, "vstr",   MP8Xq, xx, VBq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_stcl,    0x0dc00000, "stcl",   MP8Xw, xx, i4_8, CRBw, i8x4, pred, x, tfp[18][0x02]},/*PUW=110*/
  }, { /* 27 */
    {OP_vldr,    0x0dd00a00, "vldr",   WBd, xx, MP8Xd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vldr,    0x0dd00b00, "vldr",   VBq, xx, MP8Xq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_ldcl,    0x0dd00000, "ldcl",   CRBw, xx, MP8Xw, i4_8, xx, pred, x, tfp[19][0x02]},/*PUW=110*/
  }, { /* 28 */
    {INVALID,    0x0de00a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID,    0x0de00b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_stcl,    0x0de00000, "stcl",   MP8Xw, RAw, i4_8, CRBw, i8x4, xop_wb|pred, x, tfp[20][0x02]},/*PUW=111*/
  }, { /* 29 */
    {INVALID,    0x0df00a00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID,    0x0df00b00, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_ldcl,    0x0df00000, "ldcl",   CRBw, RAw, MP8Xw, i4_8, i8x4, xop_wb|pred, x, tfp[21][0x02]},/*PUW=111*/
  }, { /* 30 */
    {EXT_FPA,    0x0e000a00, "(ext fpA 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FPB,    0x0e000b00, "(ext fpB 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BIT4,   0x0e000000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 31 */
    {EXT_FPA,    0x0e100a00, "(ext fpA 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_FPB,    0x0e100b00, "(ext fpB 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BIT4,   0x0e100000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 32 */
    {EXT_FPA,    0x0e200a00, "(ext fpA 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_FPB,    0x0e200b00, "(ext fpB 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_BIT4,   0x0e200000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 33 */
    {EXT_FPA,    0x0e300a00, "(ext fpA 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_FPB,    0x0e300b00, "(ext fpB 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_BIT4,   0x0e300000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 34 */
    {EXT_FPA,    0x0e400a00, "(ext fpA 4)",  xx, xx, xx, xx, xx, no, x, 4},
    {EXT_FPB,    0x0e400b00, "(ext fpB 4)",  xx, xx, xx, xx, xx, no, x, 4},
    {EXT_BIT4,   0x0e400000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 35 */
    {EXT_FPA,    0x0e500a00, "(ext fpA 5)",  xx, xx, xx, xx, xx, no, x, 5},
    {EXT_FPB,    0x0e500b00, "(ext fpB 5)",  xx, xx, xx, xx, xx, no, x, 5},
    {EXT_BIT4,   0x0e500000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 36 */
    {EXT_FPA,    0x0e600a00, "(ext fpA 6)",  xx, xx, xx, xx, xx, no, x, 6},
    {EXT_FPB,    0x0e600b00, "(ext fpB 6)",  xx, xx, xx, xx, xx, no, x, 6},
    {EXT_BIT4,   0x0e600000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 37 */
    {EXT_FPA,    0x0e700a00, "(ext fpA 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_FPB,    0x0e700b00, "(ext fpB 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_BIT4,   0x0e700000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 38 */
    {EXT_FPA,    0x0e800a00, "(ext fpA 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_FPB,    0x0e800b00, "(ext fpB 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_BIT4,   0x0e800000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 39 */
    {EXT_FPA,    0x0e900a00, "(ext fpA 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_FPB,    0x0e900b00, "(ext fpB 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_BIT4,   0x0e900000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 40 */
    {EXT_FPA,    0x0ea00a00, "(ext fpA 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_FPB,    0x0ea00b00, "(ext fpB 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_BIT4,   0x0ea00000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 41 */
    {EXT_OPC4,   0x0eb00a00, "(ext opc4 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_OPC4,   0x0eb00b00, "(ext opc4 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_BIT4,   0x0eb00000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 42 */
    {EXT_FPA,    0x0ec00a00, "(ext fpA 11)",  xx, xx, xx, xx, xx, no, x, 11},
    {EXT_FPB,    0x0ec00b00, "(ext fpB 11)",  xx, xx, xx, xx, xx, no, x, 11},
    {EXT_BIT4,   0x0ec00000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 43 */
    {EXT_FPA,    0x0ed00a00, "(ext fpB 12)",  xx, xx, xx, xx, xx, no, x, 12},
    {EXT_FPB,    0x0ed00b00, "(ext fpB 12)",  xx, xx, xx, xx, xx, no, x, 12},
    {EXT_BIT4,   0x0ed00000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 44 */
    {EXT_FPA,    0x0ee00a00, "(ext fpA 13)",  xx, xx, xx, xx, xx, no, x, 13},
    {EXT_FPB,    0x0ee00b00, "(ext fpB 13)",  xx, xx, xx, xx, xx, no, x, 13},
    {EXT_BIT4,   0x0ee00000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 45 */
    {EXT_OPC4,   0x0ef00a00, "(ext opc4 9)",  xx, xx, xx, xx, xx, no, x,  9},
    {EXT_OPC4,   0x0ef00b00, "(ext opc4 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_BIT4,   0x0ef00000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 46 */
    {OP_vsel_eq_f32, 0xfe000a00, "vsel.eq.f32",   WBd, xx, i2_20, WAd, WCd, v8|vfp, fRZ, END_LIST},
    {OP_vsel_eq_f64, 0xfe000b00, "vsel.eq.f64",   VBq, xx, i2_20, VAq, VCq, v8|vfp, fRZ, END_LIST},
    {EXT_BIT4,       0xfe000000, "(ext bit4 11)", xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 47 */
    {OP_vsel_vs_f32, 0xfe100a00, "vsel.vs.f32",   WBd, xx, i2_20, WAd, WCd, v8|vfp, fRV, END_LIST},
    {OP_vsel_vs_f64, 0xfe100b00, "vsel.vs.f64",   VBq, xx, i2_20, VAq, VCq, v8|vfp, fRV, END_LIST},
    {EXT_BIT4,       0xfe100000, "(ext bit4 12)", xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 48 */
    {OP_vsel_ge_f32, 0xfe200a00, "vsel.ge.f32",   WBd, xx, i2_20, WAd, WCd, v8|vfp, fRNV, END_LIST},
    {OP_vsel_ge_f64, 0xfe200b00, "vsel.ge.f64",   VBq, xx, i2_20, VAq, VCq, v8|vfp, fRNV, END_LIST},
    {EXT_BIT4,       0xfe200000, "(ext bit4 11)", xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 49 */
    {OP_vsel_gt_f32, 0xfe300a00, "vsel.gt.f32",   WBd, xx, i2_20, WAd, WCd, v8|vfp, fRNZV, END_LIST},
    {OP_vsel_gt_f64, 0xfe300b00, "vsel.gt.f64",   VBq, xx, i2_20, VAq, VCq, v8|vfp, fRNZV, END_LIST},
    {EXT_BIT4,       0xfe100000, "(ext bit4 12)", xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 50 */
    {EXT_BIT6,       0xfe800a00, "(ext bit6  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BIT6,       0xfe800b00, "(ext bit6  2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_BIT4,       0xfe200000, "(ext bit4 11)", xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 51 */
    {INVALID,        0xfe900a00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0xfe900b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BIT4,       0xfe900000, "(ext bit4 12)", xx, xx, xx, xx, xx, no, x, 12},
  }, { /* 52 */
    {INVALID,        0xfea00a00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0xfea00b00, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BIT4,       0xfea00000, "(ext bit4 11)", xx, xx, xx, xx, xx, no, x, 11},
  }, { /* 53 */
    {EXT_SIMD5B,     0xfeb00a00, "(ext simd5b 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_SIMD5B,     0xfeb00b00, "(ext simd5b 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BIT4,       0xfeb00000, "(ext bit4 12)", xx, xx, xx, xx, xx, no, x, 12},
  },
};

/* Indexed by bits 6,4 but if both are set it's invalid. */
const instr_info_t A32_ext_opc4fpA[][3] = {
  { /* 0 */
    {OP_vmla_f32, 0x0e000a00, "vmla.f32",  WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {OP_vmov,     0x0e000a10, "vmov",     WAd, xx, RBd, xx, xx, pred|vfp, x, tfp[3][0x00]},
    {OP_vmls_f32, 0x0e000a40, "vmls.f32",  WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 1 */
    {OP_vnmls_f32,0x0e100a00, "vnmls.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {OP_vmov,     0x0e100a10, "vmov",     RBd, xx, WAd, xx, xx, pred|vfp, x, tfpA[0][0x01]},
    {OP_vnmla_f32,0x0e100a40, "vnmla.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 2 */
    {OP_vmul_f32, 0x0e200a00, "vmul.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e200a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmul_f32,0x0e200a40, "vnmul.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 3 */
    {OP_vadd_f32, 0x0e300a00, "vadd.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e300a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsub_f32, 0x0e300a40, "vsub.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 4 */
    {OP_vmla_f32, 0x0e400a00, "vmla.f32",  WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0e400a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmls_f32, 0x0e400a40, "vmls.f32",  WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 5 */
    {OP_vnmls_f32,0x0e500a00, "vnmls.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0e500a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmla_f32,0x0e500a40, "vnmla.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 6 */
    {OP_vmul_f32, 0x0e600a00, "vmul.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0e600a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmul_f32,0x0e600a40, "vnmul.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 7 */
    {OP_vadd_f32, 0x0e700a00, "vadd.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0e700a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsub_f32, 0x0e700a40, "vsub.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 8 */
    {OP_vdiv_f32, 0x0e800a00, "vdiv.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e800a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0e800a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_vfnms_f32,0x0e900a00, "vfnms.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e900a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnma_f32,0x0e900a40, "vfnma.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 10 */
    {OP_vfma_f32, 0x0ea00a00, "vfma.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0ea00a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f32, 0x0ea00a40, "vfms.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 11 */
    {OP_vdiv_f32, 0x0ec00a00, "vdiv.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0ec00a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0ec00a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 12 */
    {OP_vfnms_f32,0x0ed00a00, "vfnms.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0ed00a10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnma_f32,0x0ed00a40, "vfnma.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 13 */
    {OP_vfma_f32, 0x0ee00a00, "vfma.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
    {EXT_IMM1916, 0x0ee00a10, "(ext imm1916 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_vfms_f32, 0x0ee00a40, "vfms.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, DUP_ENTRY},
  },
};

/* Indexed by bits 6:4 */
const instr_info_t A32_ext_opc4fpB[][8] = {
  { /* 0 */
    {OP_vmla_f64, 0x0e000b00, "vmla.f64",  VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_32,  0x0e000b10, "vmov.32",  VAd_q, xx, RBd, i1_21, xx, pred|vfp, x, END_LIST},
    {OP_vmla_f64, 0x0e000b20, "vmla.f64",  VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_16,  0x0e000b30, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, pred|vfp, x, END_LIST},
    {OP_vmls_f64, 0x0e000b40, "vmls.f64",  VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e000b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmls_f64, 0x0e000b60, "vmls.f64",  VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_16,  0x0e000b70, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 1 */
    {OP_vnmls_f64,0x0e100b00, "vnmls.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_32,  0x0e100b10, "vmov.32",  RBd, xx, VAd_q, i1_21, xx, pred|vfp, x, tfpB[0][0x01]},
    {OP_vnmls_f64,0x0e100b20, "vnmls.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s16, 0x0e100b30, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
    {OP_vnmla_f64,0x0e100b40, "vnmla.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e100b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmla_f64,0x0e100b60, "vnmla.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s16, 0x0e100b70, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 2 */
    {OP_vmul_f64, 0x0e200b00, "vmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_32,  0x0e200b10, "vmov.32",  VAd_q, xx, RBd, i1_21, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmul_f64, 0x0e200b20, "vmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_16,  0x0e200b30, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vnmul_f64,0x0e200b40, "vnmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e200b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmul_f64,0x0e200b60, "vnmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_16,  0x0e200b70, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 3 */
    {OP_vadd_f64, 0x0e300b00, "vadd.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_32,  0x0e300b10, "vmov.32",  RBd, xx, VAd_q, i1_21, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vadd_f64, 0x0e300b20, "vadd.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s16, 0x0e300b30, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vsub_f64, 0x0e300b40, "vsub.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e300b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsub_f64, 0x0e300b60, "vsub.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s16, 0x0e300b70, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 4 */
    {OP_vmla_f64, 0x0e400b00, "vmla.f64",  VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0x0e400b10, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, END_LIST},
    {OP_vmla_f64, 0x0e400b20, "vmla.f64",  VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0x0e400b30, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmls_f64, 0x0e400b40, "vmls.f64",  VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0x0e400b50, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmls_f64, 0x0e400b60, "vmls.f64",  VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0x0e400b70, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 5 */
    {OP_vnmls_f64,0x0e500b00, "vnmls.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0x0e500b10, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {OP_vnmls_f64,0x0e500b20, "vnmls.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0x0e500b30, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vnmla_f64,0x0e500b40, "vnmla.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0x0e500b50, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vnmla_f64,0x0e500b60, "vnmla.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0x0e500b70, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 6 */
    {OP_vmul_f64, 0x0e600b00, "vmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0x0e600b10, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmul_f64, 0x0e600b20, "vmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0x0e600b30, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vnmul_f64,0x0e600b40, "vnmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0x0e600b50, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vnmul_f64,0x0e600b60, "vnmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_8,   0x0e600b70, "vmov.8",   VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 7 */
    {OP_vadd_f64, 0x0e700b00, "vadd.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0x0e700b10, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vadd_f64, 0x0e700b20, "vadd.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0x0e700b30, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vsub_f64, 0x0e700b40, "vsub.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0x0e700b50, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vsub_f64, 0x0e700b60, "vsub.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_s8,  0x0e700b70, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 8 */
    {OP_vdiv_f64, 0x0e800b00, "vdiv.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vdup_32,  0x0e800b10, "vdup.32",  WAd, xx, RBd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vdiv_f64, 0x0e800b20, "vdiv.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vdup_16,  0x0e800b30, "vdup.16",  WAd, xx, RBh, xx, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e800b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0e800b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0e800b60, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0e800b70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_vfnms_f64,0x0e900b00, "vfnms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e900b10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnms_f64,0x0e900b20, "vfnms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_u16, 0x0e900b30, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
    {OP_vfnma_f64,0x0e900b40, "vfnma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0e900b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnma_f64,0x0e900b60, "vfnma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_u16, 0x0e900b70, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 10 */
    {OP_vfma_f64, 0x0ea00b00, "vfma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vdup_32,  0x0ea00b10, "vdup.32",  VAq, xx, RBd, xx, xx, pred|vfp, x, tfpB[8][0x01]},
    {OP_vfma_f64, 0x0ea00b20, "vfma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vdup_16,  0x0ea00b30, "vdup.16",  VAq, xx, RBh, xx, xx, pred|vfp, x, tfpB[8][0x03]},
    {OP_vfms_f64, 0x0ea00b40, "vfms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID,     0x0ea00b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f64, 0x0ea00b60, "vfms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0ea00b70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 11 */
    {OP_vdiv_f64, 0x0ec00b00, "vdiv.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vdup_8,   0x0ec00b10, "vdup.8",   WAd, xx, RBb, xx, xx, pred|vfp, x, END_LIST},
    {OP_vdiv_f64, 0x0ec00b20, "vdiv.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0ec00b30, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0ec00b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0ec00b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0ec00b60, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0ec00b70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 12 */
    {OP_vfnms_f64,0x0ed00b00, "vfnms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_u8,  0x0ed00b10, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {OP_vfnms_f64,0x0ed00b20, "vfnms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_u8,  0x0ed00b30, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vfnma_f64,0x0ed00b40, "vfnma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_u8,  0x0ed00b50, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vfnma_f64,0x0ed00b60, "vfnma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vmov_u8,  0x0ed00b70, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 13 */
    {OP_vfma_f64, 0x0ee00b00, "vfma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vdup_8,   0x0ee00b10, "vdup.8",   VAq, xx, RBb, xx, xx, pred|vfp, x, tfpB[11][0x01]},
    {OP_vfma_f64, 0x0ee00b20, "vfma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0ee00b30, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f64, 0x0ee00b40, "vfms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0ee00b50, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f64, 0x0ee00b60, "vfms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,     0x0ee00b70, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 14 */
    {INVALID,     0xf5300000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_clrex,    0xf57ff01f, "clrex",  xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf5300020, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf5300030, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_dsb,      0xf57ff040, "dsb",    xx, xx, i4, xx, xx, no, x, END_LIST},
    {OP_dmb,      0xf57ff050, "dmb",    xx, xx, i4, xx, xx, no, x, END_LIST},
    {OP_isb,      0xf57ff060, "isb",    xx, xx, i4, xx, xx, no, x, END_LIST},
    {INVALID,     0xf5300070, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits 19:16 */
const instr_info_t A32_ext_bits16[][16] = {
  { /* 0 */
    {OP_vmov_f32,     0x0eb00a40, "vmov.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vneg_f32,     0x0eb10a40, "vneg.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtb_f32_f16, 0x0eb20a40, "vcvtb.f32.f16", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtb_f16_f32, 0x0eb30a40, "vcvtb.f16.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcmp_f32,     0x0eb40a40, "vcmp.f32", FPSCR, xx, WBd, WCd, xx, pred|vfp, x, END_LIST},
    {OP_vcmp_f32,     0x0eb50a40, "vcmp.f32", FPSCR, xx, WBd, k0, xx, pred|vfp, x, t16[0][0x04]},/*XXX: const is really fp, not int*/
    {OP_vrintr_f32,   0x0eb60a40, "vrintr.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vrintx_f32,   0x0eb70a40, "vrintx.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_u32, 0x0eb80a40, "vcvt.f32.u32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID,         0x0eb90a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s16, 0x0eba0a40, "vcvt.f32.s16", WBd, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_u16, 0x0ebb0a40, "vcvt.f32.u16", WBd, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_u32_f32,0x0ebc0a40, "vcvtr.u32.f32",WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_s32_f32,0x0ebd0a40, "vcvtr.s32.f32",WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s16_f32, 0x0ebe0a40, "vcvt.s16.f32", WBh, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u16_f32, 0x0ebf0a40, "vcvt.u16.f32", WBh, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
  }, { /* 1 */
    {OP_vabs_f32,     0x0eb00ac0, "vabs.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vsqrt_f32,    0x0eb10ac0, "vsqrt.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtt_f32_f16, 0x0eb20ac0, "vcvtt.f32.f16", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtt_f16_f32, 0x0eb30ac0, "vcvtt.f16.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcmpe_f32,    0x0eb40ac0, "vcmpe.f32", FPSCR, xx, WBd, WCd, xx, pred|vfp, x, END_LIST},
    {OP_vcmpe_f32,    0x0eb50ac0, "vcmpe.f32", FPSCR, xx, WBd, k0, xx, pred|vfp, x, t16[1][0x04]},/*XXX: const is really fp, not int*/
    {OP_vrintz_f32,   0x0eb60ac0, "vrintz.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_f32, 0x0eb70ac0, "vcvt.f64.f32", VBq, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_s32, 0x0eb80ac0, "vcvt.f32.s32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID,         0x0eb90ac0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s32, 0x0eba0ac0, "vcvt.f32.s32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[1][0x08]},
    {OP_vcvt_f32_u32, 0x0ebb0ac0, "vcvt.f32.u32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[0][0x08]},
    {OP_vcvt_u32_f32, 0x0ebc0ac0, "vcvt.u32.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f32, 0x0ebd0ac0, "vcvt.s32.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f32, 0x0ebe0ac0, "vcvt.s32.f32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[1][0x0d]},
    {OP_vcvt_u32_f32, 0x0ebf0ac0, "vcvt.u32.f32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[1][0x0c]},
  }, { /* 2 */
    {OP_vmov_f64,     0x0eb00b40, "vmov.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vneg_f64,     0x0eb10b40, "vneg.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtb_f64_f16, 0x0eb20b40, "vcvtb.f64.f16", VBq, xx, WCd, xx, xx, pred|vfp|v8, x, END_LIST},
    {OP_vcvtb_f16_f64, 0x0eb30b40, "vcvtb.f16.f64", WBd, xx, VCq, xx, xx, pred|vfp|v8, x, END_LIST},
    {OP_vcmp_f64,     0x0eb40b40, "vcmp.f64", FPSCR, xx, VBq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vcmp_f64,     0x0eb50b40, "vcmp.f64", FPSCR, xx, VBq, k0, xx, pred|vfp, x, t16[2][0x04]},/*XXX: const is really fp, not int*/
    {OP_vrintr_f64,   0x0eb60b40, "vrintr.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vrintx_f64,   0x0eb70b40, "vrintx.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_u32, 0x0eb80b40, "vcvt.f64.u32", VBq, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID,         0x0eb90b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s16, 0x0eba0b40, "vcvt.f64.s16", VBq, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_u16, 0x0ebb0b40, "vcvt.f64.u16", VBq, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_u32_f64,0x0ebc0b40, "vcvtr.u32.f64",WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_s32_f64,0x0ebd0b40, "vcvtr.s32.f64",WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s16_f64, 0x0ebe0b40, "vcvt.s16.f64", WBh, xx, VCq, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u16_f64, 0x0ebf0b40, "vcvt.u16.f64", WBh, xx, VCq, i5x0_5, xx, pred|vfp, x, END_LIST},
  }, { /* 3 */
    {OP_vabs_f64,     0x0eb00bc0, "vabs.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vsqrt_f64,    0x0eb10bc0, "vsqrt.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtt_f64_f16, 0x0eb20bc0, "vcvtt.f64.f16", VBq, xx, WCd, xx, xx, pred|vfp|v8, x, END_LIST},
    {OP_vcvtt_f16_f64, 0x0eb30bc0, "vcvtt.f16.f64", WBd, xx, VCq, xx, xx, pred|vfp|v8, x, END_LIST},
    {OP_vcmpe_f64,    0x0eb40bc0, "vcmpe.f64", FPSCR, xx, VBq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vcmpe_f64,    0x0eb50bc0, "vcmpe.f64", FPSCR, xx, VBq, k0, xx, pred|vfp, x, t16[3][0x04]},/*XXX: const is really fp, not int*/
    {OP_vrintz_f64,   0x0eb60bc0, "vrintz.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_f64, 0x0eb70bc0, "vcvt.f32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_s32, 0x0eb80bc0, "vcvt.f64.s32", VBq, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID,         0x0eb90bc0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s32, 0x0eba0bc0, "vcvt.f64.s32", VBq, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[3][0x08]},
    {OP_vcvt_f64_u32, 0x0ebb0bc0, "vcvt.f64.u32", VBq, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[2][0x08]},
    {OP_vcvt_u32_f64, 0x0ebc0bc0, "vcvt.u32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f64, 0x0ebd0bc0, "vcvt.s32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f64, 0x0ebe0bc0, "vcvt.s32.f64", WBd, xx, VCq, i5x0_5, xx, pred|vfp, x, t16[3][0x0d]},
    {OP_vcvt_u32_f64, 0x0ebf0bc0, "vcvt.u32.f64", WBd, xx, VCq, i5x0_5, xx, pred|vfp, x, t16[3][0x0c]},
  }, { /* 4 */
    {OP_vmov_f32,     0x0ef00a40, "vmov.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vneg_f32,     0x0ef10a40, "vneg.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,         0x0ef20a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0x0ef30a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmp_f32,     0x0ef40a40, "vcmp.f32", FPSCR, xx, WBd, WCd, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcmp_f32,     0x0ef50a40, "vcmp.f32", FPSCR, xx, WBd, k0, xx, pred|vfp, x, DUP_ENTRY},/*XXX: const is really fp, not int*/
    {OP_vrintr_f32,   0x0ef60a40, "vrintr.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vrintx_f32,   0x0ef70a40, "vrintx.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_u32, 0x0ef80a40, "vcvt.f32.u32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,         0x0ef90a40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s16, 0x0efa0a40, "vcvt.f32.s16", WBd, xx, WCh, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_u16, 0x0efb0a40, "vcvt.f32.u16", WBd, xx, WCh, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvtr_u32_f32,0x0efc0a40, "vcvtr.u32.f32",WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvtr_s32_f32,0x0efd0a40, "vcvtr.s32.f32",WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_s16_f32, 0x0efe0a40, "vcvt.s16.f32", WBh, xx, WCd, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_u16_f32, 0x0eff0a40, "vcvt.u16.f32", WBh, xx, WCd, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 5 */
    {OP_vabs_f32,     0x0ef00ac0, "vabs.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vsqrt_f32,    0x0ef10ac0, "vsqrt.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,         0x0ef20ac0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0x0ef30ac0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmpe_f32,    0x0ef40ac0, "vcmpe.f32", FPSCR, xx, WBd, WCd, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcmpe_f32,    0x0ef50ac0, "vcmpe.f32", FPSCR, xx, WBd, k0, xx, pred|vfp, x, DUP_ENTRY},/*XXX: const is really fp, not int*/
    {OP_vrintz_f32,   0x0ef60ac0, "vrintz.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_f32, 0x0ef70ac0, "vcvt.f64.f32", VBq, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_s32, 0x0ef80ac0, "vcvt.f32.s32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,         0x0ef90ac0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s32, 0x0efa0ac0, "vcvt.f32.s32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_u32, 0x0efb0ac0, "vcvt.f32.u32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_u32_f32, 0x0efc0ac0, "vcvt.u32.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_s32_f32, 0x0efd0ac0, "vcvt.s32.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_s32_f32, 0x0efe0ac0, "vcvt.s32.f32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_u32_f32, 0x0eff0ac0, "vcvt.u32.f32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 6 */
    {OP_vmov_f64,     0x0ef00b40, "vmov.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vneg_f64,     0x0ef10b40, "vneg.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,         0x0ef20b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0x0ef30b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmp_f64,     0x0ef40b40, "vcmp.f64", FPSCR, xx, VBq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcmp_f64,     0x0ef50b40, "vcmp.f64", FPSCR, xx, VBq, k0, xx, pred|vfp, x, DUP_ENTRY},/*XXX: const is really fp, not int*/
    {OP_vrintr_f64,   0x0ef60b40, "vrintr.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vrintx_f64,   0x0ef70b40, "vrintx.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_u32, 0x0ef80b40, "vcvt.f64.u32", VBq, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,         0x0ef90b40, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s16, 0x0efa0b40, "vcvt.f64.s16", VBq, xx, WCh, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_u16, 0x0efb0b40, "vcvt.f64.u16", VBq, xx, WCh, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvtr_u32_f64,0x0efc0b40, "vcvtr.u32.f64",WBd, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvtr_s32_f64,0x0efd0b40, "vcvtr.s32.f64",WBd, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_s16_f64, 0x0efe0b40, "vcvt.s16.f64", WBh, xx, VCq, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_u16_f64, 0x0eff0b40, "vcvt.u16.f64", WBh, xx, VCq, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 7 */
    {OP_vabs_f64,     0x0ef00bc0, "vabs.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vsqrt_f64,    0x0ef10bc0, "vsqrt.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,         0x0ef20bc0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0x0ef30bc0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmpe_f64,    0x0ef40bc0, "vcmpe.f64", FPSCR, xx, VBq, VCq, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcmpe_f64,    0x0ef50bc0, "vcmpe.f64", FPSCR, xx, VBq, k0, xx, pred|vfp, x, DUP_ENTRY},/*XXX: const is really fp, not int*/
    {OP_vrintz_f64,   0x0ef60bc0, "vrintz.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f32_f64, 0x0ef70bc0, "vcvt.f32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_s32, 0x0ef80bc0, "vcvt.f64.s32", VBq, xx, WCd, xx, xx, pred|vfp, x, DUP_ENTRY},
    {INVALID,         0x0ef90bc0, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s32, 0x0efa0bc0, "vcvt.f64.s32", VBq, xx, WCd, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_f64_u32, 0x0efb0bc0, "vcvt.f64.u32", VBq, xx, WCd, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_u32_f64, 0x0efc0bc0, "vcvt.u32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_s32_f64, 0x0efd0bc0, "vcvt.s32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_s32_f64, 0x0efe0bc0, "vcvt.s32.f64", WBd, xx, VCq, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
    {OP_vcvt_u32_f64, 0x0eff0bc0, "vcvt.u32.f64", WBd, xx, VCq, i5x0_5, xx, pred|vfp, x, DUP_ENTRY},
  }, { /* 8 */
    {INVALID,         0xf1000000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_setend,       0xf1010000, "setend",         xx, xx, i1_9, xx, xx, no, x, END_LIST},
    {OP_cps,          0xf1020000, "cps",            xx, xx, i5, xx, xx, no, x, END_LIST},
    {INVALID,         0xf1030000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0xf1040000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0xf1050000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0xf1060000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,         0xf1070000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_cpsie,        0xf1080000, "cpsie",          xx, xx, i3_6, xx, xx, no, x, END_LIST},
    {INVALID,         0xf1090000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_cpsie,        0xf10a0000, "cpsie",          xx, xx, i3_6, i5, xx, no, x, t16[8][0x08]},
    {INVALID,         0xf10b0000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_cpsid,        0xf10c0000, "cpsid",          xx, xx, i3_6, xx, xx, no, x, END_LIST},
    {INVALID,         0xf10d0000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
    {OP_cpsid,        0xf10e0000, "cpsid",          xx, xx, i3_6, i5, xx, no, x, t16[8][0x0c]},
    {INVALID,         0xf10f0000, "(bad)",          xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    /* These assume bit4 is not set */
    {EXT_SIMD6B,      0xf3b00000, "(ext simd6B 9)", xx, xx, xx, xx, xx, no, x, 9},
    {EXT_SIMD6B,      0xf3b10000, "(ext simd6B 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_SIMD6B,      0xf3b20000, "(ext simd6B 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_SIMD2,       0xf3b30000, "(ext simd2  0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_SIMD6B,      0xf3b40000, "(ext simd6B 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_SIMD6B,      0xf3b50000, "(ext simd6B 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_SIMD6B,      0xf3b60000, "(ext simd6B 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_SIMD2,       0xf3b70000, "(ext simd2  1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_SIMD6B,      0xf3b80000, "(ext simd6B 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_SIMD6B,      0xf3b90000, "(ext simd6B 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_SIMD6B,      0xf3ba0000, "(ext simd6B 7)", xx, xx, xx, xx, xx, no, x, 7},
    {EXT_SIMD6B,      0xf3bb0000, "(ext simd6B 8)", xx, xx, xx, xx, xx, no, x, 8},
    {EXT_SIMD2,       0xf3bc0000, "(ext simd2  2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_SIMD2,       0xf3bd0000, "(ext simd2  3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_SIMD2,       0xf3be0000, "(ext simd2  4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_SIMD2,       0xf3bf0000, "(ext simd2  5)", xx, xx, xx, xx, xx, no, x, 5},
  },
};

/* Indexed by whether RA != PC */
const instr_info_t A32_ext_RAPC[][2] = {
  { /* 0 */
    {OP_sxtab16, 0x06800070, "sxtab16", RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* rotates RDw then extracts 2 8-bit parts: model as reading whole thing, for now at least */
    {OP_sxtb16,  0x068f0070, "sxtb16", RBw, xx, RDw, ro2, xx, pred, x, END_LIST},/* ditto */
  }, { /* 1 */
    {OP_sxtab,   0x06a00070, "sxtab",  RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* rotates RDw then extracts 8 bits: model as reading whole thing, for now at least */
    {OP_sxtb,    0x06af0070, "sxtb",   RBw, xx, RDw, ro2, xx, pred, x, END_LIST},/* ditto */
  }, { /* 2 */
    {OP_sxtah,   0x06b00070, "sxtah",  RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* rotates RDw then extracts 8 bits: model as reading whole thing, for now at least */
    {OP_sxth,    0x06bf0070, "sxth",   RBw, xx, RDw, ro2, xx, pred, x, END_LIST},/* ditto */
  }, { /* 3 */
    {OP_uxtab16, 0x06c00070, "uxtab16", RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* rotates RDw then extracts 2x8 bits: model as reading whole thing, for now at least */
    {OP_uxtb16,  0x06cf0070, "uxtb16", RBw, xx, RDw, ro2, xx, pred, x, END_LIST},/* ditto */
  }, { /* 4 */
    {OP_uxtab,   0x06e00070, "uxtab",  RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* rotates RDw then extracts 8 bits: model as reading whole thing, for now at least */
    {OP_uxtb,    0x06ef0070, "uxtb",   RBw, xx, RDw, ro2, xx, pred, x, END_LIST},/* ditto */
  }, { /* 5 */
    {OP_uxtah,   0x06f00070, "uxtah",  RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* rotates RDw then extracts 16 bits: model as reading whole thing, for now at least */
    {OP_uxth,    0x06ff0070, "uxth",   RBw, xx, RDw, ro2, xx, pred, x, END_LIST},/* ditto */
  },
};

/* Indexed by whether RB != PC */
const instr_info_t A32_ext_RBPC[][2] = {
  { /* 0 */
    {EXT_IMM1916,0x0ef00a10, "(ext imm1916 4)", xx, xx, xx, xx, xx, no, x, 4},
    {OP_vmrs,    0x0ef1fa10, "vmrs",   CPSR, xx, FPSCR, xx, xx, pred|vfp, fWNZCV, ti19[4][0x00]},
  }, { /* 1 */
    {OP_smlad,   0x07000010, "smlad",  RAw, xx, RDw, RCw, RBw, pred, fWQ, END_LIST},
    {OP_smuad,   0x0700f010, "smuad",  RAw, xx, RDw, RCw, xx, pred, fWQ, END_LIST},
  }, { /* 2 */
    {OP_smladx,  0x07000030, "smladx", RAw, xx, RDw, RCw, RBw, pred, fWQ, END_LIST},
    {OP_smuadx,  0x0700f030, "smuadx", RAw, xx, RDw, RCw, xx, pred, fWQ, END_LIST},
  }, { /* 3 */
    {OP_smlsd,   0x07000050, "smlsd",  RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_smusd,   0x0700f050, "smusd",  RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
  }, { /* 4 */
    {OP_smlsdx,  0x07000070, "smlsdx", RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_smusdx,  0x0700f070, "smusdx", RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
  }, { /* 5 */
    {OP_smmla,   0x07500010, "smmla",  RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_smmul,   0x0750f010, "smmul",  RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
  }, { /* 6 */
    {OP_smmlar,  0x07500030, "smmlar", RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_smmulr,  0x0750f030, "smmulr", RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
  }, { /* 7 */
    {OP_usada8,  0x07800010, "usada8", RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_usad8,   0x0780f010, "usad8",  RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
  },
};

/* Indexed by whether RD != PC */
const instr_info_t A32_ext_RDPC[][2] = {
  { /* 0 */
    {OP_bfi,     0x07c00010, "bfi",    RBw, RDw, i5_16, i5_7, RBw, pred|srcX4, x, END_LIST},
    {OP_bfc,     0x07c0001f, "bfc",    RBw, xx, i5_16, i5_7, RBw, pred, x, END_LIST},
  },
  { /* 1 */
    {OP_bfi,     0x07d00010, "bfi",    RBw, RDw, i5_16, i5_7, RBw, pred|srcX4, x, DUP_ENTRY},
    {OP_bfc,     0x07d0001f, "bfc",    RBw, xx, i5_16, i5_7, RBw, pred, x, DUP_ENTRY},
  },
};

/* Indexed by whether imm5 11:7 is zero or not */
const instr_info_t A32_ext_imm5[][2] = {
  { /* 0 */
    {OP_mov,     0x01a00000, "mov",    RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_lsl,     0x01a00000, "lsl",    RBw, xx, RDw, i5_7, xx, pred, x, DUP_ENTRY},
  }, { /* 1 */
    {OP_rrx,     0x01a00060, "rrx",    RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_ror,     0x01a00060, "ror",    RBw, xx, RDw, i5_7, xx, pred, x, DUP_ENTRY},
  }, { /* 2 */
    {OP_movs,    0x01b00000, "movs",   RBw, xx, RDw, xx, xx, pred, fWNZ, END_LIST},
    {OP_lsls,    0x01b00000, "lsls",   RBw, xx, RDw, i5_7, xx, pred, fRC|fWNZC, DUP_ENTRY},
  }, { /* 3 */
    {OP_rrxs,    0x01b00060, "rrxs",   RBw, xx, RDw, xx, xx, pred, fWNZC, END_LIST},
    {OP_rors,    0x01b00060, "rors",   RBw, xx, RDw, i5_7, xx, pred, fRC|fWNZC, DUP_ENTRY},
  },
};

const instr_info_t A32_nopred_opc8[] = {
    /* FIXME i#1551: add A32_nopred_opc8[] table for top bits 0xf */
};

/****************************************************************************
 * Extra operands beyond the ones that fit into instr_info_t.
 * All cases where we have extra operands are single-encoding-only instructions,
 * so we can have instr_info_t.code point here.
 * The arm_table_chain.pl script now supports one exop[] entry per chain by
 * placing it last, so we can support multi-encoding instructions if one variant
 * has extra operands.
 *
 * XXX: just add more opnd fields, eat cost in data size and src line
 * length, for simpler tables?
 */
const instr_info_t A32_extra_operands[] =
{
    /* 0x00 */
    /* i#1683: writeback implicit base reg opnd must be last for -syntax_arm disasm */
    {OP_CONTD, 0x00000000, "writeback shift + base", xx, xx, i5_7, RAw, xx, no, x, END_LIST},/*xop_shift*/
    {OP_CONTD, 0x00000000, "writeback base", xx, xx, RAw, xx, xx, no, x, END_LIST},/*xop_wb*/
    {OP_CONTD, 0x00000000, "writeback index + base", xx, xx, RDw, RAw, xx, no, x, END_LIST},/*xop_wb2*/
    {OP_CONTD, 0x00000000, "<cdp/mcr/mrc cont'd>", xx, xx, i3_5, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<smlalxt cont'd>",  xx, xx, RCt, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<smlalxb cont'd>",  xx, xx, RCh, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<srs* cont'd>",  xx, xx, SPSR, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<{s,u}mlal{,d} cont'd>",  xx, xx, RCw, xx, xx, no, x, END_LIST},
};

/* clang-format on */
