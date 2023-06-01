/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2001 Hewlett-Packard Company */

/* decode_fast.c -- a partial but fast x86 decoder */

#include "../globals.h"
#include "decode_fast.h"
#include "../link.h"
#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "decode.h"
#include "decode_private.h"
#include "disassemble.h"

#ifdef DEBUG
/* case 10450: give messages to clients */
#    undef ASSERT_TRUNCATE
#    undef ASSERT_BITFIELD_TRUNCATE
#    undef ASSERT_NOT_REACHED
#    define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* This file contains tables and functions that help decode x86
   instructions so that we can determine the length of the decode
   instruction.  All code below based on tables in the ``Intel
   Architecture Software Developer's Manual,'' Volume 2: Instruction
   Set Reference, 1999.
   This decoder assumes that we are running in 32-bit, flat-address mode.
*/

/* NOTE that all of the tables in this file are indexed by the (primary
   or secondary) opcode byte.  The upper opcode nibble defines the rows,
   starting with 0 at the top.  The lower opcode nibble defines the
   columns, starting with 0 at left. */

/* Data table for fixed part of an x86 instruction.  The table is
   indexed by the 1st (primary) opcode byte.  Zero entries are
   reserved opcodes. */
static const byte fixed_length[256] = {
    1, 1, 1, 1, 2, 5, 1, 1, 1,
    1, 1, 1, 2, 5, 1, 1, /* 0 */
    1, 1, 1, 1, 2, 5, 1, 1, 1,
    1, 1, 1, 2, 5, 1, 1, /* 1 */
    1, 1, 1, 1, 2, 5, 1, 1, 1,
    1, 1, 1, 2, 5, 1, 1, /* 2 */
    1, 1, 1, 1, 2, 5, 1, 1, 1,
    1, 1, 1, 2, 5, 1, 1, /* 3 */

    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, /* 4 */
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, /* 5 */
    1, 1, 1, 1, 1, 1, 1, 1, 5,
    5, 2, 2, 1, 1, 1, 1, /* 6 */
    2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, /* 7 */

    2, 5, 2, 2, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, /* 8 */
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 7, 1, 1, 1, 1, 1, /* 9 */
    5, 5, 5, 5, 1, 1, 1, 1, 2,
    5, 1, 1, 1, 1, 1, 1, /* A */
    2, 2, 2, 2, 2, 2, 2, 2, 5,
    5, 5, 5, 5, 5, 5, 5, /* B */

    2, 2, 3, 1, 1, 1, 2, 5, 4,
    1, 3, 1, 1, 2, 1, 1, /* C */
    1, 1, 1, 1, 2, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, /* D */
    2, 2, 2, 2, 2, 2, 2, 2, 5,
    5, 7, 2, 1, 1, 1, 1, /* E */
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1 /* F */
    /* f6 and f7 OP_test immeds are handled specially in decode_sizeof() */
};

/* Data table for fixed immediate part of an x86 instruction that
   depends upon the existence of an operand-size byte.  The table is
   indexed by the 1st (primary) opcode byte.  Entries with non-zero
   values indicate opcodes with a variable-length immediate field.  We
   use this table if we've seen a operand-size prefix byte to adjust
   the fixed_length from dword to word.
 */
static const sbyte immed_adjustment[256] = {
    0, 0,  0, 0, 0, -2, 0, 0,  0,  0,  0,  0,  0,  -2, 0,  0, /* 0 */
    0, 0,  0, 0, 0, -2, 0, 0,  0,  0,  0,  0,  0,  -2, 0,  0, /* 1 */
    0, 0,  0, 0, 0, -2, 0, 0,  0,  0,  0,  0,  0,  -2, 0,  0, /* 2 */
    0, 0,  0, 0, 0, -2, 0, 0,  0,  0,  0,  0,  0,  -2, 0,  0, /* 3 */

    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0, /* 4 */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0, /* 5 */
    0, 0,  0, 0, 0, 0,  0, 0,  -2, -2, 0,  0,  0,  0,  0,  0, /* 6 */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0, /* 7 */

    0, -2, 0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 8 */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  -2, 0,  0,  0,  0,  0,  /* 9 */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  -2, 0,  0,  0,  0,  0,  0,  /* A */
    0, 0,  0, 0, 0, 0,  0, 0,  -2, -2, -2, -2, -2, -2, -2, -2, /* B */

    0, 0,  0, 0, 0, 0,  0, -2, 0,  0,  0,  0,  0,  0,  0,  0, /* C */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0, /* D */
    0, 0,  0, 0, 0, 0,  0, 0,  -2, -2, -2, -2, 0,  0,  0,  0, /* E */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0  /* F */
};

#ifdef X64
/* for x64 Intel, Jz is always a 64-bit addr ("f64" in Intel table) */
static const sbyte immed_adjustment_intel64[256] = {
    0, 0,  0, 0, 0, -2, 0, 0,  0,  0,  0,  0,  0,  -2, 0,  0, /* 0 */
    0, 0,  0, 0, 0, -2, 0, 0,  0,  0,  0,  0,  0,  -2, 0,  0, /* 1 */
    0, 0,  0, 0, 0, -2, 0, 0,  0,  0,  0,  0,  0,  -2, 0,  0, /* 2 */
    0, 0,  0, 0, 0, -2, 0, 0,  0,  0,  0,  0,  0,  -2, 0,  0, /* 3 */

    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0, /* 4 */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0, /* 5 */
    0, 0,  0, 0, 0, 0,  0, 0,  -2, -2, 0,  0,  0,  0,  0,  0, /* 6 */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0, /* 7 */

    0, -2, 0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 8 */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  -2, 0,  0,  0,  0,  0,  /* 9 */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  -2, 0,  0,  0,  0,  0,  0,  /* A */
    0, 0,  0, 0, 0, 0,  0, 0,  -2, -2, -2, -2, -2, -2, -2, -2, /* B */

    0, 0,  0, 0, 0, 0,  0, -2, 0,  0,  0,  0,  0,  0,  0,  0, /* C */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0, /* D */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  -2, -2, 0,  0,  0,  0, /* E */
    0, 0,  0, 0, 0, 0,  0, 0,  0,  0,  0,  0,  0,  0,  0,  0  /* F */
};
#endif

/* Data table for fixed immediate part of an x86 instruction that
 * depends upon the existence of an address-size byte.  The table is
 * indexed by the 1st (primary) opcode byte.
 * The value here is doubled for x64 mode.
 */
static const sbyte disp_adjustment[256] = {
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0 */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 1 */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 2 */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 3 */

    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 4 */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 5 */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 6 */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 7 */

    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 8 */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 9 */
    -2, -2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B */

    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* C */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* D */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* E */
    0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* F */
};

#ifdef X64
/* Data table for immediate adjustments that only apply when
 * in x64 mode.  We fit two types of adjustments in here:
 * default-size adjustments (positive numbers) and rex.w-prefix-based
 * adjustments (negative numbers, to be made positive when applied).
 */
static const sbyte x64_adjustment[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* 0 */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* 1 */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* 2 */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* 3 */

    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* 4 */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* 5 */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* 6 */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* 7 */

    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  /* 8 */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  /* 9 */
    4, 4, 4, 4, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  /* A */
    0, 0, 0, 0, 0, 0, 0, 0, -4, -4, -4, -4, -4, -4, -4, -4, /* B */

    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* C */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* D */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0, /* E */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0  /* F */
};
#endif

/* Prototypes for the functions that calculate the variable
 * part of the x86 instruction length. */
