/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

/* decode.c -- a decoder for DR synthetic IR */

#include "decode.h"

#include "../globals.h"
#include "encoding_common.h"
#include "instr_api.h"
#include "opnd_api.h"

/* Decodes the raw bytes of an encoded instruction \p encoded_instr into DR instruction
 * representation \p instr.
 * Returns the next instruction's PC.
 * The encoding scheme followed is described in core/ir/synthetic/encoding_common.h.
 */
byte *
decode_from_synth(dcontext_t *dcontext, byte *encoded_instr, instr_t *instr)
{
    /* Interpret the first 4 bytes of encoded_instr (which are always present) as a uint
     * for easier retrieving of category, eflags, #src, and #dst values.
     * We can do this safely because encoded_instr is 4 bytes aligned.
     */
    uint *encoding = ((uint *)&encoded_instr[0]);

    /* Decode number of destination operands.
     */
    uint num_dsts = *encoding & DST_OPND_MASK;

    /* Decode number of source operands.
     */
    uint num_srcs = (*encoding & SRC_OPND_MASK) >> SRC_OPND_SHIFT;

    instr_set_num_opnds(dcontext, instr, num_dsts, num_srcs);

    /* Decode flags.
     */
    uint eflags = (*encoding & FLAGS_MASK) >> FLAGS_SHIFT;
    uint eflags_instr = 0;
    if (eflags & SYNTHETIC_INSTR_WRITES_ARITH)
        eflags_instr |= EFLAGS_WRITE_ARITH;
    if (eflags & SYNTHETIC_INSTR_READS_ARITH)
        eflags_instr |= EFLAGS_READ_ARITH;
    instr->eflags = eflags_instr;

    /* Decode synthetic opcode as instruction category.
     */
    uint category = (*encoding & CATEGORY_MASK) >> CATEGORY_SHIFT;
    instr_set_category(instr, category);

    /* Decode register destination operands, if present.
     */
    for (uint i = 0; i < num_dsts; ++i) {
        reg_id_t dst = (reg_id_t)encoded_instr[i + INSTRUCTION_BYTES];
        // TODO i#6662: need to add virtual registers.
        // Right now using regular reg_id_t (which holds DR_REG_ values) from opnd_api.h.
        opnd_t dst_opnd = opnd_create_reg(dst);
        instr_set_dst(instr, i, dst_opnd);
    }

    /* Decode register source operands, if present.
     */
    for (uint i = 0; i < num_srcs; ++i) {
        reg_id_t src = (reg_id_t)encoded_instr[i + INSTRUCTION_BYTES + num_dsts];
        // TODO i#6662: need to add virtual registers.
        // Right now using regular reg_id_t (which holds DR_REG_ values) from opnd_api.h.
        opnd_t src_opnd = opnd_create_reg(src);
        instr_set_src(instr, i, src_opnd);
    }

    /* Compute instruction length including bytes for padding to reach 4 bytes alignment.
     */
    uint num_opnds = num_srcs + num_dsts;
    uint num_opnds_ceil = (num_opnds + INSTRUCTION_BYTES - 1) / INSTRUCTION_BYTES;
    uint instr_length = INSTRUCTION_BYTES + num_opnds_ceil * INSTRUCTION_BYTES;
    instr->length = instr_length;

    /* At this point the synthetic instruction has been fully decoded, so we set the
     * decoded flags bit.
     * This is useful to avoid trying to compute its length again when we want to
     * retrieve it using instr_length().
     */
    instr->flags |= INSTR_RAW_BITS_VALID;

    /* Compute next instruction's PC as: current PC + instruction length.
     */
    byte *next_pc = encoded_instr + instr_length;

    return next_pc;
}
