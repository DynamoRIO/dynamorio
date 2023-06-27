/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* decode.c -- a full x86 decoder */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "decode_fast.h"
#include "decode_private.h"

/*
 * XXX i#431: consider cpuid features when deciding invalid instrs:
 * for core DR, it doesn't really matter: the only bad thing is thinking
 * a valid instr is invalid, esp decoding its size improperly.
 * but for completeness and use as disassembly library might be nice.
 *
 * XXX (these are very old):
 * 1) several opcodes gdb thinks bad/not bad -- changes to ISA?
 * 2) why does gdb use Ex, Ed, & Ev for all float opcodes w/ modrm < 0xbf?
 *    a float instruction's modrm cannot specify a register, right?
 *    sizes are single-real => d, double-real => q, extended-real = 90 bits,
 *    14/28 bytes, and 98/108 bytes!
 *    should address sizes all be 'v'?
 * 3) there don't seem to be any immediate float values...?!?
 * 4) fld (0xd9 0xc0-7) in Table A-10 has 2 operands in different order
 *    than expected, plus asm prints using just 2nd one
 * 5) I don't see T...is there a T?  gdb has it for 0f26mov
 */

/* N.B.: must justify each assert, since we do not want to assert on a bad
 * instruction -- we want to fail gracefully and have the caller deal with it
 */

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

/* used for VEX decoding */
#define xx TYPE_NONE, OPSZ_NA
static const instr_info_t escape_instr = { ESCAPE, 0x000000, "(bad)", xx, xx, xx,
                                           xx,     xx,       0,       0,  0 };
static const instr_info_t escape_38_instr = {
    ESCAPE_3BYTE_38, 0x000000, "(bad)", xx, xx, xx, xx, xx, 0, 0, 0
};
static const instr_info_t escape_3a_instr = {
    ESCAPE_3BYTE_3a, 0x000000, "(bad)", xx, xx, xx, xx, xx, 0, 0, 0
};
/* used for XOP decoding */
static const instr_info_t xop_8_instr = { XOP_8_EXT, 0x000000, "(bad)", xx, xx, xx,
                                          xx,        xx,       0,       0,  0 };
static const instr_info_t xop_9_instr = { XOP_9_EXT, 0x000000, "(bad)", xx, xx, xx,
                                          xx,        xx,       0,       0,  0 };
static const instr_info_t xop_a_instr = { XOP_A_EXT, 0x000000, "(bad)", xx, xx, xx,
                                          xx,        xx,       0,       0,  0 };
#undef xx

bool
is_isa_mode_legal(dr_isa_mode_t mode)
{
#ifdef X64
    return (mode == DR_ISA_IA32 || mode == DR_ISA_AMD64);
#else
    return (mode == DR_ISA_IA32);
#endif
}

app_pc
canonicalize_pc_target(dcontext_t *dcontext, app_pc pc)
{
    return pc;
}

#ifdef X64
bool
set_x86_mode(void *drcontext, bool x86)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    dr_isa_mode_t old_mode;
    if (!dr_set_isa_mode(dcontext, x86 ? DR_ISA_IA32 : DR_ISA_AMD64, &old_mode))
        return false;
    return old_mode == DR_ISA_IA32;
}

bool
get_x86_mode(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return dr_get_isa_mode(dcontext) == DR_ISA_IA32;
}
#endif

/****************************************************************************
 * All code below based on tables in the ``Intel Architecture Software
 * Developer's Manual,'' Volume 2: Instruction Set Reference, 2001.
 */

#if defined(DEBUG) && !defined(STANDALONE_DECODER) /* currently only used in ASSERTs */
static bool
is_variable_size(opnd_size_t sz)
{
    switch (sz) {
    case OPSZ_2_short1:
    case OPSZ_4_short2:
    case OPSZ_4x8:
    case OPSZ_4x8_short2:
    case OPSZ_4x8_short2xi8:
    case OPSZ_4_short2xi4:
    case OPSZ_4_rex8_short2:
    case OPSZ_4_rex8:
    case OPSZ_6_irex10_short4:
    case OPSZ_6x10:
    case OPSZ_8_short2:
    case OPSZ_8_short4:
    case OPSZ_28_short14:
    case OPSZ_108_short94:
    case OPSZ_1_reg4:
    case OPSZ_2_reg4:
    case OPSZ_4_reg16:
    case OPSZ_32_short16:
    case OPSZ_8_rex16:
    case OPSZ_8_rex16_short4:
    case OPSZ_12_rex40_short6:
    case OPSZ_16_vex32:
    case OPSZ_16_vex32_evex64:
    case OPSZ_vex32_evex64:
    case OPSZ_8x16: return true;
    default: return false;
    }
}
#endif

opnd_size_t
resolve_var_reg_size(opnd_size_t sz, bool is_reg)
{
    switch (sz) {
    case OPSZ_1_reg4: return (is_reg ? OPSZ_4 : OPSZ_1);
    case OPSZ_2_reg4: return (is_reg ? OPSZ_4 : OPSZ_2);
    case OPSZ_4_reg16:
        return (is_reg ? OPSZ_4 /* i#1382: we distinguish sub-xmm now */
                       : OPSZ_4);
    }
    return sz;
}

/* Like all our code, we assume cs specifies default data and address sizes.
 * This routine assumes the size varies by data, NOT by address!
 */
opnd_size_t
resolve_variable_size(decode_info_t *di /*IN: x86_mode, prefixes*/, opnd_size_t sz,
                      bool is_reg)
{
    switch (sz) {
    case OPSZ_2_short1: return (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_1 : OPSZ_2);
    case OPSZ_4_short2: return (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_2 : OPSZ_4);
    case OPSZ_4x8: return (X64_MODE(di) ? OPSZ_8 : OPSZ_4);
    case OPSZ_4x8_short2:
        return (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_2
                                                : (X64_MODE(di) ? OPSZ_8 : OPSZ_4));
    case OPSZ_4x8_short2xi8:
        return (X64_MODE(di) ? (proc_get_vendor() == VENDOR_INTEL
                                    ? OPSZ_8
                                    : (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_2 : OPSZ_8))
                             : (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_2 : OPSZ_4));
    case OPSZ_4_short2xi4:
        return ((X64_MODE(di) && proc_get_vendor() == VENDOR_INTEL)
                    ? OPSZ_4
                    : (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_2 : OPSZ_4));
    case OPSZ_4_rex8_short2: /* rex.w trumps data prefix */
        return (TEST(PREFIX_REX_W, di->prefixes)
                    ? OPSZ_8
                    : (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_2 : OPSZ_4));
    case OPSZ_4_rex8: return (TEST(PREFIX_REX_W, di->prefixes) ? OPSZ_8 : OPSZ_4);
    case OPSZ_6_irex10_short4: /* rex.w trumps data prefix, but is ignored on AMD */
        DODEBUG({
            /* less annoying than a CURIOSITY assert when testing */
            if (TEST(PREFIX_REX_W, di->prefixes))
                SYSLOG_INTERNAL_INFO_ONCE("curiosity: rex.w on OPSZ_6_irex10_short4!");
        });
        return ((TEST(PREFIX_REX_W, di->prefixes) && proc_get_vendor() != VENDOR_AMD)
                    ? OPSZ_10
                    : (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_4 : OPSZ_6));
    case OPSZ_6x10: return (X64_MODE(di) ? OPSZ_10 : OPSZ_6);
    case OPSZ_8_short2: return (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_2 : OPSZ_8);
    case OPSZ_8_short4: return (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_4 : OPSZ_8);
    case OPSZ_8_rex16: return (TEST(PREFIX_REX_W, di->prefixes) ? OPSZ_16 : OPSZ_8);
    case OPSZ_8_rex16_short4: /* rex.w trumps data prefix */
        return (TEST(PREFIX_REX_W, di->prefixes)
                    ? OPSZ_16
                    : (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_4 : OPSZ_8));
    case OPSZ_12_rex40_short6: /* rex.w trumps data prefix */
        return (TEST(PREFIX_REX_W, di->prefixes)
                    ? OPSZ_40
                    : (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_6 : OPSZ_12));
    case OPSZ_16_vex32: return (TEST(PREFIX_VEX_L, di->prefixes) ? OPSZ_32 : OPSZ_16);
    case OPSZ_32_short16: return (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_16 : OPSZ_32);
    case OPSZ_28_short14: return (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_14 : OPSZ_28);
    case OPSZ_108_short94: return (TEST(PREFIX_DATA, di->prefixes) ? OPSZ_94 : OPSZ_108);
    case OPSZ_1_reg4:
    case OPSZ_2_reg4:
    case OPSZ_4_reg16: return resolve_var_reg_size(sz, is_reg);
    /* The _of_ types are not exposed to the user so convert here */
    case OPSZ_1_of_16: return OPSZ_1;
    case OPSZ_2_of_8:
    case OPSZ_2_of_16: return OPSZ_2;
    case OPSZ_4_of_8:
    case OPSZ_4_of_16: return OPSZ_4;
    case OPSZ_4_rex8_of_16: return (TEST(PREFIX_REX_W, di->prefixes) ? OPSZ_8 : OPSZ_4);
    case OPSZ_8_of_16: return OPSZ_8;
    case OPSZ_12_of_16: return OPSZ_12;
    case OPSZ_12_rex8_of_16: return (TEST(PREFIX_REX_W, di->prefixes) ? OPSZ_8 : OPSZ_12);
    case OPSZ_14_of_16: return OPSZ_14;
    case OPSZ_15_of_16: return OPSZ_15;
    case OPSZ_16_of_32:
    case OPSZ_16_of_32_evex64: return OPSZ_16;
    case OPSZ_32_of_64: return OPSZ_32;
    case OPSZ_4_of_32_evex64: return OPSZ_4;
    case OPSZ_8_of_32_evex64: return OPSZ_8;
    case OPSZ_16_vex32_evex64:
        /* XXX i#1312: There may be a conflict since LL' is also used for rounding
         * control in AVX-512 if used in combination.
         */
        return (TEST(PREFIX_EVEX_LL, di->prefixes)
                    ? OPSZ_64
                    : (TEST(PREFIX_VEX_L, di->prefixes) ? OPSZ_32 : OPSZ_16));
    case OPSZ_vex32_evex64:
        /* XXX i#1312: There may be a conflict since LL' is also used for rounding
         * control in AVX-512 if used in combination.
         */
        return (TEST(PREFIX_EVEX_LL, di->prefixes) ? OPSZ_64 : OPSZ_32);
    case OPSZ_half_16_vex32: return (TEST(PREFIX_VEX_L, di->prefixes) ? OPSZ_16 : OPSZ_8);
    case OPSZ_half_16_vex32_evex64:
        return (TEST(PREFIX_EVEX_LL, di->prefixes)
                    ? OPSZ_32
                    : (TEST(PREFIX_VEX_L, di->prefixes) ? OPSZ_16 : OPSZ_8));
    case OPSZ_quarter_16_vex32:
        return (TEST(PREFIX_VEX_L, di->prefixes) ? OPSZ_8 : OPSZ_4);
    case OPSZ_quarter_16_vex32_evex64:
        return (TEST(PREFIX_EVEX_LL, di->prefixes)
                    ? OPSZ_16
                    : (TEST(PREFIX_VEX_L, di->prefixes) ? OPSZ_8 : OPSZ_4));
    case OPSZ_eighth_16_vex32:
        return (TEST(PREFIX_VEX_L, di->prefixes) ? OPSZ_4 : OPSZ_2);
    case OPSZ_eighth_16_vex32_evex64:
        return (TEST(PREFIX_EVEX_LL, di->prefixes)
                    ? OPSZ_8
                    : (TEST(PREFIX_VEX_L, di->prefixes) ? OPSZ_4 : OPSZ_2));
    case OPSZ_8x16: return IF_X64_ELSE(OPSZ_16, OPSZ_8);
    }

    return sz;
}

opnd_size_t
expand_subreg_size(opnd_size_t sz)
{
    /* XXX i#1312: please note the comment in decode_reg. For mixed vector register sizes
     * within the instruction, this is fragile and relies on the fact that we return
     * OPSZ_16 or OPSZ_32 here. This should be handled in a better way.
     */
    switch (sz) {
    case OPSZ_2_of_8:
    case OPSZ_4_of_8: return OPSZ_8;
    case OPSZ_1_of_16:
    case OPSZ_2_of_16:
    case OPSZ_4_of_16:
    case OPSZ_4_rex8_of_16:
    case OPSZ_8_of_16:
    case OPSZ_12_of_16:
    case OPSZ_12_rex8_of_16:
    case OPSZ_14_of_16:
    case OPSZ_15_of_16:
    case OPSZ_4_reg16: return OPSZ_16;
    case OPSZ_16_of_32: return OPSZ_32;
    case OPSZ_32_of_64: return OPSZ_64;
    case OPSZ_half_16_vex32: return OPSZ_16_vex32;
    case OPSZ_half_16_vex32_evex64: return OPSZ_16_vex32_evex64;
    case OPSZ_quarter_16_vex32: return OPSZ_half_16_vex32;
    case OPSZ_quarter_16_vex32_evex64: return OPSZ_half_16_vex32_evex64;
    case OPSZ_eighth_16_vex32: return OPSZ_quarter_16_vex32;
    case OPSZ_eighth_16_vex32_evex64: return OPSZ_quarter_16_vex32_evex64;
    }
    return sz;
}

opnd_size_t
resolve_variable_size_dc(dcontext_t *dcontext, uint prefixes, opnd_size_t sz, bool is_reg)
{
    decode_info_t di;
    IF_X64(di.x86_mode = get_x86_mode(dcontext));
    di.prefixes = prefixes;
    return resolve_variable_size(&di, sz, is_reg);
}

opnd_size_t
resolve_addr_size(decode_info_t *di /*IN: x86_mode, prefixes*/)
{
    if (TEST(PREFIX_ADDR, di->prefixes))
        return (X64_MODE(di) ? OPSZ_4 : OPSZ_2);
    else
        return (X64_MODE(di) ? OPSZ_8 : OPSZ_4);
}

bool
optype_is_indir_reg(int optype)
{
    switch (optype) {
    case TYPE_INDIR_VAR_XREG:
    case TYPE_INDIR_VAR_XREG_OFFS_1:
    case TYPE_INDIR_VAR_XREG_OFFS_N:
    case TYPE_INDIR_VAR_XIREG:
    case TYPE_INDIR_VAR_XIREG_OFFS_1:
    case TYPE_INDIR_VAR_REG:
    case TYPE_INDIR_VAR_REG_OFFS_2:
    case TYPE_INDIR_VAR_REG_SIZEx2:
    case TYPE_INDIR_VAR_XREG_OFFS_8:
    case TYPE_INDIR_VAR_XREG_SIZEx8:
    case TYPE_INDIR_VAR_REG_SIZEx3x5: return true;
    }
    return false;
}