static int
sizeof_modrm(dcontext_t *dcontext, byte *pc, bool addr16, byte **rip_rel_pc);
static int
sizeof_fp_op(dcontext_t *dcontext, byte *pc, bool addr16, byte **rip_rel_pc);
static int
sizeof_escape(dcontext_t *dcontext, byte *pc, bool addr16, byte **rip_rel_pc);
static int
sizeof_3byte_38(dcontext_t *dcontext, byte *pc, bool addr16, bool vex, byte **rip_rel_pc);
static int
sizeof_3byte_3a(dcontext_t *dcontext, byte *pc, bool addr16, byte **rip_rel_pc);

enum {
    VARLEN_NONE,
    VARLEN_MODRM,
    VARLEN_FP_OP,
    VARLEN_ESCAPE,          /* 2-byte opcodes */
    VARLEN_3BYTE_38_ESCAPE, /* 3-byte opcodes 0f 38 */
    VARLEN_3BYTE_3A_ESCAPE, /* 3-byte opcodes 0f 3a */
    VARLEN_RIP_REL_1BYTE,   /* Ends in a 1-byte rip-rel immediate. */
    VARLEN_RIP_REL_4BYTE,   /* Ends in a 4-byte rip-rel immediate. */
};

/* Some macros to make the following table look better. */
#define m VARLEN_MODRM
#define f VARLEN_FP_OP
#define e VARLEN_ESCAPE
#define r1 VARLEN_RIP_REL_1BYTE
#define r4 VARLEN_RIP_REL_4BYTE

/* Data table indicating what function to use to calculate
   the variable part of the x86 instruction.  This table
   is indexed by the primary opcode.  */
static const byte variable_length[256] = {
    m,  m,  m,  m,  0,  0,  0,  0,  m,  m,  m,  m,  0,  0,  0,  e, /* 0 */
    m,  m,  m,  m,  0,  0,  0,  0,  m,  m,  m,  m,  0,  0,  0,  0, /* 1 */
    m,  m,  m,  m,  0,  0,  0,  0,  m,  m,  m,  m,  0,  0,  0,  0, /* 2 */
    m,  m,  m,  m,  0,  0,  0,  0,  m,  m,  m,  m,  0,  0,  0,  0, /* 3 */

    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 4 */
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 5 */
    0,  0,  m,  m,  0,  0,  0,  0,  0,  m,  0,  m,  0,  0,  0,  0,  /* 6 */
    r1, r1, r1, r1, r1, r1, r1, r1, r1, r1, r1, r1, r1, r1, r1, r1, /* 7 */

    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m, /* 8 */
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 9 */
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* A */
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* B */

    m,  m,  0,  0,  m,  m,  m,  m,  0,  0,  0,  0,  0,  0,  0,  0, /* C */
    m,  m,  m,  m,  0,  0,  0,  0,  f,  f,  f,  f,  f,  f,  f,  f, /* D */
    r1, r1, r1, r1, 0,  0,  0,  0,  r4, r4, 0,  r1, 0,  0,  0,  0, /* E */
    0,  0,  0,  0,  0,  0,  m,  m,  0,  0,  0,  0,  0,  0,  m,  m  /* F */
};

/* eliminate the macros */
#undef m
#undef f
#undef e

/* Data table for the additional fixed part of a two-byte opcode.
 * This table is indexed by the 2nd opcode byte.  Zero entries are
 * reserved/bad opcodes.
 * N.B.: none of these need adjustment for data16 or addr16.
 *
 * 0f0f has extra suffix opcode byte
 * 0f78 has immeds depending on prefixes: handled in decode_sizeof()
 */
static const byte escape_fixed_length[256] = {
    1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 2, /* 0 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 1 */
    1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, /* 2 */
    1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, /* 3 */

    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 4 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 5 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 6 */
    2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 7 */

    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, /* 8 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 9 */
    1, 1, 1, 1, 2, 1, 0, 0, 1, 1, 1, 1, 2, 1, 1, 1, /* A */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, /* B */

    1, 1, 2, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* C */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* D */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* E */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0  /* F */
};

/* Some macros to make the following table look better. */
#define m VARLEN_MODRM
#define e1 VARLEN_3BYTE_38_ESCAPE
#define e2 VARLEN_3BYTE_3A_ESCAPE

/* Data table indicating what function to use to calcuate
   the variable part of the escaped x86 instruction.  This table
   is indexed by the 2nd opcode byte.  */
static const byte escape_variable_length[256] = {
    m,  m,  m,  m,  0,  0,  0,  0,  0,  0,  0,  0,  0,  m,  0,  m, /* 0 */
    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m, /* 1 */
    m,  m,  m,  m,  0,  0,  0,  0,  m,  m,  m,  m,  m,  m,  m,  m, /* 2 */
    0,  0,  0,  0,  0,  0,  0,  0,  e1, 0,  e2, 0,  0,  0,  0,  0, /* 3 */

    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m, /* 4 */
    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m, /* 5 */
    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m, /* 6 */
    m,  m,  m,  m,  m,  m,  m,  0,  m,  m,  m,  m,  m,  m,  m,  m, /* 7 */

    r4, r4, r4, r4, r4, r4, r4, r4, r4, r4, r4, r4, r4, r4, r4, r4, /* 8 */
    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  /* 9 */
    0,  0,  0,  m,  m,  m,  0,  0,  0,  0,  0,  m,  m,  m,  m,  m,  /* A */
    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  /* B */

    m,  m,  m,  m,  m,  m,  m,  m,  0,  0,  0,  0,  0,  0,  0,  0, /* C */
    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m, /* D */
    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m, /* E */
    m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  m,  0  /* F */
};

/* eliminate the macros */
#undef m
#undef e

/* Data table for the additional fixed part of a three-byte opcode 0f 38.
 * This table is indexed by the 3rd opcode byte.  Zero entries are
 * reserved/bad opcodes.
 * N.B.: ALL of these have modrm bytes, and NONE of these need adjustment for data16
 * or addr16.
 */
#if 0 /* to be robust wrt future additions we assume all entries are 1 */
static const byte threebyte_38_fixed_length[256] = {
    1,1,1,1, 1,1,1,1, 1,1,1,1, 0,0,0,0,  /* 0 */
    1,0,0,0, 1,1,0,1, 0,0,0,0, 1,1,1,0,  /* 1 */
    1,1,1,1, 1,1,0,0, 1,1,1,1, 0,0,0,0,  /* 2 */
    1,1,1,1, 1,1,0,1, 1,1,1,1, 1,1,1,1,  /* 3 */
    1,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 4 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 5 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 6 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 7 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 8 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 9 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* A */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* B */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* C */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* D */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* E */
    1,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0   /* F */
};
#endif
/* Three-byte 0f 3a: all are assumed to have a 1-byte immediate as well! */
#if 0 /* to be robust wrt future additions we assume all entries are 1 */
static const byte threebyte_3a_fixed_length[256] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1,  /* 0 */
    0,0,0,0, 1,1,1,1, 1,1,1,1, 1,1,1,0,  /* 1 */
    1,1,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 2 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 3 */
    0,1,1,1, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 4 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 5 */
    1,1,1,1, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 6 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 7 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 8 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* 9 */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* A */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* B */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* C */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* D */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  /* E */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0   /* F */
};
#endif

/* Extra size when vex-encoded (from immeds) */
static const byte threebyte_38_vex_extra[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 1 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 2 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 3 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 4 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 5 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 6 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 7 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 8 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 9 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* C */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* D */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* E */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* F */
};

/* XOP.0x08 is assumed to always have an immed byte */

/* Extra size for XOP opcode 0x09 (from immeds) */
static const byte xop_9_extra[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 1 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 2 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 3 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 4 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 5 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 6 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 7 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 8 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 9 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* C */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* D */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* E */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* F */
};

