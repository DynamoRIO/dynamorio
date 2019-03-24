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

/* encode.c -- an x86 encoder */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "disassemble.h"
#include "decode_fast.h"
#include "decode_private.h"

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

/* level at which encoding attempts get dumped out...lots of logging! */
#define ENC_LEVEL 6

const char *const type_names[] = {
    "TYPE_NONE",
    "TYPE_A",       /* immediate that is absolute address */
    "TYPE_B",       /* vex.vvvv field selects general-purpose register */
    "TYPE_C",       /* reg of modrm selects control reg */
    "TYPE_D",       /* reg of modrm selects debug reg */
    "TYPE_E",       /* modrm selects reg or mem addr */
    "TYPE_G",       /* reg of modrm selects register */
    "TYPE_H",       /* vex.vvvv field selects xmm/ymm register */
    "TYPE_I",       /* immediate */
    "TYPE_J",       /* immediate that is relative offset of EIP */
    "TYPE_L",       /* top 4 bits of 8-bit immed select xmm/ymm register */
    "TYPE_M",       /* modrm select mem addr */
    "TYPE_O",       /* immediate that is memory offset */
    "TYPE_P",       /* reg of modrm selects MMX */
    "TYPE_Q",       /* modrm selects MMX or mem addr */
    "TYPE_R",       /* mod of modrm selects register */
    "TYPE_S",       /* reg of modrm selects segment register */
    "TYPE_V",       /* reg of modrm selects XMM */
    "TYPE_W",       /* modrm selects XMM or mem addr */
    "TYPE_X",       /* DS:(RE)(E)SI */
    "TYPE_Y",       /* ES:(RE)(E)SDI */
    "TYPE_P_MODRM", /* mod of modrm selects MMX */
    "TYPE_V_MODRM", /* mod of modrm selects XMM */
    "TYPE_1",
    "TYPE_FLOATCONST",
    "TYPE_XLAT",     /* DS:(RE)(E)BX+AL */
    "TYPE_MASKMOVQ", /* DS:(RE)(E)DI */
    "TYPE_FLOATMEM",
    "TYPE_VSIB",
    "TYPE_REG",
    "TYPE_XREG",
    "TYPE_VAR_REG",
    "TYPE_VARZ_REG",
    "TYPE_VAR_XREG",
    "TYPE_VAR_REGX",
    "TYPE_VAR_ADDR_XREG",
    "TYPE_REG_EX",
    "TYPE_VAR_REG_EX",
    "TYPE_VAR_XREG_EX",
    "TYPE_VAR_REGX_EX",
    "TYPE_INDIR_E",
    "TYPE_INDIR_REG",
    "TYPE_INDIR_VAR_XREG",
    "TYPE_INDIR_VAR_REG",
    "TYPE_INDIR_VAR_XIREG",
    "TYPE_INDIR_VAR_XREG_OFFS_1",
    "TYPE_INDIR_VAR_XREG_OFFS_8",
    "TYPE_INDIR_VAR_XREG_OFFS_N",
    "TYPE_INDIR_VAR_XIREG_OFFS_1",
    "TYPE_INDIR_VAR_REG_OFFS_2",
    "TYPE_INDIR_VAR_XREG_SIZEx8",
    "TYPE_INDIR_VAR_REG_SIZEx2",
    "TYPE_INDIR_VAR_REG_SIZEx3x5",
};

/* order corresponds to enum of REG_ and SEG_ constants */
const char *const reg_names[] = {
    "<NULL>", "rax",   "rcx",   "rdx",  "rbx",       "rsp",  "rbp",   "rsi",   "rdi",
    "r8",     "r9",    "r10",   "r11",  "r12",       "r13",  "r14",   "r15",   "eax",
    "ecx",    "edx",   "ebx",   "esp",  "ebp",       "esi",  "edi",   "r8d",   "r9d",
    "r10d",   "r11d",  "r12d",  "r13d", "r14d",      "r15d", "ax",    "cx",    "dx",
    "bx",     "sp",    "bp",    "si",   "di",        "r8w",  "r9w",   "r10w",  "r11w",
    "r12w",   "r13w",  "r14w",  "r15w", "al",        "cl",   "dl",    "bl",    "ah",
    "ch",     "dh",    "bh",    "r8l",  "r9l",       "r10l", "r11l",  "r12l",  "r13l",
    "r14l",   "r15l",  "spl",   "bpl",  "sil",       "dil",  "mm0",   "mm1",   "mm2",
    "mm3",    "mm4",   "mm5",   "mm6",  "mm7",       "xmm0", "xmm1",  "xmm2",  "xmm3",
    "xmm4",   "xmm5",  "xmm6",  "xmm7", "xmm8",      "xmm9", "xmm10", "xmm11", "xmm12",
    "xmm13",  "xmm14", "xmm15", "st0",  "st1",       "st2",  "st3",   "st4",   "st5",
    "st6",    "st7",   "es",    "cs",   "ss",        "ds",   "fs",    "gs",    "dr0",
    "dr1",    "dr2",   "dr3",   "dr4",  "dr5",       "dr6",  "dr7",   "dr8",   "dr9",
    "dr10",   "dr11",  "dr12",  "dr13", "dr14",      "dr15", "cr0",   "cr1",   "cr2",
    "cr3",    "cr4",   "cr5",   "cr6",  "cr7",       "cr8",  "cr9",   "cr10",  "cr11",
    "cr12",   "cr13",  "cr14",  "cr15", "<invalid>", "ymm0", "ymm1",  "ymm2",  "ymm3",
    "ymm4",   "ymm5",  "ymm6",  "ymm7", "ymm8",      "ymm9", "ymm10", "ymm11", "ymm12",
    "ymm13",  "ymm14", "ymm15",
    /* when you update here, update dr_reg_fixer[] too */
};

/* Maps sub-registers to their containing register. */
const reg_id_t dr_reg_fixer[] = {
    REG_NULL,    REG_XAX,   REG_XCX,   REG_XDX,  REG_XBX,   REG_XSP,   REG_XBP,
    REG_XSI,     REG_XDI,   REG_R8,    REG_R9,   REG_R10,   REG_R11,   REG_R12,
    REG_R13,     REG_R14,   REG_R15,   REG_XAX,  REG_XCX,   REG_XDX,   REG_XBX,
    REG_XSP,     REG_XBP,   REG_XSI,   REG_XDI,  REG_R8,    REG_R9,    REG_R10,
    REG_R11,     REG_R12,   REG_R13,   REG_R14,  REG_R15,   REG_XAX,   REG_XCX,
    REG_XDX,     REG_XBX,   REG_XSP,   REG_XBP,  REG_XSI,   REG_XDI,   REG_R8,
    REG_R9,      REG_R10,   REG_R11,   REG_R12,  REG_R13,   REG_R14,   REG_R15,
    REG_XAX,     REG_XCX,   REG_XDX,   REG_XBX,  REG_XAX,   REG_XCX,   REG_XDX,
    REG_XBX,     REG_R8,    REG_R9,    REG_R10,  REG_R11,   REG_R12,   REG_R13,
    REG_R14,     REG_R15,   REG_XSP,   REG_XBP,  REG_XSI,   REG_XDI, /* i#201 */
    REG_MM0,     REG_MM1,   REG_MM2,   REG_MM3,  REG_MM4,   REG_MM5,   REG_MM6,
    REG_MM7,     REG_YMM0,  REG_YMM1,  REG_YMM2, REG_YMM3,  REG_YMM4,  REG_YMM5,
    REG_YMM6,    REG_YMM7,  REG_YMM8,  REG_YMM9, REG_YMM10, REG_YMM11, REG_YMM12,
    REG_YMM13,   REG_YMM14, REG_YMM15, REG_ST0,  REG_ST1,   REG_ST2,   REG_ST3,
    REG_ST4,     REG_ST5,   REG_ST6,   REG_ST7,  SEG_ES,    SEG_CS,    SEG_SS,
    SEG_DS,      SEG_FS,    SEG_GS,    REG_DR0,  REG_DR1,   REG_DR2,   REG_DR3,
    REG_DR4,     REG_DR5,   REG_DR6,   REG_DR7,  REG_DR8,   REG_DR9,   REG_DR10,
    REG_DR11,    REG_DR12,  REG_DR13,  REG_DR14, REG_DR15,  REG_CR0,   REG_CR1,
    REG_CR2,     REG_CR3,   REG_CR4,   REG_CR5,  REG_CR6,   REG_CR7,   REG_CR8,
    REG_CR9,     REG_CR10,  REG_CR11,  REG_CR12, REG_CR13,  REG_CR14,  REG_CR15,
    REG_INVALID, REG_YMM0,  REG_YMM1,  REG_YMM2, REG_YMM3,  REG_YMM4,  REG_YMM5,
    REG_YMM6,    REG_YMM7,  REG_YMM8,  REG_YMM9, REG_YMM10, REG_YMM11, REG_YMM12,
    REG_YMM13,   REG_YMM14, REG_YMM15,
};

#ifdef DEBUG
void
encode_debug_checks(void)
{
    CLIENT_ASSERT(sizeof(dr_reg_fixer) / sizeof(dr_reg_fixer[0]) == REG_LAST_ENUM + 1,
                  "internal register enum error");
    CLIENT_ASSERT(sizeof(reg_names) / sizeof(reg_names[0]) == REG_LAST_ENUM + 1,
                  "reg_names missing an entry");
    CLIENT_ASSERT(sizeof(type_names) / sizeof(type_names[0]) == TYPE_BEYOND_LAST_ENUM,
                  "type_names missing an entry");
}
#endif

#if defined(DEBUG) && defined(INTERNAL) && !defined(STANDALONE_DECODER)
/* These operand types store a reg_id_t as their operand "size" */
static bool
template_optype_is_reg(int optype)
{
    switch (optype) {
    case TYPE_REG:
    case TYPE_XREG:
    case TYPE_VAR_REG:
    case TYPE_VARZ_REG:
    case TYPE_VAR_XREG:
    case TYPE_VAR_REGX:
    case TYPE_VAR_ADDR_XREG:
    case TYPE_INDIR_REG:
    case TYPE_INDIR_VAR_XREG:
    case TYPE_INDIR_VAR_REG:
    case TYPE_INDIR_VAR_XIREG:
    case TYPE_INDIR_VAR_XREG_OFFS_1:
    case TYPE_INDIR_VAR_XREG_OFFS_8:
    case TYPE_INDIR_VAR_XREG_OFFS_N:
    case TYPE_INDIR_VAR_XIREG_OFFS_1:
    case TYPE_INDIR_VAR_REG_OFFS_2:
    case TYPE_INDIR_VAR_XREG_SIZEx8:
    case TYPE_INDIR_VAR_REG_SIZEx2:
    case TYPE_INDIR_VAR_REG_SIZEx3x5:
    case TYPE_REG_EX:
    case TYPE_VAR_REG_EX:
    case TYPE_VAR_XREG_EX:
    case TYPE_VAR_REGX_EX: return true;
    }
    return false;
}
#endif

/***************************************************************************
 * Functions to see if instr operands match instr_info_t template
 */

static bool
type_instr_uses_reg_bits(int type)
{
    switch (type) {
    case TYPE_C:
    case TYPE_D:
    case TYPE_G:
    case TYPE_P:
    case TYPE_S:
    case TYPE_V: return true;
    default: return false;
    }
}

static bool
type_uses_modrm_bits(int type)
{
    switch (type) {
    case TYPE_E:
    case TYPE_M:
    case TYPE_Q:
    case TYPE_R:
    case TYPE_W:
    case TYPE_INDIR_E:
    case TYPE_P_MODRM:
    case TYPE_V_MODRM:
    case TYPE_VSIB: return true;
    default: return false;
    }
}

static bool
type_uses_vex_vvvv_bits(int type)
{
    switch (type) {
    case TYPE_B:
    case TYPE_H: return true;
    default: return false;
    }
}

/* Helper routine that sets/checks rex.w or data prefix, if necessary, for
 * variable-sized OPSZ_ constants that the user asks for.  We try to be flexible
 * setting/checking only enough prefix flags to ensure that the final template size
 * is one of the possible sizes in the request.
 */