opnd_size_t
indir_var_reg_size(decode_info_t *di, int optype)
{
    switch (optype) {
    case TYPE_INDIR_VAR_XREG:
    case TYPE_INDIR_VAR_XREG_OFFS_1:
    case TYPE_INDIR_VAR_XREG_OFFS_N:
        /* non-zero immed int adds additional, but we require client to handle that
         * b/c our decoding and encoding can't see the rest of the operands
         */
        return OPSZ_VARSTACK;

    case TYPE_INDIR_VAR_XIREG:
    case TYPE_INDIR_VAR_XIREG_OFFS_1: return OPSZ_ret;

    case TYPE_INDIR_VAR_REG: return OPSZ_REXVARSTACK;

    case TYPE_INDIR_VAR_REG_OFFS_2:
    case TYPE_INDIR_VAR_REG_SIZEx2: return OPSZ_8_rex16_short4;

    case TYPE_INDIR_VAR_XREG_OFFS_8:
    case TYPE_INDIR_VAR_XREG_SIZEx8: return OPSZ_32_short16;

    case TYPE_INDIR_VAR_REG_SIZEx3x5: return OPSZ_12_rex40_short6;

    default: CLIENT_ASSERT(false, "internal error: invalid indir reg type");
    }
    return OPSZ_0;
}

/* Returns multiplier of the operand size to use as the base-disp offs */
int
indir_var_reg_offs_factor(int optype)
{
    switch (optype) {
    case TYPE_INDIR_VAR_XREG_OFFS_1:
    case TYPE_INDIR_VAR_XREG_OFFS_8:
    case TYPE_INDIR_VAR_XREG_OFFS_N:
    case TYPE_INDIR_VAR_XIREG_OFFS_1:
    case TYPE_INDIR_VAR_REG_OFFS_2: return -1;
    }
    return 0;
}

/****************************************************************************
 * Reading all bytes of instruction
 */

static byte *
read_immed(byte *pc, decode_info_t *di, opnd_size_t size, ptr_int_t *result)
{
    size = resolve_variable_size(di, size, false);

    /* all data immediates are sign-extended.  we use the compiler's casts with
     * signed types to do our sign extensions for us.
     */
    switch (size) {
    case OPSZ_1:
        *result = (ptr_int_t)(sbyte)*pc; /* sign-extend */
        pc++;
        break;
    case OPSZ_2:
        *result = (ptr_int_t) * ((short *)pc); /* sign-extend */
        pc += 2;
        break;
    case OPSZ_4:
        *result = (ptr_int_t) * ((int *)pc); /* sign-extend */
        pc += 4;
        break;
    case OPSZ_8:
        CLIENT_ASSERT(X64_MODE(di), "decode immediate: invalid size");
        CLIENT_ASSERT(sizeof(ptr_int_t) == 8, "decode immediate: internal size error");
        *result = *((ptr_int_t *)pc);
        pc += 8;
        break;
    default:
        /* called internally w/ instr_info_t fields or hardcoded values,
         * so ok to assert */
        CLIENT_ASSERT(false, "decode immediate: unknown size");
    }
    return pc;
}

/* reads any trailing immed bytes */
static byte *
read_operand(byte *pc, decode_info_t *di, byte optype, opnd_size_t opsize)
{
    ptr_int_t val = 0;
    opnd_size_t size = opsize;
    switch (optype) {
    case TYPE_A: {
        CLIENT_ASSERT(!X64_MODE(di), "x64 has no type A instructions");
        /* ok b/c only instr_info_t fields passed */
        CLIENT_ASSERT(opsize == OPSZ_6_irex10_short4, "decode A operand error");
        if (TEST(PREFIX_DATA, di->prefixes)) {
            /* 4-byte immed */
            pc = read_immed(pc, di, OPSZ_4, &val);
#ifdef X64
            if (!X64_MODE(di)) {
                /* we do not want the sign extension that read_immed() applied */
                val &= (ptr_int_t)0x00000000ffffffff;
            }
#endif
            /* ok b/c only instr_info_t fields passed */
            CLIENT_ASSERT(di->size_immed == OPSZ_NA && di->size_immed2 == OPSZ_NA,
                          "decode A operand error");
            di->size_immed = resolve_variable_size(di, opsize, false);
            ASSERT(di->size_immed == OPSZ_4);
            di->immed = val;
        } else {
            /* 6-byte immed */
            ptr_int_t val2 = 0;
            /* little-endian: segment comes last */
            pc = read_immed(pc, di, OPSZ_4, &val2);
            pc = read_immed(pc, di, OPSZ_2, &val);
#ifdef X64
            if (!X64_MODE(di)) {
                /* we do not want the sign extension that read_immed() applied */
                val2 &= (ptr_int_t)0x00000000ffffffff;
            }
#endif
            /* ok b/c only instr_info_t fields passed */
            CLIENT_ASSERT(di->size_immed == OPSZ_NA && di->size_immed2 == OPSZ_NA,
                          "decode A operand error");
            di->size_immed = resolve_variable_size(di, opsize, false);
            ASSERT(di->size_immed == OPSZ_6);
            di->size_immed2 = resolve_variable_size(di, opsize, false);
            di->immed = val;
            di->immed2 = val2;
        }
        return pc;
    }
    case TYPE_I: {
        pc = read_immed(pc, di, opsize, &val);
        break;
    }
    case TYPE_J: {
        byte *end_pc;
        di->disp_abs = pc; /* For re-relativization support. */
        pc = read_immed(pc, di, opsize, &val);
        if (di->orig_pc != di->start_pc) {
            CLIENT_ASSERT(di->start_pc != NULL,
                          "internal decode error: start pc not set");
            end_pc = di->orig_pc + (pc - di->start_pc);
        } else
            end_pc = pc;
        /* convert from relative offset to absolute target pc */
        val = ((ptr_int_t)end_pc) + val;
        if ((!X64_MODE(di) || proc_get_vendor() != VENDOR_INTEL) &&
            TEST(PREFIX_DATA, di->prefixes)) {
            /* need to clear upper 16 bits */
            val &= (ptr_int_t)0x0000ffff;
        } /* for x64 Intel, always 64-bit addr ("f64" in Intel table) */
        break;
    }
    case TYPE_L: {
        /* part of AVX: top 4 bits of 8-bit immed select xmm/ymm register */
        pc = read_immed(pc, di, OPSZ_1, &val);
        break;
    }
    case TYPE_O: {
        /* no modrm byte, offset follows directly.  this is address-sized,
         * so 64-bit for x64, and addr prefix affects it. */
        size = resolve_addr_size(di);
        pc = read_immed(pc, di, size, &val);
        if (TEST(PREFIX_ADDR, di->prefixes)) {
            /* need to clear upper bits */
            if (X64_MODE(di))
                val &= (ptr_int_t)0xffffffff;
            else
                val &= (ptr_int_t)0x0000ffff;
        }
#ifdef X64
        if (!X64_MODE(di)) {
            /* we do not want the sign extension that read_immed() applied */
            val &= (ptr_int_t)0x00000000ffffffff;
        }
#endif
        break;
    }
    default: return pc;
    }
    if (di->size_immed == OPSZ_NA) {
        di->size_immed = size;
        di->immed = val;
    } else {
        /* ok b/c only instr_info_t fields passed */
        CLIENT_ASSERT(di->size_immed2 == OPSZ_NA, "decode operand error");
        di->size_immed2 = size;
        di->immed2 = val;
    }
    return pc;
}

/* reads the modrm byte and any following sib and offset bytes */
static byte *
read_modrm(byte *pc, decode_info_t *di)
{
    byte modrm = *pc;
    pc++;
    di->modrm = modrm;
    di->mod = (byte)((modrm >> 6) & 0x3); /* top 2 bits */
    di->reg = (byte)((modrm >> 3) & 0x7); /* middle 3 bits */
    di->rm = (byte)(modrm & 0x7);         /* bottom 3 bits */

    /* addr16 displacement */
    if (!X64_MODE(di) && TEST(PREFIX_ADDR, di->prefixes)) {
        di->has_sib = false;
        if ((di->mod == 0 && di->rm == 6) || di->mod == 2) {
            /* 2-byte disp */
            di->has_disp = true;
            if (di->mod == 0 && di->rm == 6) {
                /* treat absolute addr as unsigned */
                di->disp = (int)*((ushort *)pc); /* zero-extend */
            } else {
                /* treat relative addr as signed */
                di->disp = (int)*((short *)pc); /* sign-extend */
            }
            pc += 2;
        } else if (di->mod == 1) {
            /* 1-byte disp */
            di->has_disp = true;
            di->disp = (int)(sbyte)*pc; /* sign-extend */
            pc++;
        } else {
            di->has_disp = false;
        }
    } else {
        /* 32-bit, which sometimes has a SIB */
        if (di->rm == 4 && di->mod != 3) {
            /* need SIB */
            byte sib = *pc;
            pc++;
            di->has_sib = true;
            di->scale = (byte)((sib >> 6) & 0x3); /* top 2 bits */
            di->index = (byte)((sib >> 3) & 0x7); /* middle 3 bits */
            di->base = (byte)(sib & 0x7);         /* bottom 3 bits */
        } else {
            di->has_sib = false;
        }

        /* displacement */
        if ((di->mod == 0 && di->rm == 5) ||
            (di->has_sib && di->mod == 0 && di->base == 5) || di->mod == 2) {
            /* 4-byte disp */
            di->has_disp = true;
            di->disp = *((int *)pc);
#ifdef X64
            if (X64_MODE(di) && di->mod == 0 && di->rm == 5)
                di->disp_abs = pc; /* Used to set instr->rip_rel_pos. */
#endif
            pc += 4;
        } else if (di->mod == 1) {
            /* 1-byte disp */
            di->has_disp = true;
            di->disp = (int)(sbyte)*pc; /* sign-extend */
            pc++;
        } else {
            di->has_disp = false;
        }
    }
    return pc;
}

/* Given the potential first vex byte at pc, reads any subsequent vex
 * bytes (and any prefix bytes) and sets the appropriate prefix flags in di.
 * Sets info to the entry for the first opcode byte, and pc to point past
 * the first opcode byte.
 * Also handles xop encodings, which are quite similar to vex.
 */
static byte *
read_vex(byte *pc, decode_info_t *di, byte instr_byte,
         const instr_info_t **ret_info INOUT, bool *is_vex /*or xop*/)
{
    int idx = 0;
    const instr_info_t *info;
    byte vex_last = 0, vex_pp;
    ASSERT(ret_info != NULL && *ret_info != NULL && is_vex != NULL);
    info = *ret_info;
    if (info->type == VEX_PREFIX_EXT) {
        /* If 32-bit mode and mod selects for memory, this is not vex */
        if (X64_MODE(di) || TESTALL(MODRM_BYTE(3, 0, 0), *pc))
            idx = 1;
        else
            idx = 0;
        info = &vex_prefix_extensions[info->code][idx];
    } else if (info->type == XOP_PREFIX_EXT) {
        /* If m-mmm (what AMD calls "map_select") < 8, this is not vex */
        if ((*pc & 0x1f) < 0x8)
            idx = 0;
        else
            idx = 1;
        info = &xop_prefix_extensions[info->code][idx];
    } else
        CLIENT_ASSERT(false, "internal vex decoding error");
    if (idx == 0) {
        /* not vex */
        *ret_info = info;
        *is_vex = false;
        return pc;
    }
    *is_vex = true;
    if (TESTANY(PREFIX_REX_ALL | PREFIX_LOCK, di->prefixes) || di->data_prefix ||
        di->rep_prefix || di->repne_prefix) {
        /* #UD if combined w/ VEX prefix */
        *ret_info = &invalid_instr;
        return pc;
    }

    /* read 2nd vex byte */
    instr_byte = *pc;
    pc++;

    if (info->code == PREFIX_VEX_2B) {
        CLIENT_ASSERT(info->type == PREFIX, "internal vex decoding error");
        /* fields are: R, vvvv, L, PP.  R is inverted. */
        vex_last = instr_byte;
        if (!TEST(0x80, vex_last))
            di->prefixes |= PREFIX_REX_R;
        /* 2-byte vex implies leading 0x0f */
        *ret_info = &escape_instr;
        /* rest are shared w/ 3-byte form's final byte */
    } else if (info->code == PREFIX_VEX_3B || info->code == PREFIX_XOP) {
        byte vex_mm;
        CLIENT_ASSERT(info->type == PREFIX, "internal vex decoding error");
        /* fields are: R, X, B, m-mmmm.  R, X, and B are inverted. */
        if (!TEST(0x80, instr_byte))
            di->prefixes |= PREFIX_REX_R;
        if (!TEST(0x40, instr_byte))
            di->prefixes |= PREFIX_REX_X;
        if (!TEST(0x20, instr_byte))
            di->prefixes |= PREFIX_REX_B;
        vex_mm = instr_byte & 0x1f;
        /* our strategy is to decode through the regular tables w/ a vex-encoded
         * flag, to match Intel manuals and vex implicit-prefix flags
         */
        if (info->code == PREFIX_VEX_3B) {
            if (vex_mm == 1) {
                *ret_info = &escape_instr;
            } else if (vex_mm == 2) {
                *ret_info = &escape_38_instr;
            } else if (vex_mm == 3) {
                *ret_info = &escape_3a_instr;
            } else {
                /* #UD: reserved for future use */
                *ret_info = &invalid_instr;
                return pc;
            }
        } else {
            /* xop */
            if (vex_mm == 0x8) {
                *ret_info = &xop_8_instr;
            } else if (vex_mm == 0x9) {
                *ret_info = &xop_9_instr;
            } else if (vex_mm == 0xa) {
                *ret_info = &xop_a_instr;
            } else {
                /* #UD: reserved for future use */
                *ret_info = &invalid_instr;
                return pc;
            }
        }

        /* read 3rd vex byte */
        vex_last = *pc;
        pc++;
        /* fields are: W, vvvv, L, PP */
        /* Intel docs say vex.W1 behaves just like rex.w except in cases
         * where rex.w is ignored, so no need for a PREFIX_VEX_W flag
         */
        if (TEST(0x80, vex_last))
            di->prefixes |= PREFIX_REX_W;
        /* rest are shared w/ 2-byte form's final byte */
    } else
        CLIENT_ASSERT(false, "internal vex decoding error");

    /* shared vex fields */
    vex_pp = vex_last & 0x03;
    di->vex_vvvv = (vex_last & 0x78) >> 3;
    if (TEST(0x04, vex_last))
        di->prefixes |= PREFIX_VEX_L;
    if (vex_pp == 0x1)
        di->data_prefix = true;
    else if (vex_pp == 0x2)
        di->rep_prefix = true;
    else if (vex_pp == 0x3)
        di->repne_prefix = true;

    di->vex_encoded = true;
    return pc;
}

