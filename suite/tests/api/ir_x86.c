/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
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

/* Define DR_FAST_IR to verify that everything compiles when we call the inline
 * versions of these routines.
 */
#ifndef STANDALONE_DECODER
#    define DR_FAST_IR 1
#endif

/* Uses the DR API, using DR as a standalone library, rather than
 * being a client library working with DR on a target program.
 *
 * To run, you need to put dynamorio.dll into either the current directory
 * or system32.
 */

#ifndef USE_DYNAMO
#    error NEED USE_DYNAMO
#endif

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

#ifdef WINDOWS
#    define _USE_MATH_DEFINES 1
#    include <math.h> /* for M_PI, M_LN2, and M_LN10 for OP_fldpi, etc. */
#endif

#if defined(DEBUG) && defined(BUILD_TESTS)
/* Not in the headers because it is not generally exported. */
extern byte *
decode_cti(void *dcontext, byte *pc, instr_t *instr);
#endif

#define VERBOSE 0

#ifdef STANDALONE_DECODER
#    define ASSERT(x)                                                              \
        ((void)((!(x)) ? (fprintf(stderr, "ASSERT FAILURE: %s:%d: %s\n", __FILE__, \
                                  __LINE__, #x),                                   \
                          abort(), 0)                                              \
                       : 0))
#else
#    define ASSERT(x)                                                                 \
        ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s\n", __FILE__, \
                                     __LINE__, #x),                                   \
                          dr_abort(), 0)                                              \
                       : 0))
#endif

#define BOOLS_MATCH(b1, b2) (!!(b1) == !!(b2))

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))

static byte buf[32768];

#define DEFAULT_DISP 0x37
#define EVEX_SCALABLE_DISP 0x100
static int memarg_disp = DEFAULT_DISP;

/***************************************************************************
 * make sure the following are consistent (though they could still all be wrong :))
 * with respect to instr length and opcode:
 * - decode_fast
 * - decode
 * - INSTR_CREATE_
 * - encode
 */

/* we split testing up to avoid VS2010 from taking 25 minutes to compile
 * this file.
 * we cannot pass on variadic args as separate args to another
 * macro, so we must split ours by # args (xref PR 208603).
 */

/* we can encode+fast-decode some instrs cross-platform but we
 * leave that testing to the regression run on that platform
 */

/* these are shared among all test_all_opcodes_*() routines: */
#define MEMARG(sz) (opnd_create_base_disp(DR_REG_XCX, DR_REG_NULL, 0, memarg_disp, sz))
#define IMMARG(sz) opnd_create_immed_int(37, sz)
#define TGTARG opnd_create_instr(instrlist_last(ilist))
#define REGARG(reg) opnd_create_reg(DR_REG_##reg)
#define REGARG_PARTIAL(reg, sz) opnd_create_reg_partial(DR_REG_##reg, sz)
#define VSIBX6(sz) (opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM6, 2, 0x42, sz))
#define VSIBY6(sz) (opnd_create_base_disp(DR_REG_XDX, DR_REG_YMM6, 2, 0x17, sz))
#define VSIBZ6(sz) (opnd_create_base_disp(DR_REG_XDX, DR_REG_ZMM6, 2, 0x35, sz))
#define VSIBX15(sz) (opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM15, 2, 0x42, sz))
#define VSIBY15(sz) (opnd_create_base_disp(DR_REG_XDX, DR_REG_YMM15, 2, 0x17, sz))
#define VSIBZ31(sz) (opnd_create_base_disp(DR_REG_XDX, DR_REG_ZMM31, 2, 0x35, sz))

#define X86_ONLY 1
#define X64_ONLY 2
#define VERIFY_EVEX 4
#define FIRST_EVEX_BYTE 0x62

/****************************************************************************
 * OPCODE_FOR_CREATE 0 args
 */