static bool
size_ok_varsz(decode_info_t *di /*prefixes field is IN/OUT; x86_mode is IN*/,
              opnd_size_t size_op, opnd_size_t size_template, uint prefix_data_addr)
{
    /* FIXME: this code is getting long and complex: is there a better way?
     * Any way to resolve these var sizes further first?  Doesn't seem like it.
     */
    /* if identical sizes we shouldn't be called */
    CLIENT_ASSERT(size_op != size_template, "size_ok_varsz: internal decoding error");
    switch (size_op) {
    case OPSZ_2_short1:
        if (size_template == OPSZ_2 || size_template == OPSZ_1)
            return true; /* will take prefix or no prefix */
        if (size_template == OPSZ_4_short2 || size_template == OPSZ_8_short2) {
            di->prefixes |= prefix_data_addr;
            return true;
        }
        if (size_template == OPSZ_4_rex8_short2) {
            if (TEST(PREFIX_REX_W, di->prefixes))
                return false; /* rex.w trumps data prefix */
            di->prefixes |= prefix_data_addr;
            return true;
        }
        return false;
    case OPSZ_4_short2:
        if (size_template == OPSZ_4 || size_template == OPSZ_2)
            return true; /* will take prefix or no prefix */
        if (size_template == OPSZ_4_rex8_short2 || size_template == OPSZ_4_rex8)
            return !TEST(PREFIX_REX_W, di->prefixes);
        if (size_template == OPSZ_8_short2 || size_template == OPSZ_8_short4) {
            di->prefixes |= prefix_data_addr;
            return true;
        }
        if (size_template == OPSZ_6_irex10_short4 &&
            (proc_get_vendor() == VENDOR_AMD || !TEST(PREFIX_REX_W, di->prefixes))) {
            di->prefixes |= prefix_data_addr;
            return true;
        }
        return false;
    case OPSZ_4_rex8_short2:
        if (size_template == OPSZ_4_short2 || size_template == OPSZ_4_rex8 ||
            size_template == OPSZ_8_short2 || size_template == OPSZ_8_short4 ||
            size_template == OPSZ_2 || size_template == OPSZ_4 || size_template == OPSZ_8)
            return true; /* will take prefix or no prefix */
        if (size_template == OPSZ_6_irex10_short4 &&
            (proc_get_vendor() == VENDOR_AMD || !TEST(PREFIX_REX_W, di->prefixes))) {
            di->prefixes |= prefix_data_addr;
            return true;
        }
        return false;
    case OPSZ_4_rex8:
        if (size_template == OPSZ_8_short4 || size_template == OPSZ_4 ||
            size_template == OPSZ_8)
            return true; /* will take prefix or no prefix */
        if (size_template == OPSZ_4_short2 || size_template == OPSZ_4_rex8_short2 ||
            size_template == OPSZ_8_short2)
            return !TEST(prefix_data_addr, di->prefixes);
        if (size_template == OPSZ_6_irex10_short4 &&
            (proc_get_vendor() == VENDOR_AMD || !TEST(PREFIX_REX_W, di->prefixes))) {
            di->prefixes |= prefix_data_addr;
            return true;
        }
        return false;
    case OPSZ_6_irex10_short4:
        if (size_template == OPSZ_6 || size_template == OPSZ_4 ||
            (size_template == OPSZ_10 && proc_get_vendor() != VENDOR_AMD))
            return true; /* will take prefix or no prefix */
        if (size_template == OPSZ_4_short2)
            return !TEST(prefix_data_addr, di->prefixes);
        if (size_template == OPSZ_4_rex8_short2)
            return !TESTANY(prefix_data_addr | PREFIX_REX_W, di->prefixes);
        if (size_template == OPSZ_4_rex8)
            return !TEST(PREFIX_REX_W, di->prefixes);
        if (size_template == OPSZ_8_short4) {
            di->prefixes |= prefix_data_addr;
            return true;
        }
        return false;
    case OPSZ_8_short2:
        if (size_template == OPSZ_8 || size_template == OPSZ_2)
            return true; /* will take prefix or no prefix */
        if (size_template == OPSZ_4_short2) {
            di->prefixes |= prefix_data_addr;
            return true;
        }
        if (size_template == OPSZ_4_rex8_short2) {
            if (TEST(prefix_data_addr, di->prefixes))
                return true; /* Already shrinking so ok */
            /* FIXME - ambiguous on 64-bit (could widen to 8 or shrink to 2).
             * We choose to widen by default for 64-bit as that seems the more likely
             * usage, but whatever choice we make here could conflict with a later
             * operand and lead to encoding failure even if there was a possible match. */
            if (X64_MODE(di))
                di->prefixes |= PREFIX_REX_W;
            else
                di->prefixes |= prefix_data_addr;
            return true;
        }
        if (X64_MODE(di) && size_template == OPSZ_4_rex8) {
            di->prefixes |= PREFIX_REX_W;
            return true;
        }
        if (size_template == OPSZ_8_short4)
            return !TEST(prefix_data_addr, di->prefixes);
        return false;
    case OPSZ_8_short4:
        if (size_template == OPSZ_4_rex8 || size_template == OPSZ_8 ||
            size_template == OPSZ_4)
            return true; /* will take prefix or no prefix */
        if (size_template == OPSZ_4_short2 || size_template == OPSZ_4_rex8_short2 ||
            size_template == OPSZ_8_short2)
            return !TEST(prefix_data_addr, di->prefixes);
        if (size_template == OPSZ_6_irex10_short4 &&
            (proc_get_vendor() == VENDOR_AMD || !TEST(PREFIX_REX_W, di->prefixes))) {
            di->prefixes |= prefix_data_addr;
            return true;
        }
        return false;
    case OPSZ_8_of_16_vex32:
        if (size_template == OPSZ_8 || size_template == OPSZ_32)
            return true; /* will take prefix or no prefix */
        return false;
    case OPSZ_4_rex8_of_16:
        if (size_template == OPSZ_4 || size_template == OPSZ_8)
            return true; /* will take prefix or no prefix */
        return false;
    case OPSZ_12_rex8_of_16:
        if (size_template == OPSZ_12 || size_template == OPSZ_8)
            return true; /* will take prefix or no prefix */
        return false;
    case OPSZ_16_vex32:
        if (size_template == OPSZ_16 || size_template == OPSZ_32)
            return true; /* will take prefix or no prefix */
        return false;
    case OPSZ_28_short14:
        if (size_template == OPSZ_28 || size_template == OPSZ_14)
            return true; /* will take prefix or no prefix */
        return false;
    case OPSZ_108_short94:
        if (size_template == OPSZ_108 || size_template == OPSZ_94)
            return true; /* will take prefix or no prefix */
        return false;
    default:
        CLIENT_ASSERT(false, "size_ok_varsz() internal decoding error (invalid size)");
        break;
    }
    return false;
}

static opnd_size_t
resolve_var_x64_size(decode_info_t *di /*x86_mode is IN*/, opnd_size_t sz,
                     bool addr_short4)
{
    /* Resolve what we can based purely on x64 and addr_short4
     * (gets rid of all NxM sizes), as well as vendor where the size
     * differences are static.  FIXME - could also resolve rex
     * availability and vendor rex-varying sizes here,
     * but not without adding more types that would make
     * size_ok routines more complicated.  */
    switch (sz) {
    case OPSZ_4x8: return (X64_MODE(di) ? OPSZ_8 : OPSZ_4);
    case OPSZ_4_short2xi4:
        return (X64_MODE(di) && proc_get_vendor() == VENDOR_INTEL ? OPSZ_4
                                                                  : OPSZ_4_short2);
    case OPSZ_4x8_short2:
        return (X64_MODE(di) ? (addr_short4 ? OPSZ_8_short4 : OPSZ_8_short2)
                             : OPSZ_4_short2);
    case OPSZ_4x8_short2xi8:
        return (X64_MODE(di)
                    ? (proc_get_vendor() == VENDOR_INTEL ? OPSZ_8 : OPSZ_8_short2)
                    : OPSZ_4_short2);
    case OPSZ_6x10: return (X64_MODE(di) ? OPSZ_10 : OPSZ_6);
    }
    return sz;
}

static opnd_size_t
collapse_subreg_size(opnd_size_t sz)
{
    switch (sz) {
    case OPSZ_1_of_16: return OPSZ_1;
    case OPSZ_2_of_8:
    case OPSZ_2_of_16: return OPSZ_2;
    case OPSZ_4_of_8:
    case OPSZ_4_of_16: return OPSZ_4;
    case OPSZ_8_of_16: return OPSZ_8;
    case OPSZ_12_of_16: return OPSZ_12;
    case OPSZ_14_of_16: return OPSZ_14;
    case OPSZ_15_of_16: return OPSZ_15;
    case OPSZ_16_of_32:
        return OPSZ_16;
        /* OPSZ_8_of_16_vex32, OPSZ_4_rex8_of_16, and OPSZ_12_rex8_of_16 are kept */
    }
    return sz;
}

/* Caller should resolve the OPSZ_*_reg* sizes prior to calling this
 * routine, as here we don't know the operand types
 * Note that this routine modifies prefixes, so it is not idempotent; the
 * prefixes are stateful and are kept around as each operand is checked to
 * ensure the later ones are ok w/ prefixes needed for the earlier ones.
 */
static bool
size_ok(decode_info_t *di /*prefixes field is IN/OUT; x86_mode is IN*/,
        opnd_size_t size_op, opnd_size_t size_template, bool addr)
{
    uint prefix_data_addr = (addr ? PREFIX_ADDR : PREFIX_DATA);
    /* for OPSZ_4x8_short2, does the addr prefix select 4 instead of 2 bytes? */
    bool addr_short4 = X64_MODE(di) && addr;
    /* Assumption: the only addr-specified operands that can be short
     * are OPSZ_4x8_short2 and OPSZ_4x8_short2xi8, or
     * OPSZ_4_short2 for x86 mode on x64.
     * Stack memrefs can pass addr==true and OPSZ_4x8.
     */
    CLIENT_ASSERT(!addr || size_template == OPSZ_4x8 ||
                      size_template == OPSZ_4x8_short2xi8 ||
                      size_template ==
                          OPSZ_4x8_short2 IF_X64(
                              || (!X64_MODE(di) && size_template == OPSZ_4_short2)),
                  "internal prefix assumption error");
    size_template = resolve_var_x64_size(di, size_template, addr_short4);
    size_op = resolve_var_x64_size(di, size_op, addr_short4);
    /* all NxM sizes should be resolved (size_op is checked in the switch statement as
     * these values will hit the default assert) */
    CLIENT_ASSERT(size_template != OPSZ_6x10 && size_template != OPSZ_4x8_short2 &&
                      size_template != OPSZ_4x8_short2xi8 &&
                      size_template != OPSZ_4_short2xi4 && size_template != OPSZ_4x8,
                  "internal encoding error in size_ok()");

    /* register size checks go through reg_size_ok, so collapse sub-reg
     * sizes to the true sizes
     */
    size_op = collapse_subreg_size(size_op);
    size_template = collapse_subreg_size(size_template);

    /* First set/check rex.w or data prefix, if necessary
     * if identical size then don't need to set or check anything */
    if (size_op != size_template) {
        switch (size_op) {
        case OPSZ_1:
            if (size_template == OPSZ_2_short1) {
                di->prefixes |= prefix_data_addr;
                return true;
            }
            return false;
        case OPSZ_2:
            if (size_template == OPSZ_2_short1)
                return !TEST(prefix_data_addr, di->prefixes);
            if (size_template == OPSZ_4_short2 || size_template == OPSZ_8_short2) {
                di->prefixes |= prefix_data_addr;
                return true;
            }
            if (size_template == OPSZ_4_rex8_short2) {
                if (TEST(PREFIX_REX_W, di->prefixes))
                    return false; /* rex.w trumps data prefix */
                di->prefixes |= prefix_data_addr;
                return true;
            }
            return false;
        case OPSZ_4:
            if (size_template == OPSZ_4_short2)
                return !TEST(prefix_data_addr, di->prefixes);
            if (size_template == OPSZ_4_rex8_short2)
                return !TESTANY(prefix_data_addr | PREFIX_REX_W, di->prefixes);
            if (size_template == OPSZ_4_rex8)
                return !TEST(PREFIX_REX_W, di->prefixes);
            if (size_template == OPSZ_6_irex10_short4) {
                if (TEST(PREFIX_REX_W, di->prefixes) && proc_get_vendor() != VENDOR_AMD)
                    return false; /* rex.w trumps data prefix */
                di->prefixes |= prefix_data_addr;
                return true;
            }
            if (size_template == OPSZ_8_short4 || size_template == OPSZ_8_rex16_short4) {
                di->prefixes |= prefix_data_addr;
                return true;
            }
            if (size_template == OPSZ_4_rex8_of_16)
                return !TEST(PREFIX_REX_W, di->prefixes);
            return false;
        case OPSZ_6:
            if (size_template == OPSZ_6_irex10_short4) {
                return !TEST(prefix_data_addr, di->prefixes) &&
                    (!TEST(PREFIX_REX_W, di->prefixes) ||
                     proc_get_vendor() == VENDOR_AMD);
            }
            if (size_template == OPSZ_12_rex40_short6) {
                di->prefixes |= prefix_data_addr;
                return true;
            }
            return false;
        case OPSZ_8:
            if (X64_MODE(di) &&
                (size_template == OPSZ_4_rex8 || size_template == OPSZ_4_rex8_short2 ||
                 size_template == OPSZ_4_rex8_of_16 ||
                 size_template == OPSZ_12_rex8_of_16)) {
                di->prefixes |= PREFIX_REX_W; /* rex.w trumps data prefix */
                return true;
            }
            if (size_template == OPSZ_8_short4 || size_template == OPSZ_8_short2)
                return !TEST(prefix_data_addr, di->prefixes);
            if (size_template == OPSZ_8_rex16 || size_template == OPSZ_8_rex16_short4)
                return !TESTANY(prefix_data_addr | PREFIX_REX_W, di->prefixes);
            if (size_template == OPSZ_8_of_16_vex32)
                return !TEST(PREFIX_VEX_L, di->prefixes);
            return false;
        case OPSZ_10:
            if (X64_MODE(di) && size_template == OPSZ_6_irex10_short4 &&
                proc_get_vendor() != VENDOR_AMD) {
                di->prefixes |= PREFIX_REX_W; /* rex.w trumps data prefix */
                return true;
            }
            return false;
        case OPSZ_12:
            if (size_template == OPSZ_12_rex40_short6)
                return !TESTANY(prefix_data_addr | PREFIX_REX_W, di->prefixes);
            if (size_template == OPSZ_12_rex8_of_16)
                return !TEST(PREFIX_REX_W, di->prefixes);
            return false;
        case OPSZ_16:
            if (X64_MODE(di) &&
                (size_template == OPSZ_8_rex16 || size_template == OPSZ_8_rex16_short4)) {
                di->prefixes |= PREFIX_REX_W; /* rex.w trumps data prefix */
                return true;
            }
            if (size_template == OPSZ_32_short16) {
                di->prefixes |= prefix_data_addr;
                return true;
            }
            if (size_template == OPSZ_16_vex32)
                return !TEST(PREFIX_VEX_L, di->prefixes);
            return false; /* no matching varsz, must be exact match */
        case OPSZ_14:
            if (size_template == OPSZ_28_short14) {
                di->prefixes |= prefix_data_addr;
                return true;
            }
            return false;
        case OPSZ_15: return false; /* no variable sizes match, need identical request */
        case OPSZ_28:
            if (size_template == OPSZ_28_short14)
                return !TEST(prefix_data_addr, di->prefixes);
            return false;
        case OPSZ_32:
            if (size_template == OPSZ_32_short16)
                return !TEST(prefix_data_addr, di->prefixes);
            if (size_template == OPSZ_16_vex32 || size_template == OPSZ_8_of_16_vex32) {
                di->prefixes |= PREFIX_VEX_L;
                return true;
            }
            return false;
        case OPSZ_40:
            if (X64_MODE(di) && size_template == OPSZ_12_rex40_short6) {
                di->prefixes |= PREFIX_REX_W; /* rex.w trumps data prefix */
                return true;
            }
            return false;
        case OPSZ_94:
            if (size_template == OPSZ_108_short94) {
                di->prefixes |= prefix_data_addr;
                return true;
            }
            return false;
        case OPSZ_108:
            if (size_template == OPSZ_108_short94)
                return !TEST(prefix_data_addr, di->prefixes);
            return false;
        case OPSZ_512:
            return false; /* no variable sizes match, need identical request */
        /* We do support variable-sized requests */
        case OPSZ_8_rex16:
        case OPSZ_8_rex16_short4:
        case OPSZ_12_rex40_short6:
        case OPSZ_32_short16:
            /* not supporting client asking for these when template is not identical
             * (not worth the complexity).  similarly we don't support the client
             * asking for other var sizes when the template is one of these.
             */
            CLIENT_ASSERT(false,
                          "variable multi-stack-slot sizes not supported "
                          "as general-purpose sizes");
            break;
        case OPSZ_2_short1:
        case OPSZ_4_short2:
        case OPSZ_4_rex8_short2:
        case OPSZ_4_rex8:
        case OPSZ_6_irex10_short4:
        case OPSZ_8_short2:
        case OPSZ_8_short4:
        case OPSZ_16_vex32:
        case OPSZ_28_short14:
        case OPSZ_108_short94:
            return size_ok_varsz(di, size_op, size_template, prefix_data_addr);
        case OPSZ_1_reg4:
        case OPSZ_2_reg4:
        case OPSZ_4_reg16:
            CLIENT_ASSERT(false, "error: cannot pass OPSZ_*_reg* to size_ok()");
            return false;
        case OPSZ_2_of_8:
        case OPSZ_4_of_8:
        case OPSZ_1_of_16:
        case OPSZ_2_of_16:
        case OPSZ_4_of_16:
        case OPSZ_4_rex8_of_16:
        case OPSZ_8_of_16:
        case OPSZ_12_of_16:
        case OPSZ_12_rex8_of_16:
        case OPSZ_14_of_16:
        case OPSZ_15_of_16:
        case OPSZ_8_of_16_vex32:
        case OPSZ_16_of_32:
        case OPSZ_0:
            /* handled below */
            break;
        default:
            CLIENT_ASSERT(false, "error: unhandled OPSZ_ in size_ok()");
            return false;
        }
    }

    /* prefix doesn't come into play below here: do a direct comparison */

    DOLOG(4, LOG_EMIT, {
        if (size_op != size_template) {
            LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "size_ok: %s != %s\n",
                size_names[size_op], size_names[size_template]);
        }
    });
    return (size_op == size_template);
}