/* Given the potential first evex byte at pc, reads any subsequent evex
 * bytes (and any prefix bytes) and sets the appropriate prefix flags in di.
 * Sets info to the entry for the first opcode byte, and pc to point past
 * the first opcode byte.
 */
static byte *
read_evex(byte *pc, decode_info_t *di, byte instr_byte,
          const instr_info_t **ret_info INOUT, bool *is_evex)
{
    const instr_info_t *info;
    byte prefix_byte = 0, evex_pp = 0;
    ASSERT(ret_info != NULL && *ret_info != NULL && is_evex != NULL);
    info = *ret_info;

    CLIENT_ASSERT(info->type == EVEX_PREFIX_EXT, "internal evex decoding error");
    /* If 32-bit mode and mod selects for memory, this is not evex */
    if (X64_MODE(di) || TESTALL(MODRM_BYTE(3, 0, 0), *pc)) {
        /* P[3:2] must be 0 and P[10] must be 1, otherwise #UD */
        if (TEST(0xC, *pc) || !TEST(0x04, *(pc + 1))) {
            *ret_info = &invalid_instr;
            return pc;
        }
        *is_evex = true;
        info = &evex_prefix_extensions[0][1];
    } else {
        /* not evex */
        *is_evex = false;
        *ret_info = &evex_prefix_extensions[0][0];
        return pc;
    }

    CLIENT_ASSERT(info->code == PREFIX_EVEX, "internal evex decoding error");

    /* read 2nd evex byte */
    instr_byte = *pc;
    prefix_byte = instr_byte;
    pc++;

    if (TESTANY(PREFIX_REX_ALL | PREFIX_LOCK, di->prefixes) || di->data_prefix ||
        di->rep_prefix || di->repne_prefix) {
        /* #UD if combined w/ EVEX prefix */
        *ret_info = &invalid_instr;
        return pc;
    }

    CLIENT_ASSERT(info->type == PREFIX, "internal evex decoding error");
    /* Fields are: R, X, B, R', 00, mm.  R, X, B and R' are inverted. Intel's
     * Software Developer's Manual Vol-2A 2.6 AVX-512 ENCODING fails to mention
     * explicitly the fact that the bits are inverted in order to make the prefix
     * distinct from the bound instruction in 32-bit mode. We experimentally
     * confirmed.
     */
    if (!TEST(0x80, prefix_byte))
        di->prefixes |= PREFIX_REX_R;
    if (!TEST(0x40, prefix_byte))
        di->prefixes |= PREFIX_REX_X;
    if (!TEST(0x20, prefix_byte))
        di->prefixes |= PREFIX_REX_B;
    if (!TEST(0x10, prefix_byte))
        di->prefixes |= PREFIX_EVEX_RR;

    byte evex_mm = instr_byte & 0x3;

    if (evex_mm == 1) {
        *ret_info = &escape_instr;
    } else if (evex_mm == 2) {
        *ret_info = &escape_38_instr;
    } else if (evex_mm == 3) {
        *ret_info = &escape_3a_instr;
    } else {
        /* #UD: reserved for future use */
        *ret_info = &invalid_instr;
        return pc;
    }

    /* read 3rd evex byte */
    prefix_byte = *pc;
    pc++;

    /* fields are: W, vvvv, 1, PP */
    if (TEST(0x80, prefix_byte)) {
        di->prefixes |= PREFIX_REX_W;
    }

    evex_pp = prefix_byte & 0x03;
    di->evex_vvvv = (prefix_byte & 0x78) >> 3;

    if (evex_pp == 0x1)
        di->data_prefix = true;
    else if (evex_pp == 0x2)
        di->rep_prefix = true;
    else if (evex_pp == 0x3)
        di->repne_prefix = true;

    /* read 4th evex byte */
    prefix_byte = *pc;
    pc++;

    /* fields are: z, L', L, b, V' and aaa */
    if (TEST(0x80, prefix_byte))
        di->prefixes |= PREFIX_EVEX_z;
    if (TEST(0x40, prefix_byte))
        di->prefixes |= PREFIX_EVEX_LL;
    if (TEST(0x20, prefix_byte))
        di->prefixes |= PREFIX_VEX_L;
    if (TEST(0x10, prefix_byte))
        di->prefixes |= PREFIX_EVEX_b;
    if (!TEST(0x08, prefix_byte))
        di->prefixes |= PREFIX_EVEX_VV;

    di->evex_aaa = prefix_byte & 0x07;
    di->evex_encoded = true;
    return pc;
}

/* Given an instr_info_t PREFIX_EXT entry, reads the next entry based on the prefixes.
 * Note that this function does not initialize the opcode field in \p di but is set in
 * \p info->type.
 */
static inline const instr_info_t *
read_prefix_ext(const instr_info_t *info, decode_info_t *di)
{
    /* discard old info, get new one */
    int code = (int)info->code;
    /* The order here matters: rep, then repne, then data (i#2431). */
    int idx = (di->rep_prefix ? 1 : (di->repne_prefix ? 3 : (di->data_prefix ? 2 : 0)));
    ASSERT(!(di->rep_prefix && di->repne_prefix));
    if (di->vex_encoded)
        idx += 4;
    else if (di->evex_encoded)
        idx += 8;
    info = &prefix_extensions[code][idx];
    if (info->type == INVALID && !DYNAMO_OPTION(decode_strict)) {
        /* i#1118: some of these seem to not be invalid with
         * prefixes that land in blank slots in the decode tables.
         * Though it seems to only be btc, bsf, and bsr (the SSE*
         * instrs really do seem invalid when given unlisted prefixes),
         * we'd rather err on the side of treating as valid, which is
         * after all what gdb and dumpbin list.  Even if these
         * fault when executed, we know the length, so there's no
         * downside to listing as valid, for DR anyway.
         * Users of drdecodelib may want to be more aggressive: hence the
         * -decode_strict option.
         */
        /* Take the base entry w/o prefixes and keep the prefixes */
        if (di->evex_encoded) {
            /* i#3713/i#1312: Raise an error for investigation, but don't assert b/c
             * we need to support decoding non-code for drdecode, etc.
             */
            SYSLOG_INTERNAL_ERROR_ONCE("Possible unsupported evex encoding.");
        }
        info = &prefix_extensions[code][0 + (di->vex_encoded ? 4 : 0)];
    } else if (di->rep_prefix)
        di->rep_prefix = false;
    else if (di->repne_prefix)
        di->repne_prefix = false;
    if (di->data_prefix &&
        /* Don't remove it if the entry doesn't list 0x66:
         * e.g., OP_bsr (i#1118).
         */
        (info->opcode >> 24) == DATA_PREFIX_OPCODE)
        di->data_prefix = false;
    if (info->type == REX_B_EXT) {
        /* discard old info, get new one */
        code = (int)info->code;
        idx = (TEST(PREFIX_REX_B, di->prefixes) ? 1 : 0);
        info = &rex_b_extensions[code][idx];
    }
    return info;
}

/* Disassembles the instruction at pc into the data structures ret_info
 * and di.  Does NOT set or read di->len.
 * Returns a pointer to the pc of the next instruction.
 * If just_opcode is true, does not decode the immeds and returns NULL
 * (you must call decode_next_pc to get the next pc, but that's faster
 * than decoding the immeds)
 * Returns NULL on an invalid instruction
 */