#define OPCODE_FOR_CREATE(name, opc, icnm, flags)                 \
    do {                                                          \
        if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {     \
            instrlist_append(ilist, INSTR_CREATE_##icnm(dc));     \
            len_##name = instr_length(dc, instrlist_last(ilist)); \
        }                                                         \
    } while (0);
#define XOPCODE_FOR_CREATE(name, opc, icnm, flags)                \
    do {                                                          \
        if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {     \
            instrlist_append(ilist, XINST_CREATE_##icnm(dc));     \
            len_##name = instr_length(dc, instrlist_last(ilist)); \
        }                                                         \
    } while (0);

static void
test_all_opcodes_0(void *dc)
{
#define INCLUDE_NAME "ir_x86_0args.h"
#include "ir_x86_all_opc.h"
#undef INCLUDE_NAME
}

#undef OPCODE_FOR_CREATE
#undef XOPCODE_FOR_CREATE

/****************************************************************************
 * OPCODE_FOR_CREATE 1 arg
 */

#define OPCODE_FOR_CREATE(name, opc, icnm, flags, arg1)             \
    do {                                                            \
        if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {       \
            instrlist_append(ilist, INSTR_CREATE_##icnm(dc, arg1)); \
            len_##name = instr_length(dc, instrlist_last(ilist));   \
        }                                                           \
    } while (0);
#define XOPCODE_FOR_CREATE(name, opc, icnm, flags, arg1)            \
    do {                                                            \
        if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {       \
            instrlist_append(ilist, XINST_CREATE_##icnm(dc, arg1)); \
            len_##name = instr_length(dc, instrlist_last(ilist));   \
        }                                                           \
    } while (0);

#ifndef STANDALONE_DECODER
/* vs2005 cl takes many minute to compile w/ static drdecode lib
 * so we disable part of this test since for static we just want a
 * sanity check
 */
static void
test_all_opcodes_1(void *dc)
{
#    define INCLUDE_NAME "ir_x86_1args.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

#    undef OPCODE_FOR_CREATE
#    undef XOPCODE_FOR_CREATE

/****************************************************************************
 * OPCODE_FOR_CREATE 2 args
 */

#    define OPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2)             \
        do {                                                                  \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {             \
                instrlist_append(ilist, INSTR_CREATE_##icnm(dc, arg1, arg2)); \
                len_##name = instr_length(dc, instrlist_last(ilist));         \
            }                                                                 \
        } while (0);
#    define XOPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2)            \
        do {                                                                  \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {             \
                instrlist_append(ilist, XINST_CREATE_##icnm(dc, arg1, arg2)); \
                len_##name = instr_length(dc, instrlist_last(ilist));         \
            }                                                                 \
        } while (0);

static void
test_all_opcodes_2(void *dc)
{
#    define INCLUDE_NAME "ir_x86_2args.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_2_mm(void *dc)
{
#    define INCLUDE_NAME "ir_x86_2args_mm.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_2_avx512_vex(void *dc)
{
#    define INCLUDE_NAME "ir_x86_2args_avx512_vex.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_2_avx512_evex_mask(void *dc)
{
#    define INCLUDE_NAME "ir_x86_2args_avx512_evex_mask.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

/* No separate memarg_disp = EVEX_SCALABLE_DISP compressed displacement test needed. */

#    undef OPCODE_FOR_CREATE
#    undef XOPCODE_FOR_CREATE

/****************************************************************************
 * OPCODE_FOR_CREATE 2 args, evex encoding hint
 */

#    define OPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2)            \
        do {                                                                 \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {            \
                instrlist_append(                                            \
                    ilist,                                                   \
                    INSTR_ENCODING_HINT(INSTR_CREATE_##icnm(dc, arg1, arg2), \
                                        DR_ENCODING_HINT_X86_EVEX));         \
                len_##name = instr_length(dc, instrlist_last(ilist));        \
            }                                                                \
        } while (0);

static void
test_all_opcodes_2_avx512_evex(void *dc)
{
#    define INCLUDE_NAME "ir_x86_2args_avx512_evex.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_2_avx512_evex_scaled_disp8(void *dc)
{
#    define INCLUDE_NAME "ir_x86_2args_avx512_evex.h"
    memarg_disp = EVEX_SCALABLE_DISP;
#    include "ir_x86_all_opc.h"
    memarg_disp = DEFAULT_DISP;
#    undef INCLUDE_NAME
}

#    undef OPCODE_FOR_CREATE
#    undef XOPCODE_FOR_CREATE

/****************************************************************************
 * OPCODE_FOR_CREATE 3 args
 */

#    define OPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2, arg3)             \
        do {                                                                        \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {                   \
                instrlist_append(ilist, INSTR_CREATE_##icnm(dc, arg1, arg2, arg3)); \
                len_##name = instr_length(dc, instrlist_last(ilist));               \
            }                                                                       \
        } while (0);
#    define XOPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2, arg3)            \
        do {                                                                        \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {                   \
                instrlist_append(ilist, XINST_CREATE_##icnm(dc, arg1, arg2, arg3)); \
                len_##name = instr_length(dc, instrlist_last(ilist));               \
            }                                                                       \
        } while (0);

static void
test_all_opcodes_3(void *dc)
{
#    define INCLUDE_NAME "ir_x86_3args.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_3_avx(void *dc)
{
#    define INCLUDE_NAME "ir_x86_3args_avx.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_3_avx512_vex(void *dc)
{
#    define INCLUDE_NAME "ir_x86_3args_avx512_vex.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_3_avx512_evex_mask(void *dc)
{
#    define INCLUDE_NAME "ir_x86_3args_avx512_evex_mask.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_3_avx512_evex_mask_scaled_disp8(void *dc)
{
#    define INCLUDE_NAME "ir_x86_3args_avx512_evex_mask.h"
    memarg_disp = EVEX_SCALABLE_DISP;
#    include "ir_x86_all_opc.h"
    memarg_disp = DEFAULT_DISP;
#    undef INCLUDE_NAME
}

#    undef OPCODE_FOR_CREATE
#    undef XOPCODE_FOR_CREATE

/****************************************************************************
 * OPCODE_FOR_CREATE 3 args, evex encoding hint
 */

#    define OPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2, arg3)            \
        do {                                                                       \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {                  \
                instrlist_append(                                                  \
                    ilist,                                                         \
                    INSTR_ENCODING_HINT(INSTR_CREATE_##icnm(dc, arg1, arg2, arg3), \
                                        DR_ENCODING_HINT_X86_EVEX));               \
                len_##name = instr_length(dc, instrlist_last(ilist));              \
            }                                                                      \
        } while (0);

static void
test_all_opcodes_3_avx512_evex(void *dc)
{
#    define INCLUDE_NAME "ir_x86_3args_avx512_evex.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_3_avx512_evex_scaled_disp8(void *dc)
{
#    define INCLUDE_NAME "ir_x86_3args_avx512_evex.h"
    memarg_disp = EVEX_SCALABLE_DISP;
#    include "ir_x86_all_opc.h"
    memarg_disp = DEFAULT_DISP;
#    undef INCLUDE_NAME
}

#    undef OPCODE_FOR_CREATE
#    undef XOPCODE_FOR_CREATE

/****************************************************************************
 * OPCODE_FOR_CREATE 4 args, evex encoding hint
 */

#    define OPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2, arg3, arg4)            \
        do {                                                                             \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {                        \
                instrlist_append(                                                        \
                    ilist,                                                               \
                    INSTR_ENCODING_HINT(INSTR_CREATE_##icnm(dc, arg1, arg2, arg3, arg4), \
                                        DR_ENCODING_HINT_X86_EVEX));                     \
                len_##name = instr_length(dc, instrlist_last(ilist));                    \
            }                                                                            \
        } while (0);

static void
test_all_opcodes_4_avx512_evex(void *dc)
{
#    define INCLUDE_NAME "ir_x86_4args_avx512_evex.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_4_avx512_evex_scaled_disp8(void *dc)
{
#    define INCLUDE_NAME "ir_x86_4args_avx512_evex.h"
    memarg_disp = EVEX_SCALABLE_DISP;
#    include "ir_x86_all_opc.h"
    memarg_disp = DEFAULT_DISP;
#    undef INCLUDE_NAME
}

#    undef OPCODE_FOR_CREATE
#    undef XOPCODE_FOR_CREATE

/****************************************************************************
 * OPCODE_FOR_CREATE 4 args
 */

#    define OPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2, arg3, arg4)      \
        do {                                                                       \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {                  \
                instrlist_append(ilist,                                            \
                                 INSTR_CREATE_##icnm(dc, arg1, arg2, arg3, arg4)); \
                len_##name = instr_length(dc, instrlist_last(ilist));              \
            }                                                                      \
        } while (0);
#    define XOPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2, arg3, arg4)     \
        do {                                                                       \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {                  \
                instrlist_append(ilist,                                            \
                                 XINST_CREATE_##icnm(dc, arg1, arg2, arg3, arg4)); \
                len_##name = instr_length(dc, instrlist_last(ilist));              \
            }                                                                      \
        } while (0);

static void
test_all_opcodes_4(void *dc)
{
#    define INCLUDE_NAME "ir_x86_4args.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

/* Part A: Split up to avoid VS running out of memory (i#3992,i#4610).
 * (The _scaled_disp8 versions are what hit the OOM but we split this one too.)
 */
static void
test_all_opcodes_4_avx512_evex_mask_A(void *dc)
{
#    define INCLUDE_NAME "ir_x86_4args_avx512_evex_mask_A.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

/* Part B: Split up to avoid VS running out of memory (i#3992,i#4610).
 * (The _scaled_disp8 versions are what hit the OOM but we split this one too.)
 */
static void
test_all_opcodes_4_avx512_evex_mask_B(void *dc)
{
#    define INCLUDE_NAME "ir_x86_4args_avx512_evex_mask_B.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

/* Part C: Split up to avoid VS running out of memory (i#3992,i#4610).
 * (The _scaled_disp8 versions are what hit the OOM but we split this one too.)
 */
static void
test_all_opcodes_4_avx512_evex_mask_C(void *dc)
{
#    define INCLUDE_NAME "ir_x86_4args_avx512_evex_mask_C.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

/* Part A: Split up to avoid VS running out of memory (i#3992,i#4610). */
static void
test_all_opcodes_4_avx512_evex_mask_scaled_disp8_A(void *dc)
{
#    define INCLUDE_NAME "ir_x86_4args_avx512_evex_mask_A.h"
    memarg_disp = EVEX_SCALABLE_DISP;
#    include "ir_x86_all_opc.h"
    memarg_disp = DEFAULT_DISP;
#    undef INCLUDE_NAME
}

/* Part B: Split up to avoid VS running out of memory (i#3992,i#4610). */
static void
test_all_opcodes_4_avx512_evex_mask_scaled_disp8_B(void *dc)
{
#    define INCLUDE_NAME "ir_x86_4args_avx512_evex_mask_B.h"
    memarg_disp = EVEX_SCALABLE_DISP;
#    include "ir_x86_all_opc.h"
    memarg_disp = DEFAULT_DISP;
#    undef INCLUDE_NAME
}

/* Part C: Split up to avoid VS running out of memory (i#3992,i#4610). */
static void
test_all_opcodes_4_avx512_evex_mask_scaled_disp8_C(void *dc)
{
#    define INCLUDE_NAME "ir_x86_4args_avx512_evex_mask_C.h"
    memarg_disp = EVEX_SCALABLE_DISP;
#    include "ir_x86_all_opc.h"
    memarg_disp = DEFAULT_DISP;
#    undef INCLUDE_NAME
}

#    undef OPCODE_FOR_CREATE
#    undef XOPCODE_FOR_CREATE

/****************************************************************************
 * OPCODE_FOR_CREATE 5 args
 */

#    define OPCODE_FOR_CREATE(name, opc, icnm, flags, arg1, arg2, arg3, arg4, arg5)      \
        do {                                                                             \
            if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0) {                        \
                instrlist_append(ilist,                                                  \
                                 INSTR_CREATE_##icnm(dc, arg1, arg2, arg3, arg4, arg5)); \
                len_##name = instr_length(dc, instrlist_last(ilist));                    \
            }                                                                            \
        } while (0);

static void
test_all_opcodes_5_avx512_evex_mask(void *dc)
{
#    define INCLUDE_NAME "ir_x86_5args_avx512_evex_mask.h"
#    include "ir_x86_all_opc.h"
#    undef INCLUDE_NAME
}

static void
test_all_opcodes_5_avx512_evex_mask_scaled_disp8(void *dc)
{
#    define INCLUDE_NAME "ir_x86_5args_avx512_evex_mask.h"
    memarg_disp = EVEX_SCALABLE_DISP;
#    include "ir_x86_all_opc.h"
    memarg_disp = DEFAULT_DISP;
#    undef INCLUDE_NAME
}

#    undef OPCODE_FOR_CREATE

/*
 ****************************************************************************/

static void
test_opmask_disas_avx512(void *dc)
{
    /* Test AVX-512 k-registers. */
    byte *pc;
    const byte b1[] = { 0xc5, 0xf8, 0x90, 0xee };
    const byte b2[] = { 0x67, 0xc5, 0xf8, 0x90, 0x29 };
    char dbuf[512];
    int len;

    pc =
        disassemble_to_buffer(dc, (byte *)b1, (byte *)b1, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf, "kmovw  %k6 -> %k5\n") == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b2, (byte *)b2, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf,
                  IF_X64_ELSE("addr32 kmovw  (%ecx)[2byte] -> %k5\n",
                              "addr16 kmovw  (%bx,%di)[2byte] -> %k5\n")) == 0);
}

static void
test_disas_3_avx512_evex_mask(void *dc)
{
    /* Test AVX-512 k-registers. */
    byte *pc;
    const byte b1[] = { 0x62, 0xb1, 0x7c, 0x49, 0x10, 0xc8 };
    const byte b2[] = { 0x62, 0x21, 0x7c, 0x49, 0x10, 0xf8 };
    const byte b3[] = { 0x62, 0x61, 0x7c, 0x49, 0x11, 0x3c, 0x24 };
    const byte b4[] = { 0x62, 0x61, 0x7c, 0x49, 0x10, 0x3c, 0x24 };
    const byte b5[] = { 0x62, 0x61, 0x7c, 0x49, 0x10, 0xf9 };
    const byte b6[] = { 0x62, 0xf1, 0x7c, 0x49, 0x10, 0x0c, 0x24 };
    const byte b7[] = { 0x62, 0xf1, 0x7c, 0x49, 0x10, 0xc1 };
    const byte b8[] = { 0x62, 0xf1, 0x7c, 0x49, 0x11, 0x0c, 0x24 };

    char dbuf[512];
    int len;

#    ifdef X64
    pc =
        disassemble_to_buffer(dc, (byte *)b1, (byte *)b1, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf, "vmovups {%k1} %zmm16 -> %zmm1\n") == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b2, (byte *)b2, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf, "vmovups {%k1} %zmm16 -> %zmm31\n") == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b3, (byte *)b3, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf, "vmovups {%k1} %zmm31 -> (%rsp)[64byte]\n") == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b4, (byte *)b4, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf,
                  IF_X64_ELSE("vmovups {%k1} (%rsp)[64byte] -> %zmm31\n",
                              "vmovups {%k1} (%esp)[64byte] -> %zmm31\n")) == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b5, (byte *)b5, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf, "vmovups {%k1} %zmm1 -> %zmm31\n") == 0);
#    endif

    pc =
        disassemble_to_buffer(dc, (byte *)b6, (byte *)b6, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf,
                  IF_X64_ELSE("vmovups {%k1} (%rsp)[64byte] -> %zmm1\n",
                              "vmovups {%k1} (%esp)[64byte] -> %zmm1\n")) == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b7, (byte *)b7, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf, "vmovups {%k1} %zmm1 -> %zmm0\n") == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b8, (byte *)b8, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf,
                  IF_X64_ELSE("vmovups {%k1} %zmm1 -> (%rsp)[64byte]\n",
                              "vmovups {%k1} %zmm1 -> (%esp)[64byte]\n")) == 0);
}

#endif /* !STANDALONE_DECODER */

static void
test_disp_control_helper(void *dc, int disp, bool encode_zero_disp, bool force_full_disp,
                         bool disp16, uint len_expect)
{
    byte *pc;
    uint len;
    instr_t *instr = INSTR_CREATE_mov_ld(
        dc, opnd_create_reg(DR_REG_ECX),
        opnd_create_base_disp_ex(disp16 ? IF_X64_ELSE(DR_REG_EBX, DR_REG_BX) : DR_REG_XBX,
                                 DR_REG_NULL, 0, disp, OPSZ_4, encode_zero_disp,
                                 force_full_disp, disp16));
    pc = instr_encode(dc, instr, buf);
    len = (int)(pc - (byte *)buf);
#if VERBOSE
    pc = disassemble_with_info(dc, buf, STDOUT, true, true);
#endif
    ASSERT(len == len_expect);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    ASSERT(
        instr_num_srcs(instr) == 1 && opnd_is_base_disp(instr_get_src(instr, 0)) &&
        BOOLS_MATCH(encode_zero_disp,
                    opnd_is_disp_encode_zero(instr_get_src(instr, 0))) &&
        BOOLS_MATCH(force_full_disp, opnd_is_disp_force_full(instr_get_src(instr, 0))) &&
        BOOLS_MATCH(disp16, opnd_is_disp_short_addr(instr_get_src(instr, 0))));
    instr_destroy(dc, instr);
}

/* Test encode_zero_disp and force_full_disp control from case 4457 */
static void
test_disp_control(void *dc)
{
    /*
    0x004275b4   8b 0b                mov    (%ebx) -> %ecx
    0x004275b4   8b 4b 00             mov    $0x00(%ebx) -> %ecx
    0x004275b4   8b 8b 00 00 00 00    mov    $0x00000000 (%ebx) -> %ecx
    0x004275b4   8b 4b 7f             mov    $0x7f(%ebx) -> %ecx
    0x004275b4   8b 8b 7f 00 00 00    mov    $0x0000007f (%ebx) -> %ecx
    0x00430258   67 8b 4f 7f          addr16 mov    0x7f(%bx) -> %ecx
    0x00430258   67 8b 8f 7f 00       addr16 mov    0x007f(%bx) -> %ecx
    */
    test_disp_control_helper(dc, 0, false, false, false, 2);
    test_disp_control_helper(dc, 0, true, false, false, 3);
    test_disp_control_helper(dc, 0, true, true, false, 6);
    test_disp_control_helper(dc, 0x7f, false, false, false, 3);
    test_disp_control_helper(dc, 0x7f, false, true, false, 6);
    test_disp_control_helper(dc, 0x7f, false, false, true, 4);
    test_disp_control_helper(dc, 0x7f, false, true, true, IF_X64_ELSE(7, 5));
}

/* emits the instruction to buf (for tests that wish to do additional checks on
 * the output) */
static void
test_instr_encode(void *dc, instr_t *instr, uint len_expect)
{
    instr_t *decin;
    uint len;
    byte *pc = instr_encode(dc, instr, buf);
    len = (int)(pc - (byte *)buf);
#if VERBOSE
    disassemble_with_info(dc, buf, STDOUT, true, true);
#endif
    ASSERT(len == len_expect);
    decin = instr_create(dc);
    decode(dc, buf, decin);
    ASSERT(instr_same(instr, decin));
    instr_destroy(dc, instr);
    instr_destroy(dc, decin);
}

static void
test_instr_decode(void *dc, instr_t *instr, byte *bytes, uint bytes_len, bool size_match)
{
    instr_t *decin;
    if (size_match) {
        uint len;
        byte *pc = instr_encode(dc, instr, buf);
        len = (int)(pc - (byte *)buf);
#if VERBOSE
        disassemble_with_info(dc, buf, STDOUT, true, true);
#endif
        ASSERT(len == bytes_len);
        ASSERT(memcmp(buf, bytes, bytes_len) == 0);
    }
    decin = instr_create(dc);
    decode(dc, bytes, decin);
#if VERBOSE
    print("Comparing |");
    instr_disassemble(dc, instr, STDOUT);
    print("|\n       to |");
    instr_disassemble(dc, decin, STDOUT);
    print("|\n");
#endif
    ASSERT(instr_same(instr, decin));
    instr_destroy(dc, instr);
    instr_destroy(dc, decin);
}

/* emits the instruction to buf (for tests that wish to do additional checks on
 * the output) */
static void
test_instr_encode_and_decode(void *dc, instr_t *instr, uint len_expect,
                             /* also checks one operand's size */
                             bool src, uint opnum, opnd_size_t sz, uint bytes)
{
    opnd_t op;
    opnd_size_t opsz;
    instr_t *decin;
    uint len;
    byte *pc = instr_encode(dc, instr, buf);
    len = (int)(pc - (byte *)buf);
#if VERBOSE
    disassemble_with_info(dc, buf, STDOUT, true, true);
#endif
    ASSERT(len == len_expect);
    decin = instr_create(dc);
    decode(dc, buf, decin);
    ASSERT(instr_same(instr, decin));

    /* PR 245805: variable sizes should be resolved on decode */
    if (src)
        op = instr_get_src(decin, opnum);
    else
        op = instr_get_dst(decin, opnum);
    opsz = opnd_get_size(op);
    ASSERT(opsz == sz && opnd_size_in_bytes(opsz) == bytes);

    instr_destroy(dc, instr);
    instr_destroy(dc, decin);
}

static void
test_indirect_cti(void *dc)
{
    /*
    0x004275f4   ff d1                call   %ecx %esp -> %esp (%esp)
    0x004275f4   66 ff d1             data16 call   %cx %esp -> %esp (%esp)
    0x004275f4   67 ff d1             addr16 call   %ecx %esp -> %esp (%esp)
    0x00427794   ff 19                lcall  (%ecx) %esp -> %esp (%esp)
    0x00427794   66 ff 19             data16 lcall  (%ecx) %esp -> %esp (%esp)
    0x00427794   67 ff 1f             addr16 lcall  (%bx) %esp -> %esp (%esp)
    */
    instr_t *instr;
    byte bytes_addr16_call[] = { 0x67, 0xff, 0xd1 };
    instr = INSTR_CREATE_call_ind(dc, opnd_create_reg(DR_REG_XCX));
    test_instr_encode(dc, instr, 2);
#ifndef X64 /* only on AMD can we shorten, so we don't test it */
    instr = instr_create_2dst_2src(
        dc, OP_call_ind, opnd_create_reg(DR_REG_XSP),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, -2, OPSZ_2),
        opnd_create_reg(DR_REG_CX), opnd_create_reg(DR_REG_XSP));
    test_instr_encode(dc, instr, 3);
#endif
    /* addr16 prefix does nothing here */
    instr = INSTR_CREATE_call_ind(dc, opnd_create_reg(DR_REG_XCX));
    test_instr_decode(dc, instr, bytes_addr16_call, sizeof(bytes_addr16_call), false);

    /* invalid to have far call go through reg since needs 6 bytes */
    instr = INSTR_CREATE_call_far_ind(
        dc, opnd_create_base_disp(DR_REG_XCX, DR_REG_NULL, 0, 0, OPSZ_6));
    test_instr_encode(dc, instr, 2);
    instr = instr_create_2dst_2src(
        dc, OP_call_far_ind, opnd_create_reg(DR_REG_XSP),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, -4, OPSZ_4),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_NULL, 0, 0, OPSZ_4),
        opnd_create_reg(DR_REG_XSP));
    test_instr_encode(dc, instr, 3);
    instr = instr_create_2dst_2src(
        dc, OP_call_far_ind, opnd_create_reg(DR_REG_XSP),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, -8, OPSZ_8_rex16_short4),
        opnd_create_base_disp(IF_X64_ELSE(DR_REG_EBX, DR_REG_BX), DR_REG_NULL, 0, 0,
                              OPSZ_6),
        opnd_create_reg(DR_REG_XSP));
    test_instr_encode(dc, instr, 3);

    /* case 10710: make sure we can encode these guys
         0x00428844   0e                   push   %cs %esp -> %esp (%esp)
         0x00428844   1e                   push   %ds %esp -> %esp (%esp)
         0x00428844   16                   push   %ss %esp -> %esp (%esp)
         0x00428844   06                   push   %es %esp -> %esp (%esp)
         0x00428844   0f a0                push   %fs %esp -> %esp (%esp)
         0x00428844   0f a8                push   %gs %esp -> %esp (%esp)
         0x00428844   1f                   pop    %esp (%esp) -> %ds %esp
         0x00428844   17                   pop    %esp (%esp) -> %ss %esp
         0x00428844   07                   pop    %esp (%esp) -> %es %esp
         0x00428844   0f a1                pop    %esp (%esp) -> %fs %esp
         0x00428844   0f a9                pop    %esp (%esp) -> %gs %esp
     */
#ifndef X64
    test_instr_encode(dc, INSTR_CREATE_push(dc, opnd_create_reg(SEG_CS)), 1);
    test_instr_encode(dc, INSTR_CREATE_push(dc, opnd_create_reg(SEG_DS)), 1);
    test_instr_encode(dc, INSTR_CREATE_push(dc, opnd_create_reg(SEG_SS)), 1);
    test_instr_encode(dc, INSTR_CREATE_push(dc, opnd_create_reg(SEG_ES)), 1);
#endif
    test_instr_encode(dc, INSTR_CREATE_push(dc, opnd_create_reg(SEG_FS)), 2);
    test_instr_encode(dc, INSTR_CREATE_push(dc, opnd_create_reg(SEG_GS)), 2);
#ifndef X64
    test_instr_encode(dc, INSTR_CREATE_pop(dc, opnd_create_reg(SEG_DS)), 1);
    test_instr_encode(dc, INSTR_CREATE_pop(dc, opnd_create_reg(SEG_SS)), 1);
    test_instr_encode(dc, INSTR_CREATE_pop(dc, opnd_create_reg(SEG_ES)), 1);
#endif
    test_instr_encode(dc, INSTR_CREATE_pop(dc, opnd_create_reg(SEG_FS)), 2);
    test_instr_encode(dc, INSTR_CREATE_pop(dc, opnd_create_reg(SEG_GS)), 2);
}

static void
test_cti_prefixes(void *dc)
{
    /* case 10689: test decoding jmp/call w/ 16-bit prefixes
     *   0x00428844   66 e9 ab cd          data16 jmp    $0x55f3
     *   0x00428844   67 e9 ab cd ef 12    addr16 jmp    $0x133255f5
     */
    buf[0] = 0x66;
    buf[1] = 0xe9;
    buf[2] = 0xab;
    buf[3] = 0xcd;
    buf[4] = 0xef;
    buf[5] = 0x12;
    /* data16 (0x66) == 4 bytes, while addr16 (0x67) == 6 bytes */
#ifndef X64 /* no jmp16 for x64 */
#    if VERBOSE
    disassemble_with_info(dc, buf, STDOUT, true, true);
#    endif
    ASSERT(decode_next_pc(dc, buf) == (byte *)&buf[4]);
#endif
    buf[0] = 0x67;
#if VERBOSE
    disassemble_with_info(dc, buf, STDOUT, true, true);
#endif
    ASSERT(decode_next_pc(dc, buf) == (byte *)&buf[6]);

#if defined(DEBUG) && defined(BUILD_TESTS)
    /* Issue 4652, a c5 vex encoding where the second byte is c4
     *   c5 c4 54 2d 68 1d 0c 0a vandps 0xa0c1d68(%rip),%ymm7,%ymm5
     */
    buf[0] = 0xc5;
    buf[1] = 0xc4;
    buf[2] = 0x54;
    buf[3] = 0x2d;
    buf[4] = 0x68;
    buf[5] = 0x1d;
    buf[6] = 0x0c;
    buf[7] = 0x0a;

#    if VERBOSE
    disassemble_with_info(dc, buf, STDOUT, true, true);
#    endif
    instr_t instr;

    instr_init(dc, &instr);

    ASSERT(decode_cti(dc, buf, &instr) == (byte *)&buf[8]);

    instr_free(dc, &instr);
#endif
}

static void
test_cti_predicate(void *dc, byte *data, uint len, int opcode, dr_pred_type_t pred)
{
    instr_t *instr = instr_create(dc);
    byte *end = decode(dc, data, instr);
    ASSERT(end == data + len);
    ASSERT(instr_get_opcode(instr) == opcode);
    ASSERT(instr_is_predicated(instr));
    ASSERT(instr_get_predicate(instr) == pred);
    instr_destroy(dc, instr);
}

static void
test_cti_predicates(void *dc)
{
    byte data[][16] = {
        // 7e 10                jle    10
        { 0x7e, 0x10 },
        // 0f 84 99 00 00 00    je     403e08
        { 0x0f, 0x84, 0x99, 0x00, 0x00, 0x00 },
        // 0f 44 c2             cmovbe %edx,%eax
        { 0x0f, 0x46, 0xc2 },
    };
    test_cti_predicate(dc, data[0], 2, OP_jle_short, DR_PRED_LE);
    test_cti_predicate(dc, data[1], 6, OP_je, DR_PRED_EQ);
    test_cti_predicate(dc, data[2], 3, OP_cmovbe, DR_PRED_BE);
}

static void
test_modrm16_helper(void *dc, reg_id_t base, reg_id_t scale, uint disp, uint len)
{
    instr_t *instr;
    /* Avoid DR_REG_EAX b/c of the special 0xa0-0xa3 opcodes */
    instr = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_EBX),
                                opnd_create_base_disp(base, scale,
                                                      (scale == DR_REG_NULL ? 0 : 1),
                                                      /* we need OPSZ_4_short2 to match
                                                       * instr_same on decode! */
                                                      disp, OPSZ_4_short2));
    if (base == DR_REG_NULL && scale == DR_REG_NULL) {
        /* Don't need _ex unless abs addr, in which case should get 32-bit
         * disp!  Test both sides. */
        test_instr_encode(dc, instr, len + 1 /*32-bit disp but no prefix*/);
        instr = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_EBX),
            opnd_create_base_disp_ex(base, scale, (scale == DR_REG_NULL ? 0 : 1),
                                     /* we need OPSZ_4_short2 to match
                                      * instr_same on decode! */
                                     disp, OPSZ_4_short2, false, false, true));
        test_instr_encode(dc, instr, len);
    } else {
        test_instr_encode(dc, instr, len);
    }
}

static void
test_modrm16(void *dc)
{
    /*
     *   0x00428964   67 8b 18             addr16 mov    (%bx,%si,1) -> %ebx
     *   0x00428964   67 8b 19             addr16 mov    (%bx,%di,1) -> %ebx
     *   0x00428964   67 8b 1a             addr16 mov    (%bp,%si,1) -> %ebx
     *   0x00428964   67 8b 1b             addr16 mov    (%bp,%di,1) -> %ebx
     *   0x00428964   67 8b 1c             addr16 mov    (%si) -> %ebx
     *   0x00428964   67 8b 1d             addr16 mov    (%di) -> %ebx
     *   0x004289c4   8b 1d 7f 00 00 00    mov    0x7f -> %ebx
     *   0x004289c4   67 8b 1e 7f 00       addr16 mov    0x7f -> %ebx
     *   0x004289c4   67 8b 5e 00          addr16 mov    (%bp) -> %ebx
     *   0x004289c4   67 8b 1f             addr16 mov    (%bx) -> %ebx
     *   0x004289c4   67 8b 58 7f          addr16 mov    0x7f(%bx,%si,1) -> %ebx
     *   0x004289c4   67 8b 59 7f          addr16 mov    0x7f(%bx,%di,1) -> %ebx
     *   0x004289c4   67 8b 5a 7f          addr16 mov    0x7f(%bp,%si,1) -> %ebx
     *   0x004289c4   67 8b 5b 7f          addr16 mov    0x7f(%bp,%di,1) -> %ebx
     *   0x004289c4   67 8b 5c 7f          addr16 mov    0x7f(%si) -> %ebx
     *   0x004289c4   67 8b 5d 7f          addr16 mov    0x7f(%di) -> %ebx
     *   0x004289c4   67 8b 5e 7f          addr16 mov    0x7f(%bp) -> %ebx
     *   0x004289c4   67 8b 5f 7f          addr16 mov    0x7f(%bx) -> %ebx
     *   0x004289c4   67 8b 98 80 00       addr16 mov    0x0080(%bx,%si,1) -> %ebx
     *   0x004289c4   67 8b 99 80 00       addr16 mov    0x0080(%bx,%di,1) -> %ebx
     *   0x004289c4   67 8b 9a 80 00       addr16 mov    0x0080(%bp,%si,1) -> %ebx
     *   0x004289c4   67 8b 9b 80 00       addr16 mov    0x0080(%bp,%di,1) -> %ebx
     *   0x004289c4   67 8b 9c 80 00       addr16 mov    0x0080(%si) -> %ebx
     *   0x004289c4   67 8b 9d 80 00       addr16 mov    0x0080(%di) -> %ebx
     *   0x004289c4   67 8b 9e 80 00       addr16 mov    0x0080(%bp) -> %ebx
     *   0x004289c4   67 8b 9f 80 00       addr16 mov    0x0080(%bx) -> %ebx
     */
    test_modrm16_helper(dc, DR_REG_BX, DR_REG_SI, 0, 3);
    test_modrm16_helper(dc, DR_REG_BX, DR_REG_DI, 0, 3);
    test_modrm16_helper(dc, DR_REG_BP, DR_REG_SI, 0, 3);
    test_modrm16_helper(dc, DR_REG_BP, DR_REG_DI, 0, 3);
    test_modrm16_helper(dc, DR_REG_SI, DR_REG_NULL, 0, 3);
    test_modrm16_helper(dc, DR_REG_DI, DR_REG_NULL, 0, 3);
    test_modrm16_helper(dc, DR_REG_NULL, DR_REG_NULL, 0x7f, 5); /* must do disp16 */
    test_modrm16_helper(dc, DR_REG_BP, DR_REG_NULL, 0, 4);      /* must do disp8 */
    test_modrm16_helper(dc, DR_REG_BX, DR_REG_NULL, 0, 3);

    test_modrm16_helper(dc, DR_REG_BX, DR_REG_SI, 0x7f, 4);
    test_modrm16_helper(dc, DR_REG_BX, DR_REG_DI, 0x7f, 4);
    test_modrm16_helper(dc, DR_REG_BP, DR_REG_SI, 0x7f, 4);
    test_modrm16_helper(dc, DR_REG_BP, DR_REG_DI, 0x7f, 4);
    test_modrm16_helper(dc, DR_REG_SI, DR_REG_NULL, 0x7f, 4);
    test_modrm16_helper(dc, DR_REG_DI, DR_REG_NULL, 0x7f, 4);
    test_modrm16_helper(dc, DR_REG_BP, DR_REG_NULL, 0x7f, 4);
    test_modrm16_helper(dc, DR_REG_BX, DR_REG_NULL, 0x7f, 4);

    test_modrm16_helper(dc, DR_REG_BX, DR_REG_SI, 0x80, 5);
    test_modrm16_helper(dc, DR_REG_BX, DR_REG_DI, 0x80, 5);
    test_modrm16_helper(dc, DR_REG_BP, DR_REG_SI, 0x80, 5);
    test_modrm16_helper(dc, DR_REG_BP, DR_REG_DI, 0x80, 5);
    test_modrm16_helper(dc, DR_REG_SI, DR_REG_NULL, 0x80, 5);
    test_modrm16_helper(dc, DR_REG_DI, DR_REG_NULL, 0x80, 5);
    test_modrm16_helper(dc, DR_REG_BP, DR_REG_NULL, 0x80, 5);
    test_modrm16_helper(dc, DR_REG_BX, DR_REG_NULL, 0x80, 5);
}

/* Just check that decoding fails silently on invalid data. */
static void
test_modrm_invalid(void *dc)
{
/* Decoding succeeds on 32-bits. */
#ifdef X64
    /* Fuzzer-generated random data (i#5320). */
    byte data[16] = { 0x62, 0x03, 0xa5, 0x62, 0x03, 0xa5 };
    instr_t *instr = instr_create(dc);
    byte *end = decode(dc, data, instr);
    ASSERT(end == NULL);
    instr_destroy(dc, instr);
#endif
}

/* PR 215143: auto-magically add size prefixes */
static void
test_size_changes(void *dc)
{
    /*
     *   0x004299d4   66 51                data16 push   %cx %esp -> %esp (%esp)
     *   0x004298a4   e3 fe                jecxz  $0x004298a4 %ecx
     *   0x004298a4   67 e3 fd             addr16 jecxz  $0x004298a4 %cx
     *   0x080a5260   67 e2 fd             addr16 loop   $0x080a5260 %cx -> %cx
     *   0x080a5260   67 e1 fd             addr16 loope  $0x080a5260 %cx -> %cx
     *   0x080a5260   67 e0 fd             addr16 loopne $0x080a5260 %cx -> %cx
     */
    instr_t *instr;
    /* addr16 doesn't affect push so we only test data16 here */
#ifndef X64 /* can only shorten on AMD */
    /* push data16 */
    instr = instr_create_2dst_2src(
        dc, OP_push, opnd_create_reg(DR_REG_XSP),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, -2, OPSZ_2),
        opnd_create_reg(DR_REG_CX), opnd_create_reg(DR_REG_XSP));
    test_instr_encode(dc, instr, 2);
#endif
    /* jecxz and jcxz */
    test_instr_encode(dc, INSTR_CREATE_jecxz(dc, opnd_create_pc(buf)), 2);
    /* test non-default count register size (requires addr prefix) */
    instr = instr_create_0dst_2src(dc, OP_jecxz, opnd_create_pc(buf),
                                   opnd_create_reg(IF_X64_ELSE(DR_REG_ECX, DR_REG_CX)));
    test_instr_encode(dc, instr, 3);
    instr = instr_create_1dst_2src(
        dc, OP_loop, opnd_create_reg(IF_X64_ELSE(DR_REG_ECX, DR_REG_CX)),
        opnd_create_pc(buf), opnd_create_reg(IF_X64_ELSE(DR_REG_ECX, DR_REG_CX)));
    test_instr_encode(dc, instr, 3);
    instr = instr_create_1dst_2src(
        dc, OP_loope, opnd_create_reg(IF_X64_ELSE(DR_REG_ECX, DR_REG_CX)),
        opnd_create_pc(buf), opnd_create_reg(IF_X64_ELSE(DR_REG_ECX, DR_REG_CX)));
    test_instr_encode(dc, instr, 3);
    instr = instr_create_1dst_2src(
        dc, OP_loopne, opnd_create_reg(IF_X64_ELSE(DR_REG_ECX, DR_REG_CX)),
        opnd_create_pc(buf), opnd_create_reg(IF_X64_ELSE(DR_REG_ECX, DR_REG_CX)));
    test_instr_encode(dc, instr, 3);

    /*
     *   0x004ee0b8   a6                   cmps   %ds:(%esi) %es:(%edi) %esi %edi -> %esi
     * %edi 0x004ee0b8   67 a6                addr16 cmps   %ds:(%si) %es:(%di) %si %di ->
     * %si %di 0x004ee0b8   66 a7                data16 cmps   %ds:(%esi) %es:(%edi) %esi
     * %edi -> %esi %edi 0x004ee0b8   d7                   xlat   %ds:(%ebx,%al,1) -> %al
     *   0x004ee0b8   67 d7                addr16 xlat   %ds:(%bx,%al,1) -> %al
     *   0x004ee0b8   0f f7 c1             maskmovq %mm0 %mm1 -> %ds:(%edi)
     *   0x004ee0b8   67 0f f7 c1          addr16 maskmovq %mm0 %mm1 -> %ds:(%di)
     *   0x004ee0b8   66 0f f7 c1          maskmovdqu %xmm0 %xmm1 -> %ds:(%edi)
     *   0x004ee0b8   67 66 0f f7 c1       addr16 maskmovdqu %xmm0 %xmm1 -> %ds:(%di)
     */
    test_instr_encode(dc, INSTR_CREATE_cmps_1(dc), 1);
    instr = instr_create_2dst_4src(
        dc, OP_cmps, opnd_create_reg(IF_X64_ELSE(DR_REG_ESI, DR_REG_SI)),
        opnd_create_reg(IF_X64_ELSE(DR_REG_EDI, DR_REG_DI)),
        opnd_create_far_base_disp(SEG_DS, IF_X64_ELSE(DR_REG_ESI, DR_REG_SI), DR_REG_NULL,
                                  0, 0, OPSZ_1),
        opnd_create_far_base_disp(SEG_ES, IF_X64_ELSE(DR_REG_EDI, DR_REG_DI), DR_REG_NULL,
                                  0, 0, OPSZ_1),
        opnd_create_reg(IF_X64_ELSE(DR_REG_ESI, DR_REG_SI)),
        opnd_create_reg(IF_X64_ELSE(DR_REG_EDI, DR_REG_DI)));
    test_instr_encode(dc, instr, 2);

    instr = instr_create_2dst_4src(
        dc, OP_cmps, opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI),
        opnd_create_far_base_disp(SEG_DS, DR_REG_XSI, DR_REG_NULL, 0, 0, OPSZ_2),
        opnd_create_far_base_disp(SEG_ES, DR_REG_XDI, DR_REG_NULL, 0, 0, OPSZ_2),
        opnd_create_reg(DR_REG_XSI), opnd_create_reg(DR_REG_XDI));
    test_instr_encode_and_decode(dc, instr, 2, true /*src*/, 0, OPSZ_2, 2);

    test_instr_encode(dc, INSTR_CREATE_xlat(dc), 1);
    instr = instr_create_1dst_1src(
        dc, OP_xlat, opnd_create_reg(DR_REG_AL),
        opnd_create_far_base_disp(SEG_DS, IF_X64_ELSE(DR_REG_EBX, DR_REG_BX), DR_REG_AL,
                                  1, 0, OPSZ_1));
    test_instr_encode(dc, instr, 2);

    instr = INSTR_CREATE_maskmovq(dc, opnd_create_reg(DR_REG_MM0),
                                  opnd_create_reg(DR_REG_MM1));
    test_instr_encode(dc, instr, 3);
    instr = INSTR_PRED(
        instr_create_1dst_2src(
            dc, OP_maskmovq,
            opnd_create_far_base_disp(SEG_DS, IF_X64_ELSE(DR_REG_EDI, DR_REG_DI),
                                      DR_REG_NULL, 0, 0, OPSZ_8),
            opnd_create_reg(DR_REG_MM0), opnd_create_reg(DR_REG_MM1)),
        DR_PRED_COMPLEX);
    test_instr_encode(dc, instr, 4);

    instr = INSTR_CREATE_maskmovdqu(dc, opnd_create_reg(DR_REG_XMM0),
                                    opnd_create_reg(DR_REG_XMM1));
    test_instr_encode(dc, instr, 4);
    instr = INSTR_PRED(
        instr_create_1dst_2src(
            dc, OP_maskmovdqu,
            opnd_create_far_base_disp(SEG_DS, IF_X64_ELSE(DR_REG_EDI, DR_REG_DI),
                                      DR_REG_NULL, 0, 0, OPSZ_16),
            opnd_create_reg(DR_REG_XMM0), opnd_create_reg(DR_REG_XMM1)),
        DR_PRED_COMPLEX);
    test_instr_encode(dc, instr, 5);

    /* Test iretw, iretd, iretq (unlike most stack operation iretd (and lretd on AMD)
     * exist and are the default in 64-bit mode. As such, it has a different size/type
     * then most other stack operations).  Our instr_create routine should match stack
     * (iretq on 64-bit, iretd on 32-bit). See PR 191977. */
    instr = INSTR_CREATE_iret(dc);
#ifdef X64
    test_instr_encode_and_decode(dc, instr, 2, true /*src*/, 1, OPSZ_40, 40);
    ASSERT(buf[0] == 0x48); /* check for rex.w prefix */
#else
    test_instr_encode_and_decode(dc, instr, 1, true /*src*/, 1, OPSZ_12, 12);
#endif
    instr = instr_create_1dst_2src(
        dc, OP_iret, opnd_create_reg(DR_REG_XSP), opnd_create_reg(DR_REG_XSP),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_12));
    test_instr_encode_and_decode(dc, instr, 1, true /*src*/, 1, OPSZ_12, 12);
    instr = instr_create_1dst_2src(
        dc, OP_iret, opnd_create_reg(DR_REG_XSP), opnd_create_reg(DR_REG_XSP),
        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_6));
    test_instr_encode_and_decode(dc, instr, 2, true /*src*/, 1, OPSZ_6, 6);
    ASSERT(buf[0] == 0x66); /* check for data prefix */

#ifdef X64
    /* i#5442 */
    byte bytes_xchg_ax_r8w[] = { 0x66, 0x41, 0x90 };
    byte bytes_repne_xchg_eax_r8d[] = { 0xf2, 0x41, 0x90 };
    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_R8W), opnd_create_reg(DR_REG_AX));
    test_instr_decode(dc, instr, bytes_xchg_ax_r8w, sizeof(bytes_xchg_ax_r8w), false);

    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_R8D), opnd_create_reg(DR_REG_EAX));
    test_instr_decode(dc, instr, bytes_repne_xchg_eax_r8d,
                      sizeof(bytes_repne_xchg_eax_r8d), false);

    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_R8W), opnd_create_reg(DR_REG_AX));
    /* This really should encode to the three bytes above (see i#5446) */
    test_instr_encode_and_decode(dc, instr, 4, true /*src*/, 1, OPSZ_2, 2);
    assert(buf[0] == 0x66); /* check for data prefix */

    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_R8D));
    /* This really should encode to two bytes (see i#5446) */
    test_instr_encode_and_decode(dc, instr, 3, true /*src*/, 1, OPSZ_4, 4);
    assert(buf[0] != 0x66); /* check for no data prefix */