/* We assume size_ok() is called ahead of time to check whether a prefix
 * is needed.
 */
static bool
immed_size_ok(decode_info_t *di /*prefixes field is IN/OUT; x86_mode is IN*/,
              ptr_int_t immed, opnd_size_t opsize)
{
    opsize = resolve_variable_size(di, opsize, false);
    switch (opsize) {
    case OPSZ_1: return (immed >= INT8_MIN && immed <= INT8_MAX);
    case OPSZ_2: /* unsigned max is 65535 */
        return (immed >= INT16_MIN && immed <= INT16_MAX);
#ifndef X64
    case OPSZ_4: return true;
#else
    case OPSZ_4: return (immed >= INT32_MIN && immed <= INT32_MAX);
    case OPSZ_8: return true;
#endif
    default:
        CLIENT_ASSERT(false, "encode error: immediate has unknown size");
        return false;
    }
}

/* prefixes that aren't set by size_ok */
static bool
reg_set_ext_prefixes(decode_info_t *di /*prefixes field is IN/OUT; x86_mode is IN*/,
                     reg_id_t reg, uint which_rex)
{
#ifdef X64
    if (reg >= REG_START_x64_8 && reg <= REG_STOP_x64_8) {
        /* alternates to AH-BH that are specified via any rex prefix */
        if (!TESTANY(PREFIX_REX_ALL, di->prefixes))
            di->prefixes |= PREFIX_REX_GENERAL;
    } else if (reg_is_extended(reg))
        di->prefixes |= which_rex;
#endif
    return true; /* for use in && series */
}

static bool
reg_size_ok(decode_info_t *di /*prefixes field is IN/OUT; x86_mode is IN*/, reg_id_t reg,
            int optype, opnd_size_t opsize, bool addr)
{
    /* Although we now expose sub-register sizes (i#1382), we do not require
     * them when encoding as we have no simple way to add auto-magic creation
     * to the INSTR_CREATE_ macros.  Plus, sub-register sizes never distinguish
     * two opcodes.
     */
    if ((opsize >= OPSZ_SUBREG_START && opsize <= OPSZ_SUBREG_END) ||
        opsize == OPSZ_4_reg16) {
        opnd_size_t expanded = expand_subreg_size(opsize);
        if (expanded == OPSZ_8 &&
            (optype == TYPE_P || optype == TYPE_Q || optype == TYPE_P_MODRM))
            return (reg >= REG_START_MMX && reg <= REG_STOP_MMX);
        if (expanded == OPSZ_16 &&
            (optype == TYPE_V || optype == TYPE_V_MODRM || optype == TYPE_W ||
             optype == TYPE_H || optype == TYPE_L))
            return (reg >= REG_START_XMM && reg <= REG_STOP_XMM);
    }
    if (opsize == OPSZ_8_of_16_vex32 || optype == TYPE_VSIB) {
        if (reg >= REG_START_XMM && reg <= REG_STOP_XMM)
            return !TEST(PREFIX_VEX_L, di->prefixes);
        if (reg >= REG_START_YMM && reg <= REG_STOP_YMM) {
            di->prefixes |= PREFIX_VEX_L;
            return true;
        }
        return false;
    }
    if (opsize == OPSZ_16_of_32) {
        if (reg >= REG_START_YMM && reg <= REG_STOP_YMM) {
            /* Set VEX.L since required for some opcodes and the rest don't care */
            di->prefixes |= PREFIX_VEX_L;
            return true;
        } else
            return false;
    }
    /* We assume that only type p uses OPSZ_6_irex10_short4: w/ data16, even though it's
     * 4 bytes and would fit in a register, this is invalid.
     */
    if (opsize == OPSZ_6_irex10_short4)
        return false; /* no register of size p */
    if (size_ok(di, reg_get_size(reg), resolve_var_reg_size(opsize, true), addr)) {
        if (reg >= REG_START_YMM && reg <= REG_STOP_YMM) {
            /* Set VEX.L since required for some opcodes and the rest don't care */
            di->prefixes |= PREFIX_VEX_L;
        }
        return true;
    }
    return false;
}

static bool
reg_rm_selectable(reg_id_t reg)
{
    /* assumption: GPR registers (of all sizes) and mmx and xmm are all in a row */
    return (reg >= REG_START_64 && reg <= REG_STOP_XMM) ||
        (reg >= REG_START_YMM && reg <= REG_STOP_YMM);
}

static bool
mem_size_ok(decode_info_t *di /*prefixes field is IN/OUT; x86_mode is IN*/, opnd_t opnd,
            int optype, opnd_size_t opsize)
{
    opsize = resolve_var_reg_size(opsize, false);
    if (!opnd_is_memory_reference(opnd))
        return false;
    if (opnd_is_base_disp(opnd) && opnd_is_disp_short_addr(opnd))
        di->prefixes |= PREFIX_ADDR;
    return (
        size_ok(di, opnd_get_size(opnd), opsize, false /*!addr*/) &&
        (!opnd_is_base_disp(opnd) || opnd_get_base(opnd) == REG_NULL ||
         reg_size_ok(di, opnd_get_base(opnd), TYPE_M,
                     IF_X64(!X64_MODE(di) ? OPSZ_4_short2 :) OPSZ_4x8_short2,
                     true /*addr*/)) &&
        (!opnd_is_base_disp(opnd) || opnd_get_index(opnd) == REG_NULL ||
         reg_size_ok(di, opnd_get_index(opnd), optype == TYPE_VSIB ? TYPE_VSIB : TYPE_M,
                     IF_X64(!X64_MODE(di) ? OPSZ_4_short2 :) OPSZ_4x8_short2,
                     true /*addr*/)));
}

static bool
opnd_type_ok(decode_info_t *di /*prefixes field is IN/OUT; x86_mode is IN*/, opnd_t opnd,
             int optype, opnd_size_t opsize)
{
    DOLOG(ENC_LEVEL, LOG_EMIT, {
        dcontext_t *dcontext = get_thread_private_dcontext();
        LOG(THREAD, LOG_EMIT, ENC_LEVEL, "opnd_type_ok on operand ");
        opnd_disassemble(dcontext, opnd, THREAD);
        if (!opnd_is_pc(opnd) && !opnd_is_instr(opnd)) {
            LOG(THREAD, LOG_EMIT, ENC_LEVEL, "with size %s (%d bytes)\n",
                size_names[opnd_get_size(opnd)], opnd_size_in_bytes(opnd_get_size(opnd)));
        }
        LOG(THREAD, LOG_EMIT, ENC_LEVEL,
            "\tvs. template type %s with size %s (%d bytes)\n", type_names[optype],
            template_optype_is_reg(optype) ? reg_names[opsize] : size_names[opsize],
            template_optype_is_reg(optype) ? opnd_size_in_bytes(reg_get_size(opsize))
                                           : opnd_size_in_bytes(opsize));
    });
    switch (optype) {
    case TYPE_NONE: return opnd_is_null(opnd);
    case TYPE_REG: return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == opsize);
    case TYPE_XREG:
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, OPSZ_4x8, false /*!addr*/) &&
                opnd_get_reg(opnd) ==
                    resolve_var_reg(di, opsize, false,
                                    false _IF_X64(true) _IF_X64(false)
                                        _IF_X64(false /*!extendable*/)));
    case TYPE_VAR_REG:
        /* for TYPE_*REG*, opsize is really reg_id_t */
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, OPSZ_4_rex8_short2,
                            false /*!addr*/) &&
                opnd_get_reg(opnd) ==
                    resolve_var_reg(di, opsize, false,
                                    true _IF_X64(false) _IF_X64(true)
                                        _IF_X64(false /*!extendable*/)));
    case TYPE_VARZ_REG:
        return (
            opnd_is_reg(opnd) &&
            reg_size_ok(di, opnd_get_reg(opnd), optype, OPSZ_4_short2, false /*!addr*/) &&
            opnd_get_reg(opnd) ==
                resolve_var_reg(di, opsize, false,
                                true _IF_X64(false) _IF_X64(false)
                                    _IF_X64(false /*!extendable*/)));
    case TYPE_VAR_XREG:
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, OPSZ_4x8_short2,
                            false /*!addr*/) &&
                opnd_get_reg(opnd) ==
                    resolve_var_reg(di, opsize, false,
                                    true _IF_X64(true) _IF_X64(true)
                                        _IF_X64(false /*!extendable*/)));
    case TYPE_VAR_REGX:
        return (
            opnd_is_reg(opnd) &&
            reg_size_ok(di, opnd_get_reg(opnd), optype, OPSZ_4_rex8, false /*!addr*/) &&
            opnd_get_reg(opnd) ==
                resolve_var_reg(di, opsize, false,
                                false /*!shrink*/
                                _IF_X64(false /*default 32*/) _IF_X64(true /*can grow*/)
                                    _IF_X64(false /*!extendable*/)));
    case TYPE_VAR_ADDR_XREG:
        return (
            opnd_is_reg(opnd) &&
            reg_size_ok(di, opnd_get_reg(opnd), optype, OPSZ_4x8_short2, true /*addr*/) &&
            opnd_get_reg(opnd) ==
                resolve_var_reg(di, opsize, true,
                                true _IF_X64(true) _IF_X64(false)
                                    _IF_X64(false /*!extendable*/)));
    case TYPE_REG_EX:
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, reg_get_size(opsize),
                            false /*!addr*/) &&
                reg_set_ext_prefixes(di, opnd_get_reg(opnd), PREFIX_REX_B) &&
                opnd_get_reg(opnd) ==
                    resolve_var_reg(di, opsize, false,
                                    false _IF_X64(false) _IF_X64(false)
                                        _IF_X64(true /*extendable*/)));
    case TYPE_VAR_REG_EX:
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, OPSZ_4_rex8_short2,
                            false /*!addr*/) &&
                reg_set_ext_prefixes(di, opnd_get_reg(opnd), PREFIX_REX_B) &&
                opnd_get_reg(opnd) ==
                    resolve_var_reg(di, opsize, false,
                                    true _IF_X64(false) _IF_X64(true)
                                        _IF_X64(true /*extendable*/)));
    case TYPE_VAR_XREG_EX:
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, OPSZ_4x8_short2,
                            false /*!addr*/) &&
                reg_set_ext_prefixes(di, opnd_get_reg(opnd), PREFIX_REX_B) &&
                opnd_get_reg(opnd) ==
                    resolve_var_reg(di, opsize, false,
                                    true _IF_X64(true) _IF_X64(true)
                                        _IF_X64(true /*extendable*/)));
    case TYPE_VAR_REGX_EX:
        return (
            opnd_is_reg(opnd) &&
            reg_size_ok(di, opnd_get_reg(opnd), optype, OPSZ_4_rex8, false /*!addr*/) &&
            reg_set_ext_prefixes(di, opnd_get_reg(opnd), PREFIX_REX_B) &&
            opnd_get_reg(opnd) ==
                resolve_var_reg(di, opsize, false,
                                false _IF_X64(false) _IF_X64(true)
                                    _IF_X64(true /*extendable*/)));
    case TYPE_VSIB:
#ifndef X64
        if (TEST(PREFIX_ADDR, di->prefixes))
            return false; /* VSIB invalid w/ 16-bit addressing */
