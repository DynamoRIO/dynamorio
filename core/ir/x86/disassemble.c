/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2009 VMware, Inc.  All rights reserved.
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

/* disassemble.c -- printing of x86 instructions */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "decode_private.h"
#include "disassemble.h"

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

/* in disassemble_shared.c */
void
internal_opnd_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT,
                          dcontext_t *dcontext, opnd_t opnd, bool use_size_sfx);
void
reg_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT, reg_id_t reg,
                dr_opnd_flags_t flags, const char *prefix, const char *suffix);

#define BYTES_PER_LINE 7

int
print_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, byte *pc,
                      byte *next_pc, instr_t *instr)
{
    int sz = (int)(next_pc - pc);
    int i, extra_sz;
    if (sz > BYTES_PER_LINE) {
        extra_sz = sz - BYTES_PER_LINE;
        sz = BYTES_PER_LINE;
    } else
        extra_sz = 0;
    for (i = 0; i < sz; i++)
        print_to_buffer(buf, bufsz, sofar, " %02x", *(pc + i));
    if (!instr_valid(instr)) {
        print_to_buffer(buf, bufsz, sofar, "...?? ");
        sz += 2;
    }
    for (i = sz; i < BYTES_PER_LINE; i++)
        print_to_buffer(buf, bufsz, sofar, "   ");
    print_to_buffer(buf, bufsz, sofar, " ");
    return extra_sz;
}

void
print_extra_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, byte *pc,
                            byte *next_pc, int extra_sz, const char *extra_bytes_prefix)
{
    int i;
    if (extra_sz > 0) {
        print_to_buffer(buf, bufsz, sofar, "%s", extra_bytes_prefix);
        for (i = 0; i < extra_sz; i++)
            print_to_buffer(buf, bufsz, sofar, " %02x", *(pc + BYTES_PER_LINE + i));
        print_to_buffer(buf, bufsz, sofar, "\n");
    }
}

void
opnd_base_disp_scale_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT,
                                 opnd_t opnd)
{
    int scale = opnd_get_scale(opnd);
    if (scale > 1) {
        if (TEST(DR_DISASM_INTEL, DYNAMO_OPTION(disasm_mask)))
            print_to_buffer(buf, bufsz, sofar, "*%d", scale);
        else
            print_to_buffer(buf, bufsz, sofar, ",%d", scale);
    }
}

bool
opnd_disassemble_arch(char *buf, size_t bufsz, size_t *sofar INOUT, opnd_t opnd)
{
    /* nothing */
    return false;
}

static bool
instr_implicit_reg(instr_t *instr)
{
    /* instrs that have multiple encodings whose reg opnds are always implicit */
    switch (instr_get_opcode(instr)) {
    case OP_ins:
    case OP_rep_ins:
    case OP_outs:
    case OP_rep_outs:
    case OP_movs:
    case OP_rep_movs:
    case OP_stos:
    case OP_rep_stos:
    case OP_lods:
    case OP_rep_lods:
    case OP_cmps:
    case OP_rep_cmps:
    case OP_repne_cmps:
    case OP_scas:
    case OP_rep_scas:
    case OP_repne_scas: return true;
    }
    return false;
}