#endif
}

/* PR 332254: test xchg vs nop */
static void
test_nop_xchg(void *dc)
{
    /*   0x0000000000671460  87 c0                xchg   %eax %eax -> %eax %eax
     *   0x0000000000671460  48 87 c0             xchg   %rax %rax -> %rax %rax
     *   0x0000000000671460  41 87 c0             xchg   %r8d %eax -> %r8d %eax
     *   0x0000000000671460  46 90                nop
     *   0x0000000000671460  4e 90                nop
     *   0x0000000000671460  41 90                xchg   %r8d %eax -> %r8d %eax
     */
    instr_t *instr;
    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_EAX));
    test_instr_encode(dc, instr, 2);
#ifdef X64
    /* we don't do the optimal "48 90" instead of "48 87 c0" */
    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_RAX), opnd_create_reg(DR_REG_RAX));
    test_instr_encode(dc, instr, 3);
    /* we don't do the optimal "41 90" instead of "41 87 c0" */
    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_R8D), opnd_create_reg(DR_REG_EAX));
    test_instr_encode(dc, instr, 3);
    /* ensure we treat as nop and NOT xchg if doesn't have rex.b */
    buf[0] = 0x46;
    buf[1] = 0x90;
    instr = instr_create(dc);
#    if VERBOSE
    disassemble_with_info(dc, buf, STDOUT, true, true);