/* Extra size for XOP opcode 0x0a (from immeds) */
static const byte xop_a_extra[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0 */
    4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 1 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 2 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 3 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 4 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 5 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 6 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 7 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 8 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 9 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* C */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* D */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* E */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* F */
};

/* Returns the length of the instruction at pc.
 * If num_prefixes is non-NULL, returns the number of prefix bytes.
 * If rip_rel_pos is non-NULL, returns the offset into the instruction
 * of a rip-relative addressing displacement (for data only: ignores
 * control-transfer relative addressing), or 0 if none.
 * May return 0 size for certain invalid instructions
 */
int
decode_sizeof_ex(void *drcontext, byte *start_pc, int *num_prefixes, uint *rip_rel_pos)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    byte *pc = start_pc;
    uint opc = (uint)*pc;
    int sz = 0;
    ushort varlen;
    bool word_operands = false;  /* data16 */
    bool qword_operands = false; /* rex.w */
    bool addr16 = false;         /* really "addr32" for x64 mode */
    bool found_prefix = true;
    bool rep_prefix = false;
    bool evex_prefix = false;
    byte reg_opcode; /* reg_opcode field of modrm byte */
    byte *rip_rel_pc = NULL;

    /* Check for prefix byte(s) */
    while (found_prefix) {
        /* NOTE - rex prefixes must come after all other prefixes (including
         * prefixes that are part of the opcode xref PR 271878).  We match
         * read_instruction() in considering pre-prefix rex bytes as part of
         * the following instr, event when ignored, rather then treating them
         * as invalid.  This in effect nops improperly placed rex prefixes which
         * (xref PR 241563 and Intel Manual 2A 2.2.1) is the correct thing to do.
         * Rex prefixes are 0x40-0x4f; >=0x48 has rex.w bit set.
         */
        if (X64_MODE_DC(dcontext) && opc >= REX_PREFIX_BASE_OPCODE &&
            opc <= (REX_PREFIX_BASE_OPCODE | REX_PREFIX_ALL_OPFLAGS)) {
            if (opc >= (REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG)) {
                qword_operands = true;
                if (word_operands)
                    word_operands = false; /* rex.w trumps data16 */
            }                              /* else, doesn't affect instr size */
            opc = (uint) * (++pc);
            sz += 1;
        } else {
            switch (opc) {
            case DATA_PREFIX_OPCODE: /* operand size */
                /* rex.w before other prefixes is a nop */
                if (qword_operands)
                    qword_operands = false;
                word_operands = true;
                opc = (uint) * (++pc);
                sz += 1;
                break;
            case REPNE_PREFIX_OPCODE:
            case REP_PREFIX_OPCODE: /* REP */
                rep_prefix = true;
                /* fall through */
            case RAW_PREFIX_lock: /* LOCK */
            case CS_SEG_OPCODE:   /* segment overrides */
            case DS_SEG_OPCODE:
            case ES_SEG_OPCODE:
            case FS_SEG_OPCODE:
            case GS_SEG_OPCODE:
            case SS_SEG_OPCODE:
                opc = (uint) * (++pc);
                sz += 1;
                break;
            case ADDR_PREFIX_OPCODE:
                addr16 = true;
                opc = (uint) * (++pc);
                sz += 1;
                /* up to caller to check for addr prefix! */
                break;
            case EVEX_PREFIX_OPCODE: {
                /* If 64-bit mode or EVEX.R' bit is flipped, this is evex */
                if (X64_MODE_DC(dcontext) || TEST(0x10, *(pc + 1))) {
                    evex_prefix = true;
                }
                /* Fall-through is deliberate, EVEX is handled through VEX below */
            }
            case VEX_3BYTE_PREFIX_OPCODE:
            case VEX_2BYTE_PREFIX_OPCODE: {
                /* If 64-bit mode or mod selects for register, this is vex */
                if (evex_prefix || X64_MODE_DC(dcontext) ||
                    TESTALL(MODRM_BYTE(3, 0, 0), *(pc + 1))) {
                    /* Assumptions:
                     * - no vex-encoded instr size differs based on vex.w,
                     *   so we don't bother to set qword_operands
                     * - no vex-encoded instr size differs based on prefixes,
                     *   so we don't bother to decode vex.pp
                     */
                    bool vex3 = (opc == VEX_3BYTE_PREFIX_OPCODE);
                    byte vex_mm = 0;
                    opc = (uint) * (++pc); /* 2nd (e)vex prefix byte */
                    sz += 1;
                    if (vex3) {
                        vex_mm = (byte)(opc & 0x1f);
                        opc = (uint) * (++pc); /* 3rd vex prefix byte */
                        sz += 1;
                    } else if (evex_prefix) {
                        vex_mm = (byte)(opc & 0x3);
                        opc = (uint) * (++pc); /* 3rd evex prefix byte */
                        sz += 1;
                        opc = (uint) * (++pc); /* 4th evex prefix byte */
                        sz += 1;
                    }
                    opc = (uint) * (++pc); /* 1st opcode byte */
                    sz += 1;
                    if (num_prefixes != NULL)
                        *num_prefixes = sz;
                    /* no prefixes after vex + already did full size, so goto end */
                    bool implied_escape = (!vex3 && !evex_prefix) ||
                        ((vex3 || evex_prefix) && (vex_mm == 1));
                    if (implied_escape) {
                        sz += sizeof_escape(dcontext, pc, addr16, &rip_rel_pc);
                        goto decode_sizeof_done;
                    } else if (vex_mm == 2) {
                        sz +=
                            sizeof_3byte_38(dcontext, pc - 1, addr16, true, &rip_rel_pc);
                        goto decode_sizeof_done;
                    } else if (vex_mm == 3) {
                        sz += sizeof_3byte_3a(dcontext, pc - 1, addr16, &rip_rel_pc);
                        goto decode_sizeof_done;
                    }
                } else
                    found_prefix = false;
                break;
            }
            case 0x8f: {
                /* If XOP.map_select < 8, this is not XOP but instead OP_pop */
                byte map_select = *(pc + 1) & 0x1f;
                if (map_select >= 0x8) {
                    /* we have the same assumptions as for vex, that no instr size
                     * differs vased on vex.w or vex.pp
                     */
                    pc += 3; /* skip all 3 xop prefix bytes */
                    sz += 3;
                    opc = (uint)*pc; /* opcode byte */
                    sz += 1;
                    if (num_prefixes != NULL)
                        *num_prefixes = sz;
                    /* all have modrm */
                    sz += sizeof_modrm(dcontext, pc + 1, addr16, &rip_rel_pc);
                    if (map_select == 0x8) {
                        /* these always have an immediate byte */
                        sz += 1;
                    } else if (map_select == 0x9)
                        sz += xop_9_extra[opc];
                    else if (map_select == 0xa)
                        sz += xop_a_extra[opc];
                    else {
                        ASSERT_CURIOSITY(false && "unknown XOP map_select");
                        /* to try to handle future ISA additions we don't abort */
                    }
                    /* no prefixes after xop + already did full size, so goto end */
                    goto decode_sizeof_done;
                } else
                    found_prefix = false;
                break;
            }
            default: found_prefix = false;
            }
        }
    }
    if (num_prefixes != NULL)
        *num_prefixes = sz;
    if (word_operands) {
#ifdef X64
        /* for x64 Intel, always 64-bit addr ("f64" in Intel table)
         * FIXME: what about 2-byte jcc?
         */
        if (X64_MODE_DC(dcontext) && proc_get_vendor() == VENDOR_INTEL)
            sz += immed_adjustment_intel64[opc];
        else
#endif
            sz += immed_adjustment[opc]; /* no adjustment for 2-byte escapes */
    }
    if (addr16) {                  /* no adjustment for 2-byte escapes */
        if (X64_MODE_DC(dcontext)) /* from 64 bits down to 32 bits */
            sz += 2 * disp_adjustment[opc];
        else /* from 32 bits down to 16 bits */
            sz += disp_adjustment[opc];
    }
