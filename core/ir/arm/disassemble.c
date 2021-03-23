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

/* disassemble.c -- printing of ARM instructions */

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

static const char *const pred_names[] = {
    "",   /* DR_PRED_NONE */
    "eq", /* DR_PRED_EQ */
    "ne", /* DR_PRED_NE */
    "cs", /* DR_PRED_CS */
    "cc", /* DR_PRED_CC */
    "mi", /* DR_PRED_MI */
    "pl", /* DR_PRED_PL */
    "vs", /* DR_PRED_VS */
    "vc", /* DR_PRED_VC */
    "hi", /* DR_PRED_HI */
    "ls", /* DR_PRED_LS */
    "ge", /* DR_PRED_GE */
    "lt", /* DR_PRED_LT */
    "gt", /* DR_PRED_GT */
    "le", /* DR_PRED_LE */
    "",   /* DR_PRED_AL */
    "",   /* DR_PRED_OP */
};

/* in disassemble_shared.c */
void
internal_opnd_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT,
                          dcontext_t *dcontext, opnd_t opnd, bool use_size_sfx);
void
reg_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT, reg_id_t reg,
                dr_opnd_flags_t flags, const char *prefix, const char *suffix);

const char *
instr_predicate_name(dr_pred_type_t pred)
{
    if (pred > DR_PRED_OP)
        return NULL;
    return pred_names[pred];
}

int
print_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, byte *pc,
                      byte *next_pc, instr_t *instr)
{
    /* Follow conventions used elsewhere with split for T32, solid for the rest */
    if (instr_get_isa_mode(instr) == DR_ISA_ARM_THUMB) {
        /* remove LSB=1 (i#1688) */
        pc = PC_AS_LOAD_TGT(DR_ISA_ARM_THUMB, pc);
        next_pc = PC_AS_LOAD_TGT(DR_ISA_ARM_THUMB, next_pc);
        if (next_pc - pc == 0)
            print_to_buffer(buf, bufsz, sofar, "            ", *((ushort *)pc));
        else if (next_pc - pc == 2)
            print_to_buffer(buf, bufsz, sofar, " %04x       ", *((ushort *)pc));
        else {
            CLIENT_ASSERT(next_pc - pc == 4, "invalid thumb size");
            print_to_buffer(buf, bufsz, sofar, " %04x %04x  ", *((ushort *)pc),
                            *(((ushort *)pc) + 1));
        }
    } else {
        print_to_buffer(buf, bufsz, sofar, " %08x   ", *((uint *)pc));
    }
    return 0; /* no extra size */
}

void
print_extra_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, byte *pc,
                            byte *next_pc, int extra_sz, const char *extra_bytes_prefix)
{
    /* There are no "extra" bytes */
}

static bool
instr_is_non_list_store(instr_t *instr, int *num_tostore OUT)
{
    int opcode = instr_get_opcode(instr);
    switch (opcode) {
    case OP_str:
    case OP_strb:
    case OP_strbt:
    case OP_strex:
    case OP_strexb:
    case OP_strexh:
    case OP_strh:
    case OP_strht:
    case OP_strt:
    case OP_stc:
    case OP_stc2:
    case OP_stc2l:
    case OP_stcl:
    case OP_stl:
    case OP_stlb:
    case OP_stlex:
    case OP_stlexb:
    case OP_stlexd:
    case OP_stlexh:
    case OP_stlh:
        if (num_tostore != NULL)
            *num_tostore = 1;
        return true;
    case OP_strd:
    case OP_strexd:
        if (num_tostore != NULL)
            *num_tostore = 2;
        return true;
    }
    return false;
}

static bool
instr_is_non_list_load(instr_t *instr, int *num_toload OUT)
{
    int opcode = instr_get_opcode(instr);
    switch (opcode) {
    case OP_ldr:
    case OP_ldrb:
    case OP_ldrbt:
    case OP_ldrex:
    case OP_ldrexb:
    case OP_ldrexh:
    case OP_ldrh:
    case OP_ldrht:
    case OP_ldrt:
    case OP_ldrsb:
    case OP_ldrsbt:
    case OP_ldrsh:
    case OP_ldrsht:
    case OP_lda:
    case OP_ldab:
    case OP_ldaex:
    case OP_ldaexb:
    case OP_ldaexd:
    case OP_ldaexh:
    case OP_ldah:
    case OP_ldc:
    case OP_ldc2:
    case OP_ldc2l:
    case OP_ldcl:
        if (num_toload != NULL)
            *num_toload = 1;
        return true;
    case OP_ldrd:
    case OP_ldrexd:
        if (num_toload != NULL)
            *num_toload = 2;
        return true;
    }
    return false;
}