#    endif
    decode(dc, buf, instr);
    ASSERT(instr_get_opcode(instr) == OP_nop);
    instr_destroy(dc, instr);
    buf[0] = 0x4e;
    buf[1] = 0x90;
    instr = instr_create(dc);
#    if VERBOSE
    disassemble_with_info(dc, buf, STDOUT, true, true);
#    endif
    decode(dc, buf, instr);
    ASSERT(instr_get_opcode(instr) == OP_nop);
    instr_destroy(dc, instr);
    buf[0] = 0x41;
    buf[1] = 0x90;
    instr = instr_create(dc);
#    if VERBOSE
    disassemble_with_info(dc, buf, STDOUT, true, true);
#    endif
    decode(dc, buf, instr);
    ASSERT(instr_get_opcode(instr) == OP_xchg);
    instr_destroy(dc, instr);
#endif
}

static void
test_hint_nops(void *dc)
{
    byte *pc;
    instr_t *instr;
    instr = instr_create(dc);

    /* ensure we treat as nop. */
    buf[0] = 0x0f;
    buf[1] = 0x18;
    /* nop [eax], and then ecx, edx, ebx */
    for (buf[2] = 0x38; buf[2] <= 0x3b; buf[2]++) {
        pc = decode(dc, buf, instr);
        ASSERT(instr_get_opcode(instr) == OP_nop_modrm);
        instr_reset(dc, instr);
    }

    /* other types of hintable nop [eax] */
    buf[2] = 0x00;
    for (buf[1] = 0x19; buf[1] <= 0x1f; buf[1]++) {
        /* Intel is using these encodings now for the MPX instructions bndldx and bndstx.
         */
        if (buf[1] == 0x1a || buf[1] == 0x1b)
            continue;
        pc = decode(dc, buf, instr);
        ASSERT(instr_get_opcode(instr) == OP_nop_modrm);
        instr_reset(dc, instr);
    }

    instr_destroy(dc, instr);
}

#ifdef X64
static void
test_x86_mode(void *dc)
{
    byte *pc, *end;
    instr_t *instr;

    /* create instr that looks different in x86 vs x64 */
    instr = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_RAX), OPND_CREATE_INT32(42));
    end = instr_encode(dc, instr, buf);
    ASSERT(end - buf < BUFFER_SIZE_ELEMENTS(buf));

    /* read back in */
    set_x86_mode(dc, false /*64-bit*/);
    instr_reset(dc, instr);
    pc = decode(dc, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(instr_get_opcode(instr) == OP_add);

    /* now interpret as 32-bit where rex will be an inc */
    set_x86_mode(dc, true /*32-bit*/);
    instr_reset(dc, instr);
    pc = decode(dc, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(instr_get_opcode(instr) == OP_dec);

    /* test i#352: in x86 mode, sysexit should have esp as dest, not rsp */
    set_x86_mode(dc, true /*32-bit*/);
    buf[0] = 0x0f;
    buf[1] = 0x35;
    instr_reset(dc, instr);
    pc = decode(dc, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(instr_get_opcode(instr) == OP_sysexit);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_ESP);

    instr_destroy(dc, instr);
    set_x86_mode(dc, false /*64-bit*/);
}

static void
test_x64_abs_addr(void *dc)
{
    /* 48 a1 ef be ad de ef be ad de    mov    0xdeadbeefdeadbeef -> %rax
     * 48 a3 ef be ad de ef be ad de    mov    %rax -> 0xdeadbeefdeadbeef
     */
    instr_t *instr;
    opnd_t abs_addr = opnd_create_abs_addr((void *)0xdeadbeefdeadbeef, OPSZ_8);

    /* movabs load */
    instr = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX), abs_addr);
    test_instr_encode(dc, instr, 10); /* REX + op + 8 */

    /* movabs store */
    instr = INSTR_CREATE_mov_st(dc, abs_addr, opnd_create_reg(DR_REG_RAX));
    test_instr_encode(dc, instr, 10); /* REX + op + 8 */
}

static void
test_x64_inc(void *dc)
{
    /* i#842: inc/dec should not be encoded as 40-4f in x64 */
    instr_t *instr;

    instr = INSTR_CREATE_inc(dc, opnd_create_reg(DR_REG_EAX));
    test_instr_encode(dc, instr, 2);
}

static void
test_avx512_vnni_encoding(void *dc)
{
    /*
     * These tests are taken from binutils-2.37.90/gas/testsuite/gas/i386/avx-vnni.{s,d}
     * Each pair below differs only in assembler syntax so we took only 3 out of the 6
     * tests below (x 4 for each opcode)
     *
     *   \mnemonic %xmm2, %xmm4, %xmm2
     *   {evex} \mnemonic %xmm2, %xmm4, %xmm2
     *
     *   {vex}  \mnemonic %xmm2, %xmm4, %xmm2
     *   {vex3} \mnemonic %xmm2, %xmm4, %xmm2
     *
     *   {vex}  \mnemonic (%ecx), %xmm4, %xmm2
     *   {vex3} \mnemonic (%ecx), %xmm4, %xmm2
     *
     * +[a-f0-9]+:  62 f2 5d 08 50 d2     vpdpbusd %xmm2,%xmm4,%xmm2
     * +[a-f0-9]+:  c4 e2 59 50 d2        \{vex\} vpdpbusd %xmm2,%xmm4,%xmm2
     * +[a-f0-9]+:  c4 e2 59 50 11        \{vex\} vpdpbusd \(%ecx\),%xmm4,%xmm2
     * +[a-f0-9]+:  62 f2 5d 08 52 d2     vpdpwssd %xmm2,%xmm4,%xmm2
     * +[a-f0-9]+:  c4 e2 59 52 d2        \{vex\} vpdpwssd %xmm2,%xmm4,%xmm2
     * +[a-f0-9]+:  c4 e2 59 52 11        \{vex\} vpdpwssd \(%ecx\),%xmm4,%xmm2
     * +[a-f0-9]+:  62 f2 5d 08 51 d2     vpdpbusds %xmm2,%xmm4,%xmm2
     * +[a-f0-9]+:  c4 e2 59 51 d2        \{vex\} vpdpbusds %xmm2,%xmm4,%xmm2
     * +[a-f0-9]+:  c4 e2 59 51 11        \{vex\} vpdpbusds \(%ecx\),%xmm4,%xmm2
     * +[a-f0-9]+:  62 f2 5d 08 53 d2     vpdpwssds %xmm2,%xmm4,%xmm2
     * +[a-f0-9]+:  c4 e2 59 53 d2        \{vex\} vpdpwssds %xmm2,%xmm4,%xmm2
     * +[a-f0-9]+:  c4 e2 59 53 11        \{vex\} vpdpwssds \(%ecx\),%xmm4,%xmm2
     */
    byte expected_output[] = {
        // turn off clang-format to keep one instruction per line
        // clang-format off
        0x62, 0xf2, 0x5d, 0x08, 0x50, 0xd2,
        0xc4, 0xe2, 0x59, 0x50, 0xd2,
        0xc4, 0xe2, 0x59, 0x50, 0x11,
        0x62, 0xf2, 0x5d, 0x08, 0x52, 0xd2,
        0xc4, 0xe2, 0x59, 0x52, 0xd2,
        0xc4, 0xe2, 0x59, 0x52, 0x11,
        0x62, 0xf2, 0x5d, 0x08, 0x51, 0xd2,
        0xc4, 0xe2, 0x59, 0x51, 0xd2,
        0xc4, 0xe2, 0x59, 0x51, 0x11,
        0x62, 0xf2, 0x5d, 0x08, 0x53, 0xd2,
        0xc4, 0xe2, 0x59, 0x53, 0xd2,
        0xc4, 0xe2, 0x59, 0x53, 0x11,
        // clang-format on
    };
    int opcodes[] = { OP_vpdpbusd, OP_vpdpwssd, OP_vpdpbusds, OP_vpdpwssds };
    byte *start = buf;

    for (int opcode_num = 0; opcode_num < sizeof(opcodes) / sizeof(int); opcode_num++) {
        instr_t *instrs[3];
        int opcode = opcodes[opcode_num];

        instrs[0] = instr_create_1dst_3src(dc, opcode, REGARG(XMM2), REGARG(K0),
                                           REGARG(XMM4), REGARG(XMM2));
        instrs[1] =
            instr_create_1dst_2src(dc, opcode, REGARG(XMM2), REGARG(XMM4), REGARG(XMM2));
        memarg_disp = 0;
        instrs[2] = instr_create_1dst_2src(dc, opcode, REGARG(XMM2), REGARG(XMM4),
                                           MEMARG(OPSZ_16));
        for (int i = 0; i < 3; i++) {
            instr_t *instr = instrs[i];
            ASSERT(instr);

            byte *stop = instr_encode(dc, instr, start);
            ASSERT(stop);
#    if VERBOSE
            for (byte *x = start; x != stop; x++) {
                fprintf(stdout, "%02x ", *x);
            }
            fprintf(stdout, "\n");
#    endif
            start = stop;
            instr_destroy(dc, instr);
        }
    }
    for (int i = 0; i < sizeof(expected_output) / sizeof(byte); i++) {
        ASSERT(expected_output[i] == buf[i]);
    }
}

static void
test_avx512_bf16_encoding(void *dc)
{
    /* These tests are taken from
     * binutils-2.37.90/gas/testsuite/gas/i386/x86-64-avx512_bf16.{s,d}
     */
    // clang-format off
    // 62 02 17 40 72 f4     vcvtne2ps2bf16 %zmm28,%zmm29,%zmm30
    // 62 22 17 47 72 b4 f5 00 00 00 10  vcvtne2ps2bf16 0x10000000\(%rbp,%r14,8\),%zmm29,%zmm30\{%k7\}
    // 62 42 17 50 72 31     vcvtne2ps2bf16 \(%r9\)\{1to16\},%zmm29,%zmm30
    // 62 62 17 40 72 71 7f  vcvtne2ps2bf16 0x1fc0\(%rcx\),%zmm29,%zmm30
    // 62 62 17 d7 72 b2 00 e0 ff ff   vcvtne2ps2bf16 -0x2000\(%rdx\)\{1to16\},%zmm29,%zmm30\{%k7\}\{z\}
    // 62 02 7e 48 72 f5     vcvtneps2bf16 %zmm29,%ymm30
    // 62 22 7e 4f 72 b4 f5 00 00 00 10  vcvtneps2bf16 0x10000000\(%rbp,%r14,8\),%ymm30\{%k7\}
    // 62 42 7e 58 72 31     vcvtneps2bf16 \(%r9\)\{1to16\},%ymm30
    // 62 62 7e 48 72 71 7f  vcvtneps2bf16 0x1fc0\(%rcx\),%ymm30
    // 62 62 7e df 72 b2 00 e0 ff ff   vcvtneps2bf16 -0x2000\(%rdx\)\{1to16\},%ymm30\{%k7\}\{z\}
    // 62 02 16 40 52 f4     vdpbf16ps %zmm28,%zmm29,%zmm30
    // 62 22 16 47 52 b4 f5 00 00 00 10  vdpbf16ps 0x10000000\(%rbp,%r14,8\),%zmm29,%zmm30\{%k7\}
    // 62 42 16 50 52 31     vdpbf16ps \(%r9\)\{1to16\},%zmm29,%zmm30
    // 62 62 16 40 52 71 7f  vdpbf16ps 0x1fc0\(%rcx\),%zmm29,%zmm30
    // 62 62 16 d7 52 b2 00 e0 ff ff   vdpbf16ps -0x2000\(%rdx\)\{1to16\},%zmm29,%zmm30\{%k7\}\{z\}
    byte out00[] = { 0x62, 0x02, 0x17, 0x40, 0x72, 0xf4,}; //
    byte out01[] = { 0x62, 0x22, 0x17, 0x47, 0x72, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x10,}; //
    byte out02[] = { 0x62, 0x42, 0x17, 0x50, 0x72, 0x31,}; //
    byte out03[] = { 0x62, 0x62, 0x17, 0x40, 0x72, 0x71, 0x7f,}; //
    byte out04[] = { 0x62, 0x62, 0x17, 0xd7, 0x72, 0xb2, 0x00, 0xe0, 0xff, 0xff,}; //
    byte out05[] = { 0x62, 0x02, 0x7e, 0x48, 0x72, 0xf5,}; //
    byte out06[] = { 0x62, 0x22, 0x7e, 0x4f, 0x72, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x10,}; //
    byte out07[] = { 0x62, 0x42, 0x7e, 0x58, 0x72, 0x31,}; //
    byte out08[] = { 0x62, 0x62, 0x7e, 0x48, 0x72, 0x71, 0x7f,}; //
    byte out09[] = { 0x62, 0x62, 0x7e, 0xdf, 0x72, 0xb2, 0x00, 0xe0, 0xff, 0xff,}; //
    byte out10[] = { 0x62, 0x02, 0x16, 0x40, 0x52, 0xf4,}; //
    byte out11[] = { 0x62, 0x22, 0x16, 0x47, 0x52, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x10,}; //
    byte out12[] = { 0x62, 0x42, 0x16, 0x50, 0x52, 0x31,}; //
    byte out13[] = { 0x62, 0x62, 0x16, 0x40, 0x52, 0x71, 0x7f,}; //
    byte out14[] = { 0x62, 0x62, 0x16, 0xd7, 0x52, 0xb2, 0x00, 0xe0, 0xff, 0xff,}; //
    opnd_t test0[][4] = {
        { REGARG(ZMM30), REGARG(K0), REGARG(ZMM29), REGARG(ZMM28) },
        { REGARG(ZMM30), REGARG(K7), REGARG(ZMM29), opnd_create_base_disp(DR_REG_RBP, DR_REG_R14 , 8, 0x10000000, OPSZ_64) },
        { REGARG(ZMM30), REGARG(K0), REGARG(ZMM29), opnd_create_base_disp(DR_REG_R9 , DR_REG_NULL, 0,          0, OPSZ_4) },
        { REGARG(ZMM30), REGARG(K0), REGARG(ZMM29), opnd_create_base_disp(DR_REG_RCX, DR_REG_NULL, 0,     0x1fc0, OPSZ_64) },
        { REGARG(ZMM30), REGARG(K7), REGARG(ZMM29), opnd_create_base_disp(DR_REG_RDX, DR_REG_NULL, 0,    -0x2000, OPSZ_4) },
    };
    opnd_t test1[][3] = {
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K0), REGARG(ZMM29) },
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K7), opnd_create_base_disp(DR_REG_RBP, DR_REG_R14,  8,  0x10000000, OPSZ_64) },
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K0), opnd_create_base_disp(DR_REG_R9,  DR_REG_NULL, 0,  0,          OPSZ_4) },
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K0), opnd_create_base_disp(DR_REG_RCX, DR_REG_NULL, 0,  0x1fc0,     OPSZ_64) },
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K7), opnd_create_base_disp(DR_REG_RDX, DR_REG_NULL, 0, -0x2000,     OPSZ_4) },
    };
    // clang-format on

    // TODO: remove once these are available in some header
#    define PREFIX_EVEX_z 0x000800000
    uint prefixes[] = { 0, 0, 0, 0, PREFIX_EVEX_z };

    int expected_sizes[] = {
        sizeof(out00), sizeof(out01), sizeof(out02), sizeof(out03), sizeof(out04),
        sizeof(out05), sizeof(out06), sizeof(out07), sizeof(out08), sizeof(out09),
        sizeof(out10), sizeof(out11), sizeof(out12), sizeof(out13), sizeof(out14),
    };

    byte *expected_output[] = {
        out00, out01, out02, out03, out04, out05, out06, out07,
        out08, out09, out10, out11, out12, out13, out14,
    };

    for (int i = 0; i < 15; i++) {
        instr_t *instr;
        int set = i / 5;
        int idx = i % 5;
        if (set == 0) {
            instr = INSTR_CREATE_vcvtne2ps2bf16_mask(dc, test0[idx][0], test0[idx][1],
                                                     test0[idx][2], test0[idx][3]);
        } else if (set == 1) {
            instr = INSTR_CREATE_vcvtneps2bf16_mask(dc, test1[idx][0], test1[idx][1],
                                                    test1[idx][2]);
        } else if (set == 2) {
            instr = INSTR_CREATE_vdpbf16ps_mask(dc, test0[idx][0], test0[idx][1],
                                                test0[idx][2], test0[idx][3]);
        }
        instr_set_prefix_flag(instr, prefixes[idx]);
        test_instr_encode(dc, instr, expected_sizes[i]);
        for (int b = 0; b < expected_sizes[i]; b++) {
            ASSERT(expected_output[i][b] == buf[b]);
        }
        // instr_destroy called by test_instr_encode
    }
}

