/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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
#include "instr.h"
#include "decode.h"
#include "decode_private.h"
#include <string.h> /* for memcpy */

/* ARM decoder.
 * General strategy:
 * + We use a data-driven table-based approach, as we need to both encode and
 *   decode and a central source of data lets us move in both directions.
 */
/* FIXME i#1551: add Thumb support: for now just A32 */
/* FIXME i#1551: add A64 support: for now just A32 */

bool
is_isa_mode_legal(dr_isa_mode_t mode)
{
#ifdef X64
    return (mode == DR_ISA_ARM_A64);
#else
    return (mode == DR_ISA_ARM_THUMB || DR_ISA_ARM_A32);
#endif
}

/* We assume little-endian */
static inline int
decode_predicate(uint instr_word)
{
    return instr_word >> 28; /* bits 31:28 */
}

/* We often take bits 27:20 as an 8-bit opcode */
static inline int
decode_opc8(uint instr_word)
{
    return (instr_word >> 20) & 0xff;
}

/* We often take bits 7:4 as a 4-bit auxiliary opcode */
static inline int
decode_opc4(uint instr_word)
{
    return (instr_word >> 4) & 0xf;
}

static bool
decode_operand(decode_info_t *di, byte optype, opnd_size_t opsize, opnd_t *opnd)
{
    switch (optype) {
    case TYPE_NONE:
        *opnd = opnd_create_null();
        return true;
    /* FIXME i#1551: add decoding of each operand type */
    default:
        /* ok to assert, types coming only from instr_info_t */
        CLIENT_ASSERT(false, "decode error: unknown operand type");
    }
    return false;
}

/* Disassembles the instruction at pc into the data structures ret_info
 * and di.  Returns a pointer to the pc of the next instruction.
 * Returns NULL on an invalid instruction.
 * Caller should set di->isa_mode.
 */
static byte *
read_instruction(byte *pc, byte *orig_pc,
                 const instr_info_t **ret_info, decode_info_t *di
                 _IF_DEBUG(bool report_invalid))
{
    uint instr_word;
    const instr_info_t *info;
    uint idx;

    /* Read instr bytes and initialize di */
    di->start_pc = pc;
    di->orig_pc = orig_pc;
    instr_word = *(uint *)pc;
    pc += sizeof(instr_word);
    di->instr_word = instr_word;

    di->predicate = decode_predicate(instr_word);
    if (di->predicate == DR_PRED_OP) {
        uint opc8 = decode_opc8(instr_word);
        info = &A32_nopred_opc8[opc8];
    } else {
        uint opc8 = decode_opc8(instr_word);
        info = &A32_pred_opc8[opc8];
    }

    /* If an extension, discard the old info and get a new one */
    if (info->type == EXT_OPC4X) {
        if ((instr_word & 0x10 /*bit 4*/) == 0)
            idx = 0;
        else if ((instr_word & 0x80 /*bit 7*/) == 0)
            idx = 1;
        else
            idx = 2 + ((instr_word >> 5) & 0x3) /*bits 6:5*/;
        info = &A32_ext_opc4x[info->code][idx];
    } else if (info->type == EXT_OPC4Y) {
        if ((instr_word & 0x10 /*bit 4*/) == 0)
            idx = 0;
        else
            idx = 1 + ((instr_word >> 5) & 0x7) /*bits 7:5*/;
        info = &A32_ext_opc4y[info->code][idx];
    } else if (info->type == EXT_OPC4) {
        idx = decode_opc4(instr_word);
        info = &A32_ext_opc4[info->code][idx];
    } else if (info->type == EXT_IMM1916) {
        idx = (((instr_word >> 16) & 0xf) /*bits 19:16*/ == 0) ? 0 : 1;
        info = &A32_ext_imm1916[info->code][idx];
    } else if (info->type == EXT_BIT4) {
        idx = (instr_word >> 4) & 0x1 /*bit 4*/;
        info = &A32_ext_bit4[info->code][idx];
    }

    /* Secondary extensions */
    if (info->type == EXT_BIT9) {
        idx = (instr_word >> 9) & 0x1 /*bit 9*/;
        info = &A32_ext_bit9[info->code][idx];
    } else if (info->type == EXT_BITS8) {
        idx = (instr_word >> 8) & 0x3 /*bits 9:8*/;
        info = &A32_ext_bits8[info->code][idx];
    } else if (info->type == EXT_BITS0) {
        idx = instr_word & 0x7 /*bits 2:0*/;
        info = &A32_ext_bits0[info->code][idx];
    } else if (info->type == EXT_IMM5) {
        idx = (((instr_word >> 7) & 0x1f) /*bits 11:7*/ == 0) ? 0 : 1;
        info = &A32_ext_imm5[info->code][idx];
    } else if (info->type == EXT_RDPC) {
        idx = ((instr_word & 0xf) /*bits 3:0*/ == 0xf) ? 1 : 0;
        info = &A32_ext_RDPC[info->code][idx];
    } else if (info->type == EXT_OPC4) {
        idx = decode_opc4(instr_word);
        info = &A32_ext_opc4[info->code][idx];
    }

    /* We should now have either a valid OP_ opcode or an invalid opcode */
    if (info == NULL || info == &invalid_instr ||
        info->type < OP_FIRST || info->type > OP_LAST) {
        DODEBUG({
            /* PR 605161: don't report on DR addresses */
            if (report_invalid && !is_dynamo_address(di->start_pc)) {
                SYSLOG_INTERNAL_WARNING_ONCE("Invalid opcode encountered");
                LOG(THREAD_GET, LOG_ALL, 1, "Invalid opcode @"PFX": 0x%016x\n",
                    di->start_pc, instr_word);
            }
        });
        *ret_info = &invalid_instr;
        return NULL;
    }

    /* Unlike x86, we have a fixed size, so we're done */
    *ret_info = info;
    return pc;
}