bool
opnd_disassemble_noimplicit(char *buf, size_t bufsz, size_t *sofar INOUT,
                            dcontext_t *dcontext, instr_t *instr, byte optype,
                            opnd_t opnd, bool prev, bool multiple_encodings, bool dst,
                            int *idx INOUT)
{
    switch (optype) {
    case TYPE_REG:
    case TYPE_VAR_REG:
    case TYPE_VARZ_REG:
    case TYPE_VAR_XREG:
    case TYPE_REG_EX:
    case TYPE_VAR_REG_EX:
    case TYPE_VAR_XREG_EX:
    case TYPE_VAR_REGX_EX:
    case TYPE_VAR_REGX:
        /* we do want to print implicit operands for opcode-decides-register
         * instrs like inc-reg and pop-reg, but not for say lahf, aaa, or cdq.
         */
        if (!multiple_encodings || instr_implicit_reg(instr) ||
            /* if has implicit st0 then don't print it */
            (opnd_get_reg(opnd) == REG_ST0 && instr_memory_reference_size(instr) > 0))
            return false;
        /* else fall through */
    case TYPE_A:
    case TYPE_B:
    case TYPE_C:
    case TYPE_D:
    case TYPE_E:
    case TYPE_INDIR_E:
    case TYPE_G:
    case TYPE_H:
    case TYPE_I:
    case TYPE_J:
    case TYPE_L:
    case TYPE_M:
    case TYPE_O:
    case TYPE_P:
    case TYPE_Q:
    case TYPE_R:
    case TYPE_S:
    case TYPE_V:
    case TYPE_W:
    case TYPE_P_MODRM:
    case TYPE_V_MODRM:
    case TYPE_FLOATMEM:
    case TYPE_VSIB:
    case TYPE_1:
    case TYPE_K_REG:
    case TYPE_K_MODRM:
    case TYPE_K_MODRM_R:
    case TYPE_K_VEX:
    case TYPE_K_EVEX:
    case TYPE_T_REG:
    case TYPE_T_MODRM:
        if (prev)
            print_to_buffer(buf, bufsz, sofar, ", ");
        internal_opnd_disassemble(buf, bufsz, sofar, dcontext, opnd, false);
        return true;
    case TYPE_X:
    case TYPE_XLAT:
    case TYPE_MASKMOVQ:
        if (opnd_get_segment(opnd) != SEG_DS) {
            /* FIXME: really we should put before opcode */
            if (prev)
                print_to_buffer(buf, bufsz, sofar, ", ");
            reg_disassemble(buf, bufsz, sofar, opnd_get_segment(opnd), 0, "", "");
            return true;
        }
    case TYPE_Y:
    case TYPE_FLOATCONST:
    case TYPE_XREG:
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
        /* implicit operand */
        return false;
    default: CLIENT_ASSERT(false, "missing decode type"); /* catch any missing types */
    }
    return false;
}

static const char *
instr_opcode_name(instr_t *instr)
{
    if (TEST(DR_DISASM_INTEL, DYNAMO_OPTION(disasm_mask))) {
        switch (instr_get_opcode(instr)) {
        /* remove "l" prefix */
        case OP_call_far: return "call";
        case OP_call_far_ind: return "call";
        case OP_jmp_far: return "jmp";
        case OP_jmp_far_ind: return "jmp";
        case OP_ret_far: return "retf";
        }
    }
#ifdef X64
    if (!instr_get_x86_mode(instr)) {
        if (instr_get_opcode(instr) == OP_jecxz &&
            reg_is_pointer_sized(opnd_get_reg(instr_get_src(instr, 1))))
            return "jrcxz";
        else if (instr_get_opcode(instr) == OP_pextrd &&
                 opnd_get_size(instr_get_dst(instr, 0)) == OPSZ_PTR)
            return "pextrq";
        else if (instr_get_opcode(instr) == OP_vpextrd &&
                 opnd_get_size(instr_get_dst(instr, 0)) == OPSZ_PTR)
            return "vpextrq";
        else if (instr_get_opcode(instr) == OP_pinsrd &&
                 opnd_get_size(instr_get_src(instr, 0)) == OPSZ_PTR)
            return "pinsrq";
        else if (instr_get_opcode(instr) == OP_vpinsrd &&
                 opnd_get_size(instr_get_src(instr, 0)) == OPSZ_PTR)
            return "vpinsrq";
    }
#endif
    return NULL;
}