static byte *
read_instruction(byte *pc, byte *orig_pc, const instr_info_t **ret_info,
                 decode_info_t *di, bool just_opcode _IF_DEBUG(bool report_invalid))
{
    DEBUG_DECLARE(byte *post_suffix_pc = NULL;)
    byte instr_byte;
    const instr_info_t *info;
    bool vex_noprefix = false;
    bool evex_noprefix = false;

    /* initialize di */
    /* though we only need di->start_pc for full decode rip-rel (and
     * there only post-read_instruction()) and decode_from_copy(), and
     * di->orig_pc only for decode_from_copy(), we assume that
     * high-perf decoding uses decode_cti() and live w/ the extra
     * writes here for decode_opcode() and decode_eflags_usage().
     */
    di->start_pc = pc;
    di->orig_pc = orig_pc;
    di->size_immed = OPSZ_NA;
    di->size_immed2 = OPSZ_NA;
    di->seg_override = REG_NULL;
    di->data_prefix = false;
    di->rep_prefix = false;
    di->repne_prefix = false;
    di->vex_encoded = false;
    di->evex_encoded = false;
    di->disp_abs = 0;
    /* FIXME: set data and addr sizes to current mode
     * for now I assume always 32-bit mode (or 64 for X64_MODE(di))!
     */
    di->prefixes = 0;

    do {
        instr_byte = *pc;
        pc++;
        info = &first_byte[instr_byte];
        if (info->type == X64_EXT) {
            /* discard old info, get new one */
            info = &x64_extensions[info->code][X64_MODE(di) ? 1 : 0];
        } else if (info->type == VEX_PREFIX_EXT || info->type == XOP_PREFIX_EXT) {
            bool is_vex = false; /* or xop */
            pc = read_vex(pc, di, instr_byte, &info, &is_vex);
            /* if read_vex changes info, leave this loop */
            if (info->type != VEX_PREFIX_EXT && info->type != XOP_PREFIX_EXT)
                break;
            else {
                if (is_vex)
                    vex_noprefix = true; /* staying in loop, but ensure no prefixes */
                continue;
            }
        } else if (info->type == EVEX_PREFIX_EXT) {
            bool is_evex = false;
            pc = read_evex(pc, di, instr_byte, &info, &is_evex);
            /* if read_evex changes info, leave this loop */
            if (info->type != EVEX_PREFIX_EXT)
                break;
            else {
                if (is_evex)
                    evex_noprefix = true; /* staying in loop, but ensure no prefixes */
                continue;
            }
        }
        if (info->type == PREFIX) {
            if (vex_noprefix || evex_noprefix) {
                /* VEX/EVEX prefix must be last */
                info = &invalid_instr;
                break;
            }
            if (TESTANY(PREFIX_REX_ALL, di->prefixes)) {
                /* rex.* must come after all other prefixes (including those that are
                 * part of the opcode, xref PR 271878): so discard them if before
                 * matching the behavior of decode_sizeof().  This in effect nops
                 * improperly placed rex prefixes which (xref PR 241563 and Intel Manual
                 * 2A 2.2.1) is the correct thing to do. NOTE - windbg shows early bytes
                 * as ??, objdump as their prefix names, separate from the next instr.
                 */
                di->prefixes &= ~PREFIX_REX_ALL;
            }
            if (info->code == PREFIX_REP) {
                /* see if used as part of opcode before considering prefix */
                di->rep_prefix = true;
                di->repne_prefix = false;
            } else if (info->code == PREFIX_REPNE) {
                /* see if used as part of opcode before considering prefix */
                di->repne_prefix = true;
                di->rep_prefix = false;
            } else if (REG_START_SEGMENT <= info->code &&
                       info->code <= REG_STOP_SEGMENT) {
                CLIENT_ASSERT_TRUNCATE(di->seg_override, ushort, info->code,
                                       "decode error: invalid segment override");
                if (!X64_MODE(di) || REG_START_SEGMENT_x64 <= info->code)
                    di->seg_override = (reg_id_t)info->code;
            } else if (info->code == PREFIX_DATA) {
                /* see if used as part of opcode before considering prefix */
                di->data_prefix = true;
            } else if (TESTANY(PREFIX_REX_ALL | PREFIX_ADDR | PREFIX_LOCK, info->code)) {
                di->prefixes |= info->code;
            }
        } else
            break;
    } while (true);

    if (info->type == ESCAPE) {
        /* discard first byte, move to second */
        instr_byte = *pc;
        pc++;
        info = &second_byte[instr_byte];
    }
    if (info->type == ESCAPE_3BYTE_38 || info->type == ESCAPE_3BYTE_3a) {
        /* discard second byte, move to third */
        instr_byte = *pc;
        pc++;
        if (info->type == ESCAPE_3BYTE_38)
            info = &third_byte_38[third_byte_38_index[instr_byte]];
        else
            info = &third_byte_3a[third_byte_3a_index[instr_byte]];
    } else if (info->type == XOP_8_EXT || info->type == XOP_9_EXT ||
               info->type == XOP_A_EXT) {
        /* discard second byte, move to third */
        int idx = 0;
        instr_byte = *pc;
        pc++;
        if (info->type == XOP_8_EXT)
            idx = xop_8_index[instr_byte];
        else if (info->type == XOP_9_EXT)
            idx = xop_9_index[instr_byte];
        else if (info->type == XOP_A_EXT)
            idx = xop_a_index[instr_byte];
        else
            CLIENT_ASSERT(false, "internal invalid XOP type");
        info = &xop_extensions[idx];
    }

    /* all FLOAT_EXT and PREFIX_EXT (except nop & pause) and EXTENSION need modrm,
     * get it now
     */
    if ((info->flags & HAS_MODRM) != 0)
        pc = read_modrm(pc, di);

    if (info->type == FLOAT_EXT) {
        if (di->modrm <= 0xbf) {
            int offs = (instr_byte - 0xd8) * 8 + di->reg;
            info = &float_low_modrm[offs];
        } else {
            int offs1 = (instr_byte - 0xd8);
            int offs2 = di->modrm - 0xc0;
            info = &float_high_modrm[offs1][offs2];
        }
    } else if (info->type == REP_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = (di->rep_prefix ? 2 : 0);
        info = &rep_extensions[code][idx];
        if (di->rep_prefix)
            di->rep_prefix = false;
    } else if (info->type == REPNE_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = (di->rep_prefix ? 2 : (di->repne_prefix ? 4 : 0));
        info = &repne_extensions[code][idx];
        di->rep_prefix = false;
        di->repne_prefix = false;
    } else if (info->type == EXTENSION) {
        /* discard old info, get new one */
        info = &base_extensions[info->code][di->reg];
        /* absurd cases of using prefix on top of reg opcode extension
         * (pslldq, psrldq) => PREFIX_EXT can happen after here,
         * and MOD_EXT after that
         */
    } else if (info->type == SUFFIX_EXT) {
        /* Discard old info, get new one for complete opcode, which includes
         * a suffix byte where an immed would be (yes, ugly!).
         * We should have already read in the modrm (+ sib).
         */
        CLIENT_ASSERT(TEST(HAS_MODRM, info->flags), "decode error on 3DNow instr");
        info = &suffix_extensions[suffix_index[*pc]];
        pc++;
        DEBUG_DECLARE(post_suffix_pc = pc;)
    } else if (info->type == VEX_L_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = (di->vex_encoded) ? (TEST(PREFIX_VEX_L, di->prefixes) ? 2 : 1) : 0;
        info = &vex_L_extensions[code][idx];
    } else if (info->type == VEX_W_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = (TEST(PREFIX_REX_W, di->prefixes) ? 1 : 0);
        info = &vex_W_extensions[code][idx];
    } else if (info->type == EVEX_Wb_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = (TEST(PREFIX_REX_W, di->prefixes) ? 2 : 0) +
            (TEST(PREFIX_EVEX_b, di->prefixes) ? 1 : 0);
        info = &evex_Wb_extensions[code][idx];
    }

    /* can occur AFTER above checks (EXTENSION, in particular) */
    if (info->type == PREFIX_EXT) {
        /* discard old info, get new one */
        info = read_prefix_ext(info, di);
    }

    /* can occur AFTER above checks (PREFIX_EXT, in particular) */
    if (info->type == MOD_EXT) {
        info = &mod_extensions[info->code][(di->mod == 3) ? 1 : 0];
        /* Yes, we have yet another layer, thanks to Intel's poor choice
         * in opcodes -- why didn't they fill out the PREFIX_EXT space?
         */
        if (info->type == RM_EXT) {
            info = &rm_extensions[info->code][di->rm];
        }
        /* We have to support prefix before mod, and mod before prefix */
        if (info->type == PREFIX_EXT) {
            info = read_prefix_ext(info, di);
        }
    }

    /* can occur AFTER above checks (MOD_EXT, in particular) */
    if (info->type == E_VEX_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = 0;
        if (di->vex_encoded)
            idx = 1;
        else if (di->evex_encoded)
            idx = 2;
        info = &e_vex_extensions[code][idx];
    }

    /* can occur AFTER above checks (EXTENSION, in particular) */
    if (info->type == PREFIX_EXT) {
        /* discard old info, get new one */
        info = read_prefix_ext(info, di);
    }

    /* can occur AFTER above checks (MOD_EXT, in particular) */
    if (info->type == REX_W_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = (TEST(PREFIX_REX_W, di->prefixes) ? 1 : 0);
        info = &rex_w_extensions[code][idx];
    } else if (info->type == VEX_L_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = (di->vex_encoded) ? (TEST(PREFIX_VEX_L, di->prefixes) ? 2 : 1) : 0;
        info = &vex_L_extensions[code][idx];
    } else if (info->type == VEX_W_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = (TEST(PREFIX_REX_W, di->prefixes) ? 1 : 0);
        info = &vex_W_extensions[code][idx];
    } else if (info->type == EVEX_Wb_EXT) {
        /* discard old info, get new one */
        int code = (int)info->code;
        int idx = (TEST(PREFIX_REX_W, di->prefixes) ? 2 : 0) +
            (TEST(PREFIX_EVEX_b, di->prefixes) ? 1 : 0);
        info = &evex_Wb_extensions[code][idx];
    }

    /* This can occur after the above checks (with EVEX_Wb_EXT, in particular). */
    if (info->type == MOD_EXT) {
        info = &mod_extensions[info->code][(di->mod == 3) ? 1 : 0];
    }

    if (TEST(REQUIRES_PREFIX, info->flags)) {
        byte required = (byte)(info->opcode >> 24);
        bool *prefix_var = NULL;
        if (required == 0) { /* cannot have a prefix */
            if (prefix_var != NULL) {
                /* invalid instr */
                info = NULL;
            }
        } else {
            CLIENT_ASSERT(info->opcode > 0xffffff, "decode error in SSSE3/SSE4 instr");
            if (required == DATA_PREFIX_OPCODE)
                prefix_var = &di->data_prefix;
            else if (required == REPNE_PREFIX_OPCODE)
                prefix_var = &di->repne_prefix;
            else if (required == REP_PREFIX_OPCODE)
                prefix_var = &di->rep_prefix;
            else
                CLIENT_ASSERT(false, "internal required-prefix error");
            if (prefix_var == NULL || !*prefix_var) {
                /* Invalid instr.  TODO: have processor w/ SSE4, confirm that
                 * an exception really is raised.
                 */
                info = NULL;
            } else
                *prefix_var = false;
        }
    }

    /* we go through regular tables for vex but only some are valid w/ vex */
    if (info != NULL && di->vex_encoded) {
        if (!TEST(REQUIRES_VEX, info->flags))
            info = NULL; /* invalid encoding */
        else if (TEST(REQUIRES_VEX_L_0, info->flags) && TEST(PREFIX_VEX_L, di->prefixes))
            info = NULL; /* invalid encoding */
        else if (TEST(REQUIRES_VEX_L_1, info->flags) && !TEST(PREFIX_VEX_L, di->prefixes))
            info = NULL; /* invalid encoding */
    } else if (info != NULL && !di->vex_encoded && TEST(REQUIRES_VEX, info->flags)) {
        info = NULL; /* invalid encoding */
    } else if (info != NULL && di->evex_encoded) {
        if (!TEST(REQUIRES_EVEX, info->flags))
            info = NULL; /* invalid encoding */
        else if (TEST(REQUIRES_VEX_L_0, info->flags) && TEST(PREFIX_VEX_L, di->prefixes))
            info = NULL; /* invalid encoding */
        else if (TEST(REQUIRES_EVEX_LL_0, info->flags) &&
                 TEST(PREFIX_EVEX_LL, di->prefixes))
            info = NULL; /* invalid encoding */
        else if (TEST(REQUIRES_NOT_K0, info->flags) && di->evex_aaa == 0)
            info = NULL; /* invalid encoding */
    } else if (info != NULL && !di->evex_encoded && TEST(REQUIRES_EVEX, info->flags))
        info = NULL; /* invalid encoding */
    /* XXX: not currently marking these cases as invalid instructions:
     * - if no TYPE_H:
     *   "Note: In VEX-encoded versions, VEX.vvvv is reserved and must be 1111b otherwise
     *   instructions will #UD."
     * - "an attempt to execute VTESTPS with VEX.W=1 will cause #UD."
     * and similar for VEX.W.
     */

    /* at this point should be an instruction, so type should be an OP_ constant */
    if (info == NULL || info == &invalid_instr || info->type < OP_FIRST ||
        info->type > OP_LAST || (X64_MODE(di) && TEST(X64_INVALID, info->flags)) ||
        (!X64_MODE(di) && TEST(X86_INVALID, info->flags))) {
        /* invalid instruction: up to caller to decide what to do with it */
        /* FIXME case 10672: provide a runtime option to specify new
         * instruction formats */
        DODEBUG({
            /* don't report when decoding DR addresses, as we sometimes try to
             * decode backward (e.g., interrupted_inlined_syscall(): PR 605161)
             * XXX: better to pass in a flag when decoding that we are
             * being speculative!
             */
            if (report_invalid && !is_dynamo_address(di->start_pc)) {
                SYSLOG_INTERNAL_WARNING_ONCE("Invalid opcode encountered");
                if (info != NULL && info->type == INVALID) {
                    LOG(THREAD_GET, LOG_ALL, 1, "Invalid opcode @" PFX ": 0x%x\n",
                        di->start_pc, info->opcode);
                } else {
                    int i;
                    dcontext_t *dcontext = get_thread_private_dcontext();
                    IF_X64(bool old_mode = set_x86_mode(dcontext, di->x86_mode);)
                    int sz = decode_sizeof(dcontext, di->start_pc, NULL _IF_X64(NULL));
                    IF_X64(set_x86_mode(dcontext, old_mode));
                    LOG(THREAD_GET, LOG_ALL, 1,
                        "Error decoding " PFX " == ", di->start_pc);
                    for (i = 0; i < sz; i++) {
                        LOG(THREAD_GET, LOG_ALL, 1, "0x%x ", *(di->start_pc + i));
                    }
                    LOG(THREAD_GET, LOG_ALL, 1, "\n");
                }
            }
        });
        *ret_info = &invalid_instr;
        return NULL;
    }

#ifdef INTERNAL
    DODEBUG({ /* rep & repne should have been completely handled by now */
              /* processor will typically ignore extra prefixes, but we log this
               * internally in case it's our decode messing up instead of weird app instrs
               */
              bool spurious = report_invalid && (di->rep_prefix || di->repne_prefix);
              if (spurious) {
                  if (di->rep_prefix &&
                      /* case 6861: AMD64 opt: "rep ret" used if br tgt or after cbr */
                      pc == di->start_pc + 2 && *(di->start_pc + 1) == RAW_OPCODE_ret)
                      spurious = false;
                  if (di->repne_prefix) {
                      /* i#1899: MPX puts repne prior to branches.  We ignore here until
                       * we have full MPX decoding support (i#3581).
                       */
                      /* XXX: We assume the x86 instr_is_* routines only need the opcode.
                       * That is not true for ARM.
                       */
                      instr_t inst;
                      inst.opcode = info->type;
                      if (instr_is_cti(&inst))
                          spurious = false;
                  }
              }
              if (spurious) {
                  char bytes[17 * 3];
                  int i;
                  dcontext_t *dcontext = get_thread_private_dcontext();
                  IF_X64(bool old_mode = set_x86_mode(dcontext, di->x86_mode);)
                  int sz = decode_sizeof(dcontext, di->start_pc, NULL _IF_X64(NULL));
                  IF_X64(set_x86_mode(dcontext, old_mode));
                  CLIENT_ASSERT(sz <= 17, "decode rep/repne error: unsupported opcode?");
                  for (i = 0; i < sz; i++)
                      snprintf(&bytes[i * 3], 3, "%02x ", *(di->start_pc + i));
                  bytes[sz * 3 - 1] = '\0'; /* -1 to kill trailing space */
                  SYSLOG_INTERNAL_WARNING_ONCE(
                      "spurious rep/repne prefix @" PFX " (%s): ", di->start_pc, bytes);
              }
    });
#endif

    /* if just want opcode, stop here!  faster for caller to
     * separately call decode_next_pc than for us to decode immeds!
     */
    if (just_opcode) {
        *ret_info = info;
        return NULL;
    }

    if (di->data_prefix) {
        /* prefix was not part of opcode, it's a real prefix */
        /* From Intel manual:
         *   "For non-byte operations: if a 66H prefix is used with
         *   prefix (REX.W = 1), 66H is ignored."
         * That means non-byte-specific operations, for which 66H is
         * ignored as well, right?
         * Xref PR 593593.
         * Note that this means we could assert or remove some of
         * the "rex.w trumps data prefix" logic elsewhere in this file.
         */
        if (TEST(PREFIX_REX_W, di->prefixes)) {
            LOG(THREAD_GET, LOG_ALL, 3, "Ignoring 0x66 in presence of rex.w @" PFX "\n",
                di->start_pc);
        } else {
            di->prefixes |= PREFIX_DATA;
        }
    }
    if ((di->repne_prefix || di->rep_prefix) &&
        (TEST(PREFIX_LOCK, di->prefixes) ||
         /* xrelease can go on non-0xa3 mov_st w/o lock prefix */
         (di->repne_prefix && info->type == OP_mov_st &&
          (info->opcode & 0xa30000) != 0xa30000))) {
        /* we don't go so far as to ensure the mov_st is of the right type */
        if (di->repne_prefix)
            di->prefixes |= PREFIX_XACQUIRE;
        if (di->rep_prefix)
            di->prefixes |= PREFIX_XRELEASE;
    }

    /* read any trailing immediate bytes */
    if (info->dst1_type != TYPE_NONE)
        pc = read_operand(pc, di, info->dst1_type, info->dst1_size);
    if (info->dst2_type != TYPE_NONE)
        pc = read_operand(pc, di, info->dst2_type, info->dst2_size);
    if (info->src1_type != TYPE_NONE)
        pc = read_operand(pc, di, info->src1_type, info->src1_size);
    if (info->src2_type != TYPE_NONE)
        pc = read_operand(pc, di, info->src2_type, info->src2_size);
    if (info->src3_type != TYPE_NONE)
        pc = read_operand(pc, di, info->src3_type, info->src3_size);

    if (info->type == SUFFIX_EXT) {
        /* Shouldn't be any more bytes (immed bytes) read after the modrm+suffix! */
        DODEBUG({ CLIENT_ASSERT(pc == post_suffix_pc, "decode error on 3DNow instr"); });
    }

    /* return values */
    *ret_info = info;
    return pc;
}

/****************************************************************************
 * Full decoding
 */

/* Caller must check for rex.{r,b} extensions before calling this routine */
static reg_id_t
reg8_alternative(decode_info_t *di, reg_id_t reg, uint prefixes)
{
    if (X64_MODE(di) && reg >= REG_START_x86_8 && reg <= REG_STOP_x86_8 &&
        TESTANY(PREFIX_REX_ALL, prefixes)) {
        /* for x64, if any rex prefix exists, we use SPL...SDL instead of
         * AH..BH (this seems to be the only use of 0x40 == PREFIX_REX_GENERAL)
         */
        return (reg - REG_START_x86_8 + REG_START_x64_8);
    }
    return reg;
}

/* which register within modrm, vex or evex we're decoding */
typedef enum {
    DECODE_REG_REG,
    DECODE_REG_BASE,
    DECODE_REG_INDEX,
    DECODE_REG_RM,
    DECODE_REG_VEX,
    DECODE_REG_EVEX,
    DECODE_REG_OPMASK,
} decode_reg_t;

/* Pass in the raw opsize, NOT a size passed through resolve_variable_size(),
 * to avoid allowing OPSZ_6_irex10_short4 w/ data16.
 * To create a sub-sized register, caller must set size separately.
 */