static void
test_x64_vmovq(void *dc)
{
    /* 62 61 fd 08 d6 0c 0a vmovq  %xmm25[8byte] -> (%rdx,%rcx)[8byte]
     * 62 61 fe 08 7e 0c 0a vmovq  (%rdx,%rcx)[8byte] -> %xmm25
     */
    byte *pc;
    const byte b1[] = { 0x62, 0x61, 0xfd, 0x08, 0xd6, 0x0c, 0x0a, 0x00 };
    const byte b2[] = { 0x62, 0x61, 0xfe, 0x08, 0x7e, 0x0c, 0x0a, 0x00 };
    char dbuf[512];
    int len;

    pc =
        disassemble_to_buffer(dc, (byte *)b1, (byte *)b1, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc == &b1[7]);
    ASSERT(strcmp(dbuf, "vmovq  %xmm25[8byte] -> (%rdx,%rcx)[8byte]\n") == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b2, (byte *)b2, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc == &b2[7]);
    ASSERT(strcmp(dbuf, "vmovq  (%rdx,%rcx)[8byte] -> %xmm25\n") == 0);

    const byte expected1[] = { 0x62, 0xc1, 0xfe, 0x08, 0x7e, 0x45, 0x00 };
    const byte expected2[] = { 0x62, 0xc1, 0xfd, 0x08, 0xd6, 0x45, 0x00 };

    instr_t *instr =
        INSTR_CREATE_vmovq(dc, opnd_create_reg(DR_REG_XMM16),
                           opnd_create_base_disp_ex(DR_REG_R13, DR_REG_NULL, 0, 0, OPSZ_8,
                                                    true, false, false));
    test_instr_encode(dc, instr, 7);
    ASSERT(!memcmp(expected1, buf, 7));

    instr = INSTR_CREATE_vmovq(dc,
                               opnd_create_base_disp_ex(DR_REG_R13, DR_REG_NULL, 0, 0,
                                                        OPSZ_8, true, false, false),
                               opnd_create_reg_partial(DR_REG_XMM16, OPSZ_8));
    test_instr_encode(dc, instr, 7);
    ASSERT(!memcmp(expected2, buf, 7));
}
#endif

static void
test_regs(void *dc)
{
    reg_id_t reg;
    /* Various subregs of xax to OPSZ_1. */
#ifdef X64
    reg = reg_resize_to_opsz(DR_REG_RAX, OPSZ_1);
    ASSERT(reg == DR_REG_AL);
#endif
    reg = reg_resize_to_opsz(DR_REG_EAX, OPSZ_1);
    ASSERT(reg == DR_REG_AL);
    reg = reg_resize_to_opsz(DR_REG_AX, OPSZ_1);
    ASSERT(reg == DR_REG_AL);
    reg = reg_resize_to_opsz(DR_REG_AH, OPSZ_1);
    ASSERT(reg == DR_REG_AL);
    reg = reg_resize_to_opsz(DR_REG_AL, OPSZ_1);
    ASSERT(reg == DR_REG_AL);

    /* xax to OPSZ_2 */
#ifdef X64
    reg = reg_resize_to_opsz(DR_REG_RAX, OPSZ_2);
    ASSERT(reg == DR_REG_AX);
#endif
    reg = reg_resize_to_opsz(DR_REG_EAX, OPSZ_2);
    ASSERT(reg == DR_REG_AX);
    reg = reg_resize_to_opsz(DR_REG_AX, OPSZ_2);
    ASSERT(reg == DR_REG_AX);
    reg = reg_resize_to_opsz(DR_REG_AH, OPSZ_2);
    ASSERT(reg == DR_REG_AX);
    reg = reg_resize_to_opsz(DR_REG_AL, OPSZ_2);
    ASSERT(reg == DR_REG_AX);

    /* xax to OPSZ_4 */
#ifdef X64
    reg = reg_resize_to_opsz(DR_REG_RAX, OPSZ_4);
    ASSERT(reg == DR_REG_EAX);
#endif
    reg = reg_resize_to_opsz(DR_REG_EAX, OPSZ_4);
    ASSERT(reg == DR_REG_EAX);
    reg = reg_resize_to_opsz(DR_REG_AX, OPSZ_4);
    ASSERT(reg == DR_REG_EAX);
    reg = reg_resize_to_opsz(DR_REG_AH, OPSZ_4);
    ASSERT(reg == DR_REG_EAX);
    reg = reg_resize_to_opsz(DR_REG_AL, OPSZ_4);
    ASSERT(reg == DR_REG_EAX);

#ifdef X64
    /* xax to OPSZ_8 */
    reg = reg_resize_to_opsz(DR_REG_RAX, OPSZ_8);
    ASSERT(reg == DR_REG_RAX);
    reg = reg_resize_to_opsz(DR_REG_EAX, OPSZ_8);
    ASSERT(reg == DR_REG_RAX);
    reg = reg_resize_to_opsz(DR_REG_AX, OPSZ_8);
    ASSERT(reg == DR_REG_RAX);
    reg = reg_resize_to_opsz(DR_REG_AH, OPSZ_8);
    ASSERT(reg == DR_REG_RAX);
    reg = reg_resize_to_opsz(DR_REG_AL, OPSZ_8);
    ASSERT(reg == DR_REG_RAX);
#endif

    ASSERT(reg_is_vector_simd(DR_REG_XMM0));
    ASSERT(reg_is_vector_simd(DR_REG_XMM1));
    ASSERT(reg_is_vector_simd(DR_REG_YMM1));
    ASSERT(reg_is_vector_simd(DR_REG_ZMM1));
    ASSERT(!reg_is_vector_simd(DR_REG_MM0));
    ASSERT(!reg_is_vector_simd(DR_REG_MM1));
    ASSERT(!reg_is_vector_simd(DR_REG_XAX));
    ASSERT(!reg_is_vector_simd(DR_REG_AX));

#ifdef X64
    ASSERT(reg_is_vector_simd(DR_REG_XMM31));
    ASSERT(reg_is_vector_simd(DR_REG_YMM31));
    ASSERT(reg_is_vector_simd(DR_REG_ZMM31));
#endif

    /* Quick check of other regs. */
    reg = reg_resize_to_opsz(DR_REG_XBX, OPSZ_1);
    ASSERT(reg == DR_REG_BL);
    reg = reg_resize_to_opsz(DR_REG_XCX, OPSZ_1);
    ASSERT(reg == DR_REG_CL);
    reg = reg_resize_to_opsz(DR_REG_XDX, OPSZ_1);
    ASSERT(reg == DR_REG_DL);

    /* X64 only subregs, OPSZ_1. */
    reg = reg_resize_to_opsz(DR_REG_XDI, OPSZ_1);
    ASSERT(reg == IF_X64_ELSE(DR_REG_DIL, DR_REG_NULL));
    reg = reg_resize_to_opsz(DR_REG_XSI, OPSZ_1);
    ASSERT(reg == IF_X64_ELSE(DR_REG_SIL, DR_REG_NULL));
    reg = reg_resize_to_opsz(DR_REG_XSP, OPSZ_1);
    ASSERT(reg == IF_X64_ELSE(DR_REG_SPL, DR_REG_NULL));
    reg = reg_resize_to_opsz(DR_REG_XBP, OPSZ_1);
    ASSERT(reg == IF_X64_ELSE(DR_REG_BPL, DR_REG_NULL));

    /* X64 only subregs, OPSZ_2. */
    reg = reg_resize_to_opsz(DR_REG_XDI, OPSZ_2);
    ASSERT(reg == DR_REG_DI);
    reg = reg_resize_to_opsz(DR_REG_XSI, OPSZ_2);
    ASSERT(reg == DR_REG_SI);
    reg = reg_resize_to_opsz(DR_REG_XSP, OPSZ_2);
    ASSERT(reg == DR_REG_SP);
    reg = reg_resize_to_opsz(DR_REG_XBP, OPSZ_2);
    ASSERT(reg == DR_REG_BP);

    /* SIMD only XMM, OPSZ 16. */
    reg = reg_resize_to_opsz(DR_REG_XMM0, OPSZ_16);
    ASSERT(reg == DR_REG_XMM0);
    reg = reg_resize_to_opsz(DR_REG_XMM1, OPSZ_16);
    ASSERT(reg == DR_REG_XMM1);
    reg = reg_resize_to_opsz(DR_REG_YMM0, OPSZ_16);
    ASSERT(reg == DR_REG_XMM0);
    reg = reg_resize_to_opsz(DR_REG_YMM1, OPSZ_16);
    ASSERT(reg == DR_REG_XMM1);
    reg = reg_resize_to_opsz(DR_REG_ZMM0, OPSZ_16);
    ASSERT(reg == DR_REG_XMM0);
    reg = reg_resize_to_opsz(DR_REG_ZMM1, OPSZ_16);
    ASSERT(reg == DR_REG_XMM1);

    /* SIMD only YMM, OPSZ 32. */
    reg = reg_resize_to_opsz(DR_REG_XMM0, OPSZ_32);
    ASSERT(reg == DR_REG_YMM0);
    reg = reg_resize_to_opsz(DR_REG_XMM1, OPSZ_32);
    ASSERT(reg == DR_REG_YMM1);
    reg = reg_resize_to_opsz(DR_REG_YMM0, OPSZ_32);
    ASSERT(reg == DR_REG_YMM0);
    reg = reg_resize_to_opsz(DR_REG_YMM1, OPSZ_32);
    ASSERT(reg == DR_REG_YMM1);
    reg = reg_resize_to_opsz(DR_REG_ZMM0, OPSZ_32);
    ASSERT(reg == DR_REG_YMM0);
    reg = reg_resize_to_opsz(DR_REG_ZMM1, OPSZ_32);
    ASSERT(reg == DR_REG_YMM1);

    /* SIMD only ZMM, OPSZ 64. */
    reg = reg_resize_to_opsz(DR_REG_XMM0, OPSZ_64);
    ASSERT(reg == DR_REG_ZMM0);
    reg = reg_resize_to_opsz(DR_REG_XMM1, OPSZ_64);
    ASSERT(reg == DR_REG_ZMM1);
    reg = reg_resize_to_opsz(DR_REG_YMM0, OPSZ_64);
    ASSERT(reg == DR_REG_ZMM0);
    reg = reg_resize_to_opsz(DR_REG_YMM1, OPSZ_64);
    ASSERT(reg == DR_REG_ZMM1);
    reg = reg_resize_to_opsz(DR_REG_ZMM0, OPSZ_64);
    ASSERT(reg == DR_REG_ZMM0);
    reg = reg_resize_to_opsz(DR_REG_ZMM1, OPSZ_64);
    ASSERT(reg == DR_REG_ZMM1);

    /* SIMD only ZMM, Negation, OPSZ 64. */
    reg = reg_resize_to_opsz(DR_REG_XMM0, OPSZ_64);
    ASSERT(reg != DR_REG_XMM0);
    reg = reg_resize_to_opsz(DR_REG_XMM1, OPSZ_64);
    ASSERT(reg != DR_REG_XMM1);
    reg = reg_resize_to_opsz(DR_REG_YMM0, OPSZ_64);
    ASSERT(reg != DR_REG_XMM0);
    reg = reg_resize_to_opsz(DR_REG_YMM1, OPSZ_64);
    ASSERT(reg != DR_REG_XMM1);
    reg = reg_resize_to_opsz(DR_REG_ZMM0, OPSZ_64);
    ASSERT(reg != DR_REG_XMM0);
    reg = reg_resize_to_opsz(DR_REG_ZMM1, OPSZ_64);
    ASSERT(reg != DR_REG_XMM1);
}

static void
test_instr_opnds(void *dc)
{
    /* Verbose disasm looks like this:
     * 32-bit:
     *   0x080f1ae0  ff 25 e7 1a 0f 08    jmp    0x080f1ae7
     *   0x080f1ae6  b8 ef be ad de       mov    $0xdeadbeef -> %eax
     *   0x080f1ae0  a0 e6 1a 0f 08       mov    0x080f1ae6 -> %al
     *   0x080f1ae5  b8 ef be ad de       mov    $0xdeadbeef -> %eax
     * 64-bit:
     *   0x00000000006b8de0  ff 25 02 00 00 00    jmp    <rel> 0x00000000006b8de8
     *   0x00000000006b8de6  48 b8 ef be ad de 00 mov    $0x00000000deadbeef -> %rax
     *                       00 00 00
     *   0x00000000006b8de0  8a 05 02 00 00 00    mov    <rel> 0x00000000006b8de8 -> %al
     *   0x00000000006b8de6  48 b8 ef be ad de 00 mov    $0x00000000deadbeef -> %rax
     *                       00 00 00
     */
    instrlist_t *ilist;
    instr_t *tgt, *instr;
    byte *pc;
    short disp;

    ilist = instrlist_create(dc);

    /* test mem instr as ind jmp target */
    tgt = INSTR_CREATE_mov_imm(dc, opnd_create_reg(DR_REG_XAX),
                               opnd_create_immed_int(0xdeadbeef, OPSZ_PTR));
    /* skip rex+opcode */
    disp = IF_X64_ELSE(2, 1);
    instrlist_append(
        ilist, INSTR_CREATE_jmp_ind(dc, opnd_create_mem_instr(tgt, disp, OPSZ_PTR)));
    instrlist_append(ilist, tgt);
    pc = instrlist_encode(dc, ilist, buf, true /*instr targets*/);
    ASSERT(pc != NULL);
    instrlist_clear(dc, ilist);
#if VERBOSE
    pc = disassemble_with_info(dc, buf, STDOUT, true, true);
    pc = disassemble_with_info(dc, pc, STDOUT, true, true);
#endif
    pc = buf;
    instr = instr_create(dc);
    pc = decode(dc, pc, instr);
    ASSERT(pc != NULL);
    ASSERT(instr_get_opcode(instr) == OP_jmp_ind);
#ifdef X64
    ASSERT(opnd_is_rel_addr(instr_get_src(instr, 0)));
    ASSERT(opnd_get_addr(instr_get_src(instr, 0)) == pc + disp);
#else
    ASSERT(opnd_is_base_disp(instr_get_src(instr, 0)));
    ASSERT(opnd_get_base(instr_get_src(instr, 0)) == DR_REG_NULL);
    ASSERT(opnd_get_index(instr_get_src(instr, 0)) == DR_REG_NULL);
    ASSERT(opnd_get_disp(instr_get_src(instr, 0)) == (ptr_int_t)pc + disp);
#endif

    /* test mem instr as TYPE_O */
    tgt = INSTR_CREATE_mov_imm(dc, opnd_create_reg(DR_REG_XAX),
                               opnd_create_immed_int(0xdeadbeef, OPSZ_PTR));
    /* skip rex+opcode */
    disp = IF_X64_ELSE(2, 1);
    instrlist_append(ilist,
                     INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_AL),
                                         opnd_create_mem_instr(tgt, disp, OPSZ_1)));
    instrlist_append(ilist, tgt);
    pc = instrlist_encode(dc, ilist, buf, true /*instr targets*/);
    ASSERT(pc != NULL);
    instrlist_clear(dc, ilist);
#if VERBOSE
    pc = disassemble_with_info(dc, buf, STDOUT, true, true);
    pc = disassemble_with_info(dc, pc, STDOUT, true, true);
#endif
    pc = buf;
    instr_reset(dc, instr);
    pc = decode(dc, pc, instr);
    ASSERT(pc != NULL);
    ASSERT(instr_get_opcode(instr) == OP_mov_ld);
#ifdef X64
    ASSERT(opnd_is_rel_addr(instr_get_src(instr, 0)));
    ASSERT(opnd_get_addr(instr_get_src(instr, 0)) == pc + disp);
#else
    ASSERT(opnd_is_base_disp(instr_get_src(instr, 0)));
    ASSERT(opnd_get_base(instr_get_src(instr, 0)) == DR_REG_NULL);
    ASSERT(opnd_get_index(instr_get_src(instr, 0)) == DR_REG_NULL);
    ASSERT(opnd_get_disp(instr_get_src(instr, 0)) == (ptr_int_t)pc + disp);
#endif

    instr_destroy(dc, instr);
    instrlist_clear_and_destroy(dc, ilist);
}

static void
test_strict_invalid(void *dc)
{
    instr_t instr;
    byte *pc;
    const byte buf1[] = { 0xf2, 0x0f, 0xd8, 0xe9 }; /* psubusb w/ invalid prefix */
    const byte buf2[] = { 0xc5, 0x84, 0x41, 0xd0 }; /* kandw k0, (invalid), k2 */

    instr_init(dc, &instr);

    /* The instr should be valid by default and invalid if decode_strict */
    pc = decode(dc, (byte *)buf1, &instr);
    ASSERT(pc != NULL);

    disassemble_set_syntax(DR_DISASM_STRICT_INVALID);
    instr_reset(dc, &instr);
    pc = decode(dc, (byte *)buf1, &instr);
    ASSERT(pc == NULL);

#ifdef X64
    /* The instruction should always be invalid. In 32-bit mode, the instruction will
     * decode as lds, because the very bits[7:6] of the second byte of the 2-byte VEX
     * form are used to differentiate lds from the VEX prefix 0xc5.
     */
    instr_reset(dc, &instr);
    pc = decode(dc, (byte *)buf2, &instr);
    ASSERT(pc == NULL);
#endif

    instr_free(dc, &instr);
}

