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
 * A description of the encoding scheme is provided in core/ir/synthetic/decode.c.
 * We assume all encoded values to be little-endian.
 */
byte *
encode_to_synth(dcontext_t *dcontext, instr_t *instr, byte *encoded_instr)
{
    uint encoding = 0;
    uint shift = 0;

    /* Encode number of destination operands.
     */
    uint original_num_dsts = (uint)instr_num_dsts(instr);
    uint num_dsts = 0;
    for (uint i = 0; i < original_num_dsts; ++i) {
        opnd_t dst_opnd = instr_get_dst(instr, i);
        if (opnd_is_reg(dst_opnd))
            ++num_dsts;
    }
    encoding |= num_dsts;
    shift += NUM_OPND_BITS;

    /* Encode number of source operands.
     */
    uint original_num_srcs = (uint)instr_num_srcs(instr);
    uint num_srcs = 0;
    for (uint i = 0; i < original_num_srcs; ++i) {
        opnd_t src_opnd = instr_get_src(instr, i);
        if (opnd_is_reg(src_opnd))
            ++num_srcs;
    }
    encoding |= (num_srcs << shift);
    shift += NUM_OPND_BITS;

    /* Encode flags.
     */
    // TODO i#6662: retrieve whether arithmetic flags are used from instr_t.
    // Currently assuming all instructions both read and write at least one arithmetic
    // flag (both bits set to 1).
    uint flags = 0x3;
    encoding |= (flags << shift);
    shift += FLAGS_BITS;

    /* Encode category as synthetic opcode.
     */
    uint category = instr_get_category(instr);
    encoding |= (category << shift);

    /* Copy encoding that is common to all instructions (initial 4 bytes) to output:
     * encoded_instr.
     */
    memcpy(encoded_instr, &encoding, sizeof(encoding));

    /* Encode register source operands, if present.
     */
    uint encoding_size = (uint)sizeof(encoding); // Initial 4 bytes offset.
    uint src_reg_counter = 0;
    for (uint i = 0; i < original_num_srcs; ++i) {
        opnd_t src_opnd = instr_get_src(instr, i);
        if (opnd_is_reg(src_opnd)) {
            // TODO i#6662: need to add virtual registers.
            // Right now using regular reg_id_t (which holds DR_REG_ values) from
            // opnd_api.h.
            reg_id_t reg = opnd_get_reg(src_opnd);
            encoded_instr[src_reg_counter + encoding_size] = (byte)reg;
            ++src_reg_counter;
        }
    }

    /* Decode register source operands, if present.
     */
    uint dst_reg_counter = 0;
    for (uint i = 0; i < original_num_dsts; ++i) {
        opnd_t dst_opnd = instr_get_dst(instr, i);
        if (opnd_is_reg(dst_opnd)) {
            // TODO i#6662: need to add virtual registers.
            // Right now using regular reg_id_t (which holds DR_REG_ values) from
            // opnd_api.h.
            reg_id_t reg = opnd_get_reg(dst_opnd);
            encoded_instr[dst_reg_counter + encoding_size + num_srcs] = (byte)reg;
            ++dst_reg_counter;
        }
    }

    /* Compute next instruction's pc as: current pc + encoded instruction size.
     */
    byte *next_pc = encoded_instr + encoding_size + src_reg_counter + dst_reg_counter;

    return next_pc;
}