#ifdef X64
    if (X64_MODE_DC(dcontext)) {
        int adj64 = x64_adjustment[opc];
        if (adj64 > 0) /* default size adjustment */
            sz += adj64;
        else if (qword_operands)
            sz += -adj64; /* negative indicates prefix, not default, adjust */
        /* else, no adjustment */
    }
#endif

    /* opc now really points to opcode */
    sz += fixed_length[opc];
    varlen = variable_length[opc];

    /* for a valid instr, sz must be > 0 here, but we don't want to assert
     * since we need graceful failure
     */

    if (varlen == VARLEN_MODRM)
        sz += sizeof_modrm(dcontext, pc + 1, addr16, &rip_rel_pc);
    else if (varlen == VARLEN_ESCAPE) {
        sz += sizeof_escape(dcontext, pc + 1, addr16, &rip_rel_pc);
        /* special case: Intel and AMD added size-differing prefix-dependent instrs! */
        if (*(pc + 1) == 0x78) {
            /* XXX: if have rex.w prefix we clear word_operands: is that legal combo? */
            if (word_operands || rep_prefix) {
                /* extrq, insertq: 2 1-byte immeds */
                sz += 2;
            } /* else, vmread, w/ no immeds */
        }
    } else if (varlen == VARLEN_FP_OP) {
        sz += sizeof_fp_op(dcontext, pc + 1, addr16, &rip_rel_pc);
    } else if (varlen == VARLEN_RIP_REL_1BYTE) {
        rip_rel_pc = start_pc + sz - 1;
    } else if (varlen == VARLEN_RIP_REL_4BYTE) {
        rip_rel_pc = start_pc + sz - 4;
    } else
        CLIENT_ASSERT(varlen == VARLEN_NONE, "internal decoding error");

    /* special case that doesn't fit the mold (of course one had to exist) */
    reg_opcode = (byte)(((*(pc + 1)) & 0x38) >> 3);
    if (opc == 0xf6 && reg_opcode == 0) {
        sz += 1; /* TEST Eb,ib -- add size of immediate */
    } else if (opc == 0xf7 && reg_opcode == 0) {
        if (word_operands)
            sz += 2; /* TEST Ew,iw -- add size of immediate */
        else
            sz += 4; /* TEST El,il -- add size of immediate */
    }
    /* Another special case: xbegin. */
    if (opc == 0xc7 && *(pc + 1) == 0xf8)
        rip_rel_pc = start_pc + sz - 4;

decode_sizeof_done:
    if (rip_rel_pos != NULL) {
        if (rip_rel_pc != NULL) {
            CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_uint(rip_rel_pc - start_pc),
                          "decode_sizeof: unknown rip_rel instr type");
            *rip_rel_pos = (uint)(rip_rel_pc - start_pc);
        } else
            *rip_rel_pos = 0;
    }

    return sz;
}

int
decode_sizeof(void *drcontext, byte *start_pc,
              int *num_prefixes _IF_X64(uint *rip_rel_pos))
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
#ifdef X64
    return decode_sizeof_ex(dcontext, start_pc, num_prefixes, rip_rel_pos);
#else
    return decode_sizeof_ex(dcontext, start_pc, num_prefixes, NULL);
#endif
}

static int
sizeof_3byte_38(dcontext_t *dcontext, byte *pc, bool addr16, bool vex, byte **rip_rel_pc)
{
    int sz = 1; /* opcode past 0x0f 0x38 */
    uint opc = *(++pc);
    /* so far all 3-byte instrs have modrm bytes */
    /* to be robust for future additions we don't actually
     * use the threebyte_38_fixed_length[opc] entry and assume 1 */
    if (vex)
        sz += threebyte_38_vex_extra[opc];
    sz += sizeof_modrm(dcontext, pc + 1, addr16, rip_rel_pc);
    return sz;
}

static int
sizeof_3byte_3a(dcontext_t *dcontext, byte *pc, bool addr16, byte **rip_rel_pc)
{
    pc++;
    /* so far all 0f 3a 3-byte instrs have modrm bytes and 1-byte immeds */
    /* to be robust for future additions we don't actually
     * use the threebyte_3a_fixed_length[opc] entry and assume 1 */
    return 1 + sizeof_modrm(dcontext, pc + 1, addr16, rip_rel_pc) + 1;
}

/* Two-byte opcode map (Tables A-4 and A-5).  You use this routine
 * when you have identified the primary opcode as 0x0f.  You pass this
 * routine the next byte to determine the number of extra bytes in the
 * entire instruction.
 * May return 0 size for certain invalid instructions.
 */
static int
sizeof_escape(dcontext_t *dcontext, byte *pc, bool addr16, byte **rip_rel_pc)
{
    uint opc = (uint)*pc;
    int sz = escape_fixed_length[opc];
    ushort varlen = escape_variable_length[opc];

    /* for a valid instr, sz must be > 0 here, but we don't want to assert
     * since we need graceful failure
     */

    if (varlen == VARLEN_MODRM)
        return sz + sizeof_modrm(dcontext, pc + 1, addr16, rip_rel_pc);
    else if (varlen == VARLEN_3BYTE_38_ESCAPE) {
        return sz + sizeof_3byte_38(dcontext, pc, addr16, false, rip_rel_pc);
    } else if (varlen == VARLEN_3BYTE_3A_ESCAPE) {
        return sz + sizeof_3byte_3a(dcontext, pc, addr16, rip_rel_pc);
    } else if (varlen == VARLEN_RIP_REL_1BYTE) {
        *rip_rel_pc = pc + sz - 1;
    } else if (varlen == VARLEN_RIP_REL_4BYTE) {
        *rip_rel_pc = pc + sz - 4;
    } else
        CLIENT_ASSERT(varlen == VARLEN_NONE, "internal decoding error");

    return sz;
}

/* 32-bit addressing forms with the ModR/M Byte (Table 2-2).  You call
 * this routine with the byte following the primary opcode byte when you
 * know that the operation's next byte is a ModR/M byte.  This routine
 * passes back the size of the Eaddr specification in bytes based on the
 * following encoding of Table 2-2.
 *
 *   Mod        R/M
 *        0 1 2 3 4 5 6 7
 *    0   1 1 1 1 * 5 1 1
 *    1   2 2 2 2 3 2 2 2
 *    2   5 5 5 5 6 5 5 5
 *    3   1 1 1 1 1 1 1 1
 *   where (*) is 6 if base==5 and 2 otherwise.
 */
