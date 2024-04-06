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

/* encode.c -- an encoder for DR_ISA_REGDEPS instructions */

#include "encode.h"

#include "../globals.h"
#include "encode_api.h"
#include "encoding_common.h"
#include "instr_api.h"
#include "opnd_api.h"

/* Encodes DR instruction representation \p instr into raw bytes \p encoded_instr.
 * Returns the next instruction's PC.
 * The encoding scheme followed is described in #core/ir/isa_regdeps/encoding_common.h.
 */
byte *
encode_isa_regdeps(dcontext_t *dcontext, instr_t *instr, byte *encoded_instr)
{
    /* Use a local uint variable for easier setting of category, eflags, #src, and #dst
     * values.
     */
    uint encoding_header = 0;

    /* Encode number of register destination operands (i.e., written registers).
     */
    uint num_dsts = (uint)instr_num_dsts(instr);
    encoding_header |= num_dsts;

    /* Encode number of register source operands (i.e., read registers).
     */
    uint num_srcs = (uint)instr_num_srcs(instr);
    encoding_header |= (num_srcs << SRC_OPND_SHIFT);

    /* Encode arithmetic flags.
     */
    ASSERT(instr_arith_flags_valid(instr));
    uint eflags_instr = instr_get_arith_flags(instr, DR_QUERY_DEFAULT);
    uint eflags = 0;
    if (TESTANY(EFLAGS_WRITE_ARITH, eflags_instr))
        eflags |= REGDEPS_INSTR_WRITES_ARITH;
    if (TESTANY(EFLAGS_READ_ARITH, eflags_instr))
        eflags |= REGDEPS_INSTR_READS_ARITH;
    encoding_header |= (eflags << FLAGS_SHIFT);

    /* Encode instruction category.
     */
    uint category = instr_get_category(instr);
    encoding_header |= (category << CATEGORY_SHIFT);

    /* Copy header encoding back into encoded_instr output.
     */
    *((uint *)encoded_instr) = encoding_header;

    /* Encode register destination operands, if present.
     */
    for (uint dst_index = 0; dst_index < num_dsts; ++dst_index) {
        opnd_t dst_opnd = instr_get_dst(instr, dst_index);
        uint num_regs_used_by_opnd = (uint)opnd_num_regs_used(dst_opnd);
        for (uint opnd_index = 0; opnd_index < num_regs_used_by_opnd; ++opnd_index) {
            reg_id_t reg = opnd_get_reg_used(dst_opnd, opnd_index);
            encoded_instr[dst_index + OPND_INDEX] = (byte)reg;
        }
    }

    /* Encode register source operands, if present.
     */
    for (uint src_index = 0; src_index < num_srcs; ++src_index) {
        opnd_t src_opnd = instr_get_src(instr, src_index);
        uint num_regs_used_by_opnd = (uint)opnd_num_regs_used(src_opnd);
        for (uint opnd_index = 0; opnd_index < num_regs_used_by_opnd; ++opnd_index) {
            reg_id_t reg = opnd_get_reg_used(src_opnd, opnd_index);
            encoded_instr[src_index + OPND_INDEX + num_dsts] = (byte)reg;
        }
    }

    /* Encode largest register size, if there is at least one operand.
     */
    uint num_opnds = num_dsts + num_srcs;
    opnd_size_t max_opnd_size = instr->operation_size;
    if (num_opnds > 0)
        encoded_instr[OP_SIZE_INDEX] = (byte)max_opnd_size;

    /* Compute instruction length including bytes for padding to reach 4 bytes alignment.
     * Account for 1 additional byte containing max register operand size, if there are
     * any operands.
     */
    uint num_opnd_bytes = num_opnds > 0 ? num_opnds + 1 : 0;
    uint instr_length = ALIGN_FORWARD(HEADER_BYTES + num_opnd_bytes, ALIGN_BYTES);

    /* Compute next instruction's PC as: current PC + instruction length.
     */
    return encoded_instr + instr_length;
}