static bool
instr_is_priv_reglist(instr_t *instr)
{
    int opcode = instr_get_opcode(instr);
    switch (opcode) {
    case OP_ldm_priv:
    case OP_ldmda_priv:
    case OP_ldmdb_priv:
    case OP_ldmib_priv:
    case OP_stm_priv:
    case OP_stmda_priv:
    case OP_stmdb_priv:
    case OP_stmib_priv: return true;
    }
    return false;
}

static void
disassemble_shift(char *buf, size_t bufsz, size_t *sofar INOUT, const char *prefix,
                  const char *suffix, dr_shift_type_t shift, bool print_amount,
                  uint amount)
{
    switch (shift) {
    case DR_SHIFT_NONE: break;
    case DR_SHIFT_RRX:
        print_to_buffer(buf, bufsz, sofar, "%srrx", prefix);
        if (print_amount && !DYNAMO_OPTION(syntax_arm))
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    case DR_SHIFT_LSL:
        /* XXX i#1551: use #%d for ARM style */
        print_to_buffer(buf, bufsz, sofar, "%slsl", prefix);
        if (print_amount)
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    case DR_SHIFT_LSR:
        print_to_buffer(buf, bufsz, sofar, "%slsr", prefix);
        if (print_amount)
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    case DR_SHIFT_ASR:
        print_to_buffer(buf, bufsz, sofar, "%sasr", prefix);
        if (print_amount)
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    case DR_SHIFT_ROR:
        print_to_buffer(buf, bufsz, sofar, "%sror", prefix);
        if (print_amount)
            print_to_buffer(buf, bufsz, sofar, " %d", amount);
        break;
    default: print_to_buffer(buf, bufsz, sofar, "%s<UNKNOWN SHIFT>", prefix); break;
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

bool
opnd_disassemble_arch(char *buf, size_t bufsz, size_t *sofar INOUT, opnd_t opnd)
{
    if (opnd_is_immed_int(opnd) && TEST(DR_OPND_IS_SHIFT, opnd_get_flags(opnd))) {
        dr_shift_type_t shift = (dr_shift_type_t)opnd_get_immed_int(opnd);
        disassemble_shift(buf, bufsz, sofar, "", "", shift, false, 0);
        return true;
    }
    return false;
}

bool
opnd_disassemble_noimplicit(char *buf, size_t bufsz, size_t *sofar INOUT,
                            dcontext_t *dcontext, instr_t *instr, byte optype,
                            opnd_t opnd, bool prev, bool multiple_encodings, bool dst,
                            int *idx INOUT)
{
    /* FIXME i#1683: we need to avoid the implicit dst-as-src regs for instrs
     * such as OP_smlal.
     */
    /* XXX i#1683: we're relying on flags added by the decoder and by the
     * INSTR_CREATE_ macros: DR_OPND_IS_SHIFT, DR_OPND_IN_LIST.
     * For arbitrary level 4 instrs, we should do an encode and have our encoder
     * set these flags too.
     */
    /* XXX: better to have a format string per instr template than to do all
     * this computation for each operand disasm?  Though then we'd need to do
     * a full encode, or store a ptr in instr_t to the corresponding template.
     */

    /* XXX: better to compute these per-instr and cache instead of per-opnd */
    bool reads_list = instr_reads_reg_list(instr);
    bool writes_list = instr_writes_reg_list(instr);
    int tostore = 0;
    bool nonlist_store = instr_is_non_list_store(instr, &tostore);
    bool nonlist_load = instr_is_non_list_load(instr, &tostore);
    int max = dst ? instr_num_dsts(instr) : instr_num_srcs(instr);

    /* Writeback implicit operands for register list instrs */
    if (*idx == max - 1 /*always last*/ && opnd_is_reg(opnd) &&
        (reads_list || writes_list)) {
        opnd_t memop = writes_list ? instr_get_src(instr, 0) : instr_get_dst(instr, 0);
        CLIENT_ASSERT(opnd_is_base_disp(memop), "internal disasm error");
        if (opnd_get_reg(opnd) == opnd_get_base(memop) &&
            !TEST(DR_OPND_IN_LIST, opnd_get_flags(opnd)))
            return false; /* skip */
    }
    /* Writeback implicit operands for non-list instrs */
    if ((nonlist_store || nonlist_load) &&
        /* Base reg is always last dst, and implicit srcs are after main srcs */
        ((dst && *idx == max - 1) || (!dst && *idx >= tostore))) {
        opnd_t memop = nonlist_store ? instr_get_dst(instr, 0) : instr_get_src(instr, 0);
        CLIENT_ASSERT(opnd_is_base_disp(memop), "internal disasm error");
        /* We want to hide:
         *   1) Base reg as dst
         *   2) Base reg as src
         *   3) Disp as src, if present in memop
         *   4) Index reg as src, if present in memop
         *   5) Index shift type + amount, if present in memop
         * In order to distinguish base from index we rely on the table entries
         * always placing the writeback base last.
         */
        if (*idx == max - 1) {
            if (opnd_is_reg(opnd) && opnd_get_reg(opnd) == opnd_get_base(memop))
                return false; /* skip */
        } else if (!dst) {
            dr_shift_type_t type;
            uint amount;
            if (opnd_is_reg(opnd) && opnd_get_reg(opnd) == opnd_get_index(memop))
                return false; /* skip */
            type = opnd_get_index_shift(memop, &amount);
            if (opnd_is_immed_int(opnd) &&
                ((!TEST(DR_OPND_SHIFTED, opnd_get_flags(memop)) &&
                  /* rule out disp==0 hiding shift type */
                  max - tostore < 3 &&
                  opnd_get_immed_int(opnd) == opnd_get_disp(memop)) ||
                 (TEST(DR_OPND_SHIFTED, opnd_get_flags(memop)) &&
                  (opnd_get_immed_int(opnd) == type ||
                   opnd_get_immed_int(opnd) == amount))))
                return false; /* skip */
        }
    }

    /* Base reg for register list printed first w/o decoration */
    if (*idx == 0 && dst && (reads_list || writes_list)) {
        opnd_t memop, last;
        bool writeback;
        if (reads_list)
            memop = opnd;
        else
            memop = instr_get_src(instr, 0);
        CLIENT_ASSERT(opnd_is_base_disp(memop), "internal disasm error");
        last = instr_get_dst(instr, instr_num_dsts(instr) - 1);
        writeback = (opnd_is_reg(last) && opnd_get_reg(last) == opnd_get_base(memop) &&
                     !TEST(DR_OPND_IN_LIST, opnd_get_flags(last)));
        reg_disassemble(buf, bufsz, sofar, opnd_get_base(memop), 0, "",
                        writes_list ? (writeback ? "!, " : ", ")
                                    : (writeback ? "!" : ""));
        if (reads_list)
            return true;
    }
    if (writes_list && opnd_is_base_disp(opnd))
        return false; /* already printed */

    /* Store to memory operand ordering: skip store in dsts */
    if (instr_is_non_list_store(instr, NULL) && dst && opnd_is_base_disp(opnd))
        return false; /* skip */

    /* Now that we have the implicit opnds to skip out of the way, print ", " connector */
    if (prev) {
        bool printed = false;
        if (*idx > 0) {
            opnd_t prior =
                dst ? instr_get_dst(instr, *idx - 1) : instr_get_src(instr, *idx - 1);
            if (opnd_is_immed_int(prior) &&
                TEST(DR_OPND_IS_SHIFT, opnd_get_flags(prior))) {
                if (opnd_get_immed_int(prior) == DR_SHIFT_RRX)
                    return true; /* do not print value, which is always 1 */
                /* No comma in between */
                print_to_buffer(buf, bufsz, sofar, " ");
                printed = true;
            }
        }
        if (!printed)
            print_to_buffer(buf, bufsz, sofar, ", ");
    }

    /* Register lists */
    if (opnd_is_reg(opnd) && TEST(DR_OPND_IN_LIST, opnd_get_flags(opnd))) {
        /* For now we do not print ranges as "r0-r4" but print each reg.
         * This matches some other decoders but not all.
         */
        opnd_t adj = opnd_create_null();
        if (*idx > 0)
            adj = dst ? instr_get_dst(instr, *idx - 1) : instr_get_src(instr, *idx - 1);
        if (!opnd_is_reg(adj) || !TEST(DR_OPND_IN_LIST, opnd_get_flags(adj)))
            print_to_buffer(buf, bufsz, sofar, "{");
        internal_opnd_disassemble(buf, bufsz, sofar, dcontext, opnd, false);
        adj = opnd_create_null();
        if (*idx + 1 < max)
            adj = dst ? instr_get_dst(instr, *idx + 1) : instr_get_src(instr, *idx + 1);
        if (!opnd_is_reg(adj) || !TEST(DR_OPND_IN_LIST, opnd_get_flags(adj))) {
            print_to_buffer(buf, bufsz, sofar, "}%s",
                            instr_is_priv_reglist(instr) ? "^" : "");
        }
        return true;
    }

    internal_opnd_disassemble(buf, bufsz, sofar, dcontext, opnd, false);

    /* Store to memory operand ordering: insert store in srcs */
    if (nonlist_store && !dst && *idx + 1 == tostore) {
        opnd_t memop = instr_get_dst(instr, 0);
        CLIENT_ASSERT(opnd_is_base_disp(memop), "internal disasm error");
        print_to_buffer(buf, bufsz, sofar, ", ");
        internal_opnd_disassemble(buf, bufsz, sofar, dcontext, memop, false);
        /* FIXME i#1683: we need to print "!" but how do we tell whether
         * the memop has a disp?
         */
    }
    /* Writeback "!" */
    if (nonlist_load && opnd_is_base_disp(opnd)) {
        /* FIXME i#1683: we need to print "!" but how do we tell whether
         * the memop has a disp?
         */
    }

    return true;
}

void
print_instr_prefixes(dcontext_t *dcontext, instr_t *instr, char *buf, size_t bufsz,
                     size_t *sofar INOUT)
{
    return;
}

static bool
instr_has_built_in_pred_name(instr_t *instr)
{
    int opcode = instr_get_opcode(instr);
    switch (opcode) {
    case OP_vsel_eq_f32:
    case OP_vsel_eq_f64:
    case OP_vsel_ge_f32:
    case OP_vsel_ge_f64:
    case OP_vsel_gt_f32:
    case OP_vsel_gt_f64:
    case OP_vsel_vs_f32:
    case OP_vsel_vs_f64: return true;
    }
    return false;
}

void
print_opcode_name(instr_t *instr, const char *name, char *buf, size_t bufsz,
                  size_t *sofar INOUT)
{
    if (instr_get_opcode(instr) == OP_it && opnd_is_immed_int(instr_get_src(instr, 0)) &&
        opnd_is_immed_int(instr_get_src(instr, 1))) {
        it_block_info_t info;
        int i;
        print_to_buffer(buf, bufsz, sofar, "%s", name);
        it_block_info_init_immeds(&info, opnd_get_immed_int(instr_get_src(instr, 1)),
                                  opnd_get_immed_int(instr_get_src(instr, 0)));
        for (i = 1 /*1st is implied*/; i < info.num_instrs; i++) {
            print_to_buffer(buf, bufsz, sofar, "%c",
                            TEST(BITMAP_MASK(i), info.preds) ? 't' : 'e');
        }
    } else if (instr_is_predicated(instr)) {
        dr_pred_type_t pred = instr_get_predicate(instr);
        /* The predicate goes prior to the size specifiers:
         * "vcvtble.f64.f16", not "vcvtb.f64.f16le".
         */
        const char *dot = strchr(name, '.');
        if (dot != NULL)
            print_to_buffer(buf, bufsz, sofar, "%.*s", dot - name, name);
        else
            print_to_buffer(buf, bufsz, sofar, "%s", name);
        print_to_buffer(
            buf, bufsz, sofar, "%s%s",
            /* The . really distinguishes from the opcode for DR style */
            DYNAMO_OPTION(syntax_arm) ? "" : (pred_names[pred][0] != '\0' ? "." : ""),
            pred_names[pred]);
        if (dot != NULL)
            print_to_buffer(buf, bufsz, sofar, "%s", dot);
    } else if (DYNAMO_OPTION(syntax_arm) && instr_has_built_in_pred_name(instr)) {
        /* Remove the dot */
        const char *dot = strchr(name, '.');
        CLIENT_ASSERT(dot != NULL, "disasm internal error");
        print_to_buffer(buf, bufsz, sofar, "%.*s", dot - name, name);
        print_to_buffer(buf, bufsz, sofar, "%s", dot + 1);
    } else
        print_to_buffer(buf, bufsz, sofar, "%s", name);
}