static reg_id_t
decode_reg(decode_reg_t which_reg, decode_info_t *di, byte optype, opnd_size_t opsize)
{
    bool extend = false;
    bool avx512_extend = false;
    byte reg = 0;
    switch (which_reg) {
    case DECODE_REG_REG:
        reg = di->reg;
        extend = X64_MODE(di) && TEST(PREFIX_REX_R, di->prefixes);
        avx512_extend = TEST(PREFIX_EVEX_RR, di->prefixes);
        break;
    case DECODE_REG_BASE:
        reg = di->base;
        extend = X64_MODE(di) && TEST(PREFIX_REX_B, di->prefixes);
        break;
    case DECODE_REG_INDEX:
        reg = di->index;
        extend = X64_MODE(di) && TEST(PREFIX_REX_X, di->prefixes);
        avx512_extend = TEST(PREFIX_EVEX_VV, di->prefixes);
        break;
    case DECODE_REG_RM:
        reg = di->rm;
        extend = X64_MODE(di) && TEST(PREFIX_REX_B, di->prefixes);
        if (di->evex_encoded)
            avx512_extend = TEST(PREFIX_REX_X, di->prefixes);
        break;
    case DECODE_REG_VEX:
        /* Part of XOP/AVX: vex.vvvv selects general-purpose register.
         * It has 4 bits so no separate prefix bit is needed to extend.
         */
        reg = (~di->vex_vvvv) & 0xf; /* bit-inverted */
        extend = false;
        avx512_extend = false;
        break;
    case DECODE_REG_EVEX:
        /* Part of AVX-512: evex.vvvv selects general-purpose register.
         * It has 4 bits so no separate prefix bit is needed to extend.
         * Intel's Software Developer's Manual Vol-2A 2.6 AVX-512 ENCODING fails to
         * mention the fact that the bits are inverted in the EVEX prefix. Experimentally
         * confirmed.
         */
        reg = (~di->evex_vvvv) & 0xf; /* bit-inverted */
        extend = false;
        avx512_extend = TEST(PREFIX_EVEX_VV, di->prefixes); /* bit-inverted */
        break;
    case DECODE_REG_OPMASK:
        /* Part of AVX-512: evex.aaa selects opmask register. */
        reg = di->evex_aaa & 0x7;
        break;
    default: CLIENT_ASSERT(false, "internal unknown reg error");
    }

    switch (optype) {
    case TYPE_P:
    case TYPE_Q:
    case TYPE_P_MODRM: return (REG_START_MMX + reg); /* no x64 extensions */
    case TYPE_H:
    case TYPE_V:
    case TYPE_W:
    case TYPE_V_MODRM:
    case TYPE_VSIB: {
        reg_id_t extend_reg = extend ? reg + 8 : reg;
        extend_reg = avx512_extend ? extend_reg + 16 : extend_reg;
        /* Some instructions (those that support embedded rounding (er) control)
         * repurpose PREFIX_EVEX_LL for other things and only come in a 64 byte
         * variant.
         */
        bool operand_is_zmm = (TEST(PREFIX_EVEX_LL, di->prefixes) &&
                               expand_subreg_size(opsize) != OPSZ_16 &&
                               expand_subreg_size(opsize) != OPSZ_32) ||
            opsize == OPSZ_64;
        /* Not only do we use this for VEX .LIG and EVEX .LIG (where raw reg is
         * either OPSZ_16 or OPSZ_16_vex32 or OPSZ_32 or OPSZ_vex32_evex64) but
         * also for VSIB which currently does not get up to OPSZ_16 so we can
         * use this negative check.
         * XXX i#1312: vgather/vscatter VSIB addressing may be OPSZ_16?
         * For EVEX .LIG, raw reg will be able to be OPSZ_64 or
         * OPSZ_16_vex32_evex64.
         * XXX i#1312: improve this code here, it is not very robust. For AVX-512, this
         * relies on the fact that cases where EVEX.LL' == 1 and register is not zmm, the
         * expand_subreg_size is OPSZ_16 or OPSZ_32. The VEX OPSZ_16 case is also fragile.
         * As above PREFIX_EVEX_LL may be repurposed for embedded rounding control, so
         * honor opsizes of exactly OPSZ_32.
         */
        bool operand_is_ymm = (TEST(PREFIX_EVEX_LL, di->prefixes) &&
                               expand_subreg_size(opsize) == OPSZ_32) ||
            (TEST(PREFIX_VEX_L, di->prefixes) && expand_subreg_size(opsize) != OPSZ_16 &&
             expand_subreg_size(opsize) != OPSZ_64) ||
            opsize == OPSZ_32;
        if (operand_is_ymm && operand_is_zmm) {
            /* i#3713/i#1312: Raise an error for investigation, but don't assert b/c
             * we need to support decoding non-code for drdecode, etc.
             */
            SYSLOG_INTERNAL_ERROR_ONCE("Invalid VSIB register encoding encountered");
        }
        return (operand_is_zmm ? (DR_REG_START_ZMM + extend_reg)
                               : (operand_is_ymm ? (REG_START_YMM + extend_reg)
                                                 : (REG_START_XMM + extend_reg)));
    }
    case TYPE_S:
        if (reg >= 6)
            return REG_NULL;
        return (REG_START_SEGMENT + reg);
    case TYPE_C: return (extend ? (REG_START_CR + 8 + reg) : (REG_START_CR + reg));
    case TYPE_D: return (extend ? (REG_START_DR + 8 + reg) : (REG_START_DR + reg));
    case TYPE_K_REG:
    case TYPE_K_MODRM:
    case TYPE_K_MODRM_R:
    case TYPE_K_VEX:
    case TYPE_K_EVEX:
        /* This can happen if the fourth inverted evex.vvvv bit is not 0 and needs to
         * be treated as an illegal encoding (xref i#3719).
         */
        if (reg > DR_REG_STOP_OPMASK - DR_REG_START_OPMASK)
            return REG_NULL;
        return DR_REG_START_OPMASK + reg;
    case TYPE_T_MODRM:
    case TYPE_T_REG:
        if (reg > DR_REG_STOP_BND - DR_REG_START_BND)
            return REG_NULL;
        return DR_REG_START_BND + reg;
    case TYPE_E:
    case TYPE_G:
    case TYPE_R:
    case TYPE_B:
    case TYPE_M:
    case TYPE_INDIR_E:
    case TYPE_FLOATMEM:
        /* GPR: fall-through since variable subset of full register */
        break;
    default: CLIENT_ASSERT(false, "internal unknown reg error");
    }

    /* Do not allow a register for 'p' or 'a' types.  FIXME: maybe *_far_ind_* should
     * use TYPE_INDIR_M instead of TYPE_INDIR_E?  What other things are going to turn
     * into asserts or crashes instead of invalid instrs based on events as fragile
     * as these decode routines moving sizes around?
     */
    if (opsize != OPSZ_6_irex10_short4 && opsize != OPSZ_8_short4)
        opsize = resolve_variable_size(di, opsize, true);

    switch (opsize) {
    case OPSZ_1:
        if (extend)
            return (REG_START_8 + 8 + reg);
        else
            return reg8_alternative(di, REG_START_8 + reg, di->prefixes);
    case OPSZ_2: return (extend ? (REG_START_16 + 8 + reg) : (REG_START_16 + reg));
    case OPSZ_4: return (extend ? (REG_START_32 + 8 + reg) : (REG_START_32 + reg));
    case OPSZ_8: return (extend ? (REG_START_64 + 8 + reg) : (REG_START_64 + reg));
    case OPSZ_6:
    case OPSZ_6_irex10_short4:
    case OPSZ_8_short4:
        /* invalid: no register of size p */
        return REG_NULL;
    default:
        /* ok to assert since params controlled by us */
        CLIENT_ASSERT(false, "decode error: unknown register size");
        return REG_NULL;
    }
}

static bool
decode_modrm(decode_info_t *di, byte optype, opnd_size_t opsize, opnd_t *reg_opnd,
             opnd_t *rm_opnd)
{
    /* for x64, addr prefix affects only base/index and truncates final addr:
     * modrm + sib table is the same
     */
    bool addr16 = !X64_MODE(di) && TEST(PREFIX_ADDR, di->prefixes);

    if (reg_opnd != NULL) {
        reg_id_t reg = decode_reg(DECODE_REG_REG, di, optype, opsize);
        if (reg == REG_NULL)
            return false;
        *reg_opnd = opnd_create_reg(reg);
        opnd_set_size(reg_opnd, resolve_variable_size(di, opsize, true /*is reg*/));
    }

    if (rm_opnd != NULL) {
        reg_id_t base_reg = REG_NULL;
        int disp = 0;
        reg_id_t index_reg = REG_NULL;
        int scale = 0;
        char memtype = (optype == TYPE_VSIB ? TYPE_VSIB : TYPE_M);
        opnd_size_t memsize = resolve_addr_size(di);
        bool encode_zero_disp, force_full_disp;
        if (di->has_disp)
            disp = di->disp;
        else
            disp = 0;
        if (di->has_sib) {
            CLIENT_ASSERT(!addr16, "decode error: x86 addr16 cannot have a SIB byte");
            if (di->index == 4 &&
                /* rex.x enables r12 as index */
                (!X64_MODE(di) || !TEST(PREFIX_REX_X, di->prefixes)) &&
                optype != TYPE_VSIB) {
                /* no scale/index */
                index_reg = REG_NULL;
            } else {
                index_reg = decode_reg(DECODE_REG_INDEX, di, memtype, memsize);
                if (index_reg == REG_NULL) {
                    CLIENT_ASSERT(false, "decode error: !index: internal modrm error");
                    return false;
                }
                if (di->scale == 0)
                    scale = 1;
                else if (di->scale == 1)
                    scale = 2;
                else if (di->scale == 2)
                    scale = 4;
                else if (di->scale == 3)
                    scale = 8;
            }
            if (di->base == 5 && di->mod == 0) {
                /* no base */
                base_reg = REG_NULL;
            } else {
                base_reg = decode_reg(DECODE_REG_BASE, di, TYPE_M, memsize);
                if (base_reg == REG_NULL) {
                    CLIENT_ASSERT(false, "decode error: internal modrm decode error");
                    return false;
                }
            }
        } else {
            if (optype == TYPE_VSIB)
                return false; /* invalid w/o vsib byte */
            if ((!addr16 && di->mod == 0 && di->rm == 5) ||
                (addr16 && di->mod == 0 && di->rm == 6)) {
                /* just absolute displacement, or rip-relative for x64 */
#ifdef X64
                if (X64_MODE(di)) {
                    /* rip-relative: convert from relative offset to absolute target pc */
                    byte *addr;
                    CLIENT_ASSERT(di->start_pc != NULL,
                                  "internal decode error: start pc not set");
                    if (di->orig_pc != di->start_pc)
                        addr = di->orig_pc + di->len + di->disp;
                    else
                        addr = di->start_pc + di->len + di->disp;
                    if (TEST(PREFIX_ADDR, di->prefixes)) {
                        /* Need to clear upper 32 bits.
                         * Debuggers do not display this truncation, though
                         * both Intel and AMD manuals describe it.
                         * I did verify it w/ actual execution.
                         */
                        ASSERT_NOT_TESTED();
                        addr = (byte *)((ptr_uint_t)addr & 0xffffffff);
                    }
                    *rm_opnd = opnd_create_far_rel_addr(
                        di->seg_override, (void *)addr,
                        resolve_variable_size(di, opsize, false));
                    return true;
                } else
#endif
                    base_reg = REG_NULL;
                index_reg = REG_NULL;
            } else if (di->mod == 3) {
                /* register */
                reg_id_t rm_reg = decode_reg(DECODE_REG_RM, di, optype, opsize);
                if (rm_reg == REG_NULL) /* no assert since happens, e.g., ff d9 */
                    return false;
                else {
                    *rm_opnd = opnd_create_reg(rm_reg);
                    opnd_set_size(rm_opnd,
                                  resolve_variable_size(di, opsize, true /*is reg*/));
                    return true;
                }
            } else {
                /* non-sib reg-based memory address */
                if (addr16) {
                    /* funny order requiring custom decode */
                    switch (di->rm) {
                    case 0:
                        base_reg = REG_BX;
                        index_reg = REG_SI;
                        scale = 1;
                        break;
                    case 1:
                        base_reg = REG_BX;
                        index_reg = REG_DI;
                        scale = 1;
                        break;
                    case 2:
                        base_reg = REG_BP;
                        index_reg = REG_SI;
                        scale = 1;
                        break;
                    case 3:
                        base_reg = REG_BP;
                        index_reg = REG_DI;
                        scale = 1;
                        break;
                    case 4: base_reg = REG_SI; break;
                    case 5: base_reg = REG_DI; break;
                    case 6:
                        base_reg = REG_BP;
                        CLIENT_ASSERT(di->mod != 0,
                                      "decode error: %bp cannot have mod 0");
                        break;
                    case 7: base_reg = REG_BX; break;
                    default:
                        CLIENT_ASSERT(false, "decode error: unknown modrm rm");
                        break;
                    }
                } else {
                    /* single base reg */
                    base_reg = decode_reg(DECODE_REG_RM, di, memtype, memsize);
                    if (base_reg == REG_NULL) {
                        CLIENT_ASSERT(false,
                                      "decode error: !base: internal modrm decode error");
                        return false;
                    }
                }
            }
        }
        /* We go ahead and preserve the force bools if the original really had a 0
         * disp; up to user to unset bools when changing disp value (FIXME: should
         * we auto-unset on first mod?)
         */
        encode_zero_disp = di->has_disp && disp == 0 &&
            /* there is no bp base without a disp */
            (!addr16 || base_reg != REG_BP);
        /* With evex encoding, disp8 is subject to compression and a scale factor.
         * Hence, displacments not divisible by the scale factor need to be encoded
         * with full displacement, no need (and actually incorrect) to "force" it.
         */
        bool needs_full_disp = false;
        int compressed_disp_scale = 0;
        if (di->evex_encoded) {
            compressed_disp_scale = decode_get_compressed_disp_scale(di);
            if (compressed_disp_scale == -1)
                return false;
            if (di->mod == 1)
                disp *= compressed_disp_scale;
            else
                needs_full_disp = disp % compressed_disp_scale != 0;
        }
        force_full_disp = !needs_full_disp && di->has_disp && disp >= INT8_MIN &&
            disp <= INT8_MAX && di->mod == 2;
        if (di->seg_override != REG_NULL) {
            *rm_opnd = opnd_create_far_base_disp_ex(
                di->seg_override, base_reg, index_reg, scale, disp,
                resolve_variable_size(di, opsize, false), encode_zero_disp,
                force_full_disp, TEST(PREFIX_ADDR, di->prefixes));
        } else {
            /* Note that OP_{jmp,call}_far_ind does NOT have a far base disp
             * operand: it is a regular base disp containing 6 bytes that
             * specify a segment selector and address.  The opcode must be
             * examined to know how to interpret those 6 bytes.
             */
            *rm_opnd = opnd_create_base_disp_ex(base_reg, index_reg, scale, disp,
                                                resolve_variable_size(di, opsize, false),
                                                encode_zero_disp, force_full_disp,
                                                TEST(PREFIX_ADDR, di->prefixes));
        }
    }
    return true;
}

