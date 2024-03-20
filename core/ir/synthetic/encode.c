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

/* encode.c -- an encoder for DR synthetic IR */

#include "encode.h"

#include "../globals.h"
#include "encoding_common.h"
#include "instr_api.h"
#include "opnd_api.h"

/* Encodes DR instruction representation \p instr into raw bytes \p encoded_instr.
 * Returns next instruction's pc.
 * The encoding scheme followed is described in core/ir/synthetic/encoding_common.h.
 */
byte *
encode_to_synth(dcontext_t *dcontext, instr_t *instr, byte *encoded_instr)
{
    /* Interpret the first 4 bytes of encoded_instr (which are always present) as a uint
     * for easier retrieving of category, eflags, #src, and #dst values.
     * We can do this safely because encoded_instr is 4 bytes aligned.
     */
    uint *encoding = ((uint *)&encoded_instr[0]);

    /* Encode number of destination operands.
     * Note that a destination operand that is a memory renference, should have its
     * registers (if any) counted as source operands, since they are being read.
     * We use used_[src|dst]_reg_map to keep track of registers we've seen and avoid
     * duplicates.
     */
    byte used_src_reg_map[MAX_NUM_REGS];
    memset(used_src_reg_map, 0, sizeof(used_src_reg_map));
    uint num_srcs = 0;
    byte used_dst_reg_map[MAX_NUM_REGS];
    memset(used_dst_reg_map, 0, sizeof(used_dst_reg_map));
    uint num_dsts = 0;
    uint original_num_dsts = (uint)instr_num_dsts(instr);
    for (uint dst_index = 0; dst_index < original_num_dsts; ++dst_index) {
        opnd_t dst_opnd = instr_get_dst(instr, dst_index);
        if (opnd_is_memory_reference(dst_opnd)) {
            for (uint opnd_index = 0; opnd_index < opnd_num_regs_used(dst_opnd);
                 ++opnd_index) {
                reg_id_t reg = opnd_get_reg_used(dst_opnd, opnd_index);
                if (used_src_reg_map[reg] == 0) {
                    ++num_srcs;
                    used_src_reg_map[reg] = 1;
                }
            }
        } else {
            for (uint opnd_index = 0; opnd_index < opnd_num_regs_used(dst_opnd);
                 ++opnd_index) {
                reg_id_t reg = opnd_get_reg_used(dst_opnd, opnd_index);
                if (used_dst_reg_map[reg] == 0) {
                    ++num_dsts;
                    used_dst_reg_map[reg] = 1;
                }
            }
        }
    }
    *encoding |= num_dsts;

    /* Encode number of source operands, adding on top of already present source operands.
     */
    uint original_num_srcs = (uint)instr_num_srcs(instr);
    for (uint i = 0; i < original_num_srcs; ++i) {
        opnd_t src_opnd = instr_get_src(instr, i);
        for (uint opnd_index = 0; opnd_index < opnd_num_regs_used(src_opnd);
             ++opnd_index) {
            reg_id_t reg = opnd_get_reg_used(src_opnd, opnd_index);
            if (used_src_reg_map[reg] == 0) {
                ++num_srcs;
                used_src_reg_map[reg] = 1;
            }
        }
    }
    *encoding |= (num_srcs << SRC_OPND_SHIFT);

    /* Encode arithmetic flags.
     */
    uint eflags_instr = instr_get_arith_flags(instr, DR_QUERY_DEFAULT);
    uint eflags = 0;
    if (eflags_instr & EFLAGS_WRITE_ARITH)
        eflags |= SYNTHETIC_INSTR_WRITES_ARITH;
    if (eflags_instr & EFLAGS_READ_ARITH)
        eflags |= SYNTHETIC_INSTR_READS_ARITH;
    *encoding |= (eflags << FLAGS_SHIFT);

    /* Encode category as synthetic opcode.
     */
    uint category = instr_get_category(instr);
    *encoding |= (category << CATEGORY_SHIFT);

    /* Encode register destination operands, if present.
     */
    uint dst_reg_counter = 0;
    for (uint reg = 0; reg < MAX_NUM_REGS; ++reg) {
        // TODO i#6662: need to add virtual registers.
        // Right now using regular reg_id_t (which holds DR_REG_ values) from opnd_api.h.
        if (used_dst_reg_map[reg] == 1) {
            encoded_instr[dst_reg_counter + INSTRUCTION_BYTES] = (byte)reg;
            ++dst_reg_counter;
        }
    }

    /* Encode register source operands, if present.
     */
    uint src_reg_counter = 0;
    for (uint reg = 0; reg < MAX_NUM_REGS; ++reg) {
        // TODO i#6662: need to add virtual registers.
        // Right now using regular reg_id_t (which holds DR_REG_ values) from opnd_api.h.
        if (used_src_reg_map[reg] == 1) {
            encoded_instr[src_reg_counter + INSTRUCTION_BYTES + num_dsts] = (byte)reg;
            ++src_reg_counter;
        }
    }

    /* Compute instruction length including bytes for padding to reach 4 bytes alignment.
     */
    uint num_opnds = num_srcs + num_dsts;
    uint num_opnds_ceil = (num_opnds + INSTRUCTION_BYTES - 1) / INSTRUCTION_BYTES;
    uint instr_length = INSTRUCTION_BYTES + num_opnds_ceil * INSTRUCTION_BYTES;

    /* Compute next instruction's PC as: current PC + instruction length.
     */
    byte *next_pc = encoded_instr + instr_length;

    return next_pc;
}