static int
sizeof_modrm(dcontext_t *dcontext, byte *pc, bool addr16, byte **rip_rel_pc)
{
    int l = 0; /* return value for sizeof(eAddr) */

    uint modrm = (uint)*pc;
    int r_m = modrm & 0x7;
    uint mod = modrm >> 6;
    uint sib;

#ifdef X64
    if (rip_rel_pc != NULL && X64_MODE_DC(dcontext) && mod == 0 && r_m == 5) {
        *rip_rel_pc = pc + 1; /* no sib: next 4 bytes are disp */
    }
#endif

    if (addr16 && !X64_MODE_DC(dcontext)) {
        if (mod == 1)
            return 2; /* modrm + disp8 */
        else if (mod == 2)
            return 3; /* modrm + disp16 */
        else if (mod == 3)
            return 1; /* just modrm */
        else {
            CLIENT_ASSERT(mod == 0, "internal decoding error on addr16 prefix");
            if (r_m == 6)
                return 3; /* modrm + disp16 */
            else
                return 1; /* just modrm */
        }
        CLIENT_ASSERT(false, "internal decoding error on addr16 prefix");
    }

    /* for x64, addr16 simply truncates the computed address: there is
     * no change in disp sizes */

    if (mod == 3) /* register operand */
        return 1;

    switch (mod) { /* memory or immediate operand */
    case 0: l = (r_m == 5) ? 5 : 1; break;
    case 1: l = 2; break;
    case 2: l = 5; break;
    }
    if (r_m == 4) {
        l += 1; /* adjust for sib byte */
        sib = (uint)(*(pc + 1));
        if ((sib & 0x7) == 5) {
            if (mod == 0)
                l += 4; /* disp32(,index,s) */
        }
    }

    return l;
}

/* General floating-point instruction formats (Table B-22).  You use
 * this routine when you have identified the primary opcode as one in
 * the range 0xb8 through 0xbf.  You pass this routine the next byte
 * to determine the number of extra bytes in the entire
 * instruction. */
static int
sizeof_fp_op(dcontext_t *dcontext, byte *pc, bool addr16, byte **rip_rel_pc)
{
    if (*pc > 0xbf)
        return 1; /* entire ModR/M byte is an opcode extension */

    /* fp opcode in reg/opcode field */
    return sizeof_modrm(dcontext, pc, addr16, rip_rel_pc);
}

/* Table indicating "interesting" instructions, i.e., ones we
 * would like to decode.  Currently these are control-transfer
 * instructions and interrupts.
 * This table is indexed by the 1st (primary) opcode byte.
 * A 0 indicates we are not interested, a 1 that we are.
 * A 2 indicates a second opcode byte exists, a 3 indicates an opcode
 * extension is present in the modrm byte.
 */
static const byte interesting[256] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2, /* 0 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, /* 1 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, /* 2 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, /* 3 */

    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, /* 4 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, /* 5 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, /* 6 */
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    /* 7 */ /* jcc_short */

    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    /* 8 */ /* mov_seg */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    1,
    0,
    0,
    /* 9 */ /* call_far, popf */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, /* A */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, /* B */

    0,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    /* C */ /* ret*, int* */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    3,
    0,
    0,
    /* D */ /* fnstenv, fnsave */
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    /* E */ /* loop*, call, jmp* */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3, /* F */
};

/* Table indicating operations on the lower 6 eflags (CF,PF,AF,ZF,SF,OF)
 * This table is indexed by the 1st (primary) opcode byte.
 * We use the eflags constants from instr.h.
 * We ignore writing some of the 6 as a conservative simplification.
 * Also note that for some groups we assign values to invalid opcodes
 * just for simplicity
 */
#define x 0
#define RC EFLAGS_READ_CF
#define RP EFLAGS_READ_PF
#define RZ EFLAGS_READ_ZF
#define RS EFLAGS_READ_SF
#define RO EFLAGS_READ_OF
#define R6 EFLAGS_READ_6
#define RB (EFLAGS_READ_CF | EFLAGS_READ_ZF)
#define RL (EFLAGS_READ_SF | EFLAGS_READ_OF)
#define RE (EFLAGS_READ_SF | EFLAGS_READ_OF | EFLAGS_READ_ZF)
#define R5O (EFLAGS_READ_6 & (~EFLAGS_READ_OF))
#define WC EFLAGS_WRITE_CF
#define WZ EFLAGS_WRITE_ZF
#define W6 EFLAGS_WRITE_6
#define W5 (EFLAGS_WRITE_6 & (~EFLAGS_WRITE_CF))
#define W5O (EFLAGS_WRITE_6 & (~EFLAGS_WRITE_OF))
#define BC (EFLAGS_WRITE_6 | EFLAGS_READ_CF)
#define BA (EFLAGS_WRITE_6 | EFLAGS_READ_AF)
#define BD (EFLAGS_WRITE_6 | EFLAGS_READ_CF | EFLAGS_READ_AF)
#define B6 (EFLAGS_WRITE_6 | EFLAGS_READ_6)
#define EFLAGS_6_ESCAPE -1
#define EFLAGS_6_SPECIAL -2
#define E EFLAGS_6_ESCAPE
#define S EFLAGS_6_SPECIAL

static const int eflags_6[256] = {
    W6, W6, W6, W6, W6, W6, x,  x,  W6, W6, W6, W6, W6, W6, x,   E,  /* 0 */
    BC, BC, BC, BC, BC, BC, x,  x,  BC, BC, BC, BC, BC, BC, x,   x,  /* 1 */
    W6, W6, W6, W6, W6, W6, x,  BD, W6, W6, W6, W6, W6, W6, x,   BD, /* 2 */
    W6, W6, W6, W6, W6, W6, x,  BA, W6, W6, W6, W6, W6, W6, x,   BA, /* 3 */

    W5, W5, W5, W5, W5, W5, W5, W5, W5, W5, W5, W5, W5, W5, W5,  W5, /* 4 */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,   x,  /* 5 */
    x,  x,  x,  WZ, x,  x,  x,  x,  x,  W6, x,  W6, x,  x,  x,   x,  /* 6 */
    RO, RO, RC, RC, RZ, RZ, RB, RB, RS, RS, RP, RP, RL, RL, RE,  RE, /* 7 */

    S,  S,  S,  S,  W6, W6, x,  x,  x,  x,  x,  x,  x,  x,  x,   x,   /* 8 */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  R6, W6, W5O, R5O, /* 9 */
    x,  x,  x,  x,  x,  x,  W6, W6, W6, W6, x,  x,  x,  x,  W6,  W6,  /* A */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,   x,   /* B */

    S,  S,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  R6, R6, R6,  W6, /* C */
    S,  S,  S,  S,  W6, W6, x,  x,  x,  x,  S,  S,  x,  x,  x,   S,  /* D */
    RZ, RZ, x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,   x,  /* E */
    x,  x,  x,  x,  x,  WC, S,  S,  WC, WC, x,  x,  x,  x,  S,   S,  /* F */
};

/* Same as eflags_6 table, but for 2nd byte of 0x0f extension opcodes
 */
static const int escape_eflags_6[256] = {
    x,  x,  WZ, WZ, x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  /* 0 */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  /* 1 */
    W6, W6, W6, W6, x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  W6, W6, /* 2 */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  /* 3 */

    RO, RO, RC, RC, RZ, RZ, RB, RB, RS, RS, RP, RP, RL, RL, RE, RE, /* 4 */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  /* 5 */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  /* 6 */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  /* 7 */

    RO, RO, RC, RC, RZ, RZ, RB, RB, RS, RS, RP, RP, RL, RL, RE, RE, /* 8 */
    RO, RO, RC, RC, RZ, RZ, RB, RB, RS, RS, RP, RP, RL, RL, RE, RE, /* 9 */
    x,  x,  x,  W6, W6, W6, x,  x,  x,  x,  W6, W6, W6, W6, x,  W6, /* A */
    W6, W6, x,  W6, x,  x,  x,  x,  x,  x,  W6, W6, W6, W6, x,  x,  /* B */

    W6, W6, x,  x,  x,  x,  x,  WZ, x,  x,  x,  x,  x,  x,  x,  x, /* C */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x, /* D */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x, /* E */
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x, /* F */
};
#undef x
#undef RC
#undef RP
#undef RZ
#undef RS
#undef RO
#undef R6
#undef RB
#undef RL
#undef RE
#undef R5O
#undef WC
#undef WZ
#undef W6
#undef W5
#undef W5O
#undef BC
#undef BA
#undef BD
#undef B6
#undef E
#undef S