static ptr_int_t
get_immed(decode_info_t *di, opnd_size_t opsize)
{
    ptr_int_t val = 0;
    if (di->size_immed == OPSZ_NA) {
        /* ok b/c only instr_info_t fields passed */
        CLIENT_ASSERT(di->size_immed2 != OPSZ_NA, "decode immediate size error");
        val = di->immed2;
        di->size_immed2 = OPSZ_NA; /* mark as used up */
    } else {
        /* ok b/c only instr_info_t fields passed */
        CLIENT_ASSERT(di->size_immed != OPSZ_NA, "decode immediate size error");
        val = di->immed;
        di->size_immed = OPSZ_NA; /* mark as used up */
    }
    return val;
}

/* Also takes in reg8 for TYPE_REG_EX mov_imm */
reg_id_t
resolve_var_reg(decode_info_t *di /*IN: x86_mode, prefixes*/, reg_id_t reg32, bool addr,
                bool can_shrink _IF_X64(bool default_64) _IF_X64(bool can_grow)
                    _IF_X64(bool extendable))
{
#ifdef X64
    if (extendable && X64_MODE(di) && di->prefixes != 0 /*optimization*/) {
        /* Note that Intel's table 3-1 on +r possibilities is incorrect:
         * it lists rex.r, while Table 2-4 lists rex.b which is correct.
         */
        if (TEST(PREFIX_REX_B, di->prefixes))
            reg32 = reg32 + 8;
        else
            reg32 = reg8_alternative(di, reg32, di->prefixes);
    }
#endif

    if (addr) {
#ifdef X64
        if (X64_MODE(di)) {
            CLIENT_ASSERT(default_64, "addr-based size must be default 64");
            if (!can_shrink || !TEST(PREFIX_ADDR, di->prefixes))
                return reg_32_to_64(reg32);
            /* else leave 32 (it's addr32 not addr16) */
        } else
#endif
            if (can_shrink && TEST(PREFIX_ADDR, di->prefixes))
            return reg_32_to_16(reg32);
    } else {
#ifdef X64
        /* rex.w trumps data prefix */
        if (X64_MODE(di) &&
            ((can_grow && TEST(PREFIX_REX_W, di->prefixes)) ||
             (default_64 && (!can_shrink || !TEST(PREFIX_DATA, di->prefixes)))))
            return reg_32_to_64(reg32);
        else
#endif
            if (can_shrink && TEST(PREFIX_DATA, di->prefixes))
            return reg_32_to_16(reg32);
    }
    return reg32;
}

static reg_id_t
ds_seg(decode_info_t *di)
{
    if (di->seg_override != REG_NULL) {
#ifdef X64
        /* Although the AMD docs say that es,cs,ss,ds prefixes are NOT treated as
         * segment override prefixes and instead as NULL prefixes, Intel docs do not
         * say that, and both gdb and windbg disassemble as though the prefixes are
         * taking effect.  We therefore do not suppress those prefixes.
         */
#endif
        return di->seg_override;
    }
    return SEG_DS;
}

static bool
decode_operand(decode_info_t *di, byte optype, opnd_size_t opsize, opnd_t *opnd)
{
    /* resolving here, for non-reg, makes for simpler code: though the
     * most common types don't need this.
     */
    opnd_size_t ressize = resolve_variable_size(di, opsize, false);
    switch (optype) {
    case TYPE_NONE: *opnd = opnd_create_null(); return true;
    case TYPE_REG:
        *opnd = opnd_create_reg(opsize);
        /* here and below, for all TYPE_*REG*: no need to set size as it's a GPR */
        return true;
    case TYPE_XREG:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, false /*!addr*/,
                                                false /*!shrinkable*/
                                                _IF_X64(true /*d64*/)
                                                    _IF_X64(false /*!growable*/)
                                                        _IF_X64(false /*!extendable*/)));
        return true;
    case TYPE_VAR_REG:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, false /*!addr*/,
                                                true /*shrinkable*/
                                                _IF_X64(false /*d32*/)
                                                    _IF_X64(true /*growable*/)
                                                        _IF_X64(false /*!extendable*/)));
        return true;
    case TYPE_VARZ_REG:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, false /*!addr*/,
                                                true /*shrinkable*/
                                                _IF_X64(false /*d32*/)
                                                    _IF_X64(false /*!growable*/)
                                                        _IF_X64(false /*!extendable*/)));
        return true;
    case TYPE_VAR_XREG:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, false /*!addr*/,
                                                true /*shrinkable*/
                                                _IF_X64(true /*d64*/)
                                                    _IF_X64(false /*!growable*/)
                                                        _IF_X64(false /*!extendable*/)));
        return true;
    case TYPE_VAR_REGX:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, false /*!addr*/,
                                                false /*!shrinkable*/
                                                _IF_X64(false /*!d64*/)
                                                    _IF_X64(true /*growable*/)
                                                        _IF_X64(false /*!extendable*/)));
        return true;
    case TYPE_VAR_ADDR_XREG:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, true /*addr*/,
                                                true /*shrinkable*/
                                                _IF_X64(true /*d64*/)
                                                    _IF_X64(false /*!growable*/)
                                                        _IF_X64(false /*!extendable*/)));
        return true;
    case TYPE_REG_EX:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, false /*!addr*/,
                                                false /*!shrink*/
                                                _IF_X64(false /*d32*/)
                                                    _IF_X64(false /*!growable*/)
                                                        _IF_X64(true /*extendable*/)));
        return true;
    case TYPE_VAR_REG_EX:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, false /*!addr*/,
                                                true /*shrinkable*/
                                                _IF_X64(false /*d32*/)
                                                    _IF_X64(true /*growable*/)
                                                        _IF_X64(true /*extendable*/)));
        return true;
    case TYPE_VAR_XREG_EX:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, false /*!addr*/,
                                                true /*shrinkable*/
                                                _IF_X64(true /*d64*/)
                                                    _IF_X64(false /*!growable*/)
                                                        _IF_X64(true /*extendable*/)));
        return true;
    case TYPE_VAR_REGX_EX:
        *opnd = opnd_create_reg(resolve_var_reg(di, opsize, false /*!addr*/,
                                                false /*!shrink*/
                                                _IF_X64(false /*d64*/)
                                                    _IF_X64(true /*growable*/)
                                                        _IF_X64(true /*extendable*/)));
        return true;
    case TYPE_FLOATMEM:
    case TYPE_M:
    case TYPE_VSIB:
        /* ensure referencing memory */
        if (di->mod >= 3)
            return false;
        /* fall through */
    case TYPE_E:
    case TYPE_Q:
    case TYPE_W: return decode_modrm(di, optype, opsize, NULL, opnd);
    case TYPE_R:
    case TYPE_P_MODRM:
    case TYPE_V_MODRM:
        /* ensure referencing a register */
        if (di->mod != 3)
            return false;
        return decode_modrm(di, optype, opsize, NULL, opnd);
    case TYPE_G:
    case TYPE_P:
    case TYPE_V:
    case TYPE_S:
    case TYPE_C:
    case TYPE_D: return decode_modrm(di, optype, opsize, opnd, NULL);
    case TYPE_I:
        *opnd = opnd_create_immed_int(get_immed(di, opsize), ressize);
        return true;
    case TYPE_1:
        CLIENT_ASSERT(opsize == OPSZ_0, "internal decode inconsistency");
        *opnd = opnd_create_immed_int(1, ressize);
        return true;
    case TYPE_FLOATCONST:
        CLIENT_ASSERT(opsize == OPSZ_0, "internal decode inconsistency");
        /* i#386: avoid floating-point instructions */
        *opnd = opnd_create_immed_float_for_opcode(di->opcode);
        return true;
    case TYPE_J:
        if (di->seg_override == SEG_JCC_NOT_TAKEN || di->seg_override == SEG_JCC_TAKEN) {
            /* SEG_DS - taken,     pt */
            /* SEG_CS - not taken, pn */
            /* starting from RH9 I see code using this */
            LOG(THREAD_GET, LOG_EMIT, 5, "disassemble: branch hint %s:\n",
                di->seg_override == SEG_JCC_TAKEN ? "pt" : "pn");
            if (di->seg_override == SEG_JCC_NOT_TAKEN)
                di->prefixes |= PREFIX_JCC_NOT_TAKEN;
            else
                di->prefixes |= PREFIX_JCC_TAKEN;
            di->seg_override = REG_NULL;
            STATS_INC(num_branch_hints);
        }
        /* just ignore other segment prefixes -- don't assert */
        *opnd = opnd_create_pc((app_pc)get_immed(di, opsize));
        return true;
    case TYPE_A: {
        /* ok since instr_info_t fields */
        CLIENT_ASSERT(!X64_MODE(di), "x64 has no type A instructions");
        CLIENT_ASSERT(opsize == OPSZ_6_irex10_short4, "decode A operand error");
        /* just ignore segment prefixes -- don't assert */
        if (TEST(PREFIX_DATA, di->prefixes)) {
            /* 4-byte immed */
            ptr_int_t val = get_immed(di, opsize);
            *opnd = opnd_create_far_pc((ushort)(((ptr_int_t)val & 0xffff0000) >> 16),
                                       (app_pc)((ptr_int_t)val & 0x0000ffff));
        } else {
            /* 6-byte immed */
            /* ok since instr_info_t fields */
            CLIENT_ASSERT(di->size_immed == OPSZ_6 && di->size_immed2 == OPSZ_6,
                          "decode A operand 6-byte immed error");
            ASSERT(CHECK_TRUNCATE_TYPE_short(di->immed));
            *opnd = opnd_create_far_pc((ushort)(short)di->immed, (app_pc)di->immed2);
            di->size_immed = OPSZ_NA;
            di->size_immed2 = OPSZ_NA;
        }
        return true;
    }
    case TYPE_O: {
        /* no modrm byte, offset follows directly */
        ptr_int_t immed = get_immed(di, resolve_addr_size(di));
        *opnd = opnd_create_far_abs_addr(di->seg_override, (void *)immed, ressize);
        return true;
    }
    case TYPE_X:
        /* this means the memory address DS:(E)SI */
        if (!X64_MODE(di) && TEST(PREFIX_ADDR, di->prefixes)) {
            *opnd =
                opnd_create_far_base_disp(ds_seg(di), REG_SI, REG_NULL, 0, 0, ressize);
        } else if (!X64_MODE(di) || TEST(PREFIX_ADDR, di->prefixes)) {
            *opnd =
                opnd_create_far_base_disp(ds_seg(di), REG_ESI, REG_NULL, 0, 0, ressize);
        } else {
            *opnd =
                opnd_create_far_base_disp(ds_seg(di), REG_RSI, REG_NULL, 0, 0, ressize);
        }
        return true;
    case TYPE_Y:
        /* this means the memory address ES:(E)DI */
        if (!X64_MODE(di) && TEST(PREFIX_ADDR, di->prefixes))
            *opnd = opnd_create_far_base_disp(SEG_ES, REG_DI, REG_NULL, 0, 0, ressize);
        else if (!X64_MODE(di) || TEST(PREFIX_ADDR, di->prefixes))
            *opnd = opnd_create_far_base_disp(SEG_ES, REG_EDI, REG_NULL, 0, 0, ressize);
        else
            *opnd = opnd_create_far_base_disp(SEG_ES, REG_RDI, REG_NULL, 0, 0, ressize);
        return true;
    case TYPE_XLAT:
        /* this means the memory address DS:(E)BX+AL */
        if (!X64_MODE(di) && TEST(PREFIX_ADDR, di->prefixes))
            *opnd = opnd_create_far_base_disp(ds_seg(di), REG_BX, REG_AL, 1, 0, ressize);
        else if (!X64_MODE(di) || TEST(PREFIX_ADDR, di->prefixes))
            *opnd = opnd_create_far_base_disp(ds_seg(di), REG_EBX, REG_AL, 1, 0, ressize);
        else
            *opnd = opnd_create_far_base_disp(ds_seg(di), REG_RBX, REG_AL, 1, 0, ressize);
        return true;
    case TYPE_MASKMOVQ:
        /* this means the memory address DS:(E)DI */
        if (!X64_MODE(di) && TEST(PREFIX_ADDR, di->prefixes)) {
            *opnd =
                opnd_create_far_base_disp(ds_seg(di), REG_DI, REG_NULL, 0, 0, ressize);
        } else if (!X64_MODE(di) || TEST(PREFIX_ADDR, di->prefixes)) {
            *opnd =
                opnd_create_far_base_disp(ds_seg(di), REG_EDI, REG_NULL, 0, 0, ressize);
        } else {
            *opnd =
                opnd_create_far_base_disp(ds_seg(di), REG_RDI, REG_NULL, 0, 0, ressize);
        }
        return true;
    case TYPE_INDIR_REG:
        /* FIXME: how know data size?  for now just use reg size: our only use
         * of this does not have a varying hardcoded reg, fortunately. */
        *opnd = opnd_create_base_disp(opsize, REG_NULL, 0, 0, reg_get_size(opsize));
        return true;
    case TYPE_INDIR_VAR_XREG:         /* indirect reg varies by ss only, base is 4x8,
                                       * opsize varies by data16 */
    case TYPE_INDIR_VAR_REG:          /* indirect reg varies by ss only, base is 4x8,
                                       * opsize varies by rex and data16 */
    case TYPE_INDIR_VAR_XIREG:        /* indirect reg varies by ss only, base is 4x8,
                                       * opsize varies by data16 except on 64-bit Intel */
    case TYPE_INDIR_VAR_XREG_OFFS_1:  /* TYPE_INDIR_VAR_XREG + an offset */
    case TYPE_INDIR_VAR_XREG_OFFS_8:  /* TYPE_INDIR_VAR_XREG + an offset + scale */
    case TYPE_INDIR_VAR_XREG_OFFS_N:  /* TYPE_INDIR_VAR_XREG + an offset + scale */
    case TYPE_INDIR_VAR_XIREG_OFFS_1: /* TYPE_INDIR_VAR_XIREG + an offset + scale */
    case TYPE_INDIR_VAR_REG_OFFS_2:   /* TYPE_INDIR_VAR_REG + offset + scale */
    case TYPE_INDIR_VAR_XREG_SIZEx8:  /* TYPE_INDIR_VAR_XREG + scale */
    case TYPE_INDIR_VAR_REG_SIZEx2:   /* TYPE_INDIR_VAR_REG + scale */
    case TYPE_INDIR_VAR_REG_SIZEx3x5: /* TYPE_INDIR_VAR_REG + scale */
    {
        reg_id_t reg = resolve_var_reg(di, opsize, true /*doesn't matter*/,
                                       false /*!shrinkable*/
                                       _IF_X64(true /*d64*/) _IF_X64(false /*!growable*/)
                                           _IF_X64(false /*!extendable*/));
        opnd_size_t sz =
            resolve_variable_size(di, indir_var_reg_size(di, optype), false /*not reg*/);
        /* NOTE - needs to match size in opnd_type_ok() and instr_create_api.h */
        *opnd = opnd_create_base_disp(
            reg, REG_NULL, 0, indir_var_reg_offs_factor(optype) * opnd_size_in_bytes(sz),
            sz);
        return true;
    }
    case TYPE_INDIR_E:
        /* how best mark as indirect?
         * in current usage decode_modrm will be treated as indirect, becoming
         * a base_disp operand, vs. an immed, which becomes a pc operand
         * besides, Ap is just as indirect as i_Ep!
         */
        return decode_operand(di, TYPE_E, opsize, opnd);
    case TYPE_L: {
        CLIENT_ASSERT(!TEST(PREFIX_EVEX_LL, di->prefixes), "XXX i#1312: unsupported.");
        /* part of AVX: top 4 bits of 8-bit immed select xmm/ymm register */
        ptr_int_t immed = get_immed(di, OPSZ_1);
        reg_id_t reg = (reg_id_t)(immed & 0xf0) >> 4;
        *opnd = opnd_create_reg(((TEST(PREFIX_VEX_L, di->prefixes) &&
                                  /* see .LIG notes above */
                                  expand_subreg_size(opsize) != OPSZ_16)
                                     ? REG_START_YMM
                                     : REG_START_XMM) +
                                reg);
        opnd_set_size(opnd, resolve_variable_size(di, opsize, true /*is reg*/));
        return true;
    }
    case TYPE_H: {
        /* As part of AVX and AVX-512, vex.vvvv selects xmm/ymm/zmm register. Note that
         * vex.vvvv and evex.vvvv are a union.
         */
        if (di->evex_encoded)
            *opnd = opnd_create_reg(decode_reg(DECODE_REG_EVEX, di, optype, opsize));
        else
            *opnd = opnd_create_reg(decode_reg(DECODE_REG_VEX, di, optype, opsize));
        opnd_set_size(opnd, resolve_variable_size(di, opsize, true /*is reg*/));
        return true;
    }
    case TYPE_B: {
        /* Part of XOP/AVX/AVX-512: vex.vvvv or evex.vvvv selects general-purpose
         * register.
         */
        if (di->evex_encoded)
            *opnd = opnd_create_reg(decode_reg(DECODE_REG_EVEX, di, optype, opsize));
        else
            *opnd = opnd_create_reg(decode_reg(DECODE_REG_VEX, di, optype, opsize));
        /* no need to set size as it's a GPR */
        return true;
    }
    case TYPE_K_MODRM: {
        /* part of AVX-512: modrm.rm selects opmask register or mem addr */
        if (di->mod != 3) {
            return decode_modrm(di, optype, opsize, NULL, opnd);
        }
        /* fall through*/
    }
    case TYPE_K_MODRM_R: {
        /* part of AVX-512: modrm.rm selects opmask register */
        *opnd = opnd_create_reg(decode_reg(DECODE_REG_RM, di, optype, opsize));
        return true;
    }
    case TYPE_K_REG: {
        /* part of AVX-512: modrm.reg selects opmask register */
        *opnd = opnd_create_reg(decode_reg(DECODE_REG_REG, di, optype, opsize));
        return true;
    }
    case TYPE_K_VEX: {
        /* part of AVX-512: vex.vvvv selects opmask register */
        reg_id_t reg = decode_reg(DECODE_REG_VEX, di, optype, opsize);
        if (reg == REG_NULL)
            return false;
        *opnd = opnd_create_reg(reg);
        return true;
    }
    case TYPE_K_EVEX: {
        /* part of AVX-512: evex.aaa selects opmask register */
        *opnd = opnd_create_reg(decode_reg(DECODE_REG_OPMASK, di, optype, opsize));
        return true;
    }
    case TYPE_T_REG: {
        /* MPX: modrm.reg selects bnd register */
        reg_id_t reg = decode_reg(DECODE_REG_REG, di, optype, opsize);
        if (reg == REG_NULL)
            return false;
        *opnd = opnd_create_reg(reg);
        return true;
    }
    case TYPE_T_MODRM: return decode_modrm(di, optype, opsize, NULL, opnd);
    default:
        /* ok to assert, types coming only from instr_info_t */
        CLIENT_ASSERT(false, "decode error: unknown operand type");
    }
    return false;
}

