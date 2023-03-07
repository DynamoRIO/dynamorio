/* **********************************************************
 * Copyright (c) 2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "disassemble.h"

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
    "al", /* DR_PRED_AL */
    "nv", /* DR_PRED_NV */
};

int
print_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, byte *pc,
                      byte *next_pc, instr_t *instr)
{
    print_to_buffer(buf, bufsz, sofar, " %08x   ", *(uint *)pc);
    return 0;
}

void
print_extra_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, byte *pc,
                            byte *next_pc, int extra_sz, const char *extra_bytes_prefix)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

static const char *
shift_name(dr_shift_type_t shift)
{
    static const char *const names[] = { "lsl", "lsr", "asr", "ror", "mul" };
    int i = shift;
    return (0 <= i && i < sizeof(names) / sizeof(*names) ? names[i] : "<UNKNOWN SHIFT>");
}

static const char *
extend_name(dr_extend_type_t extend)
{
    static const char *const names[] = { "uxtb", "uxth", "uxtw", "uxtx",
                                         "sxtb", "sxth", "sxtw", "sxtx" };
    int i = extend;
    return (0 <= i && i < sizeof(names) / sizeof(*names) ? names[i]
                                                         : "<UNKNOWN EXTENSION>");
}

void
opnd_base_disp_scale_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT,
                                 opnd_t opnd)
{
    bool scaled;
    uint amount;
    dr_extend_type_t extend = opnd_get_index_extend(opnd, &scaled, &amount);
    const char *name = extend_name(extend);
    if (scaled) {
        if (extend == DR_EXTEND_UXTX)
            name = shift_name(DR_SHIFT_LSL);

        print_to_buffer(buf, bufsz, sofar, ",%s #%d", name, amount);
    } else if (extend != DR_EXTEND_UXTX)
        print_to_buffer(buf, bufsz, sofar, ",%s", name);
}

bool
opnd_disassemble_arch(char *buf, size_t bufsz, size_t *sofar INOUT, opnd_t opnd)
{
    if (opnd_is_immed_int(opnd) && TEST(DR_OPND_IS_SHIFT, opnd_get_flags(opnd))) {
        dr_shift_type_t t = (dr_shift_type_t)opnd_get_immed_int(opnd);
        print_to_buffer(buf, bufsz, sofar, "%s", shift_name(t));
        return true;
    }
    if (opnd_is_immed_int(opnd) && TEST(DR_OPND_IS_EXTEND, opnd_get_flags(opnd))) {
        dr_extend_type_t t = (dr_extend_type_t)opnd_get_immed_int(opnd);
        print_to_buffer(buf, bufsz, sofar, "%s", extend_name(t));
        return true;
    }
    if (opnd_is_immed_int(opnd) && TEST(DR_OPND_IS_CONDITION, opnd_get_flags(opnd))) {
        dr_pred_type_t pred = DR_PRED_EQ + opnd_get_immed_int(opnd);
        print_to_buffer(buf, bufsz, sofar, "%s", pred_names[pred]);
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
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

void
print_instr_prefixes(dcontext_t *dcontext, instr_t *instr, char *buf, size_t bufsz,
                     size_t *sofar INOUT)
{
}

void
print_opcode_name(instr_t *instr, const char *name, char *buf, size_t bufsz,
                  size_t *sofar INOUT)
{
    if (instr_get_predicate(instr) != DR_PRED_NONE) {
        if (instr_get_opcode(instr) == OP_bcond) {
            print_to_buffer(buf, bufsz, sofar, "b.%s",
                            pred_names[instr_get_predicate(instr)]);
        } else {
            print_to_buffer(buf, bufsz, sofar, "%s.%s", name,
                            pred_names[instr_get_predicate(instr)]);
        }
    } else
        print_to_buffer(buf, bufsz, sofar, "%s", name);
}