/* This routine converts a signed 8-bit offset into a target pc.  The
 * formal parameter pc should point to the beginning of the branch
 * instruction containing the offset and having length len in bytes.
 * The x86 architecture calculates offsets from the beginning of the
 * instruction following the branch. */
static app_pc
convert_8bit_offset(byte *pc, byte offset, uint len)
{
    return ((app_pc)pc) + (((int)(offset << 24)) >> 24) + len;
}

static bool
intercept_fip_save(byte *pc, byte byte0, byte byte1)
{
    if ((byte0 == 0xdd && ((byte1 >> 3) & 0x7) == 6) /* dd /6 == OP_fnsave */ ||
        (byte0 == 0xd9 && ((byte1 >> 3) & 0x7) == 6) /* d9 /6 == OP_fnstenv */)
        return true;
    if (byte0 == 0x0f && byte1 == 0xae) {
        int opc_ext;
        byte byte2 = *(pc + 2);
        opc_ext = (byte2 >> 3) & 0x7;
        return opc_ext == 0 || /* 0f ae /0 == OP_fxsave */
            opc_ext == 4 ||    /* 0f ae /4 == OP_xsave */
            opc_ext == 6;      /* 0f ae /6 == OP_xsaveopt */
    }
    if (byte0 == 0x0f && byte1 == 0xc7) {
        int opc_ext;
        byte byte2 = *(pc + 2);
        opc_ext = (byte2 >> 3) & 0x7;
        return opc_ext == 4; /* 0f c7 /4 == OP_xsavec */
    }
    return false;
}

static bool
get_implied_mm_e_vex_opcode_bytes(byte *pc, int prefixes, byte vex_mm, byte *byte0,
                                  byte *byte1)
{
    switch (vex_mm) {
    case 1:
        *byte0 = 0x0f;
        *byte1 = *(pc + prefixes);
        return true;
    case 2:
        *byte0 = 0x0f;
        *byte1 = 0x38;
        return true;
    case 3:
        *byte0 = 0x0f;
        *byte1 = 0x3a;
        return true;
    default: return false;
    }
}

/* Decodes only enough of the instruction at address pc to determine
 * its size, its effects on the 6 arithmetic eflags, and whether it is
 * a control-transfer instruction.  If it is, the operands fields of
 * instr are filled in.  If not, only the raw bits fields of instr are
 * filled in.  This corresponds to a Level 3 decoding for control
 * transfer instructions but a Level 1 decoding plus arithmetic eflags
 * information for all other instructions.
 *
 * Fills in the PREFIX_SEG_GS and PREFIX_SEG_FS prefix flags for all instrs.
 * Does NOT fill in any other prefix flags unless this is a cti instr
 * and the flags affect the instr.
 *
 * Assumes that instr is already initialized, but uses the x86/x64 mode
 * for the current thread rather than that set in instr.
 * If caller is re-using same instr struct over multiple decodings,
 * should call instr_reset or instr_reuse.
 * Returns the address of the byte following the instruction.
 * Returns NULL on decoding an invalid instr and sets opcode to OP_INVALID.
 */