#endif
        /* fall through */
    case TYPE_FLOATMEM:
    case TYPE_M: return mem_size_ok(di, opnd, optype, opsize);
    case TYPE_E:
    case TYPE_Q:
    case TYPE_W:
    case TYPE_INDIR_E:
        return (mem_size_ok(di, opnd, optype, opsize) ||
                (opnd_is_reg(opnd) &&
                 reg_size_ok(di, opnd_get_reg(opnd), optype, opsize, false /*!addr*/) &&
                 reg_rm_selectable(opnd_get_reg(opnd))));
    case TYPE_G:
    case TYPE_R:
    case TYPE_B:
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, opsize, false /*!addr*/) &&
                reg_is_gpr(opnd_get_reg(opnd)));
    case TYPE_P:
    case TYPE_V:
    case TYPE_P_MODRM:
    case TYPE_V_MODRM:
        /* We are able to rule out segment registers b/c they should use TYPE_S
         * (OP_mov_seg) or hardcoded (push cs) (if we don't rule them out
         * they can match a 16-bit GPR slot by size alone); CR and DR also
         * have separate types (TYPE_C and TYPE_D).
         */
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, opsize, false /*!addr*/) &&
                reg_rm_selectable(opnd_get_reg(opnd))); /* reg, not rm, but see above */
    case TYPE_C:
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, opsize, false /*!addr*/) &&
                opnd_get_reg(opnd) >= REG_START_CR && opnd_get_reg(opnd) <= REG_STOP_CR);
    case TYPE_D:
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, opsize, false /*!addr*/) &&
                opnd_get_reg(opnd) >= REG_START_DR && opnd_get_reg(opnd) <= REG_STOP_DR);
    case TYPE_S:
        return (opnd_is_reg(opnd) && opnd_get_reg(opnd) >= REG_START_SEGMENT &&
                opnd_get_reg(opnd) <= REG_STOP_SEGMENT);
    case TYPE_I:
        /* we allow instr: it means 4/8-byte immed equal to pc of instr */
        return ((opnd_is_near_instr(opnd) &&
                 (size_ok(di, opnd_get_size(opnd), opsize, false /*!addr*/) ||
                  /* Though we recommend using instrlist_insert_{mov,push}_instr_addr(),
                   * we will accept a pointer-sized 8-byte instr_t when encoded
                   * to low 2GB (w/o top bit set, else sign-extended).
                   */
                  (X64_MODE(di) &&
                   ((ptr_uint_t)di->final_pc) + (ptr_uint_t)opnd_get_instr(opnd)->note -
                           di->cur_note <
                       INT_MAX &&
                   size_ok(di, OPSZ_4, opsize, false)))) ||
                (opnd_is_immed_int(opnd) &&
                 size_ok(di, opnd_get_size(opnd), opsize, false /*!addr*/) &&
                 immed_size_ok(di, opnd_get_immed_int(opnd), opsize)));
    case TYPE_1:
        /* FIXME (xref PR 229127): Ib vs c1: if say "1, OPSZ_1" will NOT match c1 and
         * will get the Ib version: do we want to match c1?  What if they really want
         * an immed byte in the encoding?  OTOH, we do match constant registers
         * automatically w/ no control from the user.
         * Currently, we document in instr_create.h that the user must
         * specify OPSZ_0 in order to get c1.
         */
        return (opnd_is_immed_int(opnd) && opnd_get_immed_int(opnd) == 1 &&
                size_ok(di, opnd_get_size(opnd), opsize, false /*!addr*/));
    case TYPE_FLOATCONST:
        return (opnd_is_immed_float(opnd)); /* FIXME: is actual float const decoded? */
    case TYPE_J:
        /* FIXME PR 225937: support 16-bit data16 immediates */
        /* FIXME: need relative pc offset to test immed_size_ok, but all we have
         * here is absolute pc or instr: but we don't auto-select opcode, and opcode
         * selects immed size (except for data16 which we don't support),
         * so we don't need to choose among templates now: we'll complain
         * at emit time if we have reachability issues. */
        return (opnd_is_near_pc(opnd) || opnd_is_near_instr(opnd));
    case TYPE_A: {
        CLIENT_ASSERT(!X64_MODE(di), "x64 has no type A instructions");
#ifdef IA32_ON_IA64
        if (opsize != OPSZ_6_irex10_short4) {
            return (opnd_is_near_instr(opnd) ||
                    (opnd_is_near_pc(opnd) &&
                     immed_size_ok(di, (uint)opnd_get_pc(opnd), opsize)));
        }
#endif
        return (opnd_is_far_pc(opnd) || opnd_is_far_instr(opnd));
    }
    case TYPE_O:
        return ((opnd_is_abs_addr(opnd) ||
#ifdef X64
                 /* We'll take a relative address that rip-rel won't reach:
                  * after all, OPND_CREATE_ABSMEM() makes a rip-rel.
                  */
                 (opnd_is_rel_addr(opnd) &&
                  (!REL32_REACHABLE(di->final_pc + MAX_INSTR_LENGTH,
                                    (byte *)opnd_get_addr(opnd)) ||
                   !REL32_REACHABLE(di->final_pc + 4, (byte *)opnd_get_addr(opnd)))) ||
#endif
                 (!X64_MODE(di) && opnd_is_mem_instr(opnd))) &&
                size_ok(di, opnd_get_size(opnd), opsize, false /*!addr*/));
    case TYPE_X:
        /* this means the memory address DS:(RE)(E)SI */
        if (opnd_is_far_base_disp(opnd)) {
            reg_id_t base = opnd_get_base(opnd);
            /* reg_size_ok will set PREFIX_ADDR if necessary */
            return (reg_size_ok(di, base, optype, OPSZ_4x8_short2, true /*addr*/) &&
                    reg_is_segment(opnd_get_segment(opnd)) &&
                    base ==
                        resolve_var_reg(di, REG_ESI, true,
                                        true _IF_X64(true) _IF_X64(false)
                                            _IF_X64(false)) &&
                    opnd_get_index(opnd) == REG_NULL && opnd_get_disp(opnd) == 0 &&
                    size_ok(di, opnd_get_size(opnd), opsize, false /*!addr*/));
        } else {
            return false;
        }
    case TYPE_Y:
        /* this means the memory address ES:(RE)(E)DI */
        if (opnd_is_far_base_disp(opnd)) {
            reg_id_t base = opnd_get_base(opnd);
            /* reg_size_ok will set PREFIX_ADDR if necessary */
            return (reg_size_ok(di, base, optype, OPSZ_4x8_short2, true /*addr*/) &&
                    opnd_get_segment(opnd) == SEG_ES &&
                    base ==
                        resolve_var_reg(di, REG_EDI, true,
                                        true _IF_X64(true) _IF_X64(false)
                                            _IF_X64(false)) &&
                    opnd_get_index(opnd) == REG_NULL && opnd_get_disp(opnd) == 0 &&
                    size_ok(di, opnd_get_size(opnd), opsize, false /*!addr*/));
        } else {
            return false;
        }
    case TYPE_XLAT:
        /* this means the memory address DS:(RE)(E)BX+AL */
        if (opnd_is_far_base_disp(opnd)) {
            reg_id_t base = opnd_get_base(opnd);
            /* reg_size_ok will set PREFIX_ADDR if necessary */
            return (reg_size_ok(di, base, optype, OPSZ_4x8_short2, true /*addr*/) &&
                    reg_is_segment(opnd_get_segment(opnd)) &&
                    base ==
                        resolve_var_reg(di, REG_EBX, true,
                                        true _IF_X64(true) _IF_X64(false)
                                            _IF_X64(false)) &&
                    opnd_get_index(opnd) == REG_AL && opnd_get_scale(opnd) == 1 &&
                    opnd_get_disp(opnd) == 0 &&
                    size_ok(di, opnd_get_size(opnd), opsize, false /*!addr*/));
        } else {
            return false;
        }
    case TYPE_MASKMOVQ:
        /* this means the memory address DS:(RE)(E)DI */
        if (opnd_is_far_base_disp(opnd)) {
            reg_id_t base = opnd_get_base(opnd);
            /* reg_size_ok will set PREFIX_ADDR if necessary */
            return (reg_size_ok(di, base, optype, OPSZ_4x8_short2, true /*addr*/) &&
                    reg_is_segment(opnd_get_segment(opnd)) &&
                    base ==
                        resolve_var_reg(di, REG_EDI, true,
                                        true _IF_X64(true) _IF_X64(false)
                                            _IF_X64(false)) &&
                    opnd_get_index(opnd) == REG_NULL && opnd_get_disp(opnd) == 0 &&
                    size_ok(di, opnd_get_size(opnd), opsize, false /*!addr*/));
        } else {
            return false;
        }
    case TYPE_INDIR_REG:
        /* far_ ok */
        return (opnd_is_base_disp(opnd) && opnd_get_base(opnd) == opsize &&
                opnd_get_index(opnd) == REG_NULL && opnd_get_disp(opnd) == 0 &&
                /* FIXME: how know data size?  for now just use reg size... */
                size_ok(di, opnd_get_size(opnd), reg_get_size(opsize), false /*!addr*/));
    case TYPE_INDIR_VAR_XREG:         /* indirect reg that varies by ss only, base is 4x8,
                                       * opsize that varies by data16 */
    case TYPE_INDIR_VAR_REG:          /* indrect reg that varies by ss only, base is 4x8,
                                       * opsize that varies by rex & data16 */
    case TYPE_INDIR_VAR_XIREG:        /* indrect reg that varies by ss only, base is 4x8,
                                       * opsize that varies by data16 except on
                                       * 64-bit Intel */
    case TYPE_INDIR_VAR_XREG_OFFS_1:  /* TYPE_INDIR_VAR_XREG + an offset */
    case TYPE_INDIR_VAR_XREG_OFFS_8:  /* TYPE_INDIR_VAR_XREG + an offset + scale */
    case TYPE_INDIR_VAR_XREG_OFFS_N:  /* TYPE_INDIR_VAR_XREG + an offset + scale */
    case TYPE_INDIR_VAR_XIREG_OFFS_1: /* TYPE_INDIR_VAR_XIREG + an offset + scale */
    case TYPE_INDIR_VAR_REG_OFFS_2:   /* TYPE_INDIR_VAR_REG + offset + scale */
    case TYPE_INDIR_VAR_XREG_SIZEx8:  /* TYPE_INDIR_VAR_XREG + scale */
    case TYPE_INDIR_VAR_REG_SIZEx2:   /* TYPE_INDIR_VAR_REG + scale */
    case TYPE_INDIR_VAR_REG_SIZEx3x5: /* TYPE_INDIR_VAR_REG + scale */
        if (opnd_is_base_disp(opnd)) {
            reg_id_t base = opnd_get_base(opnd);
            /* NOTE - size needs to match decode_operand() and instr_create.h. */
            bool sz_ok = size_ok(di, opnd_get_size(opnd), indir_var_reg_size(di, optype),
                                 false /*!addr*/);
            /* must be after size_ok potentially sets di flags */
            opnd_size_t sz =
                resolve_variable_size(di, opnd_get_size(opnd), false /*not reg*/);
            int disp = indir_var_reg_offs_factor(optype) * (int)opnd_size_in_bytes(sz);
            /* reg_size_ok will set PREFIX_ADDR if 16-bit reg is asked for.
             * these are all specified as 32-bit, so we hardcode OPSZ_VARSTACK
             * for reg_size_ok.
             * to generalize we'll want opsize_var_size(reg_get_size(opsize)) or sthg.
             */
            CLIENT_ASSERT(reg_get_size(opsize) == OPSZ_4, "internal decoding error");
            return (reg_size_ok(di, base, optype, OPSZ_4x8, true /*addr*/) &&
                    base ==
                        resolve_var_reg(di, opsize, true /*doesn't matter*/,
                                        false /*!shrinkable*/ _IF_X64(true /*d64*/)
                                            _IF_X64(false /*!growable*/)
                                                _IF_X64(false /*!extendable*/)) &&
                    opnd_get_index(opnd) == REG_NULL &&
                    /* we're forgiving here, rather than adding complexity
                     * of a disp_equals_minus_size flag or sthg (i#164)
                     */
                    (opnd_get_disp(opnd) == disp || opnd_get_disp(opnd) == disp / 2 ||
                     opnd_get_disp(opnd) == disp * 2) &&
                    sz_ok);
        } else {
            return false;
        }
    case TYPE_H:
    case TYPE_L:
        return (opnd_is_reg(opnd) &&
                reg_size_ok(di, opnd_get_reg(opnd), optype, opsize, false /*!addr*/) &&
                reg_is_xmm(opnd_get_reg(opnd)));
    default:
        CLIENT_ASSERT(false, "encode error: type ok: unknown operand type");
        return false;
    }
}

const instr_info_t *
instr_info_extra_opnds(const instr_info_t *info)
{
    if (TEST(HAS_EXTRA_OPERANDS, info->flags)) {
        if (TEST(EXTRAS_IN_CODE_FIELD, info->flags))
            return (const instr_info_t *)(info->code);
        else /* extra operands are in next entry */
            return (info + 1);
    }
    return NULL;
}

/* macro for speed so we don't have to pass opnds around */
#define TEST_OPND(di, iitype, iisize, iinum, inst_num, get_op)                           \
    if (iitype != TYPE_NONE) {                                                           \
        if (inst_num < iinum)                                                            \
            return false;                                                                \
        if (!opnd_type_ok(di, get_op, iitype, iisize))                                   \
            return false;                                                                \
        if (type_instr_uses_reg_bits(iitype)) {                                          \
            if (!opnd_is_null(using_reg_bits) && !opnd_same(using_reg_bits, get_op))     \
                return false;                                                            \
            using_reg_bits = get_op;                                                     \
        } else if (type_uses_modrm_bits(iitype)) {                                       \
            if (!opnd_is_null(using_modrm_bits) && !opnd_same(using_modrm_bits, get_op)) \
                return false;                                                            \
            using_modrm_bits = get_op;                                                   \
        } else if (type_uses_vex_vvvv_bits(iitype)) {                                    \
            if (!opnd_is_null(using_vvvv_bits) && !opnd_same(using_vvvv_bits, get_op))   \
                return false;                                                            \
            using_vvvv_bits = get_op;                                                    \
        }                                                                                \
    } else if (inst_num >= iinum)                                                        \
        return false;

/* May be called a 2nd time to check size prefix consistency.
 * FIXME optimization: in 2nd pass we only need to call opnd_type_ok()
 * and don't need to check reg, modrm, numbers, etc.
 */
static bool
encoding_possible_pass(decode_info_t *di, instr_t *in, const instr_info_t *ii)
{
    DEBUG_DECLARE(dcontext_t *dcontext = get_thread_private_dcontext();)
    /* make sure multiple operands aren't using same modrm bits */
    opnd_t using_reg_bits = opnd_create_null();
    opnd_t using_modrm_bits = opnd_create_null();
    opnd_t using_vvvv_bits = opnd_create_null();

    /* for efficiency we separately test 2 dsts, 3 srcs */
    TEST_OPND(di, ii->dst1_type, ii->dst1_size, 1, in->num_dsts, instr_get_dst(in, 0));
    TEST_OPND(di, ii->dst2_type, ii->dst2_size, 2, in->num_dsts, instr_get_dst(in, 1));
    TEST_OPND(di, ii->src1_type, ii->src1_size, 1, in->num_srcs, instr_get_src(in, 0));
    TEST_OPND(di, ii->src2_type, ii->src2_size, 2, in->num_srcs, instr_get_src(in, 1));
    TEST_OPND(di, ii->src3_type, ii->src3_size, 3, in->num_srcs, instr_get_src(in, 2));

    if ((ii->flags & HAS_EXTRA_OPERANDS) != 0) {
        /* extra operands to test! */
        int offs = 1;
        ii = instr_info_extra_opnds(ii);
        LOG(THREAD, LOG_EMIT, ENC_LEVEL, "encoding_possible extra operands\n");
        do {
            LOG(THREAD, LOG_EMIT, ENC_LEVEL,
                "encoding possible checking extra operands for " PFX "\n", ii->opcode);
            CLIENT_ASSERT(ii->type == OP_CONTD,
                          "encode error: extra operand template mismatch");

            TEST_OPND(di, ii->dst1_type, ii->dst1_size, (offs * 2 + 1), in->num_dsts,
                      instr_get_dst(in, (offs * 2 + 0)));
            TEST_OPND(di, ii->dst2_type, ii->dst2_size, (offs * 2 + 2), in->num_dsts,
                      instr_get_dst(in, (offs * 2 + 1)));
            TEST_OPND(di, ii->src1_type, ii->src1_size, (offs * 3 + 1), in->num_srcs,
                      instr_get_src(in, (offs * 3 + 0)));
            TEST_OPND(di, ii->src2_type, ii->src2_size, (offs * 3 + 2), in->num_srcs,
                      instr_get_src(in, (offs * 3 + 1)));
            TEST_OPND(di, ii->src3_type, ii->src3_size, (offs * 3 + 3), in->num_srcs,
                      instr_get_src(in, (offs * 3 + 2)));
            offs++;
            ii = instr_info_extra_opnds(ii);
        } while (ii != NULL);
    }

    return true;
}

/* Does not check operands beyond 2 dsts and 3 srcs!
 * Modifies in's prefixes to reflect whether operand or data size
 * prefixes are required.
 * Assumes caller has set di->x86_mode (i.e., ignores in's mode).
 */
bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t *ii)
{
    DEBUG_DECLARE(dcontext_t *dcontext = get_thread_private_dcontext();)
    if (ii == NULL || in == NULL)
        return false;
    LOG(THREAD, LOG_EMIT, ENC_LEVEL, "\nencoding_possible on " PFX "\n", ii->opcode);

    if (TEST(X64_MODE(di) ? X64_INVALID : X86_INVALID, ii->flags))
        return false;

    /* For size prefixes we use the di prefix field since that's what
     * the decode.c routines use; we transfer to the instr's prefix field
     * when done.  The first
     * operand that would need a prefix to match its template sets the
     * prefixes.  Rather than force operands that don't want prefixes
     * to say so (thus requiring a 3-value field: uninitialized,
     * prefix, and no-prefix, and extra work in the common case) we
     * instead do a 2nd pass if any operand wanted a prefix.
     * If an operand wants no prefix and the flag is set, the match fails.
     * I.e., first pass: does anyone want a prefix?  If so, 2nd pass: does
     * everyone want a prefix?  We also re-check the immed sizes on the 2nd
     * pass.
     *
     * If an operand specifies a variable-sized size, it will take on either of
     * the default size or the prefix size.
     */
    di->prefixes &= ~PREFIX_SIZE_SPECIFIERS;
    if (!encoding_possible_pass(di, in, ii))
        return false;
    if (TESTANY(PREFIX_SIZE_SPECIFIERS, di->prefixes)) {
        LOG(THREAD, LOG_EMIT, ENC_LEVEL, "\tflags needed: " PFX "\n", in->prefixes);
        if (!encoding_possible_pass(di, in, ii))
            return false;
    }
    LOG(THREAD, LOG_EMIT, ENC_LEVEL, "\ttemplate match w/ flags: " PFX "\n",
        in->prefixes);
    return true;
}

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr)
{
    memset(di, 0, sizeof(*di));
    IF_X64(di->x86_mode = instr_get_x86_mode(instr));
}

/* num is 0-based */
byte
instr_info_opnd_type(const instr_info_t *info, bool src, int num)
{
    if (num < 0) {
        CLIENT_ASSERT(false, "internal decode error");
        return TYPE_NONE;
    }
    if ((src && num >= 3) || (!src && num >= 2)) {
        const instr_info_t *nxt = instr_info_extra_opnds(info);
        if (nxt == NULL) {
            CLIENT_ASSERT(false, "internal decode error");
            return TYPE_NONE;
        } else
            return instr_info_opnd_type(nxt, src, src ? (num - 3) : (num - 2));
    } else {
        if (src) {
            if (num == 0)
                return info->src1_type;
            else if (num == 1)
                return info->src2_type;
            else if (num == 2)
                return info->src3_type;
            else {
                CLIENT_ASSERT(false, "internal decode error");
                return TYPE_NONE;
            }
        } else {
            if (num == 0)
                return info->dst1_type;
            else if (num == 1)
                return info->dst2_type;
            else {
                CLIENT_ASSERT(false, "internal decode error");
                return TYPE_NONE;
            }
        }
    }
    return TYPE_NONE;
}

/***************************************************************************
 * Actual encoding
 */

static byte *
encode_immed(decode_info_t *di, byte *pc)
{
    /* 1st or 2nd immed? */
    ptr_int_t val;
    opnd_size_t size;
    if (di->size_immed != OPSZ_NA) {
        /* do we need to pc-relativize a target pc? */
        if (di->immed_pc_relativize) {
            int len;
            size = resolve_variable_size(di, di->size_immed, false);
            len = opnd_size_in_bytes(size);
            /* offset is from start of next instruction */
            val = di->immed - ((ptr_int_t)pc + len);
        } else if (di->immed_subtract_length) {
            /* this code means that the immed holds not the absolute pc but
             * the offset not counting the instruction length
             */
            int len;
            size = resolve_variable_size(di, di->size_immed, false);
            len = opnd_size_in_bytes(size);
            /* just need to subtract the total instr length from the offset */
            /* HACK: di->modrm was set with the number of instruction bytes
             * prior to this immed
             */
            val = di->immed - (len + di->modrm);
        } else if (di->immed_pc_rel_offs) {
            /* this code means that the immed holds not the absolute pc but
             * the offset
             */
            size = di->size_immed; /* TYPE_I put real size there */
            CLIENT_ASSERT((size == OPSZ_4_short2 && !TEST(PREFIX_DATA, di->prefixes)) ||
                              (size == OPSZ_4) || IF_X64_ELSE((size == OPSZ_8), false),
                          "encode error: immediate has invalid size");
            /* we want val to be pc of target instr
             * immed is the difference between us and target
             * HACK: di->modrm was set with the number of instruction bytes
             * prior to this immed
             */
            val = di->immed + (ptr_int_t)pc - di->modrm;
            if (di->immed_shift > 0)
                val >>= di->immed_shift;
#ifdef X64
                /* we auto-truncate below to the proper size rather than complaining */
#endif
        } else {
            val = di->immed;
            size = di->size_immed;
        }
        di->size_immed = OPSZ_NA; /* mark as used */
    } else {
        CLIENT_ASSERT(di->size_immed2 != OPSZ_NA,
                      "encode error: immediate has invalid size");
        val = di->immed2;
        size = di->size_immed2;
        di->size_immed2 = OPSZ_NA; /* mark as used */
    }

    /* variable-sized */
    size = resolve_variable_size(di, size, false);

    switch (size) {
    case OPSZ_1:
        *pc = (byte)(val);
        pc += 1;
        break;
    case OPSZ_2:
        *((short *)pc) = (short)(val);
        pc += 2;
        break;
    case OPSZ_4:
        *((int *)pc) = (int)val;
        pc += 4;
        break;
#ifdef X64
    case OPSZ_8:
        *((int64 *)pc) = val;
        pc += 8;
        break;
#endif
    case OPSZ_6:
        CLIENT_ASSERT(di->size_immed2 == size,
                      "encode error: immediate has invalid size OPSZ_6");
        di->size_immed2 = OPSZ_NA;
        *((int *)pc) = (int)di->immed2;
        pc += 4;
        *((short *)pc) = (short)(di->immed);
        pc += 2;
        break;
#ifdef X64
    case OPSZ_10:
        CLIENT_ASSERT(di->size_immed2 == size,
                      "encode error: immediate has invalid size OPSZ_10");
        di->size_immed2 = OPSZ_NA;
        *((ptr_int_t *)pc) = di->immed2;
        pc += 8;
        *((short *)pc) = (short)(di->immed);
        pc += 2;
        break;
#endif
    default:
        LOG(THREAD_GET, LOG_EMIT, 1, "ERROR: encode_immed: unhandled size: %d\n", size);
        CLIENT_ASSERT(false, "encode error: immediate has unknown size");
    }
    return pc;
}

static void
encode_reg_ext_prefixes(decode_info_t *di, reg_id_t reg, uint which_rex)
{
#ifdef X64
    reg_set_ext_prefixes(di, reg, which_rex);
#endif
}

#ifdef X64
static void
encode_rel_addr(decode_info_t *di, opnd_t opnd)
{
    /* Unlike TYPE_J and TYPE_I, who use immed values, can assume
     * there are no other immeds, and have encode_immed complete
     * the pc relativization once the final pc is known, we have
     * to use a different mechanism as we're dealing with a disp
     * and can have other immeds.
     * We simply have instr_encode check for this exact modrrm
     * and use a new field disp_abs to store our target.
     */
    CLIENT_ASSERT(opnd_is_rel_addr(opnd),
                  "encode error: invalid type for pc-relativization");
    di->has_sib = false;
    di->mod = 0;
    di->rm = 5;
    di->has_disp = true;
    di->disp_abs = (byte *)opnd_get_addr(opnd);
    /* PR 253327: since we have no explicit request for addr32, we
     * deduce it here, w/ a conservative range estimate of instr length.
     * However, we consult use_addr_prefix_on_short_disp() first, which will
     * probably disallow for most x64 processors for performance reasons.
     */
    if (use_addr_prefix_on_short_disp() && (ptr_uint_t)di->disp_abs <= INT_MAX &&
        (!REL32_REACHABLE(di->final_pc + MAX_INSTR_LENGTH, di->disp_abs) ||
         !REL32_REACHABLE(di->final_pc + 4, di->disp_abs)))
        di->prefixes |= PREFIX_ADDR;
}
#endif

static void
encode_base_disp(decode_info_t *di, opnd_t opnd)
{
    reg_id_t base, index;
    int scale, disp;
    /* in 64-bit mode, addr prefix simply truncates registers and final address */
    bool addr16 = !X64_MODE(di) && TEST(PREFIX_ADDR, di->prefixes);

    /* user can use opnd_create_abs_addr() but it will internally be a base-disp
     * if its disp is 32-bit: if it's larger it has to be TYPE_O and not get here!
     */
    CLIENT_ASSERT(opnd_is_base_disp(opnd),
                  "encode error: operand type mismatch (expecting base_disp type)");
    if (di->mod < 5) {
        /* mod, rm, & sib have already been set, probably b/c
         * we have a src that equals a dst.
         * just exit.
         */
        return;
    }

    base = opnd_get_base(opnd);
    index = opnd_get_index(opnd);
    scale = opnd_get_scale(opnd);
    disp = opnd_get_disp(opnd);
    if (base == REG_NULL && index == REG_NULL) {
        /* absolute displacement */
        if (!addr16 && di->seg_override != REG_NULL &&
            ((!X64_MODE(di) && disp >= INT16_MIN && disp <= INT16_MAX) ||
             (X64_MODE(di) && disp >= INT32_MIN && disp <= INT32_MAX)) &&
            !opnd_is_disp_force_full(opnd)) {
            /* already have segment prefix, so adding addr16 prefix
             * won't make worse (already in slow decoder on processor), so try to
             * reduce size: unless on newer microarch: see comments in
             * use_addr_prefix_on_short_disp().
             * if a client doesn't want this happening to a patch-later value, should
             * use a large bogus value that won't trigger this, or
             * specify force_full_disp.
             */
            /* For x64 wanting addr32 to address high 2GB of low 4GB, caller
             * should set disp_short_addr on the base-disp opnd, which is
             * done automatically for opnd_create_abs_addr().  That sets
             * PREFIX_ADDR earlier in the encoding process.
             */
            if (!X64_MODE(di) && /* disp always 32-bit for x64 */
                use_addr_prefix_on_short_disp()) {
                di->prefixes |= PREFIX_ADDR; /* for 16-bit disp */
                addr16 = true;
            }
        }
        if (X64_MODE(di)) {
            /* need a sib byte to do abs (not rip-relative) */
            di->mod = 0;
            di->rm = 4;
            di->has_sib = true;
            di->scale = 0;
            di->index = 4;
            di->base = 5;
            di->has_disp = true;
            di->disp = disp;
            /* if rex.x is set we'll have r12 instead of no base */
            CLIENT_ASSERT(!TEST(PREFIX_REX_X, di->prefixes),
                          "encode error: for x64 cannot encode abs addr w/ rex.x");
        } else {
            di->has_sib = false;
            di->mod = 0;
            di->rm = (byte)((addr16) ? 6 : 5);
            di->has_disp = true;
            di->disp = disp;
        }
    } else {
        if (disp == 0 &&
            /* must use 8-bit disp for 0x0(%ebp) or 0x0(%r13) */
            ((!addr16 &&
              base !=
                  REG_EBP /* x64 w/ addr prefix => ebp */
                      IF_X64(&&base != REG_RBP && base != REG_R13 && base != REG_R13D)) ||
             /* must use 8-bit disp for 0x0(%bp) */
             (addr16 && (base != REG_BP || index != REG_NULL))) &&
            !opnd_is_disp_encode_zero(opnd)) {
            /* no disp */
            di->mod = 0;
            di->has_disp = false;
        } else if (disp >= INT8_MIN && disp <= INT8_MAX &&
                   !opnd_is_disp_force_full(opnd)) {
            /* 8-bit disp */
            di->mod = 1;
            di->has_disp = true;
            di->disp = disp;
        } else {
            /* 32/16-bit disp */
            di->mod = 2;
            di->has_disp = true;
            di->disp = disp;
        }
        if (addr16) {
            di->has_sib = false;
            if (base == REG_BX && index == REG_SI)
                di->rm = 0;
            else if (base == REG_BX && index == REG_DI)
                di->rm = 1;
            else if (base == REG_BP && index == REG_SI)
                di->rm = 2;
            else if (base == REG_BP && index == REG_DI)
                di->rm = 3;
            else if (base == REG_SI && index == REG_NULL)
                di->rm = 4;
            else if (base == REG_DI && index == REG_NULL)
                di->rm = 5;
            else if (base == REG_BP && index == REG_NULL)
                di->rm = 6;
            else if (base == REG_BX && index == REG_NULL)
                di->rm = 7;
            else {
                CLIENT_ASSERT(false, "encode error: invalid 16-bit base+index");
                di->rm = 0;
            }
        } else if (index == REG_NULL &&
                   base !=
                       REG_ESP /* x64 w/ addr prefix => esp */
                           IF_X64(&&base != REG_RSP && base != REG_R12 &&
                                  base != REG_R12D)) {
            /* don't need SIB byte */
            di->has_sib = false;
            encode_reg_ext_prefixes(di, base, PREFIX_REX_B);
            di->rm = reg_get_bits(base);
        } else {
            /* need SIB byte */
            di->has_sib = true;
            di->rm = 4;
            if (index == REG_NULL) {
                di->index = 4;
                di->scale = 0; /* does it matter?!? */
            } else {
                /* note that r13 can be an index register */
                CLIENT_ASSERT(index != REG_ESP IF_X64(&&index != REG_RSP),
                              "encode error: xsp cannot be an index register");
                CLIENT_ASSERT(reg_is_32bit(index) ||
                                  (X64_MODE(di) && reg_is_64bit(index)) ||
                                  reg_is_xmm(index) /* VSIB */,
                              "encode error: index must be general-purpose register");
                encode_reg_ext_prefixes(di, index, PREFIX_REX_X);
                if (X64_MODE(di) && reg_is_32bit(index))
                    di->prefixes |= PREFIX_ADDR;
                di->index = reg_get_bits(index);
                switch (scale) {
                case 1: di->scale = 0; break;
                case 2: di->scale = 1; break;
                case 4: di->scale = 2; break;
                case 8: di->scale = 3; break;
                default: CLIENT_ASSERT(false, "encode error: invalid scale");
                }
            }
            if (base == REG_NULL) {
                di->base = 5;
                di->mod = 0;
                di->has_disp = true;
                di->disp = disp;
            } else {
                /* can't do nodisp(ebp) or nodisp(r13) */
                CLIENT_ASSERT(di->mod != 0 ||
                                  (base !=
                                   REG_EBP IF_X64(&&base != REG_RBP && base != REG_R13 &&
                                                  base != REG_R13D)),
                              "encode error: xbp/r13 base must have disp");
                encode_reg_ext_prefixes(di, base, PREFIX_REX_B);
                if (X64_MODE(di) && reg_is_32bit(base)) {
                    CLIENT_ASSERT(
                        index == REG_NULL ||
                            (reg_is_32bit(index) && TEST(PREFIX_ADDR, di->prefixes)),
                        "encode error: index and base must be same width");
                    di->prefixes |= PREFIX_ADDR;
                }
                di->base = reg_get_bits(base);
            }
        }
    }
}

static void
set_immed(decode_info_t *di, ptr_int_t val, opnd_size_t opsize)
{
    if (di->size_immed == OPSZ_NA) {
        di->immed = val;
        di->size_immed = opsize;
    } else {
        CLIENT_ASSERT(di->size_immed2 == OPSZ_NA,
                      "encode error: >4-byte immed encoding error");
        di->immed2 = val;
        di->size_immed2 = opsize;
    }
}

static byte *
get_mem_instr_addr(decode_info_t *di, opnd_t opnd)
{
    CLIENT_ASSERT(opnd_is_mem_instr(opnd), "internal encode error");
    return di->final_pc + ((ptr_int_t)opnd_get_instr(opnd)->note - di->cur_note) +
        opnd_get_mem_instr_disp(opnd);
}

