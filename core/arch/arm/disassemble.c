/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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

/* disassemble.c -- printing of ARM instructions */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "decode_private.h"
#include "disassemble.h"
#include <string.h>

#if defined(INTERNAL) || defined(DEBUG) || defined(CLIENT_INTERFACE)

#ifdef DEBUG
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
# undef ASSERT_TRUNCATE
# undef ASSERT_BITFIELD_TRUNCATE
# undef ASSERT_NOT_REACHED
# define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
# define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
# define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

static const char * const pred_names[] = {
    "",    /* DR_PRED_NONE */
    ".eq", /* DR_PRED_EQ */
    ".ne", /* DR_PRED_NE */
    ".cs", /* DR_PRED_CS */
    ".cc", /* DR_PRED_CC */
    ".mi", /* DR_PRED_MI */
    ".pl", /* DR_PRED_PL */
    ".vs", /* DR_PRED_VS */
    ".vc", /* DR_PRED_VC */
    ".hi", /* DR_PRED_HI */
    ".ls", /* DR_PRED_LS */
    ".ge", /* DR_PRED_GE */
    ".lt", /* DR_PRED_LT */
    ".gt", /* DR_PRED_GT */
    ".le", /* DR_PRED_LE */
    "",    /* DR_PRED_AL */
    "",    /* DR_PRED_OP */
};

/* in disassemble_shared.c */
void
internal_opnd_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT,
                          dcontext_t *dcontext, opnd_t opnd,
                          bool use_size_sfx);

const char *
instr_predicate_name(dr_pred_type_t pred)
{
    if (pred > DR_PRED_OP)
        return NULL;
    return pred_names[pred];
}

int
print_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT,
                      byte *pc, byte *next_pc, instr_t *instr)
{
    /* Follow conventions used elsewhere with split for T32, solid for the rest */
    if (instr_get_isa_mode(instr) == DR_ISA_ARM_THUMB) {
        if (next_pc - pc == 0)
            print_to_buffer(buf, bufsz, sofar, "            ", *((ushort *)pc));
        else if (next_pc - pc == 2)
            print_to_buffer(buf, bufsz, sofar, " %04x       ", *((ushort *)pc));
        else {
            CLIENT_ASSERT(next_pc - pc == 4, "invalid thumb size");
            print_to_buffer(buf, bufsz, sofar, " %04x %04x  ",
                            *((ushort *)pc), *(((ushort *)pc)+1));
        }
    } else {
        print_to_buffer(buf, bufsz, sofar, " %08x   ", *((uint *)pc));
    }
    return 0; /* no extra size */
}

void
print_extra_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT,
                            byte *pc, byte *next_pc, int extra_sz,
                            const char *extra_bytes_prefix)
{
    /* There are no "extra" bytes */
}

static void
disassemble_shift(char *buf, size_t bufsz, size_t *sofar INOUT, const char *prefix,
                  const char *suffix,
                  dr_shift_type_t shift, bool immed_amount, uint amount)
{
    switch (shift) {
    case DR_SHIFT_NONE:
        break;
    case DR_SHIFT_RRX:
        print_to_buffer(buf, bufsz, sofar, "%srrx", prefix);
        /* XXX i#1551: do not print amount for ARM style */
        if (immed_amount)
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    case DR_SHIFT_LSL:
        /* XXX i#1551: use #%d for ARM style */
        print_to_buffer(buf, bufsz, sofar, "%slsl", prefix);
        if (immed_amount)
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    case DR_SHIFT_LSR:
        print_to_buffer(buf, bufsz, sofar, "%slsr", prefix);
        if (immed_amount)
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    case DR_SHIFT_ASR:
        print_to_buffer(buf, bufsz, sofar, "%sasr", prefix);
        if (immed_amount)
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    case DR_SHIFT_ROR:
        print_to_buffer(buf, bufsz, sofar, "%sror", prefix);
        if (immed_amount)
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    default:
        print_to_buffer(buf, bufsz, sofar, ",UNKNOWN SHIFT");
        break;
    }
    print_to_buffer(buf, bufsz, sofar, "%s", suffix);
}

void
opnd_base_disp_scale_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT,
                                 opnd_t opnd)
{
    uint amount;
    dr_shift_type_t shift = opnd_get_index_shift(opnd, &amount);
    disassemble_shift(buf, bufsz, sofar, ",", "", shift, true, amount);
}

int
opnd_disassemble_src_arch(char *buf, size_t bufsz, size_t *sofar INOUT,
                          instr_t *instr, int idx)
{
    opnd_t src = instr_get_src(instr, idx);
    if (opnd_is_reg(src) && TEST(DR_OPND_SHIFTED, opnd_get_flags(src)) &&
        idx + 2 < instr_num_srcs(instr)) {
        opnd_t nxt = instr_get_src(instr, idx + 1);
        if (opnd_is_immed_int(nxt)) {
            dr_shift_type_t shift = (dr_shift_type_t) opnd_get_immed_int(nxt);
            opnd_t nxt2 = instr_get_src(instr, idx + 2);
            bool immed_amount = opnd_is_immed_int(nxt2);
            disassemble_shift(buf, bufsz, sofar, " ", "", shift, immed_amount,
                              immed_amount ? opnd_get_immed_int(nxt2) : 0);
            return (immed_amount ? idx + 2 : idx + 1);
        }
    }
    return idx;
}

bool
opnd_disassemble_noimplicit(char *buf, size_t bufsz, size_t *sofar INOUT,
                            dcontext_t *dcontext, instr_t *instr,
                            byte optype, opnd_t opnd, bool prev, bool multiple_encodings)
{
    /* FIXME i#1683: we need to avoid the implicit dst-as-src regs for instrs
     * such as OP_smlal.
     */
    if (prev)
        print_to_buffer(buf, bufsz, sofar, ", ");
    internal_opnd_disassemble(buf, bufsz, sofar, dcontext, opnd, false);
    return true;
}

const char *
instr_opcode_name_arch(instr_t *instr, const instr_info_t *info)
{
    return NULL;
}

const char *
instr_opcode_name_suffix_arch(instr_t *instr)
{
    return NULL;
}

void
print_instr_prefixes(dcontext_t *dcontext, instr_t *instr,
                     char *buf, size_t bufsz, size_t *sofar INOUT)
{
    return;
}

int
print_opcode_suffix(instr_t *instr, char *buf, size_t bufsz, size_t *sofar INOUT)
{
    /* FIXME i#1551: but for SIMD we want cond before <dt>, but <dt> is inside the name.
     * Should we look for '.'?
     */
    dr_pred_type_t pred = instr_get_predicate(instr);
    size_t pre_sofar = *sofar;
    print_to_buffer(buf, bufsz, sofar, "%s", pred_names[pred]);
    if (instr_get_opcode(instr) == OP_it &&
        opnd_is_immed_int(instr_get_src(instr, 0)) &&
        opnd_is_immed_int(instr_get_src(instr, 1))) {
        it_block_info_t info;
        int i;
        it_block_info_init_immeds(&info, opnd_get_immed_int(instr_get_src(instr, 1)),
                                  opnd_get_immed_int(instr_get_src(instr, 0)));
        for (i = 1/*1st is implied*/; i < info.num_instrs; i++) {
            print_to_buffer(buf, bufsz, sofar, "%c",
                            TEST(BITMAP_MASK(i), info.preds) ? 't' : 'e');

        }
    }
    return *sofar - pre_sofar;
}

#endif /* INTERNAL || CLIENT_INTERFACE */
