/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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

/* decode_shared.c -- shared decoding data */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"

#ifdef DEBUG
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
#    undef ASSERT_TRUNCATE
#    undef ASSERT_BITFIELD_TRUNCATE
#    undef ASSERT_NOT_REACHED
#    define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* Arch-specific routines */
#ifdef DEBUG
void
encode_debug_checks(void);
void
decode_debug_checks_arch(void);
#endif

const char *const size_names[] = {
    "OPSZ_NA",
    "OPSZ_lea",
    "OPSZ_1",
    "OPSZ_2",
    "OPSZ_4",
    "OPSZ_6",
    "OPSZ_8",
    "OPSZ_10",
    "OPSZ_16",
    "OPSZ_14",
    "OPSZ_28",
    "OPSZ_94",
    "OPSZ_108",
    "OPSZ_512",
    "OPSZ_2_short1",
    "OPSZ_4_short2",
    "OPSZ_4_rex8_short2",
    "OPSZ_4_rex8",
    "OPSZ_6_irex10_short4",
    "OPSZ_8_short2",
    "OPSZ_8_short4",
    "OPSZ_28_short14",
    "OPSZ_108_short94",
    "OPSZ_4x8",
    "OPSZ_6x10",
    "OPSZ_4x8_short2",
    "OPSZ_4x8_short2xi8",
    "OPSZ_4_short2xi4",
    "OPSZ_1_reg4",
    "OPSZ_2_reg4",
    "OPSZ_4_reg16",
    "OPSZ_xsave",
    "OPSZ_12",
    "OPSZ_32",
    "OPSZ_40",
    "OPSZ_32_short16",
    "OPSZ_8_rex16",
    "OPSZ_8_rex16_short4",
    "OPSZ_12_rex40_short6",
    "OPSZ_16_vex32",
    "OPSZ_15",
    "OPSZ_3",
    "OPSZ_1b",
    "OPSZ_2b",
    "OPSZ_3b",
    "OPSZ_4b",
    "OPSZ_5b",
    "OPSZ_6b",
    "OPSZ_7b",
    "OPSZ_9b",
    "OPSZ_10b",
    "OPSZ_11b",
    "OPSZ_12b",
    "OPSZ_20b",
    "OPSZ_25b",
    "OPSZ_VAR_REGLIST",
    "OPSZ_20",
    "OPSZ_24",
    "OPSZ_36",
    "OPSZ_44",
    "OPSZ_48",
    "OPSZ_52",
    "OPSZ_56",
    "OPSZ_60",
    "OPSZ_64",
    "OPSZ_68",
    "OPSZ_72",
    "OPSZ_76",
    "OPSZ_80",
    "OPSZ_84",
    "OPSZ_88",
    "OPSZ_92",
    "OPSZ_96",
    "OPSZ_100",
    "OPSZ_104",
    "OPSZ_112",
    "OPSZ_116",
    "OPSZ_120",
    "OPSZ_124",
    "OPSZ_128",
    "OPSZ_SCALABLE",
    "OPSZ_SCALABLE_PRED",
    "OPSZ_16_vex32_evex64",
    "OPSZ_vex32_evex64",
    "OPSZ_16_of_32_evex64",
    "OPSZ_32_of_64",
    "OPSZ_4_of_32_evex64",
    "OPSZ_8_of_32_evex64",
    "OPSZ_8x16",
    "OPSZ_1_of_4",
    "OPSZ_2_of_4",
    "OPSZ_1_of_8",
    "OPSZ_2_of_8",
    "OPSZ_4_of_8",
    "OPSZ_1_of_16",
    "OPSZ_2_of_16",
    "OPSZ_4_of_16",
    "OPSZ_4_rex8_of_16",
    "OPSZ_8_of_16",
    "OPSZ_12_of_16",
    "OPSZ_12_rex8_of_16",
    "OPSZ_14_of_16",
    "OPSZ_15_of_16",
    "OPSZ_16_of_32",
    "OPSZ_half_16_vex32",
    "OPSZ_half_16_vex32_evex64",
    "OPSZ_quarter_16_vex32",
    "OPSZ_quarter_16_vex32_evex64",
    "OPSZ_eighth_16_vex32",
    "OPSZ_eighth_16_vex32_evex64",
};