static void
encode_operand(decode_info_t *di, int optype, opnd_size_t opsize, opnd_t opnd)
{
    switch (optype) {
    case TYPE_NONE:
    case TYPE_REG:
    case TYPE_XREG:
    case TYPE_VAR_REG:
    case TYPE_VARZ_REG:
    case TYPE_VAR_XREG:
    case TYPE_VAR_REGX:
    case TYPE_VAR_ADDR_XREG:
    case TYPE_1:
    case TYPE_FLOATCONST:
    case TYPE_INDIR_REG:
    case TYPE_INDIR_VAR_XREG:
    case TYPE_INDIR_VAR_REG:
    case TYPE_INDIR_VAR_XIREG:
    case TYPE_INDIR_VAR_XREG_OFFS_1:
    case TYPE_INDIR_VAR_XREG_OFFS_8:
    case TYPE_INDIR_VAR_XREG_OFFS_N:
    case TYPE_INDIR_VAR_XIREG_OFFS_1:
    case TYPE_INDIR_VAR_REG_OFFS_2:
    case TYPE_INDIR_VAR_XREG_SIZEx8:
    case TYPE_INDIR_VAR_REG_SIZEx2:
    case TYPE_INDIR_VAR_REG_SIZEx3x5: return;
    case TYPE_REG_EX:
    case TYPE_VAR_REG_EX:
    case TYPE_VAR_XREG_EX:
    case TYPE_VAR_REGX_EX:
        encode_reg_ext_prefixes(di, opnd_get_reg(opnd), PREFIX_REX_B);
        return;
    case TYPE_VSIB:
        CLIENT_ASSERT(opnd_is_base_disp(opnd),
                      "encode error: VSIB operand must be base-disp");
        /* fall through */
    case TYPE_FLOATMEM:
    case TYPE_M:
        CLIENT_ASSERT(opnd_is_memory_reference(opnd),
                      "encode error: M operand must be mem ref");
        /* fall through */
    case TYPE_INDIR_E:
    case TYPE_E:
    case TYPE_Q:
    case TYPE_W:
    case TYPE_R:       /* we already ensured this is a reg, not memory */
    case TYPE_P_MODRM: /* we already ensured this is a reg, not memory */
    case TYPE_V_MODRM: /* we already ensured this is a reg, not memory */
        if (opnd_is_memory_reference(opnd)) {
            if (opnd_is_far_memory_reference(opnd)) {
                di->seg_override = opnd_get_segment(opnd);
                /* should be just a SEG_ constant */
                CLIENT_ASSERT(di->seg_override >= REG_START_SEGMENT &&
                                  di->seg_override <= REG_STOP_SEGMENT,
                              "encode error: invalid segment override");
            }
            if (opnd_is_mem_instr(opnd)) {
                byte *addr = get_mem_instr_addr(di, opnd);
#ifdef X64
                if (X64_MODE(di))
                    encode_rel_addr(di, opnd_create_rel_addr(addr, opnd_get_size(opnd)));
                else
#endif
                    encode_base_disp(di, opnd_create_abs_addr(addr, opnd_get_size(opnd)));
                di->has_instr_opnds = true;
            } else {
#ifdef X64
                if (X64_MODE(di) && opnd_is_rel_addr(opnd))
                    encode_rel_addr(di, opnd);
                else if (X64_MODE(di) && opnd_is_abs_addr(opnd) &&
                         !opnd_is_base_disp(opnd)) {
                    /* try to fit it as rip-rel */
                    opnd.kind = REL_ADDR_kind;
                    encode_rel_addr(di, opnd);
                } else
#endif
                    encode_base_disp(di, opnd);
            }
        } else {
            CLIENT_ASSERT(opnd_is_reg(opnd),
                          "encode error: modrm not selecting mem but not selecting reg");
            if (di->mod < 5) {
                /* already set (by a dst equal to src, probably) */
                CLIENT_ASSERT(di->mod == 3 && di->rm == reg_get_bits(opnd_get_reg(opnd)),
                              "encode error: modrm mismatch");
                return;
            }
            di->mod = 3;
            encode_reg_ext_prefixes(di, opnd_get_reg(opnd), PREFIX_REX_B);
            di->rm = reg_get_bits(opnd_get_reg(opnd));
        }
        return;
    case TYPE_G:
    case TYPE_P:
    case TYPE_V:
    case TYPE_S:
    case TYPE_C:
    case TYPE_D: {
        CLIENT_ASSERT(opnd_is_reg(opnd), "encode error: operand must be a register");
        if (di->reg < 8) {
            /* already set (by a dst equal to src, probably) */
            CLIENT_ASSERT(di->reg == reg_get_bits(opnd_get_reg(opnd)),
                          "encode error: modrm mismatch");
            return;
        }
        encode_reg_ext_prefixes(di, opnd_get_reg(opnd), PREFIX_REX_R);
        di->reg = reg_get_bits(opnd_get_reg(opnd));
        return;
    }
    case TYPE_I:
        if (opnd_is_near_instr(opnd)) {
            /* allow instr as immed, that means we want to put in the 4/8-byte
             * pc of target instr as the immed
             * This only works if the instr has no other immeds!
             */
            instr_t *target_instr = opnd_get_instr(opnd);
            ptr_uint_t target = (ptr_uint_t)target_instr->note - di->cur_note;
            /* We don't know the encode pc yet, so we put it in as pc-relative and
             * fix it up later.
             * The size was already checked, so just use the template size.
             */
            set_immed(di, (ptr_uint_t)target, opsize);
            /* this immed is pc-relative except it needs to have the
             * instruction length subtracted from it -- we indicate that
             * like this:
             */
            CLIENT_ASSERT(di->size_immed2 == OPSZ_NA,
                          "encode error: immed size already set");
            di->size_immed = resolve_variable_size(di, opsize, false);
            /* And now we ask to be adjusted to become an absolute pc: */
            di->immed_pc_rel_offs = true; /* == immed needs +pc */
            di->immed_shift = opnd_get_shift(opnd);
            di->has_instr_opnds = true;
        } else {
            CLIENT_ASSERT(opnd_is_immed_int(opnd), "encode error: opnd not immed int");
            set_immed(di, opnd_get_immed_int(opnd), opsize);
        }
        return;
    case TYPE_J: {
        ptr_uint_t target;
        /* Since we don't know pc values right now, we convert
         * from an absolute pc to a relative offset in encode_immed.
         * Here we simply set the immed to the absolute pc target.
         */
        if (opnd_is_near_instr(opnd)) {
            /* assume the note fields have been set with relative offsets
             * from some start pc, and that our caller put our note in
             * di->cur_note
             */
            instr_t *target_instr = opnd_get_instr(opnd);
            target = (ptr_uint_t)target_instr->note - di->cur_note;
            /* target is now a pc-relative target, so we can encode as is */
            set_immed(di, target, opsize);
            /* this immed is pc-relative except it needs to have the
             * instruction length subtracted from it -- we indicate that
             * like this:
             */
            CLIENT_ASSERT(di->size_immed2 == OPSZ_NA,
                          "encode error: immed size already set");
            di->size_immed = opsize;
            di->immed_subtract_length = true; /* == immed needs -length */
            di->has_instr_opnds = true;
        } else {
            CLIENT_ASSERT(opnd_is_near_pc(opnd), "encode error: opnd not pc");
            target = (ptr_uint_t)opnd_get_pc(opnd);
            set_immed(di, target, opsize);
            /* TYPE_J never has other immeds in the same instruction */
            CLIENT_ASSERT(di->size_immed2 == OPSZ_NA,
                          "encode error: immed size already set");
            di->immed_pc_relativize = true;
            di->size_immed = opsize;
        }
        return;
    }
    case TYPE_A: {
        ptr_uint_t target;

#ifdef IA32_ON_IA64
        if (opsize == OPSZ_4_short2) {
            if (opnd_is_near_instr(opnd)) {
                /* assume the note fields have been set with relative offsets
                 * from some start pc
                 */
                instr_t *target_instr = opnd_get_instr(opnd);
                target = (ptr_uint_t)target_instr->note;
                /* target is absolute address of instr ready to go */
                set_immed(di, target, opsize);
                di->has_instr_opnds = true;
            } else {
                CLIENT_ASSERT(opnd_is_near_pc(opnd), "encode error: opnd not pc");
                target = (ptr_uint_t)opnd_get_pc(opnd);
                set_immed(di, target, opsize);
            }
            return;
        }
#endif

        CLIENT_ASSERT(!X64_MODE(di), "x64 has no type A instructions");
        CLIENT_ASSERT(opsize == OPSZ_6_irex10_short4 || opsize == OPSZ_6 ||
                          opsize == OPSZ_4 ||
                          (opsize == OPSZ_10 && proc_get_vendor() != VENDOR_AMD),
                      "encode error: A operand size mismatch");
        CLIENT_ASSERT(di->size_immed == OPSZ_NA && di->size_immed2 == OPSZ_NA,
                      "encode error: A operand size mismatch");
        if (opnd_is_far_instr(opnd)) {
            /* caller set di.cur_note w/ the pc where we'll be encoding this */
            ptr_int_t source = (ptr_uint_t)di->cur_note;
            instr_t *target_instr = opnd_get_instr(opnd);
            ptr_int_t dest = (ptr_uint_t)target_instr->note;
            ptr_uint_t encode_pc = (ptr_uint_t)di->final_pc;
            /* A label shouldn't be very far away and thus we should not overflow
             * (unless client asked to encode at very high address or sthg,
             * which we won't support).
             */
            CLIENT_ASSERT((dest >= source && encode_pc + (dest - source) >= encode_pc) ||
                              (dest < source && encode_pc + (dest - source) < encode_pc),
                          "label is too far from targeter wrt encode pc");
            target = encode_pc + (dest - source);
            CLIENT_ASSERT(opsize == OPSZ_6_irex10_short4,
                          "far instr size set to unsupported value");
            di->has_instr_opnds = true;
        } else {
            CLIENT_ASSERT(opnd_is_far_pc(opnd),
                          "encode error: A operand must be far pc or far instr");
            target = (ptr_uint_t)opnd_get_pc(opnd);
        }
        /* XXX PR 225937: allow client to specify whether data16 or not
         * instead of auto-adding the prefix if offset is small
         */
        if (target <= USHRT_MAX &&
            /* we can't use data16 on a far call as it changes the stack size */
            di->opcode != OP_call_far) {
            int val = (opnd_get_segment_selector(opnd) << 16) | ((short)target);
            di->prefixes |= PREFIX_DATA;
            set_immed(di, val, OPSZ_4);
        } else if (target > UINT_MAX) {
            CLIENT_ASSERT(proc_get_vendor() == VENDOR_INTEL,
                          "cannot use 8-byte far pc on AMD processor");
            di->prefixes |= PREFIX_REX_W;
            set_immed(di, opnd_get_segment_selector(opnd), OPSZ_10);
            set_immed(di, target, OPSZ_10);
        } else {
            set_immed(di, opnd_get_segment_selector(opnd), OPSZ_6);
            set_immed(di, target, OPSZ_6);
        }
        return;
    }
    case TYPE_O: {
        ptr_int_t addr;
        CLIENT_ASSERT(opnd_is_abs_addr(opnd) ||
                          /* rel addr => abs if won't reach */
                          IF_X64(opnd_is_rel_addr(opnd) ||)(!X64_MODE(di) &&
                                                            opnd_is_mem_instr(opnd)),
                      "encode error: O operand must be absolute mem ref");
        if (opnd_is_mem_instr(opnd)) {
            addr = (ptr_int_t)get_mem_instr_addr(di, opnd);
            di->has_instr_opnds = true;
        } else
            addr = (ptr_int_t)opnd_get_addr(opnd);
        if (opnd_is_far_abs_addr(opnd)) {
            di->seg_override = opnd_get_segment(opnd);
            /* should be just a SEG_ constant */
            CLIENT_ASSERT(di->seg_override >= REG_START_SEGMENT &&
                              di->seg_override <= REG_STOP_SEGMENT,
                          "encode error: invalid segment override");
            if ((!X64_MODE(di) && addr >= INT16_MIN && addr <= INT16_MAX) ||
                (X64_MODE(di) && addr >= INT32_MIN && addr <= INT32_MAX)) {
                /* same optimization as in encode_base_disp -- see comments there */
                if (use_addr_prefix_on_short_disp()) {
                    di->prefixes |= PREFIX_ADDR;
                }
            }
        }
        set_immed(di, addr, resolve_addr_size(di));
        return;
    }

    /* assume that opnd_type_ok has already been called --
     * nothing to do unless has an override, these are implicit operands */
    case TYPE_X:        /* this means the memory address DS:(RE)(E)SI */
    case TYPE_XLAT:     /* this means the memory address DS:(RE)(E)BX+AL */
    case TYPE_MASKMOVQ: /* this means the memory address DS:(RE)(E)DI */
        if (opnd_get_segment(opnd) != SEG_DS)
            di->seg_override = opnd_get_segment(opnd);
        return;
    case TYPE_Y: /* this means the memory address ES:(RE)(E)DI */
        return;  /* no override possible */

    case TYPE_L: {
        reg_id_t reg = opnd_get_reg(opnd);
        ptr_int_t immed =
            (reg_is_ymm(reg) ? (reg - REG_START_YMM) : (reg - REG_START_XMM));
        immed = (immed << 4);
        set_immed(di, immed, OPSZ_1);
        return;
    }
    case TYPE_H: {
        reg_id_t reg = opnd_get_reg(opnd);
        di->vex_vvvv =
            (byte)(reg_is_ymm(reg) ? (reg - REG_START_YMM) : (reg - REG_START_XMM));
        di->vex_vvvv = (~di->vex_vvvv) & 0xf;
        return;
    }
    case TYPE_B: {
        /* There are 4 bits in vvvv so no prefix bit is needed. */
        reg_id_t reg = opnd_get_reg(opnd);
        encode_reg_ext_prefixes(di, reg, 0);
        di->vex_vvvv = reg_get_bits(reg);
#ifdef X64
        if (reg_is_extended(reg)) /* reg_get_bits does % 8 */
            di->vex_vvvv |= 0x8;
#endif
        di->vex_vvvv = (~di->vex_vvvv) & 0xf;
        return;
    }

    default: CLIENT_ASSERT(false, "encode error: unknown operand type");
    }
}

static byte
encode_vex_final_prefix_byte(byte cur_byte, decode_info_t *di, const instr_info_t *info)
{
    cur_byte |= (di->vex_vvvv << 3) | (TEST(PREFIX_VEX_L, di->prefixes) ? 0x04 : 0x00);
    /* we override OPCODE_SUFFIX for vex to mean "requires vex.L" */
    if (TEST(OPCODE_SUFFIX, info->opcode))
        cur_byte |= 0x04;
    /* OPCODE_{MODRM,SUFFIX} mean something else for vex */
    if (info->opcode > 0xffffff) {
        byte prefix = (byte)(info->opcode >> 24);
        if (prefix == 0x66)
            cur_byte |= 0x1;
        else if (prefix == 0xf3)
            cur_byte |= 0x2;
        else if (prefix == 0xf2)
            cur_byte |= 0x3;
        else
            CLIENT_ASSERT(false, "unknown vex prefix");
    }
    return cur_byte;
}

