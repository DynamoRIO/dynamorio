/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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

#if defined(DEBUG) && defined(CLIENT_INTERFACE)
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
#ifdef X86
    "<invalid>" /* was <NULL> */,
    "<invalid>" /* was rax */,
    "<invalid>" /* was rcx */,
    "<invalid>" /* was rdx */,
    "<invalid>" /* was rbx */,
    "<invalid>" /* was rsp */,
    "<invalid>" /* was rbp */,
    "<invalid>" /* was rsi */,
    "<invalid>" /* was rdi */,
    "<invalid>" /* was r8 */,
    "<invalid>" /* was r9 */,
    "<invalid>" /* was r10 */,
    "<invalid>" /* was r11 */,
    "<invalid>" /* was r12 */,
    "<invalid>" /* was r13 */,
    "<invalid>" /* was r14 */,
    "<invalid>" /* was r15 */,
    "<invalid>" /* was eax */,
    "<invalid>" /* was ecx */,
    "<invalid>" /* was edx */,
    "<invalid>" /* was ebx */,
    "<invalid>" /* was esp */,
    "<invalid>" /* was ebp */,
    "<invalid>" /* was esi */,
    "<invalid>" /* was edi */,
    "<invalid>" /* was r8d */,
    "<invalid>" /* was r9d */,
    "<invalid>" /* was r10d */,
    "<invalid>" /* was r11d */,
    "<invalid>" /* was r12d */,
    "<invalid>" /* was r13d */,
    "<invalid>" /* was r14d */,
    "<invalid>" /* was r15d */,
    "<invalid>" /* was ax */,
    "<invalid>" /* was cx */,
    "<invalid>" /* was dx */,
    "<invalid>" /* was bx */,
    "<invalid>" /* was sp */,
    "<invalid>" /* was bp */,
    "<invalid>" /* was si */,
    "<invalid>" /* was di */,
    "<invalid>" /* was r8w */,
    "<invalid>" /* was r9w */,
    "<invalid>" /* was r10w */,
    "<invalid>" /* was r11w */,
    "<invalid>" /* was r12w */,
    "<invalid>" /* was r13w */,
    "<invalid>" /* was r14w */,
    "<invalid>" /* was r15w */,
    "<invalid>" /* was al */,
    "<invalid>" /* was cl */,
    "<invalid>" /* was dl */,
    "<invalid>" /* was bl */,
    "<invalid>" /* was ah */,
    "<invalid>" /* was ch */,
    "<invalid>" /* was dh */,
    "<invalid>" /* was bh */,
    "<invalid>" /* was r8l */,
    "<invalid>" /* was r9l */,
    "<invalid>" /* was r10l */,
    "<invalid>" /* was r11l */,
    "<invalid>" /* was r12l */,
    "<invalid>" /* was r13l */,
    "<invalid>" /* was r14l */,
    "<invalid>" /* was r15l */,
    "<invalid>" /* was spl */,
    "<invalid>" /* was bpl */,
    "<invalid>" /* was sil */,
    "<invalid>" /* was dil */,
    "<invalid>" /* was mm0 */,
    "<invalid>" /* was mm1 */,
    "<invalid>" /* was mm2 */,
    "<invalid>" /* was mm3 */,
    "<invalid>" /* was mm4 */,
    "<invalid>" /* was mm5 */,
    "<invalid>" /* was mm6 */,
    "<invalid>" /* was mm7 */,
    "<invalid>" /* was xmm0 */,
    "<invalid>" /* was xmm1 */,
    "<invalid>" /* was xmm2 */,
    "<invalid>" /* was xmm3 */,
    "<invalid>" /* was xmm4 */,
    "<invalid>" /* was xmm5 */,
    "<invalid>" /* was xmm6 */,
    "<invalid>" /* was xmm7 */,
    "<invalid>" /* was xmm8 */,
    "<invalid>" /* was xmm9 */,
    "<invalid>" /* was xmm10 */,
    "<invalid>" /* was xmm11 */,
    "<invalid>" /* was xmm12 */,
    "<invalid>" /* was xmm13 */,
    "<invalid>" /* was xmm14 */,
    "<invalid>" /* was xmm15 */,
    "<invalid>" /* was st0 */,
    "<invalid>" /* was st1 */,
    "<invalid>" /* was st2 */,
    "<invalid>" /* was st3 */,
    "<invalid>" /* was st4 */,
    "<invalid>" /* was st5 */,
    "<invalid>" /* was st6 */,
    "<invalid>" /* was st7 */,
    "<invalid>" /* was es */,
    "<invalid>" /* was cs */,
    "<invalid>" /* was ss */,
    "<invalid>" /* was ds */,
    "<invalid>" /* was fs */,
    "<invalid>" /* was gs */,
    "<invalid>" /* was dr0 */,
    "<invalid>" /* was dr1 */,
    "<invalid>" /* was dr2 */,
    "<invalid>" /* was dr3 */,
    "<invalid>" /* was dr4 */,
    "<invalid>" /* was dr5 */,
    "<invalid>" /* was dr6 */,
    "<invalid>" /* was dr7 */,
    "<invalid>" /* was dr8 */,
    "<invalid>" /* was dr9 */,
    "<invalid>" /* was dr10 */,
    "<invalid>" /* was dr11 */,
    "<invalid>" /* was dr12 */,
    "<invalid>" /* was dr13 */,
    "<invalid>" /* was dr14 */,
    "<invalid>" /* was dr15 */,
    "<invalid>" /* was cr0 */,
    "<invalid>" /* was cr1 */,
    "<invalid>" /* was cr2 */,
    "<invalid>" /* was cr3 */,
    "<invalid>" /* was cr4 */,
    "<invalid>" /* was cr5 */,
    "<invalid>" /* was cr6 */,
    "<invalid>" /* was cr7 */,
    "<invalid>" /* was cr8 */,
    "<invalid>" /* was cr9 */,
    "<invalid>" /* was cr10 */,
    "<invalid>" /* was cr11 */,
    "<invalid>" /* was cr12 */,
    "<invalid>" /* was cr13 */,
    "<invalid>" /* was cr14 */,
    "<invalid>" /* was cr15 */,
    "<invalid>" /* was <invalid> */,
#endif
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
    "OPSZ_8_of_16_vex32",
    "OPSZ_16_of_32",
};

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
dr_set_isa_mode(dcontext_t *dcontext, dr_isa_mode_t new_mode,
                dr_isa_mode_t *old_mode_out OUT)
{
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
dr_get_isa_mode(dcontext_t *dcontext)
{
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