static void
test_tsx(void *dc)
{
    /* Test the xacquire and xrelease prefixes */
    byte *pc;
    const byte b1[] = { 0xf3, 0xa3, 0x9a, 0x7a, 0x21, 0x02, 0xfa, 0x8c, 0xec, 0xa3 };
    const byte b2[] = { 0xf3, 0x89, 0x39 };
    const byte b3[] = { 0xf2, 0x89, 0x39 };
    const byte b4[] = { 0xf2, 0xf0, 0x00, 0x00 };
    char buf[512];
    int len;

    pc = disassemble_to_buffer(dc, (byte *)b1, (byte *)b1, false /*no pc*/,
                               false /*no bytes*/, buf, BUFFER_SIZE_ELEMENTS(buf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(buf,
                  IF_X64_ELSE("mov    %eax -> 0xa3ec8cfa02217a9a[4byte]\n",
                              "mov    %eax -> 0x02217a9a[4byte]\n")) == 0);

    pc = disassemble_to_buffer(dc, (byte *)b2, (byte *)b2, false /*no pc*/,
                               false /*no bytes*/, buf, BUFFER_SIZE_ELEMENTS(buf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(buf,
                  IF_X64_ELSE("mov    %edi -> (%rcx)[4byte]\n",
                              "mov    %edi -> (%ecx)[4byte]\n")) == 0);

    pc = disassemble_to_buffer(dc, (byte *)b3, (byte *)b3, false /*no pc*/,
                               false /*no bytes*/, buf, BUFFER_SIZE_ELEMENTS(buf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(buf,
                  IF_X64_ELSE("xacquire mov    %edi -> (%rcx)[4byte]\n",
                              "xacquire mov    %edi -> (%ecx)[4byte]\n")) == 0);

    pc = disassemble_to_buffer(dc, (byte *)b4, (byte *)b4, false /*no pc*/,
                               false /*no bytes*/, buf, BUFFER_SIZE_ELEMENTS(buf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(buf,
                  IF_X64_ELSE(
                      "xacquire lock add    %al (%rax)[1byte] -> (%rax)[1byte]\n",
                      "xacquire lock add    %al (%eax)[1byte] -> (%eax)[1byte]\n")) == 0);
}

static void
test_vsib_helper(void *dc, dr_mcontext_t *mc, instr_t *instr, reg_t base, int index_idx,
                 int scale, int disp, int count, opnd_size_t index_sz, bool is_evex,
                 bool expect_write)
{
    uint memopidx, memoppos;
    app_pc addr;
    bool write;
    for (memopidx = 0;
         instr_compute_address_ex_pos(instr, mc, memopidx, &addr, &write, &memoppos);
         memopidx++) {
        /* should be a read from 1st source */
        ptr_int_t index =
            ((index_sz == OPSZ_4) ?
                                  /* this only works w/ the mask all enabled */
                 (mc->simd[index_idx].u32[memopidx])
                                  :
#ifdef X64
                                  (mc->simd[index_idx].u64[memopidx])
#else
                                  ((((int64)mc->simd[index_idx].u32[memopidx * 2 + 1])
                                    << 32) |
                                   mc->simd[index_idx].u32[memopidx * 2])
#endif
            );
        ASSERT(write == expect_write);
        ASSERT((is_evex && !write && memoppos == 1) ||
               (is_evex && write && memoppos == 0) || memoppos == 0);
        ASSERT((ptr_int_t)addr == base + disp + scale * index);
    }
    ASSERT(memopidx == count);
}

static void
test_vsib(void *dc)
{
    dr_mcontext_t mc;
    instr_t *instr;

    /* Test VSIB addressing */
    byte *pc;
    const byte b1[] = { 0xc4, 0xe2, 0xe9, 0x90, 0x24, 0x42 };
    /* Invalid b/c modrm doesn't ask for SIB */
    const byte b2[] = { 0xc4, 0xe2, 0xe9, 0x90, 0x00 };
    char dbuf[512];
    int len;

    pc =
        disassemble_to_buffer(dc, (byte *)b1, (byte *)b1, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL);
    ASSERT(strcmp(dbuf,
                  IF_X64_ELSE(
                      "vpgatherdq (%rdx,%xmm0,2)[8byte] %xmm2 -> %xmm4 %xmm2\n",
                      "vpgatherdq (%edx,%xmm0,2)[8byte] %xmm2 -> %xmm4 %xmm2\n")) == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b2, (byte *)b2, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc == NULL);

    /* AVX VEX opcodes */

    /* Test mem addr emulation */
    mc.size = sizeof(mc);
    mc.flags = DR_MC_ALL;
    mc.xcx = 0x42;
    mc.simd[1].u32[0] = 0x11111111;
    mc.simd[1].u32[1] = 0x22222222;
    mc.simd[1].u32[2] = 0x33333333;
    mc.simd[1].u32[3] = 0x44444444;
    mc.simd[1].u32[4] = 0x12345678;
    mc.simd[1].u32[5] = 0x87654321;
    mc.simd[1].u32[6] = 0xabababab;
    mc.simd[1].u32[7] = 0xcdcdcdcd;
    /* mask */
    mc.simd[2].u32[0] = 0xf1111111;
    mc.simd[2].u32[1] = 0xf2222222;
    mc.simd[2].u32[2] = 0xf3333333;
    mc.simd[2].u32[3] = 0xf4444444;
    mc.simd[2].u32[4] = 0xf5444444;
    mc.simd[2].u32[5] = 0xf6444444;
    mc.simd[2].u32[6] = 0xf7444444;
    mc.simd[2].u32[7] = 0xf8444444;

    /* test index size 4 and mem size 8 */
    instr = INSTR_CREATE_vgatherdpd(
        dc, opnd_create_reg(DR_REG_XMM0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_8),
        opnd_create_reg(DR_REG_XMM2));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 2, OPSZ_4, false /* !evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    /* test index size 8 and mem size 4 */
    instr = INSTR_CREATE_vgatherqpd(
        dc, opnd_create_reg(DR_REG_XMM0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_8),
        opnd_create_reg(DR_REG_XMM2));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 2, OPSZ_8, false /* !evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    /* test index size 4 and mem size 4 */
    instr = INSTR_CREATE_vgatherdps(
        dc, opnd_create_reg(DR_REG_XMM0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_4),
        opnd_create_reg(DR_REG_XMM2));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 4, OPSZ_4, false /* !evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    /* test index size 8 and mem size 4 */
    instr = INSTR_CREATE_vgatherqps(
        dc, opnd_create_reg(DR_REG_XMM0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_4),
        opnd_create_reg(DR_REG_XMM2));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 2, OPSZ_8, false /* !evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    /* test 256-byte */
    instr = INSTR_CREATE_vgatherdps(
        dc, opnd_create_reg(DR_REG_YMM0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_YMM1, 2, 0x12, OPSZ_4),
        opnd_create_reg(DR_REG_YMM2));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 8, OPSZ_4, false /* !evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    /* test mask not selecting things -- in the middle complicates
     * our helper checks so we just do the ends
     */
    mc.simd[2].u32[0] = 0x71111111;
    mc.simd[2].u32[1] = 0x32222222;
    mc.simd[2].u32[2] = 0x13333333;
    mc.simd[2].u32[3] = 0x04444444;
    mc.simd[2].u32[4] = 0x65444444;
    mc.simd[2].u32[5] = 0x56444444;
    mc.simd[2].u32[6] = 0x47444444;
    mc.simd[2].u32[7] = 0x28444444;
    instr = INSTR_CREATE_vgatherdps(
        dc, opnd_create_reg(DR_REG_YMM0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_YMM1, 2, 0x12, OPSZ_4),
        opnd_create_reg(DR_REG_YMM2));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 0 /*nothing*/, OPSZ_4,
                     false /* !evex */, false /* !expect_write */);
    instr_destroy(dc, instr);

    /* AVX-512 EVEX opcodes */

    /* evex mask */
    mc.opmask[0] = 0xffff;

    /* test index size 4 and mem size 8 */
    instr = INSTR_CREATE_vgatherdpd_mask(
        dc, opnd_create_reg(DR_REG_XMM0), opnd_create_reg(DR_REG_K0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_8));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 2, OPSZ_4, true /* evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_vscatterdpd_mask(
        dc, opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_8),
        opnd_create_reg(DR_REG_K0), opnd_create_reg(DR_REG_XMM0));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 2, OPSZ_4, true /* evex */,
                     true /* expect_write */);
    instr_destroy(dc, instr);

    /* test index size 8 and mem size 4 */
    instr = INSTR_CREATE_vgatherqpd_mask(
        dc, opnd_create_reg(DR_REG_XMM0), opnd_create_reg(DR_REG_K0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_8));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 2, OPSZ_8, true /* evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_vscatterqpd_mask(
        dc, opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_8),
        opnd_create_reg(DR_REG_K0), opnd_create_reg(DR_REG_XMM0));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 2, OPSZ_8, true /* evex */,
                     true /* expect_write */);
    instr_destroy(dc, instr);

    /* test index size 4 and mem size 4 */
    instr = INSTR_CREATE_vgatherdps_mask(
        dc, opnd_create_reg(DR_REG_XMM0), opnd_create_reg(DR_REG_K0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_4));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 4, OPSZ_4, true /* evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_vscatterdps_mask(
        dc, opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_4),
        opnd_create_reg(DR_REG_K0), opnd_create_reg(DR_REG_XMM0));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 4, OPSZ_4, true /* evex */,
                     true /* expect_write */);
    instr_destroy(dc, instr);

    /* test index size 8 and mem size 4 */
    instr = INSTR_CREATE_vgatherqps_mask(
        dc, opnd_create_reg(DR_REG_XMM0), opnd_create_reg(DR_REG_K0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_4));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 2, OPSZ_8, true /* evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_vscatterqps_mask(
        dc, opnd_create_base_disp(DR_REG_XCX, DR_REG_XMM1, 2, 0x12, OPSZ_4),
        opnd_create_reg(DR_REG_K0), opnd_create_reg(DR_REG_XMM0));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 2, OPSZ_8, true /* evex */,
                     true /* expect_write */);
    instr_destroy(dc, instr);

    /* test 256-bit */
    instr = INSTR_CREATE_vgatherdps_mask(
        dc, opnd_create_reg(DR_REG_YMM0), opnd_create_reg(DR_REG_K0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_YMM1, 2, 0x12, OPSZ_4));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 8, OPSZ_4, true /* evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_vscatterdps_mask(
        dc, opnd_create_base_disp(DR_REG_XCX, DR_REG_YMM1, 2, 0x12, OPSZ_4),
        opnd_create_reg(DR_REG_K0), opnd_create_reg(DR_REG_YMM0));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 8, OPSZ_4, true /* evex */,
                     true /* expect_write */);
    instr_destroy(dc, instr);

    /* test 512-bit */
    instr = INSTR_CREATE_vgatherdps_mask(
        dc, opnd_create_reg(DR_REG_ZMM0), opnd_create_reg(DR_REG_K0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_ZMM1, 2, 0x12, OPSZ_4));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 16, OPSZ_4, true /* evex */,
                     false /* !expect_write */);
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_vscatterdps_mask(
        dc, opnd_create_base_disp(DR_REG_XCX, DR_REG_ZMM1, 2, 0x12, OPSZ_4),
        opnd_create_reg(DR_REG_K0), opnd_create_reg(DR_REG_ZMM0));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 16, OPSZ_4, true /* evex */,
                     true /* expect_write */);
    instr_destroy(dc, instr);

    /* test mask not selecting things -- in the middle complicates
     * our helper checks so we just do the ends
     */
    mc.opmask[0] = 0x0;

    instr = INSTR_CREATE_vgatherdps_mask(
        dc, opnd_create_reg(DR_REG_YMM0), opnd_create_reg(DR_REG_K0),
        opnd_create_base_disp(DR_REG_XCX, DR_REG_YMM1, 2, 0x12, OPSZ_4));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 0 /*nothing*/, OPSZ_4,
                     true /* evex */, false /* !expect_write */);
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_vscatterdps_mask(
        dc, opnd_create_base_disp(DR_REG_XCX, DR_REG_YMM1, 2, 0x12, OPSZ_4),
        opnd_create_reg(DR_REG_K0), opnd_create_reg(DR_REG_YMM0));
    test_vsib_helper(dc, &mc, instr, mc.xcx, 1, 2, 0x12, 0 /*nothing*/, OPSZ_4,
                     true /* evex */, true /* expect_write */);
    instr_destroy(dc, instr);

    /* Test invalid k0 mask with scatter/gather opcodes. */
    const byte b_scattergatherinv[] = { /* vpscatterdd %xmm0,(%rax,%xmm1,2){%k0} */
                                        0x62, 0xf2, 0x7d, 0x08, 0xa0, 0x04, 0x48,
                                        /* vpscatterdq %xmm0,(%rax,%xmm1,2){%k0} */
                                        0x62, 0xf2, 0xfd, 0x08, 0xa0, 0x04, 0x48,
                                        /* vpscatterqd %xmm0,(%rax,%xmm1,2){%k0} */
                                        0x62, 0xf2, 0x7d, 0x08, 0xa1, 0x04, 0x48,
                                        /* vpscatterqq %xmm0,(%rax,%xmm1,2){%k0} */
                                        0x62, 0xf2, 0xfd, 0x08, 0xa1, 0x04, 0x48,
                                        /* vscatterdps %xmm0,(%rax,%xmm1,2){%k0} */
                                        0x62, 0xf2, 0x7d, 0x08, 0xa2, 0x04, 0x48,
                                        /* vscatterdpd %xmm0,(%rax,%xmm1,2){%k0} */
                                        0x62, 0xf2, 0xfd, 0x08, 0xa2, 0x04, 0x48,
                                        /* vscatterqps %xmm0,(%rax,%xmm1,2){%k0} */
                                        0x62, 0xf2, 0x7d, 0x08, 0xa3, 0x04, 0x48,
                                        /* vscatterqpd %xmm0,(%rax,%xmm1,2){%k0} */
                                        0x62, 0xf2, 0xfd, 0x08, 0xa3, 0x04, 0x48,
                                        /* vpgatherdd (%rax,%xmm1,2),%xmm0{%k0} */
                                        0x62, 0xf2, 0x7d, 0x08, 0x90, 0x04, 0x48,
                                        /* vpgatherdq (%rax,%xmm1,2),%xmm0{%k0} */
                                        0x62, 0xf2, 0xfd, 0x08, 0x90, 0x04, 0x48,
                                        /* vpgatherqd (%rax,%xmm1,2),%xmm0{%k0} */
                                        0x62, 0xf2, 0x7d, 0x08, 0x91, 0x04, 0x48,
                                        /* vpgatherqq (%rax,%xmm1,2),%xmm0{%k0} */
                                        0x62, 0xf2, 0xfd, 0x08, 0x91, 0x04, 0x48,
                                        /* vgatherdps (%rax,%xmm1,2),%xmm0{%k0} */
                                        0x62, 0xf2, 0x7d, 0x08, 0x92, 0x04, 0x48,
                                        /* vgatherdpd (%rax,%xmm1,2),%xmm0{%k0} */
                                        0x62, 0xf2, 0xfd, 0x08, 0x92, 0x04, 0x48,
                                        /* vgatherqps (%rax,%xmm1,2),%xmm0{%k0} */
                                        0x62, 0xf2, 0x7d, 0x08, 0x93, 0x04, 0x48,
                                        /* vgatherqpd (%rax,%xmm1,2),%xmm0{%k0} */
                                        0x62, 0xf2, 0xfd, 0x08, 0x93, 0x04, 0x48
    };
    instr_t invinstr;
    instr_init(dc, &invinstr);
    for (int i = 0; i < sizeof(b_scattergatherinv); i += 7) {
        pc = decode(dc, (byte *)&b_scattergatherinv[i], &invinstr);
        ASSERT(pc == NULL);
    }
    instr_free(dc, &invinstr);
}