static byte *
encode_vex_prefixes(byte *field_ptr, decode_info_t *di, const instr_info_t *info,
                    bool *output_initial_opcode)
{
    byte val;
    byte vex_mm = (byte)((info->opcode & 0x00ff0000) >> 16);
    /* We're out flags for REQUIRES_XOP, so XOP instrs have REQUIRES_VEX and we
     * rely on XOP.map_select being disjoint from VEX.m-mmmm:
     */
    bool xop = (vex_mm >= 0x08 && vex_mm < 0x0f); /* XOP instead of VEX */
    CLIENT_ASSERT(output_initial_opcode != NULL, "required param");
    if (TESTANY(PREFIX_REX_X | PREFIX_REX_B | PREFIX_REX_W, di->prefixes) ||
        /* 3-byte vex shortest encoding for 0x0f 0x3[8a], and the same
         * size but I'm assuming faster decode in processor for 0x0f
         */
        TEST(OPCODE_THREEBYTES, info->opcode) ||
        /* XOP is always 3 bytes */
        xop ||
        /* 2-byte requires leading 0x0f */
        ((info->opcode & 0x00ff0000) >> 16) != 0x0f) {
        /* need 3-byte vex */
        *output_initial_opcode = true;
        /* first vex byte */
        if (xop)
            *field_ptr = 0x8f;
        else
            *field_ptr = 0xc4;
        field_ptr++;
        /* second vex byte */
        val = /* these are negated */
            (TEST(PREFIX_REX_R, di->prefixes) ? 0x00 : 0x80) |
            (TEST(PREFIX_REX_X, di->prefixes) ? 0x00 : 0x40) |
            (TEST(PREFIX_REX_B, di->prefixes) ? 0x00 : 0x20);
        if (xop) {
            byte map_select = (byte)((info->opcode & 0x00ff0000) >> 16);
            CLIENT_ASSERT(TEST(OPCODE_THREEBYTES, info->opcode), "internal invalid XOP");
            CLIENT_ASSERT(map_select < 0x20, "XOP.map_select only has 5 bits");
            val |= map_select;
        } else {
            if (TEST(OPCODE_THREEBYTES, info->opcode)) {
                byte op3 = (byte)((info->opcode & 0x00ff0000) >> 16);
                if (op3 == 0x38)
                    val |= 0x02;
                else if (op3 == 0x3a)
                    val |= 0x03;
                else
                    CLIENT_ASSERT(false, "unknown 3-byte opcode");
            } else {
                byte op3 = (byte)((info->opcode & 0x00ff0000) >> 16);
                if (op3 == 0x0f)
                    val |= 0x01;
            }
        }
        *field_ptr = val;
        field_ptr++;
        /* third vex byte */
        val = (TEST(PREFIX_REX_W, di->prefixes) ? 0x80 : 0x00);
        /* we override OPCODE_MODRM for vex to mean "requires vex.W" */
        if (TEST(OPCODE_MODRM, info->opcode))
            val = 0x80;
        val = encode_vex_final_prefix_byte(val, di, info);
        *field_ptr = val;
        field_ptr++;
    } else {
        /* 2-byte vex */
        /* first vex byte */
        *field_ptr = 0xc5;
        field_ptr++;
        /* second vex byte */
        val = (TEST(PREFIX_REX_R, di->prefixes) ? 0x00 : 0x80); /* negated */
        val = encode_vex_final_prefix_byte(val, di, info);
        *field_ptr = val;
        field_ptr++;
        /* 2-byte requires leading implied 0x0f */
        ASSERT(((info->opcode & 0x00ff0000) >> 16) == 0x0f);
        *output_initial_opcode = true;
    }
    return field_ptr;
}

/* special-case (==fast) encoder for cti instructions
 * this routine cannot handle indirect branches or rets or far jmp/call;
 * it can handle loop/jecxz but it does NOT check for data16!
 */
static byte *
encode_cti(instr_t *instr, byte *copy_pc, byte *final_pc,
           bool check_reachable _IF_DEBUG(bool assert_reachable))
{
    byte *pc = copy_pc;
    const instr_info_t *info = instr_get_instr_info(instr);
    opnd_t opnd;
    ptr_uint_t target;

    if (info == NULL) {
        CLIENT_ASSERT(false, "encode internal error: encode_cti with wrong opcode");
        return NULL;
    }

    if (instr->prefixes != 0) {
        if (TEST(PREFIX_JCC_TAKEN, instr->prefixes)) {
            *pc = RAW_PREFIX_jcc_taken;
            pc++;
        } else if (TEST(PREFIX_JCC_NOT_TAKEN, instr->prefixes)) {
            *pc = RAW_PREFIX_jcc_not_taken;
            pc++;
        }
        /* assumption: no 16-bit targets */
        CLIENT_ASSERT(
            !TESTANY(~(PREFIX_JCC_TAKEN | PREFIX_JCC_NOT_TAKEN), instr->prefixes),
            "encode cti error: non-branch-hint prefixes not supported");
    }

    /* output opcode */
    /* first opcode byte */
    *pc = (byte)((info->opcode & 0x00ff0000) >> 16);
    pc++;
    /* second opcode byte, if there is one */
    if (TEST(OPCODE_TWOBYTES, info->opcode)) {
        *pc = (byte)((info->opcode & 0x0000ff00) >> 8);
        pc++;
    }
    ASSERT(!TEST(OPCODE_THREEBYTES, info->opcode)); /* no cti has 3 opcode bytes */

    /* we assume only one operand: 1st src == jump target, but we do
     * not check that, for speed
     */
    opnd = instr_get_target(instr);
    if (opnd_is_near_pc(opnd)) {
        target = (ptr_uint_t)opnd_get_pc(opnd);
    } else if (opnd_is_near_instr(opnd)) {
        instr_t *in = opnd_get_instr(opnd);
        target = (ptr_uint_t)final_pc + ((ptr_uint_t)in->note - (ptr_uint_t)instr->note);
    } else {
        target = 0; /* avoid compiler warning */
        CLIENT_ASSERT(false, "encode_cti error: opnd must be near pc or near instr");
    }

    if (instr_is_cti_short(instr)) {
        /* 8-bit offset */
        ptr_int_t offset;
        /* handled w/ mangled bytes */
        CLIENT_ASSERT(!instr_is_cti_short_rewrite(instr, NULL),
                      "encode_cti error: jecxz/loop already mangled");
        /* offset is from start of next instr */
        offset = target - ((ptr_int_t)(pc + 1 - copy_pc + final_pc));
        if (check_reachable && !(offset >= INT8_MIN && offset <= INT8_MAX)) {
            CLIENT_ASSERT(!assert_reachable,
                          "encode_cti error: target beyond 8-bit reach");
            return NULL;
        }
        *((char *)pc) = (char)offset;
        pc++;
    } else {
        /* 32-bit offset */
        /* offset is from start of next instr */
        ptr_int_t offset = target - ((ptr_int_t)(pc + 4 - copy_pc + final_pc));
#ifdef X64
        if (check_reachable && !REL32_REACHABLE_OFFS(offset)) {
            CLIENT_ASSERT(!assert_reachable,
                          "encode_cti error: target beyond 32-bit reach");
            return NULL;
        }
#endif
        *((int *)pc) = (int)offset;
        pc += 4;
    }
    return pc;
}

/* PR 251479: support general re-relativization.
 * Takes in a level 0-3 instruction and encodes it by copying its
 * raw bytes to dst_pc. For x64, if it is marked as having a rip-relative
 * displacement, that displacement is re-relativized to reach
 * its current target from the encoded location.
 * Returns NULL on failure to encode (due to reachability).
 */
byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr, byte *dst_pc,
                                 byte *final_pc)
{
    byte *orig_dst_pc = dst_pc;
    ASSERT(instr_raw_bits_valid(instr));
    /* FIXME i#731: if want to support ctis as well, need
     * instr->rip_rel_disp_sz and need to set both for non-x64 as well
     * in decode_sizeof(): or only in decode_cti()?
     */
    /* For PR 251646 we have special support for mangled jecxz/loop* */
    if (instr_is_cti_short_rewrite(instr, NULL)) {
        app_pc target;
        CLIENT_ASSERT(opnd_is_pc(instr_get_target(instr)),
                      "cti_short_rewrite: must have pc target");
        target = opnd_get_pc(instr_get_target(instr));
        memcpy(dst_pc, instr->bytes, instr->length - 4);
        dst_pc += instr->length - 4;
        if (!REL32_REACHABLE(final_pc + instr->length, target)) {
            CLIENT_ASSERT(false, "mangled jecxz/loop*: target out of 32-bit reach");
            return NULL;
        }
        *((int *)dst_pc) = (int)(target - (final_pc + instr->length));
    }
#ifdef X64
    /* We test the flag directly to support cases where the raw bits are
     * being set by private_instr_encode() */
    else if (instr_rip_rel_valid(instr) && instr_get_rip_rel_pos(instr) > 0) {
        /* x64 4-byte rip-relative data address displacement */
        byte *target;
        ptr_int_t new_offs;
        bool addr32 = false;
        uint rip_rel_pos = instr_get_rip_rel_pos(instr); /* disp offs within instr */
        DEBUG_DECLARE(bool ok;)
        ASSERT(!instr_is_level_0(instr));
        DEBUG_DECLARE(ok =) instr_get_rel_addr_target(instr, &target);
        ASSERT(ok);
        new_offs = target - (final_pc + instr->length);
        /* PR 253327: we don't record whether addr32 so we have to deduce it now */
        if ((ptr_uint_t)target <= INT_MAX) {
            int num_prefixes;
            int i;
            IF_X64(bool old_mode = set_x86_mode(dcontext, instr_get_x86_mode(instr));)
            decode_sizeof(dcontext, instr->bytes, &num_prefixes, NULL);
            IF_X64(set_x86_mode(dcontext, old_mode));
            for (i = 0; i < num_prefixes; i++) {
                if (*(instr->bytes + i) == ADDR_PREFIX_OPCODE) {
                    addr32 = true;
                    break;
                }
            }
        }
        if (!addr32 && !REL32_REACHABLE_OFFS(new_offs)) {
            /* unreachable: not clear whether routing through register here
             * is worth the complexities of the length changing, so for now
             * we fail and rely on caller to do a conservative estimate
             * of reachability and transform this instruction before encoding.
             */
            CLIENT_ASSERT(false,
                          "encoding failed re-relativizing rip-relative "
                          "address whose target is unreachable");
            return NULL;
        }
        memcpy(dst_pc, instr->bytes, rip_rel_pos);
        dst_pc += rip_rel_pos;
        *((int *)dst_pc) = (int)new_offs;
        if (rip_rel_pos + 4U < instr->length) {
            /* suffix byte */
            memcpy(dst_pc + 4, instr->bytes + rip_rel_pos + 4,
                   instr->length - (rip_rel_pos + 4));
        }
    } else
#endif
        memcpy(dst_pc, instr->bytes, instr->length);
    return orig_dst_pc + instr->length;
}

/* Encodes instruction instr.  The parameter copy_pc points
 * to the address of this instruction in the fragment cache.
 * Checks for and fixes pc-relative instructions.
 * N.B: if instr is a jump with an instr_t target, the caller MUST set the note
 * field in the target instr_t prior to calling instr_encode on the jump instr.
 *
 * Returns the pc after the encoded instr, or NULL if the instruction cannot be encoded.
 * Note that if instr_is_label(instr) it will be  encoded as a 0-byte instruction.
 * If a pc-relative operand cannot reach its target:
 *   If reachable == NULL, we assert and encoding fails (returning NULL);
 *   Else, encoding continues, and *reachable is set to false.
 * Else, if reachable != NULL, *reachable is set to true.
 */