dr_pred_type_t
decode_predicate_from_instr_info(uint opcode, const instr_info_t *info)
{
    if (TESTANY(HAS_PRED_CC | HAS_PRED_COMPLEX, info->flags)) {
        if (TEST(HAS_PRED_CC, info->flags)) {
            if (opcode >= OP_jo && opcode <= OP_jnle)
                return DR_PRED_O + opcode - OP_jo;
            else if (opcode >= OP_jo_short && opcode <= OP_jnle_short)
                return DR_PRED_O + opcode - OP_jo_short;
            else
                return DR_PRED_O + instr_cmovcc_to_jcc(opcode) - OP_jo;
        } else
            return DR_PRED_COMPLEX;
    }
    return DR_PRED_NONE;
}

/* Helper function to determine the vector length based on EVEX.L, EVEX.L'. */
static opnd_size_t
decode_get_vector_length(bool vex_l, bool evex_ll)
{
    if (!vex_l && !evex_ll)
        return OPSZ_16;
    else if (vex_l && !evex_ll)
        return OPSZ_32;
    else if (!vex_l && evex_ll)
        return OPSZ_64;
    else {
        /* i#3713/i#1312: Raise an error for investigation while we're still solidifying
         * our AVX-512 decoder, but don't assert b/c we need to support decoding non-code
         * for drdecode, etc.
         */
        SYSLOG_INTERNAL_ERROR_ONCE("Invalid AVX-512 vector length encountered.");
    }
    return OPSZ_NA;
}

int
decode_get_compressed_disp_scale(decode_info_t *di)
{
    dr_tuple_type_t tuple_type = di->tuple_type;
    bool broadcast = TEST(PREFIX_EVEX_b, di->prefixes);
    opnd_size_t input_size = di->input_size;
    if (input_size == OPSZ_NA) {
        if (TEST(PREFIX_REX_W, di->prefixes))
            input_size = OPSZ_8;
        else
            input_size = OPSZ_4;
    }

    opnd_size_t vl = decode_get_vector_length(TEST(di->prefixes, PREFIX_VEX_L),
                                              TEST(di->prefixes, PREFIX_EVEX_LL));
    if (vl == OPSZ_NA)
        return -1;
    switch (tuple_type) {
    case DR_TUPLE_TYPE_FV:
        CLIENT_ASSERT(input_size == OPSZ_4 || input_size == OPSZ_8,
                      "invalid input size.");
        if (broadcast) {
            switch (vl) {
            case OPSZ_16: return input_size == OPSZ_4 ? 4 : 8;
            case OPSZ_32: return input_size == OPSZ_4 ? 4 : 8;
            case OPSZ_64: return input_size == OPSZ_4 ? 4 : 8;
            default: CLIENT_ASSERT(false, "invalid vector length.");
            }
        } else {
            switch (vl) {
            case OPSZ_16: return 16;
            case OPSZ_32: return 32;
            case OPSZ_64: return 64;
            default: CLIENT_ASSERT(false, "invalid vector length.");
            }
        }
        break;
    case DR_TUPLE_TYPE_HV:
        CLIENT_ASSERT(input_size == OPSZ_4, "invalid input size.");
        if (broadcast) {
            switch (vl) {
            case OPSZ_16: return 4;
            case OPSZ_32: return 4;
            case OPSZ_64: return 4;
            default: CLIENT_ASSERT(false, "invalid vector length.");
            }
        } else {
            switch (vl) {
            case OPSZ_16: return 8;
            case OPSZ_32: return 16;
            case OPSZ_64: return 32;
            default: CLIENT_ASSERT(false, "invalid vector length.");
            }
        }
        break;
    case DR_TUPLE_TYPE_FVM:
        switch (vl) {
        case OPSZ_16: return 16;
        case OPSZ_32: return 32;
        case OPSZ_64: return 64;
        default: CLIENT_ASSERT(false, "invalid vector length.");
        }
        break;
    case DR_TUPLE_TYPE_T1S:
        CLIENT_ASSERT(vl == OPSZ_16 || vl == OPSZ_32 || vl == OPSZ_64,
                      "invalid vector length.");
        if (input_size == OPSZ_1) {
            return 1;
        } else if (input_size == OPSZ_2) {
            return 2;
        } else if (input_size == OPSZ_4) {
            return 4;
        } else if (input_size == OPSZ_8) {
            return 8;
        } else {
            CLIENT_ASSERT(false, "invalid input size.");
        }
        break;
    case DR_TUPLE_TYPE_T1F:
        CLIENT_ASSERT(vl == OPSZ_16 || vl == OPSZ_32 || vl == OPSZ_64,
                      "invalid vector length.");
        if (input_size == OPSZ_4) {
            return 4;
        } else if (input_size == OPSZ_8) {
            return 8;
        } else {
            CLIENT_ASSERT(false, "invalid input size.");
        }
        break;
    case DR_TUPLE_TYPE_T2:
        if (input_size == OPSZ_4) {
            CLIENT_ASSERT(vl == OPSZ_16 || vl == OPSZ_32 || vl == OPSZ_64,
                          "invalid vector length.");
            return 8;
        } else if (input_size == OPSZ_8) {
            CLIENT_ASSERT(vl == OPSZ_32 || vl == OPSZ_64, "invalid vector length.");
            return 16;
        } else {
            CLIENT_ASSERT(false, "invalid input size.");
        }
        break;
    case DR_TUPLE_TYPE_T4:
        if (input_size == OPSZ_4) {
            CLIENT_ASSERT(vl == OPSZ_32 || vl == OPSZ_64, "invalid vector length.");
            return 16;
        } else if (input_size == OPSZ_8) {
            CLIENT_ASSERT(vl == OPSZ_64, "invalid vector length.");
            return 32;
        } else {
            CLIENT_ASSERT(false, "invalid input size.");
        }
        break;
    case DR_TUPLE_TYPE_T8:
        CLIENT_ASSERT(input_size == OPSZ_4, "invalid input size.");
        CLIENT_ASSERT(vl == OPSZ_64, "invalid vector length.");
        return 32;
    case DR_TUPLE_TYPE_HVM:
        switch (vl) {
        case OPSZ_16: return 8;
        case OPSZ_32: return 16;
        case OPSZ_64: return 32;
        default: CLIENT_ASSERT(false, "invalid vector length.");
        }
        break;
    case DR_TUPLE_TYPE_QVM:
        switch (vl) {
        case OPSZ_16: return 4;
        case OPSZ_32: return 8;
        case OPSZ_64: return 16;
        default: CLIENT_ASSERT(false, "invalid vector length.");
        }
        break;
    case DR_TUPLE_TYPE_OVM:
        switch (vl) {
        case OPSZ_16: return 2;
        case OPSZ_32: return 4;
        case OPSZ_64: return 8;
        default: CLIENT_ASSERT(false, "invalid vector length.");
        }
        break;
    case DR_TUPLE_TYPE_M128:
        switch (vl) {
        case OPSZ_16: return 16;
        case OPSZ_32: return 16;
        case OPSZ_64: return 16;
        default: CLIENT_ASSERT(false, "invalid vector length.");
        }
        break;
    case DR_TUPLE_TYPE_DUP:
        switch (vl) {
        case OPSZ_16: return 8;
        case OPSZ_32: return 32;
        case OPSZ_64: return 64;
        default: CLIENT_ASSERT(false, "invalid vector length.");
        }
        break;
    case DR_TUPLE_TYPE_NONE: return 1;
    default: CLIENT_ASSERT(false, "unknown tuple type."); return -1;
    }
    return -1;
}

void
decode_get_tuple_type_input_size(const instr_info_t *info, decode_info_t *di)
{
    /* The upper DR_TUPLE_TYPE_BITS bits of the flags field are the evex tuple type. */
    di->tuple_type = (dr_tuple_type_t)(info->flags >> DR_TUPLE_TYPE_BITPOS);
    if (TEST(DR_EVEX_INPUT_OPSZ_1, info->flags))
        di->input_size = OPSZ_1;
    else if (TEST(DR_EVEX_INPUT_OPSZ_2, info->flags))
        di->input_size = OPSZ_2;
    else if (TEST(DR_EVEX_INPUT_OPSZ_4, info->flags))
        di->input_size = OPSZ_4;
    else if (TEST(DR_EVEX_INPUT_OPSZ_8, info->flags))
        di->input_size = OPSZ_8;
    else
        di->input_size = OPSZ_NA;
}

/****************************************************************************
 * Exported routines
 */

/* Decodes only enough of the instruction at address pc to determine
 * its eflags usage, which is returned in usage as EFLAGS_ constants
 * or'ed together.
 * This corresponds to halfway between Level 1 and Level 2: a Level 1 decoding
 * plus eflags information (usually only at Level 2).
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instruction.
 *
 * N.B.: an instruction that has an "undefined" effect on eflags is considered
 * to write to eflags.  This is fine since programs shouldn't be reading
 * eflags after an undefined modification to them, but a weird program that
 * relies on some undefined eflag thing might behave differently under dynamo
 * than not!
 */
byte *
decode_eflags_usage(void *drcontext, byte *pc, uint *usage, dr_opnd_query_flags_t flags)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    const instr_info_t *info;
    decode_info_t di;
    IF_X64(di.x86_mode = get_x86_mode(dcontext));

    /* don't decode immeds, instead use decode_next_pc, it's faster */
    read_instruction(pc, pc, &info, &di, true /* just opcode */ _IF_DEBUG(true));

    *usage = instr_eflags_conditionally(
        info->eflags, decode_predicate_from_instr_info(info->type, info), flags);
    pc = decode_next_pc(dcontext, pc);
    /* failure handled fine -- we'll go ahead and return the NULL */

    return pc;
}

/* Decodes the opcode and eflags usage of instruction at address pc
 * into instr.
 * This corresponds to a Level 2 decoding.
 * Assumes that instr is already initialized, but uses the x86/x64 mode
 * for the current thread rather than that set in instr.
 * If caller is re-using same instr struct over multiple decodings,
 * should call instr_reset or instr_reuse.
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instruction.
 */