byte *
decode_eflags_usage(dcontext_t *dcontext, byte *pc, uint *usage,
                    dr_opnd_query_flags_t flags)
{
    const instr_info_t *info;
    decode_info_t di;
    di.isa_mode = dr_get_isa_mode(dcontext);
    pc = read_instruction(pc, pc, &info, &di _IF_DEBUG(true));
    *usage = instr_eflags_conditionally(info->eflags, di.predicate, flags);
    /* we're fine returning NULL on failure */
    return pc;
}

byte *
decode_opcode(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    const instr_info_t *info;
    decode_info_t di;
    di.isa_mode = dr_get_isa_mode(dcontext);
    pc = read_instruction(pc, pc, &info, &di _IF_DEBUG(true));
    instr_set_isa_mode(instr, di.isa_mode);
    instr_set_opcode(instr, info->type);
    if (!instr_valid(instr)) {
        CLIENT_ASSERT(!instr_valid(instr), "decode_opcode: invalid instr");
        return NULL;
    }
    instr->eflags = info->eflags;
    instr_set_eflags_valid(instr, true);
    instr_set_operands_valid(instr, false);
    instr_set_raw_bits(instr, pc, pc - di.orig_pc);
    return pc;
}

/* XXX: some of this code could be shared with x86/decode.c */
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

    di.isa_mode = dr_get_isa_mode(dcontext);
    next_pc = read_instruction(pc, orig_pc, &info, &di
                               _IF_DEBUG(!TEST(INSTR_IGNORE_INVALID, instr->flags)));
    instr_set_isa_mode(instr, di.isa_mode);
    instr_set_opcode(instr, info->type);
    /* failure up to this point handled fine -- we set opcode to OP_INVALID */
    if (next_pc == NULL) {
        LOG(THREAD, LOG_INTERP, 3, "decode: invalid instr at "PFX"\n", pc);
        CLIENT_ASSERT(!instr_valid(instr), "decode: invalid instr");
        return NULL;
    }
    instr->eflags = info->eflags;
    instr_set_eflags_valid(instr, true);
    /* since we don't use set_src/set_dst we must explicitly say they're valid */
    instr_set_operands_valid(instr, true);

    if (di.predicate != DR_PRED_OP)
        instr_set_predicate(instr, di.predicate + DR_PRED_EQ);

    /* operands */
    do {
        if (info->dst1_type != TYPE_NONE) {
            if (!decode_operand(&di, info->dst1_type, info->dst1_size,
                                &(dsts[instr_num_dsts++])))
                goto decode_invalid;
        }
        if (info->dst2_type != TYPE_NONE) {
            if (!decode_operand(&di, info->dst2_type, info->dst2_size,
                                TEST(DECODE_4_SRCS, info->flags) ?
                                &(srcs[instr_num_srcs++]) :
                                &(dsts[instr_num_dsts++])))
                goto decode_invalid;
        }
        if (info->src1_type != TYPE_NONE) {
            if (!decode_operand(&di, info->src1_type, info->src1_size,
                                TEST(DECODE_3_DSTS, info->flags) ?
                                &(dsts[instr_num_dsts++]) :
                                &(srcs[instr_num_srcs++])))
                goto decode_invalid;
        }
        if (info->src2_type != TYPE_NONE) {
            if (!decode_operand(&di, info->src2_type, info->src2_size,
                                &(srcs[instr_num_srcs++])))
                goto decode_invalid;
        }
        if (info->src3_type != TYPE_NONE) {
            if (!decode_operand(&di, info->src3_type, info->src3_size,
                                &(srcs[instr_num_srcs++])))
                goto decode_invalid;
        }
        info = instr_info_extra_opnds(info);
    } while (info != NULL);

    /* now copy operands into their real slots */
    instr_set_num_opnds(dcontext, instr, instr_num_dsts, instr_num_srcs);
    if (instr_num_dsts > 0) {
        memcpy(instr->dsts, dsts, instr_num_dsts*sizeof(opnd_t));
    }
    if (instr_num_srcs > 0) {
        instr->src0 = srcs[0];
        if (instr_num_srcs > 1) {
            memcpy(instr->srcs, &(srcs[1]), (instr_num_srcs-1)*sizeof(opnd_t));
        }
    }

    if (orig_pc != pc) {
        /* We do not want to copy when encoding and condone an invalid
         * relative target
         */
        instr_set_raw_bits_valid(instr, false);
        instr_set_translation(instr, orig_pc);
    } else {
        /* we set raw bits AFTER setting all srcs and dsts b/c setting
         * a src or dst marks instr as having invalid raw bits
         */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(next_pc - pc)));
        instr_set_raw_bits(instr, pc, (uint)(next_pc - pc));
    }

    return next_pc;

 decode_invalid:
    instr_set_operands_valid(instr, false);
    instr_set_opcode(instr, OP_INVALID);
    return NULL;
}