byte *
decode_cti(void *drcontext, byte *pc, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    byte byte0, byte1;
    byte *start_pc = pc;

    /* find and remember the instruction and its size */
    int prefixes;
    /* next two needed for eflags analysis */
    int eflags;
    int i;
    byte modrm = 0; /* used only for EFLAGS_6_SPECIAL */
    /* PR 251479: we need to know about all rip-relative addresses.
     * Since change/setting raw bits invalidates, we must set this
     * on every return.
     */
    uint rip_rel_pos;
    int sz = decode_sizeof_ex(dcontext, pc, &prefixes, &rip_rel_pos);
    if (sz == 0) {
        /* invalid instruction! */
        instr_set_opcode(instr, OP_INVALID);
        return NULL;
    }
    instr_set_opcode(instr, OP_UNDECODED);
    IF_X64(instr_set_x86_mode(instr, get_x86_mode(dcontext)));

    byte0 = *(pc + prefixes);
    byte1 = *(pc + prefixes + 1);

    /* we call instr_set_raw_bits on every return from here, not up
     * front, because any instr_set_src, instr_set_dst, or
     * instr_set_opcode will kill original bits state
     */

    /* Fill in SEG_FS and SEG_GS override prefixes, ignore rest for now.
     * We rely on having these set during bb building.
     * FIXME - could be done in decode_sizeof which is already walking these
     * bytes, but would need to complicate its interface and prefixes are
     * fairly rare to begin with.
     */
    if (prefixes > 0) {
        for (i = 0; i < prefixes; i++, pc++) {
            switch (*pc) {
            case FS_SEG_OPCODE: instr_set_prefix_flag(instr, PREFIX_SEG_FS); break;
            case GS_SEG_OPCODE: instr_set_prefix_flag(instr, PREFIX_SEG_GS); break;
            case VEX_2BYTE_PREFIX_OPCODE:
                /* VEX 2-byte prefix implies 0x0f opcode */
                byte0 = 0x0f;
                byte1 = *(pc + prefixes);
                /* There are no prefixes after vex. */
                pc = start_pc + prefixes;
                i = prefixes;
                break;
            case EVEX_PREFIX_OPCODE:
                instr_set_prefix_flag(instr, PREFIX_EVEX);
                /* fall-through */
            case VEX_3BYTE_PREFIX_OPCODE: {
                /* EVEX and VEX 3-byte prefixes imply instruction opcodes by encoding mm
                 * bits in the second prefix byte. In theory, there are 5 VEX mm bits, but
                 * only 2 of them are used.
                 */
                byte vex_mm = (byte)(*(pc + 1) & 0x3);
                if (!get_implied_mm_e_vex_opcode_bytes(pc, prefixes, vex_mm, &byte0,
                                                       &byte1)) {
                    /* invalid instruction! */
                    instr_set_opcode(instr, OP_INVALID);
                    return NULL;
                }
                /* There are no prefixes after (e)vex. */
                pc = start_pc + prefixes;
                i = prefixes;
            }
            default: break;
            }
        }
    }

    /* eflags analysis
     * we do this even if -unsafe_ignore_eflags b/c it doesn't cost that
     * much and we can use the analysis to detect any bb that reads a flag
     * prior to writing it
     * i#3267: eflags lookup possibly incorrect for instructions with VEX prefix.
     * (and instructions with EVEX prefix once AVX512 has been added).
     */
    eflags = eflags_6[byte0];
    if (eflags == EFLAGS_6_ESCAPE) {
        eflags = escape_eflags_6[byte1];
        if (eflags == EFLAGS_6_SPECIAL)
            modrm = *(pc + 2);
    } else if (eflags == EFLAGS_6_SPECIAL) {
        modrm = byte1;
    }
    if (eflags == EFLAGS_6_SPECIAL) {
        /* a number of cases exist beyond the ability of 2 tables
         * to distinguish
         */
        int opc_ext = (modrm >> 3) & 7; /* middle 3 bits */
        if (byte0 <= 0x84) {
            /* group 1* (80-83): all W6 except /2,/3=B */
            if (opc_ext == 2 || opc_ext == 3)
                eflags = EFLAGS_WRITE_6 | EFLAGS_READ_CF;
            else
                eflags = EFLAGS_WRITE_6;
        } else if (byte0 <= 0xd3) {
            /* group 2* (c0,c1,d0-d3): /0,/1=WC|WO, /2,/3=WC|WO|RC, /4,/5,/7=W6 */
            if (opc_ext == 0 || opc_ext == 1)
                eflags = EFLAGS_WRITE_CF | EFLAGS_WRITE_OF;
            else if (opc_ext == 2 || opc_ext == 3)
                eflags = EFLAGS_WRITE_CF | EFLAGS_WRITE_OF | EFLAGS_READ_CF;
            else if (opc_ext == 4 || opc_ext == 5 || opc_ext == 7)
                eflags = EFLAGS_WRITE_6;
            else
                eflags = 0;
        } else if (byte0 <= 0xdf) {
            /* floats: dac0-dadf and dbc0-dbdf = RC|RP|RZ */
            if ((byte0 == 0xda || byte0 == 0xdb) && modrm >= 0xc0 && modrm <= 0xdf)
                eflags = EFLAGS_READ_CF | EFLAGS_READ_PF | EFLAGS_READ_ZF;
            /* floats: dbe8-dbf7 and dfe8-dff7 = WC|WP|WZ */
            else if ((byte0 == 0xdb || byte0 == 0xdf) && modrm >= 0xe8 && modrm <= 0xf7)
                eflags = EFLAGS_WRITE_CF | EFLAGS_WRITE_PF | EFLAGS_WRITE_ZF;
            else
                eflags = 0;
        } else if (byte0 <= 0xf7) {
            /* group 3a (f6) & 3b (f7): all W except /2 (OP_not) */
            if (opc_ext == 2)
                eflags = 0;
            else
                eflags = EFLAGS_WRITE_6;
        } else {
            /* group 4 (fe) & 5 (ff): /0,/1=W5 */
            if (opc_ext == 0 || opc_ext == 1)
                eflags = EFLAGS_WRITE_6 & (~EFLAGS_WRITE_CF);
            else
                eflags = 0;
        }
    }
    instr->eflags = eflags;
    instr_set_arith_flags_valid(instr, true);

    if (interesting[byte0] == 0) {
        /* assumption: opcode already OP_UNDECODED */
        /* assumption: operands are already marked invalid (instr was reset) */
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (start_pc + sz);
    }

    /* FIXME: would further "interesting" table produce any noticeable
     * performance improvement?
     */

    if (prefixes > 0) {
        /* prefixes are rare on ctis
         * rather than handle them all here, just do full decode
         * FIXME: if we start to see more and more jcc branch hints we
         * may change our minds here!  This is case 211206/6749.
         */
        if (decode(dcontext, start_pc, instr) == NULL)
            return NULL;
        else
            return (start_pc + sz);
    }

#ifdef FOOL_CPUID
    /* for fooling program into thinking hardware is different than it is */
    if (byte0 == 0x0f && byte1 == 0xa2) { /* cpuid */
        instr_set_opcode(instr, OP_cpuid);
        /* don't bother to set dsts/srcs */
        instr_set_operands_valid(instr, false);
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (start_pc + sz);
    }
#endif

    /* prefixes won't make a difference for 8-bit-offset jumps */

    if (byte0 == 0xeb) { /* jmp_short */
        app_pc tgt = convert_8bit_offset(pc, byte1, 2);
        instr_set_opcode(instr, OP_jmp_short);
        instr_set_num_opnds(dcontext, instr, 0, 1);
        instr_set_target(instr, opnd_create_pc(tgt));
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (pc + 2);
    }

    if ((byte0 & 0xf0) == 0x70) { /* jcc_short */
        /* 2-byte pc-relative jumps with an 8-bit displacement */
        app_pc tgt = convert_8bit_offset(pc, byte1, 2);
        /* Set the instr's opcode field.  Relies on special ordering
         * in opcode enum. */
        instr_set_opcode(instr, OP_jo_short + (byte0 & 0x0f));

        /* calculate the branch's target address */
        instr_set_num_opnds(dcontext, instr, 0, 1);
        instr_set_target(instr, opnd_create_pc(tgt));

        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (pc + 2);
    }

    if (byte0 == 0xe8) { /* call */
        int offset = *((int *)(pc + 1));
        app_pc tgt = pc + offset + 5;
        instr_set_opcode(instr, OP_call);
        instr_set_num_opnds(dcontext, instr, 2, 2);
        instr_set_target(instr, opnd_create_pc(tgt));
        instr_set_src(instr, 1, opnd_create_reg(REG_XSP));
        instr_set_dst(instr, 0, opnd_create_reg(REG_XSP));
        instr_set_dst(instr, 1,
                      opnd_create_base_disp(
                          REG_XSP, REG_NULL, 0, 0,
                          resolve_variable_size_dc(dcontext, 0, OPSZ_call, false)));
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (pc + 5);
    }

    if (byte0 == 0xe9) { /* jmp */
        int offset = *((int *)(pc + 1));
        app_pc tgt = pc + offset + 5;
        instr_set_opcode(instr, OP_jmp);
        instr_set_num_opnds(dcontext, instr, 0, 1);
        instr_set_target(instr, opnd_create_pc(tgt));
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (pc + 5);
    }

    if ((byte0 == 0x0f) && ((byte1 & 0xf0) == 0x80)) { /* jcc */
        /* 6-byte pc-relative jumps with a 32-bit displacement */
        /* calculate the branch's target address */
        int offset = *((int *)(pc + 2));
        app_pc tgt = pc + offset + 6;
        /* Set the instr's opcode field.  Relies on special ordering
         * in opcode enum. */
        instr_set_opcode(instr, OP_jo + (byte1 & 0x0f));

        instr_set_num_opnds(dcontext, instr, 0, 1);
        instr_set_target(instr, opnd_create_pc(tgt));

        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (pc + 6);
    }

    if (byte0 == 0xff) { /* check for indirect calls/branches */
        /* dispatch based on bits 5,4,3 in mod_rm byte */
        uint opc = (byte1 >> 3) & 0x7;

        if (opc >= 2 && opc <= 5) {
            /* this is an indirect jump or call */
            /* we care about the operands and prefixes, so just do the full decode
             */
            if (decode(dcontext, start_pc, instr) == NULL)
                return NULL;
            else
                return (start_pc + sz);
        }
        /* otherwise it wasn't an indirect branch so continue */
    }

    if ((byte0 & 0xf0) == 0xc0) { /* check for returns */
        byte nibble1 = (byte)(byte0 & 0x0f);
        switch (nibble1) {
        case 2:   /* ret w/ 2-byte immed */
        case 0xa: /* far ret w/ 2-byte immed */
            /* we bailed out to decode() earlier if any prefixes */
            CLIENT_ASSERT(prefixes == 0, "decode_cti: internal prefix error");
            instr_set_opcode(instr, nibble1 == 2 ? OP_ret : OP_ret_far);
            instr_set_num_opnds(dcontext, instr, 1, 3);
            instr_set_dst(instr, 0, opnd_create_reg(REG_XSP));
            instr_set_src(instr, 0, opnd_create_immed_int(*((short *)(pc + 1)), OPSZ_2));
            instr_set_src(instr, 1, opnd_create_reg(REG_XSP));
            instr_set_src(
                instr, 2,
                opnd_create_base_disp(
                    REG_XSP, REG_NULL, 0, 0,
                    resolve_variable_size_dc(
                        dcontext, 0, nibble1 == 2 ? OPSZ_ret : OPSZ_REXVARSTACK, false)));
            instr_set_raw_bits(instr, start_pc, sz);
            instr_set_rip_rel_pos(instr, rip_rel_pos);
            return (pc + 3);
        case 3: /* ret w/ no immed */
            instr_set_opcode(instr, OP_ret);
            instr_set_raw_bits(instr, start_pc, sz);
            instr_set_rip_rel_pos(instr, rip_rel_pos);
            /* we don't set any operands and leave to an up-decode for that */
            return (pc + 1);
        case 0xb: /* far ret w/ no immed */
            instr_set_opcode(instr, OP_ret_far);
            instr_set_raw_bits(instr, start_pc, sz);
            instr_set_rip_rel_pos(instr, rip_rel_pos);
            /* we don't set any operands and leave to an up-decode for that */
            return (pc + 1);
        }
        /* otherwise it wasn't a return so continue */
    }

    if ((byte0 & 0xf0) == 0xe0) { /* check for a funny 8-bit branch */
        byte nibble1 = (byte)(byte0 & 0x0f);

        /* determine the opcode */
        if (nibble1 == 0) { /* loopne */
            instr_set_opcode(instr, OP_loopne);
        } else if (nibble1 == 1) { /* loope */
            instr_set_opcode(instr, OP_loope);
        } else if (nibble1 == 2) { /* loop */
            instr_set_opcode(instr, OP_loop);
        } else if (nibble1 == 3) { /* jecxz */
            instr_set_opcode(instr, OP_jecxz);
        } else if (nibble1 == 10) { /* jmp_far */
            /* we need prefix info (data size controls immediate offset size),
             * this is rare so go ahead and do full decode
             */
            if (decode(dcontext, start_pc, instr) == NULL)
                return NULL;
            else
                return (start_pc + sz);
        }
        if (instr_opcode_valid(instr)) {
            /* calculate the branch's target address */
            app_pc tgt = convert_8bit_offset(pc, byte1, 2);
            /* all (except jmp far) use ecx as a source */
            instr_set_num_opnds(dcontext, instr, 0, 2);
            /* if we made it here, no addr prefix, so REG_XCX not REG_ECX or REG_CX */
            CLIENT_ASSERT(prefixes == 0, "decoding internal inconsistency");
            instr_set_src(instr, 1, opnd_create_reg(REG_XCX));
            instr_set_target(instr, opnd_create_pc(tgt));
            instr_set_raw_bits(instr, start_pc, sz);
            instr_set_rip_rel_pos(instr, rip_rel_pos);
            return (pc + 2);
        }
        /* otherwise it wasn't a funny 8-bit cbr so continue */
    }

    if (byte0 == 0x9a) { /* check for far-absolute calls */
        /* we need prefix info, this is rare so we do a full decode
         */
        if (decode(dcontext, start_pc, instr) == NULL)
            return NULL;
        else
            return (start_pc + sz);
    }

    /* both win32 and linux want to know about interrupts */
    if (byte0 == 0xcd) { /* int */
        instr_set_opcode(instr, OP_int);
        instr_set_num_opnds(dcontext, instr, 2, 2);
        instr_set_dst(instr, 0, opnd_create_reg(REG_XSP));
        instr_set_dst(instr, 1, opnd_create_base_disp(REG_XSP, REG_NULL, 0, 0, OPSZ_4));
        instr_set_src(instr, 0, opnd_create_immed_int((sbyte)byte1, OPSZ_1));
        instr_set_src(instr, 1, opnd_create_reg(REG_XSP));
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (pc + 2);
    }
    /* sys{enter,exit,call,ret} */
    if (byte0 == 0x0f &&
        (byte1 == 0x34 || byte1 == 0x35 || byte1 == 0x05 || byte1 == 0x07)) {
        if (byte1 == 0x34) {
            instr_set_opcode(instr, OP_sysenter);
            instr_set_num_opnds(dcontext, instr, 1, 0);
            instr_set_dst(instr, 0, opnd_create_reg(REG_XSP));
        } else if (byte1 == 0x35) {
            instr_set_opcode(instr, OP_sysexit);
            instr_set_num_opnds(dcontext, instr, 1, 0);
            instr_set_dst(instr, 0, opnd_create_reg(REG_XSP));
        } else if (byte1 == 0x05) {
            instr_set_opcode(instr, OP_syscall);
            instr_set_num_opnds(dcontext, instr, 1, 0);
            instr_set_dst(instr, 0, opnd_create_reg(REG_XCX));
        } else if (byte1 == 0x07) {
            instr_set_opcode(instr, OP_sysret);
            instr_set_num_opnds(dcontext, instr, 0, 0);
        }
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (pc + 2);
    }
    /* iret */
    if (byte0 == 0xcf) {
        instr_set_opcode(instr, OP_iret);
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (pc + 1);
    }
    /* popf */
    if (byte0 == 0x9d) {
        reg_id_t stack_sized_reg = REG_XSP;
#ifdef X64
        if (dr_get_isa_mode(dcontext) == DR_ISA_IA32) {
            stack_sized_reg = REG_ESP;
        }
#endif
        instr_set_opcode(instr, OP_popf);
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_num_opnds(dcontext, instr, 1, 2);
        instr_set_src(instr, 0, opnd_create_reg(stack_sized_reg));
        instr_set_src(
            instr, 1,
            opnd_create_base_disp(
                stack_sized_reg, REG_NULL, 0, 0,
                resolve_variable_size_dc(dcontext, prefixes, OPSZ_VARSTACK, false)));
        instr_set_dst(instr, 0, opnd_create_reg(stack_sized_reg));
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (pc + 1);
    }

#ifdef UNIX
    /* mov_seg instruction detection for i#107: mangling seg update/query. */
    if (INTERNAL_OPTION(mangle_app_seg) && (byte0 == 0x8c || byte0 == 0x8e)) {
        instr_set_opcode(instr, OP_mov_seg);
        instr_set_raw_bits(instr, start_pc, sz);
        instr_set_rip_rel_pos(instr, rip_rel_pos);
        return (start_pc + sz);
    }
#endif

    /* i#698: we must intercept floating point instruction pointer saves.
     * Rare enough that we do a full decode on an opcode match.
     */
    if (intercept_fip_save(pc, byte0, byte1)) {
        if (decode(dcontext, start_pc, instr) == NULL)
            return NULL;
        else
            return (start_pc + sz);
    }

    /* all non-pc-relative instructions */
    /* assumption: opcode already OP_UNDECODED */
    instr_set_raw_bits(instr, start_pc, sz);
    instr_set_rip_rel_pos(instr, rip_rel_pos);
    /* assumption: operands are already marked invalid (instr was reset) */
    return (start_pc + sz);
}

