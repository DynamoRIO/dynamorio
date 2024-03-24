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
    /* Use a local uint variable for easier setting of category, eflags, #src, and #dst
     * values.
     */
    uint encoding = 0;

    /* Encode number of register destination operands.
     * Note that a destination operand that is a memory renference should have its
     * registers (if any) counted as source operands, since they are being read.
     * We use [src|dst]_reg_to_size to keep track of registers we've seen and avoid
     * duplicates, and also to record their size, which we encode later.
     */
    opnd_size_t src_reg_to_size[MAX_NUM_REGS];
    memset((void *)src_reg_to_size, 0, sizeof(src_reg_to_size));
    uint num_srcs = 0;
    opnd_size_t dst_reg_to_size[MAX_NUM_REGS];
    memset((void *)dst_reg_to_size, 0, sizeof(dst_reg_to_size));
    uint num_dsts = 0;
    uint original_num_dsts = (uint)instr_num_dsts(instr);
    for (uint dst_index = 0; dst_index < original_num_dsts; ++dst_index) {
        opnd_t dst_opnd = instr_get_dst(instr, dst_index);
        uint num_regs_used_by_opnd = (uint)opnd_num_regs_used(dst_opnd);
        if (opnd_is_memory_reference(dst_opnd)) {
            for (uint opnd_index = 0; opnd_index < num_regs_used_by_opnd; ++opnd_index) {
                reg_id_t reg = opnd_get_reg_used(dst_opnd, opnd_index);
                /* Map sub-registers to their containing register.
                 */
                reg_id_t reg_canonical = reg_to_pointer_sized(reg);
                opnd_size_t reg_size = reg_get_size(reg);
                if (src_reg_to_size[reg_canonical] != 0) {
                    ++num_srcs;
                    src_reg_to_size[reg_canonical] = reg_size;
                }
            }
        } else {
            for (uint opnd_index = 0; opnd_index < num_regs_used_by_opnd; ++opnd_index) {
                reg_id_t reg = opnd_get_reg_used(dst_opnd, opnd_index);
                /* Map sub-registers to their containing register.
                 */
                reg_id_t reg_canonical = reg_to_pointer_sized(reg);
                opnd_size_t reg_size = reg_get_size(reg);
                if (dst_reg_to_size[reg_canonical] != 0) {
                    ++num_dsts;
                    dst_reg_to_size[reg_canonical] = reg_size;
                }
            }
        }
    }
    encoding |= num_dsts;

    /* Encode number of register source operands, adding on top of already existing ones.
     */
    uint original_num_srcs = (uint)instr_num_srcs(instr);
    for (uint i = 0; i < original_num_srcs; ++i) {
        opnd_t src_opnd = instr_get_src(instr, i);
        uint num_regs_used_by_opnd = (uint)opnd_num_regs_used(src_opnd);
        for (uint opnd_index = 0; opnd_index < num_regs_used_by_opnd; ++opnd_index) {
            reg_id_t reg = opnd_get_reg_used(src_opnd, opnd_index);
            /* Map sub-registers to their containing register.
             */
            reg_id_t reg_canonical = reg_to_pointer_sized(reg);
            opnd_size_t reg_size = reg_get_size(reg);
            if (src_reg_to_size[reg_canonical] != 0) {
                ++num_srcs;
                src_reg_to_size[reg_canonical] = reg_size;
            }
        }
    }
    encoding |= (num_srcs << SRC_OPND_SHIFT);

    /* Encode arithmetic flags.
     */
    uint eflags_instr = instr_get_arith_flags(instr, DR_QUERY_DEFAULT);
    uint eflags = 0;
    if (TESTANY(EFLAGS_WRITE_ARITH, eflags_instr))
        eflags |= SYNTHETIC_INSTR_WRITES_ARITH;
    if (TESTANY(EFLAGS_READ_ARITH, eflags_instr))
        eflags |= SYNTHETIC_INSTR_READS_ARITH;
    encoding |= (eflags << FLAGS_SHIFT);

    /* Encode category as synthetic opcode.
     */
    uint category = instr_get_category(instr);
    encoding |= (category << CATEGORY_SHIFT);

    /* Copy encoding back into encoded_instr output.
     */
    *((uint *)&encoded_instr[0]) = encoding;

    /* Encode register destination operands and their sizes, if present.
     */
    uint dst_reg_counter = 0;
    if (num_dsts > 0) {
        for (uint reg = 0; reg < MAX_NUM_REGS; ++reg) {
            if (dst_reg_to_size[reg] != 0) {
                /* XXX i#6662: we might want to consider doing some kind of register
                 * shuffling.
                 */
                encoded_instr[dst_reg_counter + HEADER_BYTES] = (byte)reg;
                encoded_instr[dst_reg_counter + 1 + HEADER_BYTES] =
                    (byte)dst_reg_to_size[reg];
                dst_reg_counter += OPERAND_BYTES;
            }
        }
    }

    /* Encode register source operands and their sizes, if present.
     */
    uint src_reg_counter = 0;
    if (num_srcs > 0) {
        for (uint reg = 0; reg < MAX_NUM_REGS; ++reg) {
            if (src_reg_to_size[reg]) {
                /* XXX i#6662: we might want to consider doing some kind of register
                 * shuffling.
                 */
                encoded_instr[src_reg_counter + HEADER_BYTES + num_dsts] = (byte)reg;
                encoded_instr[src_reg_counter + 1 + HEADER_BYTES] =
                    (byte)src_reg_to_size[reg];
                src_reg_counter += OPERAND_BYTES;
            }
        }
    }

    /* Compute instruction length including bytes for padding to reach 4 bytes alignment.
     */
    uint num_opnd_bytes = dst_reg_counter + src_reg_counter;
    uint instr_length = ALIGN_FORWARD(HEADER_BYTES + num_opnd_bytes, HEADER_BYTES);

    /* Compute next instruction's PC as: current PC + instruction length.
     */
    return encoded_instr + instr_length;
}