/* AArch64 Scalable Vector Extension's vector length in bits. */
int sve_veclen;
int sve_veclens[] = { 128,  256,  384,  512,  640,  768,  896,  1024,
                      1152, 1280, 1408, 1536, 1664, 1792, 1920, 2048 };

void
dr_set_sve_vector_length(int vl)
{
    /* TODO i#3044: Vector length will be read from h/w when running on SVE. */
    for (int i = 0; i < sizeof(sve_veclens); i++) {
        if (vl == sve_veclens[i]) {
            sve_veclen = vl;
            return;
        }
    }
    CLIENT_ASSERT(false, "invalid SVE vector length");
}

int
dr_get_sve_vector_length(void)
{
    return sve_veclen;
}

/* point at this when you need a canonical invalid instr
 * type is OP_INVALID so can be copied to instr->opcode
 */
#define xx 0 /* TYPE_NONE */, OPSZ_NA
const instr_info_t invalid_instr = { OP_INVALID, 0x000000, "(bad)", xx, xx, xx,
                                     xx,         xx,       0,       0,  0 };
#undef xx

/* PR 302344: used for shared traces -tracedump_origins where we
 * need to change the mode but we have no dcontext.
 * We update this in d_r_decode_init() once we have runtime options,
 * but this is the only version for drdecodelib.
 */
static dr_isa_mode_t initexit_isa_mode = DEFAULT_ISA_MODE_STATIC;

/* The decode and encode routines use a per-thread persistent flag that
 * indicates which processor mode to use.  This routine sets that flag to the
 * indicated value and optionally returns the old value.  Be sure to restore the
 * old value prior to any further application execution to avoid problems in
 * mis-interpreting application code.
 */
bool
dr_set_isa_mode(void *drcontext, dr_isa_mode_t new_mode, dr_isa_mode_t *old_mode_out OUT)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    dr_isa_mode_t old_mode;
    /* We would disallow but some early init routines need to use global heap */
    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();
    /* Support GLOBAL_DCONTEXT or NULL for standalone/static modes */
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT) {
#if !defined(STANDALONE_DECODER)
        CLIENT_ASSERT(!dynamo_initialized || dynamo_exited || dcontext == GLOBAL_DCONTEXT,
                      "internal isa mode error");
#endif
        old_mode = initexit_isa_mode;
        if (is_isa_mode_legal(new_mode))
            initexit_isa_mode = new_mode;
    } else {
        old_mode = dcontext->isa_mode;
        if (is_isa_mode_legal(new_mode))
            dcontext->isa_mode = new_mode;
    }
    if (old_mode_out != NULL)
        *old_mode_out = old_mode;
    return is_isa_mode_legal(new_mode);
}

/* The decode and encode routines use a per-thread persistent flag that
 * indicates which processor mode to use.  This routine returns the value of
 * that flag.
 */
dr_isa_mode_t
dr_get_isa_mode(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
#if !defined(STANDALONE_DECODER) && defined(DEBUG)
    dcontext_t *orig_dcontext = dcontext;
#endif
    /* We would disallow but some early init routines need to use global heap */
    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();
    /* Support GLOBAL_DCONTEXT or NULL for standalone/static modes */
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT) {
#if !defined(STANDALONE_DECODER)
        CLIENT_ASSERT(!dynamo_initialized || dynamo_exited ||
                          orig_dcontext == GLOBAL_DCONTEXT,
                      "internal isa mode error");
#endif
        return initexit_isa_mode;
    } else
        return dcontext->isa_mode;
}

#ifdef DEBUG
void
decode_debug_checks(void)
{
    CLIENT_ASSERT(sizeof(size_names) / sizeof(size_names[0]) == OPSZ_LAST_ENUM,
                  "size_names missing an entry");
    encode_debug_checks();
    decode_debug_checks_arch();
}
#endif

void
d_r_decode_init(void)
{
    /* DEFAULT_ISA_MODE is no longer constant so we set it here */
    initexit_isa_mode = DEFAULT_ISA_MODE;

    DODEBUG({ decode_debug_checks(); });
}