byte *
decode_opcode(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    const instr_info_t *info;
    decode_info_t di;
    int sz;
    /* PR 251479: we need to know about all rip-relative addresses.
     * Since change/setting raw bits invalidates, we must set this
     * on every return. */
    uint rip_rel_pos;
    IF_X64(di.x86_mode = instr_get_x86_mode(instr));
    /* when pass true to read_instruction it doesn't decode immeds,
     * so have to call decode_next_pc, but that ends up being faster
     * than decoding immeds!
     */
    read_instruction(pc, pc, &info, &di,
                     true /* just opcode */
                     _IF_DEBUG(!TEST(INSTR_IGNORE_INVALID, instr->flags)));
    sz = decode_sizeof_ex(dcontext, pc, NULL, &rip_rel_pos);
    IF_X64(instr_set_x86_mode(instr, get_x86_mode(dcontext)));
    instr_set_opcode(instr, info->type);
    /* read_instruction sets opcode to OP_INVALID for illegal instr.
     * decode_sizeof will return 0 for _some_ illegal instrs, so we
     * check it first since it's faster than instr_valid, but we have to
     * also check instr_valid to catch all illegal instrs.
     */
    if (sz == 0 || !instr_valid(instr)) {
        CLIENT_ASSERT(!instr_valid(instr), "decode_opcode: invalid instr");
        return NULL;
    }
    instr->eflags = info->eflags;
    instr_set_eflags_valid(instr, true);
    /* operands are NOT set */
    instr_set_operands_valid(instr, false);
    /* raw bits are valid though and crucial for encoding */
    instr_set_raw_bits(instr, pc, sz);
    /* must set rip_rel_pos after setting raw bits */
    instr_set_rip_rel_pos(instr, rip_rel_pos);
    return pc + sz;
}

#if defined(DEBUG) && !defined(STANDALONE_DECODER)
/* PR 215143: we must resolve variable sizes at decode time */
static bool
check_is_variable_size(opnd_t op)
{
    if (opnd_is_memory_reference(op) ||
        /* reg_get_size() fails on fp registers since no OPSZ for them */
        (opnd_is_reg(op) && !reg_is_fp(opnd_get_reg(op))))
        return !is_variable_size(opnd_get_size(op));
    /* else no legitimate size to check */
    return true;
}
#endif

/* Decodes the instruction at address pc into instr, filling in the
 * instruction's opcode, eflags usage, prefixes, and operands.
 * This corresponds to a Level 3 decoding.
 * Assumes that instr is already initialized, but uses the x86/x64 mode
 * for the current thread rather than that set in instr.
 * If caller is re-using same instr struct over multiple decodings,
 * should call instr_reset or instr_reuse.
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instruction.
 */
static byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr)
{
    const instr_info_t *info;
    decode_info_t di;
    byte *next_pc;
    int instr_num_dsts = 0, instr_num_srcs = 0;
    opnd_t dsts[8];
    opnd_t srcs[8];

    CLIENT_ASSERT(instr->opcode == OP_INVALID || instr->opcode == OP_UNDECODED,
                  "decode: instr is already decoded, may need to call instr_reset()");

    IF_X64(di.x86_mode = get_x86_mode(dcontext));
    next_pc = read_instruction(pc, orig_pc, &info, &di,
                               false /* not just opcode,
                                        decode operands too */
                               _IF_DEBUG(!TEST(INSTR_IGNORE_INVALID, instr->flags)));
    instr_set_opcode(instr, info->type);
    IF_X64(instr_set_x86_mode(instr, di.x86_mode));
    /* failure up to this point handled fine -- we set opcode to OP_INVALID */
    if (next_pc == NULL) {
        LOG(THREAD, LOG_INTERP, 3, "decode: invalid instr at " PFX "\n", pc);
        CLIENT_ASSERT(!instr_valid(instr), "decode: invalid instr");
        return NULL;
    }
    instr->eflags = info->eflags;
    instr_set_eflags_valid(instr, true);
    /* since we don't use set_src/set_dst we must explicitly say they're valid */
    instr_set_operands_valid(instr, true);
    /* read_instruction doesn't set di.len since only needed for rip-relative opnds */
    IF_X64(
        CLIENT_ASSERT_TRUNCATE(di.len, int, next_pc - pc, "internal truncation error"));
    di.len = (int)(next_pc - pc);
    di.opcode = info->type; /* used for opnd_create_immed_float_for_opcode */

    decode_get_tuple_type_input_size(info, &di);
    instr->prefixes |= di.prefixes;

    /* operands */
    do {
        if (info->dst1_type != TYPE_NONE) {
            if (!decode_operand(&di, info->dst1_type, info->dst1_size,
                                &(dsts[instr_num_dsts++])))
                goto decode_invalid;
            ASSERT(check_is_variable_size(dsts[instr_num_dsts - 1]));
        }
        if (info->dst2_type != TYPE_NONE) {
            if (!decode_operand(&di, info->dst2_type, info->dst2_size,
                                &(dsts[instr_num_dsts++])))
                goto decode_invalid;
            ASSERT(check_is_variable_size(dsts[instr_num_dsts - 1]));
        }
        if (info->src1_type != TYPE_NONE) {
            if (!decode_operand(&di, info->src1_type, info->src1_size,
                                &(srcs[instr_num_srcs++])))
                goto decode_invalid;
            ASSERT(check_is_variable_size(srcs[instr_num_srcs - 1]));
        }
        if (info->src2_type != TYPE_NONE) {
            if (!decode_operand(&di, info->src2_type, info->src2_size,
                                &(srcs[instr_num_srcs++])))
                goto decode_invalid;
            ASSERT(check_is_variable_size(srcs[instr_num_srcs - 1]));
        }
        if (info->src3_type != TYPE_NONE) {
            if (!decode_operand(&di, info->src3_type, info->src3_size,
                                &(srcs[instr_num_srcs++])))
                goto decode_invalid;
            ASSERT(check_is_variable_size(srcs[instr_num_srcs - 1]));
        }
        /* extra operands:
         * we take advantage of the fact that all instructions that need extra
         * operands have only one encoding, so the code field points to instr_info_t
         * structures containing the extra operands
         */
        if ((info->flags & HAS_EXTRA_OPERANDS) != 0) {
            if ((info->flags & EXTRAS_IN_CODE_FIELD) != 0)
                info = (const instr_info_t *)(info->code);
            else /* extra operands are in next entry */
                info = info + 1;
        } else
            break;
    } while (true);

    /* some operands add to di.prefixes so we copy again */
    instr->prefixes |= di.prefixes;
    if (di.seg_override == SEG_FS)
        instr->prefixes |= PREFIX_SEG_FS;
    if (di.seg_override == SEG_GS)
        instr->prefixes |= PREFIX_SEG_GS;

    /* now copy operands into their real slots */
    instr_set_num_opnds(dcontext, instr, instr_num_dsts, instr_num_srcs);
    if (instr_num_dsts > 0) {
        memcpy(instr->dsts, dsts, instr_num_dsts * sizeof(opnd_t));
    }
    if (instr_num_srcs > 0) {
        /* remember that src0 is static */
        instr->src0 = srcs[0];
        if (instr_num_srcs > 1) {
            memcpy(instr->srcs, &(srcs[1]), (instr_num_srcs - 1) * sizeof(opnd_t));
        }
    }

    if (TESTANY(HAS_PRED_CC | HAS_PRED_COMPLEX, info->flags))
        instr_set_predicate(instr, decode_predicate_from_instr_info(di.opcode, info));

    /* check for invalid prefixes that depend on operand types */
    if (TEST(PREFIX_LOCK, di.prefixes)) {
        /* check for invalid opcode, list on p3-397 of IA-32 vol 2 */
        switch (instr_get_opcode(instr)) {
        case OP_add:
        case OP_adc:
        case OP_and:
        case OP_btc:
        case OP_btr:
        case OP_bts:
        case OP_cmpxchg:
        case OP_cmpxchg8b:
        case OP_dec:
        case OP_inc:
        case OP_neg:
        case OP_not:
        case OP_or:
        case OP_sbb:
        case OP_sub:
        case OP_xor:
        case OP_xadd:
        case OP_xchg: {
            /* still illegal unless dest is mem op rather than src */
            CLIENT_ASSERT(instr->num_dsts > 0, "internal lock prefix check error");
            if (!opnd_is_memory_reference(instr->dsts[0])) {
                LOG(THREAD, LOG_INTERP, 3, "decode: invalid lock prefix at " PFX "\n",
                    pc);
                goto decode_invalid;
            }
            break;
        }
        default: {
            LOG(THREAD, LOG_INTERP, 3, "decode: invalid lock prefix at " PFX "\n", pc);
            goto decode_invalid;
        }
        }
    }
    /* PREFIX_XRELEASE is allowed w/o LOCK on mov_st, but use of it or PREFIX_XACQUIRE
     * in other situations does not result in #UD so we ignore.
     */

    if (orig_pc != pc)
        instr_set_translation(instr, orig_pc);
    /* We set raw bits AFTER setting all srcs and dsts b/c setting
     * a src or dst marks instr as having invalid raw bits.
     */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(next_pc - pc)));
    instr_set_raw_bits(instr, pc, (uint)(next_pc - pc));
    if (di.disp_abs > di.start_pc) {
        CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_int(di.disp_abs - di.start_pc),
                      "decode: internal rip-rel error");
        /* We must do this AFTER setting raw bits to avoid being invalidated. */
        instr_set_rip_rel_pos(instr, (int)(di.disp_abs - di.start_pc));
    }

    return next_pc;

decode_invalid:
    instr_set_operands_valid(instr, false);
    instr_set_opcode(instr, OP_INVALID);
    return NULL;
}

byte *
decode(void *drcontext, byte *pc, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return decode_common(dcontext, pc, pc, instr);
}

byte *
decode_from_copy(void *drcontext, byte *copy_pc, byte *orig_pc, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return decode_common(dcontext, copy_pc, orig_pc, instr);
}

const instr_info_t *
get_next_instr_info(const instr_info_t *info)
{
    return (const instr_info_t *)(info->code);
}

byte
decode_first_opcode_byte(int opcode)
{
    const instr_info_t *info = op_instr[opcode];
    return (byte)((info->opcode & 0x00ff0000) >> 16);
}

DR_API
const char *
decode_opcode_name(int opcode)
{
    const instr_info_t *info = op_instr[opcode];
    return info->name;
}

const instr_info_t *
opcode_to_encoding_info(uint opc, dr_isa_mode_t isa_mode)
{
    return op_instr[opc];
}

app_pc
dr_app_pc_as_jump_target(dr_isa_mode_t isa_mode, app_pc pc)
{
    return pc;
}

app_pc
dr_app_pc_as_load_target(dr_isa_mode_t isa_mode, app_pc pc)
{
    return pc;
}

#ifdef DEBUG
void
decode_debug_checks_arch(void)
{
    /* empty */
}
#endif

#ifdef DECODE_UNIT_TEST
#    include "instr_create_shared.h"

/* FIXME: Tried putting this inside a separate unit-decode.c file, but
 *        required creating a unit-decode_table.c file.  Since the
 *        infrastructure is not fully set up, currently leaving this here
 * FIXME: beef up to check if something went wrong
 */
static bool
unit_check_decode_ff_opcode()
{
    static int do_once = 0;
    instr_t instr;
    byte modrm, sib;
    byte raw_bytes[] = {
        0xff, 0x0,  0x0,  0xaa, 0xbb, 0xcc, 0xdd, 0xee,
        0xff, 0xab, 0xbc, 0xcd, 0xde, 0xef, 0xfa,
    };
    app_pc next_pc = NULL;

    for (modrm = 0x0; modrm < 0xff; modrm++) {
        raw_bytes[1] = modrm;
        for (sib = 0x0; sib < 0xff; sib++) {
            raw_bytes[2] = sib;

            /* set up instr for decode_opcode */
            instr_init(GLOBAL_DCONTEXT, &instr);
            instr.bytes = raw_bytes;
            instr.length = 15;
            instr_set_raw_bits_valid(&instr, true);
            instr_set_operands_valid(&instr, false);

            next_pc = decode_opcode(GLOBAL_DCONTEXT, instr.bytes, &instr);
            if (next_pc != NULL && instr.opcode != OP_INVALID &&
                instr.opcode != OP_UNDECODED) {
                print_file(STDERR, "## %02x %02x %02x len=%d\n", instr.bytes[0],
                           instr.bytes[1], instr.bytes[2], instr.length);
            }
        }
    }
    return 0;
}

/* Standalone building is still broken so I tested this by calling
 * from a real DR build.
 */
#    define CHECK_ENCODE_OPCODE(dcontext, instr, pc, opc, ...)           \
        instr = INSTR_CREATE_##opc(dcontext, ##__VA_ARGS__);             \
        instr_encode(dcontext, instr, pc);                               \
        instr_reset(dcontext, instr);                                    \
        decode(dcontext, pc, instr);                                     \
        /* FIXME: use EXPECT */                                          \
        CLIENT_ASSERT(instr_get_opcode(instr) == OP_##opc, "unit test"); \
        instr_destroy(dcontext, instr);

/* FIXME: case 8212: add checks for every single instr type */
static bool
unit_check_sse3()
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    byte buf[32];
    instr_t *instr;
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, mwait);
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, monitor);
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, haddpd, opnd_create_reg(REG_XMM7),
                        opnd_create_reg(REG_XMM2));
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, haddps, opnd_create_reg(REG_XMM7),
                        opnd_create_reg(REG_XMM2));
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, hsubpd, opnd_create_reg(REG_XMM7),
                        opnd_create_reg(REG_XMM2));
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, hsubps, opnd_create_reg(REG_XMM7),
                        opnd_create_reg(REG_XMM2));
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, addsubpd, opnd_create_reg(REG_XMM7),
                        opnd_create_reg(REG_XMM2));
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, addsubps, opnd_create_reg(REG_XMM7),
                        opnd_create_reg(REG_XMM2));
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, lddqu, opnd_create_reg(REG_XMM7),
                        opnd_create_base_disp(REG_NULL, REG_NULL, 0, 0, OPSZ_16));
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, movsldup, opnd_create_reg(REG_XMM7),
                        opnd_create_reg(REG_XMM2));
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, movshdup, opnd_create_reg(REG_XMM7),
                        opnd_create_reg(REG_XMM2));
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, movddup, opnd_create_reg(REG_XMM7),
                        opnd_create_reg(REG_XMM2));
    /* not sse3 but I fixed it at same time so here to test */
    CHECK_ENCODE_OPCODE(dcontext, instr, buf, cmpxchg8b,
                        opnd_create_base_disp(REG_NULL, REG_NULL, 0, 0, OPSZ_8));
    return true;
}

int
main()
{
    bool res;
    standalone_init();
    res = unit_check_sse3();
    res = unit_check_decode_ff_opcode() && res;
    standalone_exit();
    return res;
}

#endif /* DECODE_UNIT_TEST */