byte *
instr_encode_arch(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc,
                  bool check_reachable,
                  bool *has_instr_opnds /*OUT OPTIONAL*/
                      _IF_DEBUG(bool assert_reachable))
{
    const instr_info_t *info;
    decode_info_t di;

    /* pointer to and into the instruction binary */
    byte *cache_pc = copy_pc;
    byte *field_ptr = cache_pc;
    byte *disp_relativize_at = NULL;
    uint opc;
    bool output_initial_opcode = false;
    if (has_instr_opnds != NULL)
        *has_instr_opnds = false;

    /* first handle the already-encoded instructions */
    if (instr_raw_bits_valid(instr)) {
        CLIENT_ASSERT(check_reachable,
                      "internal encode error: cannot encode raw "
                      "bits and ignore reachability");
        /* copy raw bits, possibly re-relativizing */
        return copy_and_re_relativize_raw_instr(dcontext, instr, cache_pc, final_pc);
    }
    CLIENT_ASSERT(instr_operands_valid(instr), "instr_encode error: operands invalid");
    opc = instr_get_opcode(instr);
    if ((instr_is_cbr(instr) &&
         (!instr_is_cti_loop(instr) ||
          /* no addr16 */
          reg_is_pointer_sized(opnd_get_reg(instr_get_src(instr, 1))))) ||
        /* no indirect or far */
        opc == OP_jmp_short || opc == OP_jmp || opc == OP_call) {
        if (!TESTANY(~(PREFIX_JCC_TAKEN | PREFIX_JCC_NOT_TAKEN), instr->prefixes)) {
            /* encode_cti cannot handle funny prefixes or indirect branches or rets */
            return encode_cti(instr, copy_pc, final_pc,
                              check_reachable _IF_DEBUG(assert_reachable));
        }
    }

    /* else really encode */
    info = instr_get_instr_info(instr);
    if (info == NULL) {
        CLIENT_ASSERT(instr_is_label(instr), "instr_encode: invalid instr");
        return (instr_is_label(instr) ? copy_pc : NULL);
    }

    /* first, walk through instr list to find format that matches
     * this instr's operands
     */
    DOLOG(ENC_LEVEL, LOG_EMIT, { d_r_loginst(dcontext, 1, instr, "\n--- encoding"); });

    memset(&di, 0, sizeof(decode_info_t));
    di.opcode = opc;
    IF_X64(di.x86_mode = instr_get_x86_mode(instr));
    /* while only PREFIX_SIGNIFICANT should be set by the user, internally
     * we set di.prefixes to communicate size prefixes between
     * opnd_type_ok() and here, first clearing out the size specifiers
     * in encoding_possible().
     */
    di.prefixes = instr->prefixes;
    di.vex_vvvv = 0xf; /* 4 1's by default */

    /* We check predication, to help clients who are generating instrs from
     * having incorrect analysis results on their own gencode.
     * We assume each opcode has constant predication info.
     */
    if (instr_get_predicate(instr) != decode_predicate_from_instr_info(opc, info)) {
        if (instr_get_predicate(instr) == DR_PRED_NONE)
            CLIENT_ASSERT(false, "instr is missing a predicate");
        else
            CLIENT_ASSERT(false, "instr contains an invalid predicate for its opcode");
        return NULL;
    }

    /* Used for PR 253327 addr32 rip-relative and instr_t targets, including
     * during encoding_possible().
     */
    di.start_pc = cache_pc;
    di.final_pc = final_pc;

    while (!encoding_possible(&di, instr, info)) {
        LOG(THREAD, LOG_EMIT, ENC_LEVEL, "\tencoding for 0x%x no good...\n",
            info->opcode);
        info = get_next_instr_info(info);
        /* stop when hit end of list or when hit extra operand tables (OP_CONTD) */
        if (info == NULL || info->type == OP_CONTD) {
            DOLOG(1, LOG_EMIT, {
                LOG(THREAD, LOG_EMIT, 1, "ERROR: Could not find encoding for: ");
                instr_disassemble(dcontext, instr, THREAD);
                LOG(THREAD, LOG_EMIT, 1, "\n");
            });
            CLIENT_ASSERT(false, "instr_encode error: no encoding found (see log)");
            /* FIXME: since labels (case 4468) have a legal length 0
             * we may want to return a separate status code for failure.
             */
            return NULL;
        }
    }

    /* fill out the other fields of di */
    di.size_immed = OPSZ_NA;
    di.size_immed2 = OPSZ_NA;
    /* these (illegal) values indicate uninitialization */
    di.reg = 8;
    di.mod = 5;

    /* prefixes */
    di.seg_override = REG_NULL; /* operands will fill in */

    /* instr_t* operand support */
    di.cur_note = (ptr_int_t)instr->note;

    /* operands
     * we can ignore extra operands here, since all extra operands
     * are hardcoded
     */
    if (info->dst1_type != TYPE_NONE)
        encode_operand(&di, info->dst1_type, info->dst1_size, instr_get_dst(instr, 0));
    if (info->dst2_type != TYPE_NONE)
        encode_operand(&di, info->dst2_type, info->dst2_size, instr_get_dst(instr, 1));
    if (info->src1_type != TYPE_NONE)
        encode_operand(&di, info->src1_type, info->src1_size, instr_get_src(instr, 0));
    if (info->src2_type != TYPE_NONE)
        encode_operand(&di, info->src2_type, info->src2_size, instr_get_src(instr, 1));
    if (info->src3_type != TYPE_NONE)
        encode_operand(&di, info->src3_type, info->src3_size, instr_get_src(instr, 2));
    if (di.mod == 5 && di.reg < 8) { /* mod may never be set (e.g., OP_extrq) */
        /* follow lead of below where we set to all 1's */
        di.mod = 3;
        CLIENT_ASSERT(di.rm == 0, "internal error: mod not set but rm was");
        di.rm = 7;
    }

    /* finally, do the actual bit writing */

    /* output the prefix byte(s) */
    if (di.prefixes != 0) {
        if (TEST(PREFIX_LOCK, di.prefixes)) {
            *field_ptr = RAW_PREFIX_lock;
            field_ptr++;
        }
        if (TEST(PREFIX_XACQUIRE, di.prefixes)) {
            *field_ptr = RAW_PREFIX_xacquire;
            field_ptr++;
        }
        if (TEST(PREFIX_XRELEASE, di.prefixes)) {
            *field_ptr = RAW_PREFIX_xrelease;
            field_ptr++;
        }
        if (TEST(PREFIX_JCC_TAKEN, di.prefixes)) {
            *field_ptr = RAW_PREFIX_jcc_taken;
            field_ptr++;
        } else if (TEST(PREFIX_JCC_NOT_TAKEN, di.prefixes)) {
            *field_ptr = RAW_PREFIX_jcc_not_taken;
            field_ptr++;
        }
    }
    if (TEST(PREFIX_DATA, di.prefixes)) {
        *field_ptr = DATA_PREFIX_OPCODE;
        field_ptr++;
    }
    /* N.B.: we assume the order of 0x67 <seg> in coarse_is_indirect_stub()
     * and instr_is_tls_xcx_spill()
     */
    if (TEST(PREFIX_ADDR, di.prefixes)) {
        *field_ptr = ADDR_PREFIX_OPCODE;
        field_ptr++;
    }
    if (di.seg_override != REG_NULL) {
        switch (di.seg_override) {
        case SEG_ES: *field_ptr = 0x26; break;
        case SEG_CS: *field_ptr = 0x2e; break;
        case SEG_SS: *field_ptr = 0x36; break;
        case SEG_DS: *field_ptr = 0x3e; break;
        case SEG_FS: *field_ptr = 0x64; break;
        case SEG_GS: *field_ptr = 0x65; break;
        default: CLIENT_ASSERT(false, "instr_encode error: unknown segment prefix");
        }
        field_ptr++;
    }

    /* vex prefix must be last and if present includes prefix bytes, rex flags,
     * and some opcode bytes
     */
    if (TEST(REQUIRES_VEX, info->flags)) {
        field_ptr = encode_vex_prefixes(field_ptr, &di, info, &output_initial_opcode);
    } else {
        CLIENT_ASSERT(!TEST(PREFIX_VEX_L, di.prefixes), "internal encode vex error");

        /* output the opcode required prefix byte (if needed) */
        if (info->opcode > 0xffffff &&
            /* if OPCODE_{MODRM,SUFFIX} there can be no prefix-opcode byte */
            !TESTANY(OPCODE_MODRM | OPCODE_SUFFIX, info->opcode)) {
            /* prefix byte is part of opcode */
            *field_ptr = (byte)(info->opcode >> 24);
            field_ptr++;
        }

        if (TEST(REQUIRES_REX, info->flags)) {
            /* We could add other rex flags by overloading OPCODE_SUFFIX or
             * possibly OPCODE_MODRM (but the latter only for instrs that
             * aren't in mod_ext).  For now this flag implies rex.w.
             */
            di.prefixes |= PREFIX_REX_W;
        }

        /* NOTE - the rex prefix must be the last prefix (even if the other prefix is
         * part of the opcode). Xref PR 271878. */
        if (TESTANY(PREFIX_REX_ALL, di.prefixes)) {
            byte rexval = REX_PREFIX_BASE_OPCODE;
            if (TEST(PREFIX_REX_W, di.prefixes))
                rexval |= REX_PREFIX_W_OPFLAG;
            if (TEST(PREFIX_REX_R, di.prefixes))
                rexval |= REX_PREFIX_R_OPFLAG;
            if (TEST(PREFIX_REX_X, di.prefixes))
                rexval |= REX_PREFIX_X_OPFLAG;
            if (TEST(PREFIX_REX_B, di.prefixes))
                rexval |= REX_PREFIX_B_OPFLAG;
            *field_ptr = rexval;
            field_ptr++;
        }
    }

    if (!output_initial_opcode) {
        /* output the opcode byte(s) (opcode required prefixes are handled above) */
        if (TEST(OPCODE_THREEBYTES, info->opcode)) {
            /* implied initial opcode byte */
            *field_ptr = 0x0f;
            field_ptr++;
        }
        /* first opcode byte */
        *field_ptr = (byte)((info->opcode & 0x00ff0000) >> 16);
        field_ptr++;
    }

    /* second opcode byte, if there is one */
    if (TEST(OPCODE_TWOBYTES, info->opcode)) {
        *field_ptr = (byte)((info->opcode & 0x0000ff00) >> 8);
        field_ptr++;
    }
    /* /n: part of opcode is in reg of modrm byte */
    if (TEST(OPCODE_REG, info->opcode)) {
        CLIENT_ASSERT(di.reg == 8,
                      "instr_encode error: /n opcode inconsistency"); /* unset */
        di.reg = (byte)(info->opcode & 0x00000007);
        if (di.mod == 5) {
            /* modrm only used for opcode
             * mod and rm are arbitrary: seem to be set to all 1's by compilers
             */
            di.mod = 3;
            di.rm = 7;
        }
    }
    /* opcode depends on entire modrm byte */
    if (!TEST(REQUIRES_VEX, info->flags) && TEST(OPCODE_MODRM, info->opcode)) {
        /* modrm is encoded in prefix byte */
        *field_ptr = (byte)(info->opcode >> 24);
        field_ptr++;
        di.mod = 5; /* prevent modrm output from opnds below */
    }

    /* output modrm byte(s) */
    if (di.mod != 5) {
        byte modrm;
        if (di.reg == 8) /* if never set, set to 0 */
            di.reg = 0;
        CLIENT_ASSERT(di.mod <= 0x3 && di.reg <= 0x7 && di.rm <= 0x7,
                      "encode error: invalid modrm");
        modrm = MODRM_BYTE(di.mod, di.reg, di.rm);
        *field_ptr = modrm;
        field_ptr++;
        if (di.has_sib) {
            byte sib = (byte)((di.scale << 6) | (di.index << 3) | (di.base));
            CLIENT_ASSERT(di.scale <= 0x3 && di.index <= 0x7 && di.base <= 0x7,
                          "encode error: invalid scale/index/base");
            *field_ptr = sib;
            field_ptr++;
        }
        if (di.has_disp) {
            if (di.mod == 1) {
                *field_ptr = (byte)di.disp;
                field_ptr++;
            } else if (!X64_MODE(&di) && TEST(PREFIX_ADDR, di.prefixes)) {
                CLIENT_ASSERT_TRUNCATE(*((short *)field_ptr), ushort, di.disp,
                                       "encode error: modrm disp too large for 16-bit");
                *((short *)field_ptr) = (ushort)di.disp;
                field_ptr += 2;
            } else {
                if (X64_MODE(&di) && di.mod == 0 && di.rm == 5) {
                    /* pc-relative, but we don't know size of immeds yet */
                    disp_relativize_at = field_ptr;
                } else {
                    *((int *)field_ptr) = di.disp;
                }
                field_ptr += 4;
            }
        }
    }

    /* output immed byte(s) */
    /* HACK: to tell an instr target of a control transfer instruction
     * our length, store into di.modrm the bytes so far
     */
    CLIENT_ASSERT_TRUNCATE(di.modrm, byte, (field_ptr - cache_pc),
                           "encode error: instr too long");
    di.modrm = (byte)(field_ptr - cache_pc);
    if (di.size_immed != OPSZ_NA)
        field_ptr = encode_immed(&di, field_ptr);
    if (di.size_immed2 != OPSZ_NA)
        field_ptr = encode_immed(&di, field_ptr);

    /* suffix opcode */
    if (!TEST(REQUIRES_VEX, info->flags) && TEST(OPCODE_SUFFIX, info->opcode)) {
        /* none of these have immeds, currently (and presumably never will have) */
        ASSERT_CURIOSITY(di.size_immed == OPSZ_NA && di.size_immed2 == OPSZ_NA);
        /* modrm is encoded in prefix byte */
        *field_ptr = (byte)(info->opcode >> 24);
        field_ptr++;
    }

    if (disp_relativize_at != NULL) {
        CLIENT_ASSERT(X64_MODE(&di), "encode error: no rip-relative in x86 mode!");
        if (check_reachable &&
            !CHECK_TRUNCATE_TYPE_int(di.disp_abs - (field_ptr - copy_pc + final_pc)) &&
            /* PR 253327: we auto-add addr prefix for out-of-reach low tgt */
            (!TEST(PREFIX_ADDR, di.prefixes) || (ptr_uint_t)di.disp_abs > INT_MAX)) {
            CLIENT_ASSERT(!assert_reachable,
                          "encode error: rip-relative reference out of 32-bit reach");
            return NULL;
        }
        *((int *)disp_relativize_at) =
            (int)(di.disp_abs - (field_ptr - copy_pc + final_pc));
        /* in case caller is caching these bits (in particular,
         * private_instr_encode()), set rip_rel_pos */
        CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_byte(disp_relativize_at - di.start_pc),
                      "internal encode error: rip-relative instr pos too large");
        IF_X64(instr_set_rip_rel_pos(instr, (byte)(disp_relativize_at - di.start_pc)));
    }

#if DEBUG_DISABLE /* turn back on if want to debug */
    if (d_r_stats->loglevel >= 3) {
        byte *pc = cache_pc;
        LOG(THREAD, LOG_EMIT, 3, "instr_encode on: ");
        instr_disassemble(dcontext, instr, THREAD);
        LOG(THREAD, LOG_EMIT, 3, " : ");
        do {
            LOG(THREAD, LOG_EMIT, 3, " %02x", *pc);
            pc++;
        } while (pc < field_ptr);
        LOG(THREAD, LOG_EMIT, 3, "\n");
    }
    if (0 && instr_raw_bits_valid(instr)) {
        /* encoding in original bits */
        int i;
        int len = (field_ptr - cache_pc);
        byte ours, orig;
        for (i = 0; i < len; i++) {
            ours = *(cache_pc + i);
            orig = *(((byte *)instr_get_raw_bits(instr)) + i);
            if (ours != orig) {
                /* special cases: difference is ok, not an error: */
                if (i == 0 && ours == 0x66 && (orig == 0xf2 || orig == 0xf3) &&
                    /* allow rep/repne & data prefixes to have order swapped */
                    *(cache_pc + i + 1) == orig &&
                    *(((byte *)instr_get_raw_bits(instr)) + i + 1) == ours) {
                    i++; /* skip next */
                } else if (i == 0 &&
                           ((ours == 0x3a && orig == 0x38) ||
                            (ours == 0x3b && orig == 0x39) ||
                            (ours == 0x03 && orig == 0x01) ||
                            (ours == 0x33 && orig == 0x31))) {
                    /* Gv, Ev vs Ev, Gv equivalent when both opnds regs */
                    i++; /* skip next */
                } else if ((i == 1 || i == 2) && ((ours | 0x3c) == 0x3c) &&
                           ((orig | 0x7c) == 0x7c) &&
                           *(((byte *)instr_get_raw_bits(instr)) + i + 2) == 0x0) {
                    /* mod=0, rm=4, no disp same as mod=1, rm=4, disp=0 */
                    /* I see these for (%esp) */
                    i = len; /* skip rest */
                } else if (i == 0 && ours == 0xd8 && orig == 0xdc &&
                           *(cache_pc + i + 1) == 0xc0 &&
                           *(((byte *)instr_get_raw_bits(instr)) + i + 1) == 0xc0) {
                    /* fadd st0, st0 => st0: two ways to do it */
                    i++; /* skip rest */
                } else {
                    LOG(THREAD, LOG_EMIT, 3,
                        "Error: encoding does not match original bits!\n");
                    for (i = 0; i < len; i++) {
                        ours = *(cache_pc + i);
                        LOG(THREAD, LOG_EMIT, 3, " %02x", ours);
                    }
                    LOG(THREAD, LOG_EMIT, 3, " vs. ");
                    for (i = 0; i < len; i++) {
                        orig = *(((byte *)instr_get_raw_bits(instr)) + i);
                        LOG(THREAD, LOG_EMIT, 3, " %02x", orig);
                    }
                    LOG(THREAD, LOG_EMIT, 3, "\n");
                    CLIENT_ASSERT(false, "instr_encode error: encode != raw bits");
                }
            }
        }
    }
#endif

    if (has_instr_opnds != NULL)
        *has_instr_opnds = di.has_instr_opnds;
    return field_ptr;
}