/* Returns a pointer to the pc of the next instruction
 * Returns NULL on decoding an invalid instruction.
 */
byte *
decode_next_pc(void *drcontext, byte *pc)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    int sz = decode_sizeof(dcontext, pc, NULL _IF_X64(NULL));
    if (sz == 0)
        return NULL;
    else
        return pc + sz;
}

/* Decodes the size of the instruction at address pc and points instr
 * at the raw bits for the instruction.
 * This corresponds to a Level 1 decoding.
 * Assumes that instr is already initialized, but uses the x86/x64 mode
 * for the current thread rather than that set in instr.
 * If caller is re-using same instr struct over multiple decodings,
 * should call instr_reset or instr_reuse.
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instr and sets opcode to OP_INVALID.
 */
byte *
decode_raw(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    int sz = decode_sizeof(dcontext, pc, NULL _IF_X64(NULL));
    IF_X64(instr_set_x86_mode(instr, get_x86_mode(dcontext)));
    if (sz == 0) {
        /* invalid instruction! */
        instr_set_opcode(instr, OP_INVALID);
        return NULL;
    }
    instr_set_opcode(instr, OP_UNDECODED);
    instr_set_raw_bits(instr, pc, sz);
    /* assumption: operands are already marked invalid (instr was reset) */
    return (pc + sz);
}