static void
test_disasm_sizes(void *dc)
{
    byte *pc;
    char dbuf[512];
    int len;

    {
        const byte b1[] = { 0xac };
        const byte b2[] = { 0xad };
        pc = disassemble_to_buffer(dc, (byte *)b1, (byte *)b1, false, false, dbuf,
                                   BUFFER_SIZE_ELEMENTS(dbuf), &len);
        ASSERT(pc != NULL);
        ASSERT(strcmp(dbuf,
                      IF_X64_ELSE("lods   %ds:(%rsi)[1byte] %rsi -> %al %rsi\n",
                                  "lods   %ds:(%esi)[1byte] %esi -> %al %esi\n")) == 0);
        pc = disassemble_to_buffer(dc, (byte *)b2, (byte *)b2, false, false, dbuf,
                                   BUFFER_SIZE_ELEMENTS(dbuf), &len);
        ASSERT(pc != NULL);
        ASSERT(strcmp(dbuf,
                      IF_X64_ELSE("lods   %ds:(%rsi)[4byte] %rsi -> %eax %rsi\n",
                                  "lods   %ds:(%esi)[4byte] %esi -> %eax %esi\n")) == 0);
    }
#ifdef X64
    {
        const byte b3[] = { 0x48, 0xad };
        pc = disassemble_to_buffer(dc, (byte *)b3, (byte *)b3, false, false, dbuf,
                                   BUFFER_SIZE_ELEMENTS(dbuf), &len);
        ASSERT(pc != NULL);
        ASSERT(strcmp(dbuf, "lods   %ds:(%rsi)[8byte] %rsi -> %rax %rsi\n") == 0);
    }
#endif

#ifdef X64
    {
        const byte b1[] = { 0xc7, 0x80, 0x90, 0xe4, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 };
        const byte b2[] = { 0x48, 0xc7, 0x80, 0x90, 0xe4, 0xff,
                            0xff, 0x00, 0x00, 0x00, 0x00 };
        pc = disassemble_to_buffer(dc, (byte *)b1, (byte *)b1, false, false, dbuf,
                                   BUFFER_SIZE_ELEMENTS(dbuf), &len);
        ASSERT(pc != NULL);
        ASSERT(strcmp(dbuf, "mov    $0x00000000 -> 0xffffe490(%rax)[4byte]\n") == 0);
        pc = disassemble_to_buffer(dc, (byte *)b2, (byte *)b2, false, false, dbuf,
                                   BUFFER_SIZE_ELEMENTS(dbuf), &len);
        ASSERT(pc != NULL);
        ASSERT(strcmp(dbuf, "mov    $0x0000000000000000 -> 0xffffe490(%rax)[8byte]\n") ==
               0);
    }
#endif
}

static void
test_predication(void *dc)
{
    byte *pc;
    uint usage;
    instr_t *instr = INSTR_CREATE_vmaskmovps(
        dc, opnd_create_reg(DR_REG_XMM0), opnd_create_reg(DR_REG_XMM1), MEMARG(OPSZ_16));
    ASSERT(instr_reads_from_reg(instr, DR_REG_XMM1, DR_QUERY_DEFAULT));
    ASSERT(instr_reads_from_reg(instr, DR_REG_XMM1, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_reads_from_reg(instr, DR_REG_XMM1, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(instr_reads_from_reg(instr, DR_REG_XMM1, 0));
    ASSERT(!instr_writes_to_reg(instr, DR_REG_XMM0, DR_QUERY_DEFAULT));
    ASSERT(instr_writes_to_reg(instr, DR_REG_XMM0, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_writes_to_reg(instr, DR_REG_XMM0, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_writes_to_reg(instr, DR_REG_XMM0, 0));
    pc = instr_encode(dc, instr, buf);
    ASSERT(pc != NULL);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    ASSERT(instr_reads_from_reg(instr, DR_REG_XMM1, DR_QUERY_DEFAULT));
    ASSERT(instr_reads_from_reg(instr, DR_REG_XMM1, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_reads_from_reg(instr, DR_REG_XMM1, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(instr_reads_from_reg(instr, DR_REG_XMM1, 0));
    ASSERT(!instr_writes_to_reg(instr, DR_REG_XMM0, DR_QUERY_DEFAULT));
    ASSERT(instr_writes_to_reg(instr, DR_REG_XMM0, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_writes_to_reg(instr, DR_REG_XMM0, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_writes_to_reg(instr, DR_REG_XMM0, 0));

    instr_destroy(dc, instr);
    instr = INSTR_CREATE_cmovcc(dc, OP_cmovnle, opnd_create_reg(DR_REG_EAX),
                                opnd_create_reg(DR_REG_ECX));
    ASSERT(instr_reads_from_reg(instr, DR_REG_ECX, DR_QUERY_DEFAULT));
    ASSERT(instr_reads_from_reg(instr, DR_REG_ECX, DR_QUERY_INCLUDE_ALL));
    ASSERT(!instr_reads_from_reg(instr, DR_REG_ECX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_reads_from_reg(instr, DR_REG_ECX, 0));
    ASSERT(!instr_writes_to_reg(instr, DR_REG_EAX, DR_QUERY_DEFAULT));
    ASSERT(instr_writes_to_reg(instr, DR_REG_EAX, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_writes_to_reg(instr, DR_REG_EAX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_writes_to_reg(instr, DR_REG_EAX, 0));
    pc = instr_encode(dc, instr, buf);
    ASSERT(pc != NULL);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    ASSERT(instr_reads_from_reg(instr, DR_REG_ECX, DR_QUERY_DEFAULT));
    ASSERT(instr_reads_from_reg(instr, DR_REG_ECX, DR_QUERY_INCLUDE_ALL));
    ASSERT(!instr_reads_from_reg(instr, DR_REG_ECX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_reads_from_reg(instr, DR_REG_ECX, 0));
    ASSERT(!instr_writes_to_reg(instr, DR_REG_EAX, DR_QUERY_DEFAULT));
    ASSERT(instr_writes_to_reg(instr, DR_REG_EAX, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_writes_to_reg(instr, DR_REG_EAX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_writes_to_reg(instr, DR_REG_EAX, 0));

    /* bsf always writes to eflags */
    instr_destroy(dc, instr);
    instr =
        INSTR_CREATE_bsf(dc, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ECX));
    ASSERT(TESTALL(EFLAGS_WRITE_6, instr_get_eflags(instr, DR_QUERY_DEFAULT)));
    ASSERT(TESTALL(EFLAGS_WRITE_6, instr_get_eflags(instr, DR_QUERY_INCLUDE_ALL)));
    ASSERT(TESTALL(EFLAGS_WRITE_6, instr_get_eflags(instr, DR_QUERY_INCLUDE_COND_DSTS)));
    ASSERT(TESTALL(EFLAGS_WRITE_6, instr_get_eflags(instr, 0)));
    pc = instr_encode(dc, instr, buf);
    ASSERT(pc != NULL);
    ASSERT(decode_eflags_usage(dc, buf, &usage, DR_QUERY_DEFAULT) != NULL &&
           TESTALL(EFLAGS_WRITE_6, usage));
    ASSERT(decode_eflags_usage(dc, buf, &usage, DR_QUERY_INCLUDE_ALL) != NULL &&
           TESTALL(EFLAGS_WRITE_6, usage));
    ASSERT(decode_eflags_usage(dc, buf, &usage, DR_QUERY_INCLUDE_COND_DSTS) != NULL &&
           TESTALL(EFLAGS_WRITE_6, usage));
    ASSERT(decode_eflags_usage(dc, buf, &usage, 0) != NULL &&
           TESTALL(EFLAGS_WRITE_6, usage));
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    ASSERT(TESTALL(EFLAGS_WRITE_6, instr_get_eflags(instr, DR_QUERY_DEFAULT)));
    ASSERT(TESTALL(EFLAGS_WRITE_6, instr_get_eflags(instr, DR_QUERY_INCLUDE_ALL)));
    ASSERT(TESTALL(EFLAGS_WRITE_6, instr_get_eflags(instr, DR_QUERY_INCLUDE_COND_DSTS)));
    ASSERT(TESTALL(EFLAGS_WRITE_6, instr_get_eflags(instr, 0)));

    instr_destroy(dc, instr);
}

/* Destroys instr when done.  Differs from test_instr_decode() by not having raw
 * bytes available.
 */
static void
test_encode_matches_decode(void *dc, instr_t *instr)
{
    byte *pc = instr_encode(dc, instr, buf);
    ASSERT(pc != NULL);
    instr_t *ins2 = instr_create(dc);
    decode(dc, buf, ins2);
    ASSERT(instr_same(instr, ins2));
    instr_destroy(dc, instr);
    instr_destroy(dc, ins2);
}

static void
test_xinst_create(void *dc)
{
    reg_id_t reg = DR_REG_XDX;
    /* load 1 byte zextend */
    test_encode_matches_decode(
        dc,
        XINST_CREATE_load_1byte_zext4(
            dc, opnd_create_reg(reg_resize_to_opsz(reg, OPSZ_4)), MEMARG(OPSZ_1)));
    /* load 1 byte */
    test_encode_matches_decode(
        dc,
        XINST_CREATE_load_1byte(dc, opnd_create_reg(reg_resize_to_opsz(reg, OPSZ_1)),
                                MEMARG(OPSZ_1)));
    /* load 2 bytes */
    test_encode_matches_decode(
        dc,
        XINST_CREATE_load_2bytes(dc, opnd_create_reg(reg_resize_to_opsz(reg, OPSZ_2)),
                                 MEMARG(OPSZ_2)));
    /* store 1 byte */
    test_encode_matches_decode(
        dc,
        XINST_CREATE_store_1byte(dc, MEMARG(OPSZ_1),
                                 opnd_create_reg(reg_resize_to_opsz(reg, OPSZ_1))));
    /* store 1 byte */
    test_encode_matches_decode(
        dc,
        XINST_CREATE_store_2bytes(dc, MEMARG(OPSZ_2),
                                  opnd_create_reg(reg_resize_to_opsz(reg, OPSZ_2))));
    /* indirect call through a register */
    test_encode_matches_decode(dc, XINST_CREATE_call_reg(dc, REGARG(XBX)));

    /* Variations of adding. */
    test_encode_matches_decode(dc, XINST_CREATE_add(dc, REGARG(XBX), IMMARG(OPSZ_4)));
    test_encode_matches_decode(dc, XINST_CREATE_add(dc, REGARG(XSI), REGARG(XDI)));
    test_encode_matches_decode(
        dc, XINST_CREATE_add_2src(dc, REGARG(XBX), REGARG(XAX), IMMARG(OPSZ_4)));
    test_encode_matches_decode(
        dc, XINST_CREATE_add_2src(dc, REGARG(XSI), REGARG(XDI), REGARG(XBP)));
}

static void
test_stack_pointer_size(void *dc)
{
    /* Test i#2281 where we had the stack pointer size incorrectly varying.
     * We can't simply append these to dis-udis86-randtest.raw b/c our test
     * there uses -syntax_intel.  We could make a new raw DR-style test.
     */
    byte *pc;
    char dbuf[512];
    int len;
    const byte bytes_push[] = { 0x67, 0x51 };
    const byte bytes_ret[] = { 0x67, 0xc3 };
    const byte bytes_enter[] = { 0x67, 0xc8, 0xab, 0xcd, 0xef };
    const byte bytes_leave[] = { 0x67, 0xc9 };

    pc =
        disassemble_to_buffer(dc, (byte *)bytes_push, (byte *)bytes_push, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL && pc - (byte *)bytes_push == sizeof(bytes_push));
    ASSERT(strcmp(dbuf,
                  IF_X64_ELSE(
                      "addr32 push   %rcx %rsp -> %rsp 0xfffffff8(%rsp)[8byte]\n",
                      "addr16 push   %ecx %esp -> %esp 0xfffffffc(%esp)[4byte]\n")) == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)bytes_ret, (byte *)bytes_ret, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL && pc - (byte *)bytes_ret == sizeof(bytes_ret));
    ASSERT(strcmp(dbuf,
                  IF_X64_ELSE("addr32 ret    %rsp (%rsp)[8byte] -> %rsp\n",
                              "addr16 ret    %esp (%esp)[4byte] -> %esp\n")) == 0);

    pc = disassemble_to_buffer(dc, (byte *)bytes_enter, (byte *)bytes_enter,
                               false /*no pc*/, false /*no bytes*/, dbuf,
                               BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL && pc - (byte *)bytes_enter == sizeof(bytes_enter));
    ASSERT(
        strcmp(dbuf,
               IF_X64_ELSE(
                   "addr32 enter  $0xcdab $0xef %rsp %rbp -> %rsp 0xfffffff8(%rsp)[8byte]"
                   " %rbp\n",
                   "addr16 enter  $0xcdab $0xef %esp %ebp -> %esp 0xfffffffc(%esp)[4byte]"
                   " %ebp\n")) == 0);

    pc = disassemble_to_buffer(dc, (byte *)bytes_leave, (byte *)bytes_leave,
                               false /*no pc*/, false /*no bytes*/, dbuf,
                               BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc != NULL && pc - (byte *)bytes_leave == sizeof(bytes_leave));
    ASSERT(strcmp(dbuf,
                  IF_X64_ELSE("addr32 leave  %rbp %rsp (%rbp)[8byte] -> %rsp %rbp\n",
                              "addr16 leave  %ebp %esp (%ebp)[4byte] -> %esp %ebp\n")) ==
           0);
}

static void
test_reg_exact_reads(void *dc)
{
    instr_t *instr = INSTR_CREATE_mov_ld(dc, OPND_CREATE_MEMPTR(DR_REG_XAX, 5),
                                         opnd_create_reg(DR_REG_XBX));

    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XBX, DR_QUERY_DEFAULT));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XBX, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XBX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XBX, 0));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XAX, DR_QUERY_DEFAULT));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XAX, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XAX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XAX, 0));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XCX, DR_QUERY_DEFAULT));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XCX, DR_QUERY_INCLUDE_ALL));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XCX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XCX, 0));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_AX, DR_QUERY_DEFAULT));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_AX, DR_QUERY_INCLUDE_ALL));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_AX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_AX, 0));

    instr_destroy(dc, instr);
    instr = INSTR_CREATE_mov_ld(dc, OPND_CREATE_MEM16(DR_REG_XAX, 5),
                                opnd_create_reg(DR_REG_BX));

    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XBX, DR_QUERY_DEFAULT));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XBX, DR_QUERY_INCLUDE_ALL));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XBX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XBX, 0));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XAX, DR_QUERY_DEFAULT));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XAX, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XAX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XAX, 0));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XCX, DR_QUERY_DEFAULT));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XCX, DR_QUERY_INCLUDE_ALL));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XCX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_XCX, 0));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_BX, DR_QUERY_DEFAULT));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_BX, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_BX, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_BX, 0));

    instr_destroy(dc, instr);
    instr =
        INSTR_CREATE_pxor(dc, opnd_create_reg(DR_REG_XMM0), opnd_create_reg(DR_REG_XMM1));

    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XMM0, DR_QUERY_DEFAULT));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XMM0, DR_QUERY_INCLUDE_ALL));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XMM0, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(instr_reads_from_exact_reg(instr, DR_REG_XMM0, 0));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_YMM0, DR_QUERY_DEFAULT));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_YMM0, DR_QUERY_INCLUDE_ALL));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_YMM0, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_YMM0, 0));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_ZMM0, DR_QUERY_DEFAULT));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_ZMM0, DR_QUERY_INCLUDE_ALL));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_ZMM0, DR_QUERY_INCLUDE_COND_DSTS));
    ASSERT(!instr_reads_from_exact_reg(instr, DR_REG_ZMM0, 0));

    instr_destroy(dc, instr);
}