static const char *
instr_opcode_name_suffix(instr_t *instr)
{
    if (TESTANY(DR_DISASM_INTEL | DR_DISASM_ATT, DYNAMO_OPTION(disasm_mask))) {
        /* add "b" or "d" suffix */
        switch (instr_get_opcode(instr)) {
        case OP_pushf:
        case OP_popf:
        case OP_xlat:
        case OP_ins:
        case OP_rep_ins:
        case OP_outs:
        case OP_rep_outs:
        case OP_movs:
        case OP_rep_movs:
        case OP_stos:
        case OP_rep_stos:
        case OP_lods:
        case OP_rep_lods:
        case OP_cmps:
        case OP_rep_cmps:
        case OP_repne_cmps:
        case OP_scas:
        case OP_rep_scas:
        case OP_repne_scas: {
            uint sz = instr_memory_reference_size(instr);
            if (sz == 1)
                return "b";
            else if (sz == 2)
                return "w";
            else if (sz == 4)
                return "d";
            else if (sz == 8)
                return "q";
        }
        case OP_pusha:
        case OP_popa: {
            uint sz = instr_memory_reference_size(instr);
            if (sz == 16)
                return "w";
            else if (sz == 32)
                return "d";
        }
        case OP_iret: {
            uint sz = instr_memory_reference_size(instr);
            if (sz == 6)
                return "w";
            else if (sz == 12)
                return "d";
            else if (sz == 40)
                return "q";
        }
        }
    }
    if (TEST(DR_DISASM_ATT, DYNAMO_OPTION(disasm_mask)) && instr_operands_valid(instr)) {
        /* XXX: requiring both src and dst.  Ideally we'd wait until we
         * see if there is a register or in some cases an immed operand
         * and then go back and add the suffix.  This will do for now.
         */
        if (instr_num_srcs(instr) > 0 && !opnd_is_reg(instr_get_src(instr, 0)) &&
            instr_num_dsts(instr) > 0 && !opnd_is_reg(instr_get_dst(instr, 0))) {
            uint sz = instr_memory_reference_size(instr);
            if (sz == 1)
                return "b";
            else if (sz == 2)
                return "w";
            else if (sz == 4)
                return "l";
            else if (sz == 8)
                return "q";
        }
    }
    return "";
}

void
print_opcode_name(instr_t *instr, const char *name, char *buf, size_t bufsz,
                  size_t *sofar INOUT)
{
    const char *subst_name = instr_opcode_name(instr);
    print_to_buffer(buf, bufsz, sofar, "%s%s", subst_name == NULL ? name : subst_name,
                    instr_opcode_name_suffix(instr));
}

void
print_instr_prefixes(dcontext_t *dcontext, instr_t *instr, char *buf, size_t bufsz,
                     size_t *sofar INOUT)
{
    if (TEST(PREFIX_XACQUIRE, instr->prefixes))
        print_to_buffer(buf, bufsz, sofar, "xacquire ");
    if (TEST(PREFIX_XRELEASE, instr->prefixes))
        print_to_buffer(buf, bufsz, sofar, "xrelease ");
    if (TEST(PREFIX_LOCK, instr->prefixes))
        print_to_buffer(buf, bufsz, sofar, "lock ");
    /* Note that we do not try to figure out data16 or addr16 prefixes
     * if they are not already set from a recent decode;
     * we don't want to enforce a valid encoding at this point.
     *
     * To walk the operands and find addr16, we'd need to look for
     * opnd_is_disp_short_addr() as well as push/pop of REG_SP, jecxz/loop* of
     * REG_CX, or string ops, maskmov*, or xlat of REG_DI or REG_SI.
     * For data16, we'd look for 16-bit reg or OPSZ_2 immed or base_disp.
     */
    if (!TEST(DR_DISASM_INTEL, DYNAMO_OPTION(disasm_mask))) {
        if (TEST(PREFIX_DATA, instr->prefixes))
            print_to_buffer(buf, bufsz, sofar, "data16 ");
        if (TEST(PREFIX_ADDR, instr->prefixes)) {
            print_to_buffer(buf, bufsz, sofar,
                            X64_MODE_DC(dcontext) ? "addr32 " : "addr16 ");
        }
#if 0 /* disabling for PR 256226 */
        if (TEST(PREFIX_REX_W, instr->prefixes))
            print_to_buffer(buf, bufsz, sofar, "rex.w ");
#endif
    }
}

int
print_opcode_suffix(instr_t *instr, char *buf, size_t bufsz, size_t *sofar INOUT)
{
    if (TEST(PREFIX_JCC_TAKEN, instr->prefixes)) {
        print_to_buffer(buf, bufsz, sofar, ",pt");
        return 2;
    } else if (TEST(PREFIX_JCC_NOT_TAKEN, instr->prefixes)) {
        print_to_buffer(buf, bufsz, sofar, ",pn");
        return 2;
    }
    return 0;
}