byte *
decode(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    return decode_common(dcontext, pc, pc, instr);
}

byte *
decode_from_copy(dcontext_t *dcontext, byte *copy_pc, byte *orig_pc, instr_t *instr)
{
    return decode_common(dcontext, copy_pc, orig_pc, instr);
}

byte *
decode_cti(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    /* XXX i#1551: build a fast decoder for branches -- though it may not make
     * sense for 32-bit where many instrs can write to the pc.
     */
    return decode(dcontext, pc, instr);
}

byte *
decode_next_pc(dcontext_t *dcontext, byte *pc)
{
    /* FIXME i#1551: check for invalid opcodes */
    /* FIXME i#1551: add Thumb support */
    return pc + 4;
}

int
decode_sizeof(dcontext_t *dcontext, byte *pc, int *num_prefixes
              _IF_X64(uint *rip_rel_pos))
{
    /* FIXME i#1551: check for invalid opcodes */
    /* FIXME i#1551: add Thumb support */
    return 4;
}

/* XXX: share this with x86 */
byte *
decode_raw(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    /* XXX i#1551: set isa_mode of instr once we add that feature */
    int sz = decode_sizeof(dcontext, pc, NULL _IF_X64(NULL));
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

const instr_info_t *
instr_info_extra_opnds(const instr_info_t *info)
{
    /* XXX i#1551: pick proper *_extra_operands table */
    if (TEST(DECODE_EXTRA_SHIFT, info->flags))
        return &A32_extra_operands[0];
    else if (TEST(DECODE_EXTRA_WRITEBACK, info->flags))
        return &A32_extra_operands[1];
    else if (TEST(DECODE_EXTRA_OPERANDS, info->flags))
        return (const instr_info_t *)(info->code);
    else
        return NULL;
}

/* num is 0-based */
byte
instr_info_opnd_type(const instr_info_t *info, bool src, int num)
{
    int i = 0;
    while (info != NULL) {
        if (!src && i++ == num)
            return info->dst1_type;
        if (TEST(DECODE_4_SRCS, info->flags)) {
            if (src && i++ == num)
                return info->dst2_type;
        } else {
            if (!src && i++ == num)
                return info->dst2_type;
        }
        if (TEST(DECODE_3_DSTS, info->flags)) {
            if (!src && i++ == num)
                return info->src1_type;
        } else {
            if (src && i++ == num)
                return info->src1_type;
        }
        if (src && i++ == num)
            return info->src2_type;
        if (src && i++ == num)
            return info->src3_type;
        info = instr_info_extra_opnds(info);
    }
    CLIENT_ASSERT(false, "internal decode error");
    return TYPE_NONE;
}

const instr_info_t *
get_next_instr_info(const instr_info_t * info)
{
    return (const instr_info_t *)(info->code);
}

byte
decode_first_opcode_byte(int opcode)
{
    CLIENT_ASSERT(false, "should not be used on ARM");
    return 0;
}

const instr_info_t *
opcode_to_encoding_info(uint opc, dr_isa_mode_t isa_mode)
{
    if (isa_mode == DR_ISA_ARM_A32)
        return op_instr_A32[opc];
    CLIENT_ASSERT(false, "NYI i#1551");
    return &invalid_instr;
}

DR_API
const char *
decode_opcode_name(int opcode)
{
    const instr_info_t * info =
        opcode_to_encoding_info(opcode, dr_get_isa_mode(get_thread_private_dcontext()));
    if (info != NULL)
        return info->name;
    else
        return "<unknown>";
}

opnd_size_t
resolve_variable_size(decode_info_t *di, opnd_size_t sz, bool is_reg)
{
    return sz;
}

bool
optype_is_indir_reg(int optype)
{
    return false;
}


#ifdef DECODE_UNIT_TEST
/* FIXME i#1551: add unit tests here.  How divide vs suite/tests/api/ tests? */
# include "instr_create.h"

int main()
{
    bool res = true;
    standalone_init();
    return res;
}

#endif /* DECODE_UNIT_TEST */