static void
test_re_relativization_disp32_opc16(void *dcontext, byte opc1, byte opc2)
{
    byte buf_dec_enc[] = { opc1, opc2,
                           /* disp32 of 0 which targets the next PC. */
                           0x00, 0x00, 0x00, 0x00,
                           /* We encode here. */
                           0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    instr_t instr;
    instr_init(dcontext, &instr);
    byte *pc = decode_from_copy(dcontext, buf_dec_enc, buf_dec_enc + 1, &instr);
    ASSERT(pc != NULL);
    ASSERT(instr_raw_bits_valid(&instr)); /* i#731. */
    ASSERT(opnd_get_pc(instr_get_src(&instr, 0)) == buf_dec_enc + 7);
    pc = instr_encode(dcontext, &instr, buf_dec_enc + 6);
    ASSERT(pc != NULL);
    instr_reset(dcontext, &instr);
    pc = decode(dcontext, buf_dec_enc + 6, &instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_pc(instr_get_src(&instr, 0)) == buf_dec_enc + 7);
    instr_free(dcontext, &instr);
}

static void
test_re_relativization_disp8_opc8(void *dcontext, byte opc)
{
    byte buf_dec_enc[] = { opc,
                           /* disp8 of 0 which targets the next PC. */
                           0x00,
                           /* We encode here. */ 0x90, 0x90 };
    instr_t instr;
    instr_init(dcontext, &instr);
    byte *pc = decode_from_copy(dcontext, buf_dec_enc, buf_dec_enc + 1, &instr);
    ASSERT(pc != NULL);
    ASSERT(instr_raw_bits_valid(&instr)); /* i#731. */
    ASSERT(opnd_get_pc(instr_get_src(&instr, 0)) == buf_dec_enc + 3);
    pc = instr_encode(dcontext, &instr, buf_dec_enc + 2);
    ASSERT(pc != NULL);
    instr_reset(dcontext, &instr);
    pc = decode(dcontext, buf_dec_enc + 2, &instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_pc(instr_get_src(&instr, 0)) == buf_dec_enc + 3);
    instr_free(dcontext, &instr);
}

/* XXX: Have DR export its raw opcodes, which overlap this list. */
enum {
    RAW_OPCODE_jmp_short = 0xeb,
    RAW_OPCODE_jcc_short_start = 0x70,
    RAW_OPCODE_jcc_short_end = 0x7f,
    RAW_OPCODE_jcc_byte1 = 0x0f,
    RAW_OPCODE_jcc_byte2_start = 0x80,
    RAW_OPCODE_jcc_byte2_end = 0x8f,
    RAW_OPCODE_loop_start = 0xe0,
    RAW_OPCODE_loop_end = 0xe3,
    RAW_OPCODE_xbegin_byte1 = 0xc7,
    RAW_OPCODE_xbegin_byte2 = 0xf8,
};

static void
test_re_relativization(void *dcontext)
{
    instr_t instr;
    instr_init(dcontext, &instr);
    byte *pc;

    /* Test the i#4017 2-byte nop where re-encoding results in a 1-byte length. */
    const byte buf_nop2[] = { 0x66, 0x90 };
    instr_reset(dcontext, &instr);
    pc = decode_from_copy(dcontext, (byte *)buf_nop2, (byte *)buf_nop2 + 1, &instr);
    ASSERT(pc != NULL);
    ASSERT(instr_length(dcontext, &instr) == sizeof(buf_nop2));

    /* Test i#731 on short jumps. */
    test_re_relativization_disp8_opc8(dcontext, RAW_OPCODE_jmp_short);
    test_re_relativization_disp8_opc8(dcontext, RAW_OPCODE_loop_start);
    test_re_relativization_disp8_opc8(dcontext, RAW_OPCODE_loop_end);
    test_re_relativization_disp8_opc8(dcontext, RAW_OPCODE_jcc_short_start);
    test_re_relativization_disp8_opc8(dcontext, RAW_OPCODE_jcc_short_end);

    /* Test xbegin. */
    test_re_relativization_disp32_opc16(dcontext, RAW_OPCODE_xbegin_byte1,
                                        RAW_OPCODE_xbegin_byte2);
    /* Test jcc. */
    test_re_relativization_disp32_opc16(dcontext, RAW_OPCODE_jcc_byte1,
                                        RAW_OPCODE_jcc_byte2_start);
    test_re_relativization_disp32_opc16(dcontext, RAW_OPCODE_jcc_byte1,
                                        RAW_OPCODE_jcc_byte2_end);

    instr_free(dcontext, &instr);
}

static void
test_noalloc(void *dcontext)
{
    byte buf[128];
    byte *pc, *end;

    instr_t *to_encode = XINST_CREATE_load(dcontext, opnd_create_reg(DR_REG_XAX),
                                           OPND_CREATE_MEMPTR(DR_REG_XAX, 42));
    end = instr_encode(dcontext, to_encode, buf);
    ASSERT(end - buf < BUFFER_SIZE_ELEMENTS(buf));
    instr_destroy(dcontext, to_encode);

    instr_noalloc_t noalloc;
    instr_noalloc_init(dcontext, &noalloc);
    instr_t *instr = instr_from_noalloc(&noalloc);
    pc = decode(dcontext, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_XAX);

    instr_reset(dcontext, instr);
    pc = decode(dcontext, buf, instr);
    ASSERT(pc != NULL);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_XAX);

    /* There should be no leak reported even w/o a reset b/c there's no
     * extra heap.
     */
}

static void
test_opnd(void *dc)
{
    opnd_t op = opnd_create_reg(DR_REG_EAX);
    ASSERT(opnd_get_reg(op) == DR_REG_EAX);
    bool found = opnd_replace_reg(&op, DR_REG_AX, DR_REG_CX);
    ASSERT(!found);
    found = opnd_replace_reg_resize(&op, DR_REG_AX, DR_REG_CX);
    ASSERT(found);
    ASSERT(opnd_get_reg(op) == DR_REG_ECX);

    op = opnd_create_reg(DR_REG_AL);
    ASSERT(opnd_get_reg(op) == DR_REG_AL);
    found = opnd_replace_reg(&op, DR_REG_XAX, DR_REG_XCX);
    ASSERT(!found);
    found = opnd_replace_reg_resize(&op, DR_REG_XAX, DR_REG_XCX);
    ASSERT(found);
    ASSERT(opnd_get_reg(op) == DR_REG_CL);

    op = opnd_create_far_base_disp_ex(DR_SEG_DS, DR_REG_XAX, DR_REG_XCX, 2, 42, OPSZ_PTR,
                                      true, true, true);
    ASSERT(opnd_get_base(op) == DR_REG_XAX);
    ASSERT(opnd_get_index(op) == DR_REG_XCX);
    ASSERT(opnd_get_scale(op) == 2);
    ASSERT(opnd_get_disp(op) == 42);
    ASSERT(opnd_is_disp_encode_zero(op));
    ASSERT(opnd_is_disp_force_full(op));
    ASSERT(opnd_is_disp_short_addr(op));

    /* Ensure extra fields are preserved by opnd_replace_reg*(). */
    found = opnd_replace_reg(&op, DR_REG_AX, DR_REG_DX);
    ASSERT(!found);
    found = opnd_replace_reg_resize(&op, DR_REG_AX, DR_REG_DX);
    ASSERT(found);
    ASSERT(opnd_get_base(op) == DR_REG_XDX);
    ASSERT(opnd_get_index(op) == DR_REG_XCX);
    ASSERT(opnd_get_scale(op) == 2);
    ASSERT(opnd_get_disp(op) == 42);
    ASSERT(opnd_is_disp_encode_zero(op));
    ASSERT(opnd_is_disp_force_full(op));
    ASSERT(opnd_is_disp_short_addr(op));

    instr_t *instr = XINST_CREATE_load(
        /* Test the trickiest conversion: high 8-bit. */
        dc, opnd_create_reg(DR_REG_AH),
        opnd_create_far_base_disp_ex(DR_SEG_DS, DR_REG_XCX, DR_REG_XAX, 2, 42, OPSZ_PTR,
                                     true, true, true));
    found = instr_replace_reg_resize(instr, DR_REG_AX, DR_REG_DX);
    ASSERT(found);
    ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_DH);
    ASSERT(opnd_get_base(instr_get_src(instr, 0)) == DR_REG_XCX);
    ASSERT(opnd_get_index(instr_get_src(instr, 0)) == DR_REG_XDX);
    ASSERT(opnd_get_scale(instr_get_src(instr, 0)) == 2);
    ASSERT(opnd_get_disp(instr_get_src(instr, 0)) == 42);
    ASSERT(opnd_is_disp_encode_zero(instr_get_src(instr, 0)));
    ASSERT(opnd_is_disp_force_full(instr_get_src(instr, 0)));
    ASSERT(opnd_is_disp_short_addr(instr_get_src(instr, 0)));
    instr_destroy(dc, instr);

    /* XXX: test other routines like opnd_defines_use() */
}

static void
test_simd_zeroes_upper(void *dc)
{
    instr_t *instr;

    instr =
        INSTR_CREATE_pxor(dc, opnd_create_reg(DR_REG_XMM0), opnd_create_reg(DR_REG_XMM0));
    ASSERT(!instr_zeroes_ymmh(instr));
    ASSERT(!instr_zeroes_zmmh(instr));
    instr_destroy(dc, instr);

    instr =
        INSTR_CREATE_vpxor(dc, opnd_create_reg(DR_REG_XMM0), opnd_create_reg(DR_REG_XMM0),
                           opnd_create_reg(DR_REG_XMM0));
    ASSERT(instr_zeroes_ymmh(instr));
    ASSERT(instr_zeroes_zmmh(instr));
    instr_destroy(dc, instr);

    instr =
        INSTR_CREATE_vpxor(dc, opnd_create_reg(DR_REG_YMM0), opnd_create_reg(DR_REG_YMM0),
                           opnd_create_reg(DR_REG_YMM0));
    ASSERT(!instr_zeroes_ymmh(instr));
    ASSERT(instr_zeroes_zmmh(instr));
    instr_destroy(dc, instr);

    instr =
        INSTR_CREATE_vpxor(dc, opnd_create_reg(DR_REG_ZMM0), opnd_create_reg(DR_REG_ZMM0),
                           opnd_create_reg(DR_REG_ZMM0));
    ASSERT(!instr_zeroes_ymmh(instr));
    ASSERT(!instr_zeroes_zmmh(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_vzeroupper(dc);
    ASSERT(!instr_zeroes_ymmh(instr));
    ASSERT(instr_zeroes_zmmh(instr));
    instr_destroy(dc, instr);

    instr = INSTR_CREATE_vzeroall(dc);
    ASSERT(instr_zeroes_ymmh(instr));
    ASSERT(instr_zeroes_zmmh(instr));
    instr_destroy(dc, instr);
}

static void
test_evex_compressed_disp_with_segment_prefix(void *dc)
{
#ifdef X64
    byte *pc;
    const byte b[] = { 0x65, 0x67, 0x62, 0x01, 0xc5, 0x00, 0xc4, 0x62, 0x21, 0x00 };
    char dbuf[512];
    int len;

    pc =
        disassemble_to_buffer(dc, (byte *)b, (byte *)b, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc == &b[0] + sizeof(b));
    ASSERT(
        strcmp(
            dbuf,
            "addr32 vpinsrw %xmm23[14byte] %gs:0x42(%r10d)[2byte] $0x00 -> %xmm28\n") ==
        0);
#endif
}

static void
test_extra_leading_prefixes(void *dc)
{
    byte *pc;
    char dbuf[512];
    int len;
#ifdef X64
    const byte b1[] = { 0xf3, 0xf2, 0x4b, 0x0f, 0x70, 0x76, 0x00, 0xff };
    const byte b2[] = { 0xf3, 0xf2, 0x0f, 0xbc, 0xf2 };

    pc =
        disassemble_to_buffer(dc, (byte *)b1, (byte *)b1, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc == &b1[0] + sizeof(b1));
    ASSERT(strcmp(dbuf, "pshuflw 0x00(%r14)[16byte] $0xff -> %xmm6\n") == 0);

    pc =
        disassemble_to_buffer(dc, (byte *)b2, (byte *)b2, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc == &b2[0] + sizeof(b2));
    ASSERT(strcmp(dbuf, "bsf    %edx -> %esi\n") == 0);
#endif

    /* Only FS/GS prefixes are accepted on x64, so different prefixes "win" here */
    const byte b3[] = { 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x64,
                        0x26, 0x26, 0x26, 0x26, 0x13, 0x04, 0x0a };
    pc =
        disassemble_to_buffer(dc, (byte *)b3, (byte *)b3, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc == &b3[0] + sizeof(b3));
#ifdef X64
    ASSERT(strcmp(dbuf, "adc    %fs:(%rdx,%rcx)[4byte] %eax -> %eax\n") == 0);
#else
    ASSERT(strcmp(dbuf, "adc    %es:(%edx,%ecx)[4byte] %eax -> %eax\n") == 0);
#endif
}

static void
test_ud1_operands(void *dc)
{
    byte *pc;
    char dbuf[512];
    int len;

    const byte b[] = { 0x67, 0x0f, 0xb9, 0x40, 0x16 };
    pc =
        disassemble_to_buffer(dc, (byte *)b, (byte *)b, false /*no pc*/,
                              false /*no bytes*/, dbuf, BUFFER_SIZE_ELEMENTS(dbuf), &len);
    ASSERT(pc == &b[0] + sizeof(b));
#ifdef X64
    ASSERT(strcmp(dbuf, "addr32 ud1    %eax 0x16(%eax)[4byte]\n") == 0);
#else
    ASSERT(strcmp(dbuf, "addr16 ud1    %eax 0x16(%bx,%si)[4byte]\n") == 0);
#endif
}

static void
test_disasm_to_buffer(void *dc)
{
    /* Test disassemble_to_buffer() corner cases. */
    byte *pc;
    byte b[] = { 0x90 };
    char dbuf[512];
    const char *expect = "nop\n";
    size_t expect_len = strlen(expect);
    int len;
    /* Test plenty of room. */
    pc = disassemble_to_buffer(dc, b, b, false /*no pc*/, false /*no bytes*/, dbuf,
                               expect_len + 10, &len);
    ASSERT(pc == b + 1);
    ASSERT(strcmp(dbuf, expect) == 0);
    ASSERT(len == expect_len);
    /* Test just enough room. */
    pc = disassemble_to_buffer(dc, b, b, false /*no pc*/, false /*no bytes*/, dbuf,
                               expect_len + 1 /*null*/, &len);
    ASSERT(pc == b + 1);
    ASSERT(strcmp(dbuf, expect) == 0);
    ASSERT(len == expect_len);
    /* Test not enough room for null. */
    pc = disassemble_to_buffer(dc, b, b, false /*no pc*/, false /*no bytes*/, dbuf,
                               expect_len, &len);
    ASSERT(pc == b + 1);
    ASSERT(strncmp(dbuf, expect, len) == 0);
    ASSERT(len == expect_len - 1);
    /* Test not enough room for full string. */
    pc = disassemble_to_buffer(dc, b, b, false /*no pc*/, false /*no bytes*/, dbuf,
                               expect_len - 1, &len);
    ASSERT(pc == b + 1);
    ASSERT(strncmp(dbuf, expect, len) == 0);
    ASSERT(len == expect_len - 2);
}

int
main(int argc, char *argv[])
{
#ifdef STANDALONE_DECODER
    void *dcontext = GLOBAL_DCONTEXT;
#else
    void *dcontext = dr_standalone_init();

    /* simple test of deadlock_avoidance, etc. being disabled in standalone */
    void *x = dr_mutex_create();
    dr_mutex_lock(x);
    dr_mutex_unlock(x);
    dr_mutex_destroy(x);
#endif

    test_all_opcodes_0(dcontext);
#ifndef STANDALONE_DECODER /* speed up compilation */
    test_all_opcodes_1(dcontext);
    test_all_opcodes_2(dcontext);
    test_all_opcodes_2_mm(dcontext);
    test_all_opcodes_3(dcontext);
    test_all_opcodes_3_avx(dcontext);
    test_all_opcodes_4(dcontext);
#endif

    test_disp_control(dcontext);

    test_indirect_cti(dcontext);

    test_cti_prefixes(dcontext);

    test_cti_predicates(dcontext);

#ifndef X64
    test_modrm16(dcontext);
#endif

    test_modrm_invalid(dcontext);

    test_size_changes(dcontext);

    test_nop_xchg(dcontext);

    test_hint_nops(dcontext);

#ifdef X64
    test_x86_mode(dcontext);

    test_x64_abs_addr(dcontext);

    test_x64_inc(dcontext);

    test_avx512_vnni_encoding(dcontext);

    test_avx512_bf16_encoding(dcontext);

    test_x64_vmovq(dcontext);
#endif

    test_regs(dcontext);

    test_instr_opnds(dcontext);

    test_strict_invalid(dcontext);

    test_tsx(dcontext);

    test_vsib(dcontext);

    test_disasm_sizes(dcontext);

    test_predication(dcontext);

    test_xinst_create(dcontext);

    test_stack_pointer_size(dcontext);

    test_reg_exact_reads(dcontext);

    test_re_relativization(dcontext);

    test_noalloc(dcontext);

    test_opnd(dcontext);

    test_simd_zeroes_upper(dcontext);

    test_evex_compressed_disp_with_segment_prefix(dcontext);

    test_extra_leading_prefixes(dcontext);

    test_ud1_operands(dcontext);

    test_disasm_to_buffer(dcontext);

#ifndef STANDALONE_DECODER /* speed up compilation */
    test_all_opcodes_2_avx512_vex(dcontext);
    test_all_opcodes_3_avx512_vex(dcontext);
    test_opmask_disas_avx512(dcontext);
    test_all_opcodes_3_avx512_evex_mask(dcontext);
    test_disas_3_avx512_evex_mask(dcontext);
    test_all_opcodes_5_avx512_evex_mask(dcontext);
    test_all_opcodes_4_avx512_evex_mask_A(dcontext);
    test_all_opcodes_4_avx512_evex_mask_B(dcontext);
    test_all_opcodes_4_avx512_evex_mask_C(dcontext);
    test_all_opcodes_4_avx512_evex(dcontext);
    test_all_opcodes_3_avx512_evex(dcontext);
    test_all_opcodes_2_avx512_evex(dcontext);
    test_all_opcodes_2_avx512_evex_mask(dcontext);
    /* We're testing a scalable displacement for evex instructions in addition to the
     * default displacement. The default displacement will become a full 32-bit
     * displacement, while the scalable displacement will get compressed to 8-bit.
     */
    test_all_opcodes_3_avx512_evex_mask_scaled_disp8(dcontext);
    test_all_opcodes_5_avx512_evex_mask_scaled_disp8(dcontext);
    test_all_opcodes_4_avx512_evex_mask_scaled_disp8_A(dcontext);
    test_all_opcodes_4_avx512_evex_mask_scaled_disp8_B(dcontext);
    test_all_opcodes_4_avx512_evex_mask_scaled_disp8_C(dcontext);
    test_all_opcodes_4_avx512_evex_scaled_disp8(dcontext);
    test_all_opcodes_3_avx512_evex_scaled_disp8(dcontext);
    test_all_opcodes_2_avx512_evex_scaled_disp8(dcontext);
#endif

    print("all done\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    return 0;
}
