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
 * Returns next instruction's pc.
 *
 * The smallest encoded instruction (i.e., with no operands) has 4 bytes and follows
 * this scheme:
 * |----------------------| |--| |----| |----|
 * 31..               ..10  9,8   7..4   3..0
 *       category          eflags #src  #dst
 *
 * 22 bits, category: holds values of type #dr_instr_category_t. Note that an instruction
 * can belong to more than one category.
 * 2 bits, eflags: most significant bit set to 1 indicates the instruction reads at least
 * one arithmetic flag, least significant bit set to 1 indicates the instruction writes
 * at least one arithmetic flag.
 * 4 bits, #src: number of source operands (read) that are registers.
 * 4 bits, #dst: number of destination operands (written) that are registers.
 * (Note that we are only interested in register dependencies, hence operands that are
 * not register, such as immediates or memory references, are not present)
 *
 * Instructions with operands can reach up to 14 bytes: the 4 bytes described above, and
 * up to 10 register operands of 1 byte each.
 * For example, an instruction with 4 operands (2 src, 2 dst) has 4 additional bytes that
 * are encoded following this scheme:
 * |--------| |--------| |--------| |--------|
 * 31.. ..24  23.. ..16  15..  ..8  7..   ..0
 * src_opnd0  src_opnd1  dst_opnd0  dst_opnd1
 *
 * We assume all encoded values to be little-endian.
 */
byte *
decode_from_synth(dcontext_t *dcontext, byte *encoded_instr, instr_t *instr)
{
    uint encoding = 0;
    uint shift = 0;

    /* Copy encoded_instr in a uint for easier retrieving of values.
     */
    memcpy(&encoding, encoded_instr, sizeof(encoding));

    /* Decode number of destination operands.
     */
    uint num_dsts_mask = ((1U << NUM_OPND_BITS) - 1);
    uint num_dsts = encoding & num_dsts_mask;
    shift += NUM_OPND_BITS;

    /* Decode number of source operands.
     */
    uint num_srcs_mask = ((1U << NUM_OPND_BITS) - 1) << shift;
    uint num_srcs = (encoding & num_srcs_mask) >> shift;
    shift += NUM_OPND_BITS;

    instr_set_num_opnds(dcontext, instr, num_dsts, num_srcs);

    /* Decode flags.
     */
    // TODO i#6662: need to set flags in instr_t.
    // Following code commented to remove currently unused variables.
    // uint flags_mask = ((1U << FLAGS_BITS) - 1) << shift;
    // uint flags = (encoding & flags_mask) >> shift;
    shift += FLAGS_BITS;

    /* Decode synthetic opcode as instruction category.
     */
    uint category_mask = ((1U << CATEGORY_BITS) - 1) << shift;
    uint category = (encoding & category_mask) >> shift;
    instr_set_category(instr, category);

    /* Decode register source operands, if present.
     */
    uint encoding_size = (uint)sizeof(encoding); // Initial 4 bytes offset.
    for (uint i = 0; i < num_srcs; ++i) {
        ushort src = (ushort)encoded_instr[i + encoding_size];
        // TODO i#6662: need to add virtual registers.
        // Right now using regular reg_id_t (which holds DR_REG_ values) from
        // opnd_api.h.
        opnd_t src_opnd = opnd_create_reg(src);
        instr_set_src(instr, i, src_opnd);
    }

    /* Decode register destination operands, if present.
     */
    for (uint i = 0; i < num_dsts; ++i) {
        ushort dst = (uint)encoded_instr[i + encoding_size + num_srcs];
        // TODO i#6662: need to add virtual registers.
        // Right now using regular reg_id_t (which holds DR_REG_ values) from
        // opnd_api.h.
        opnd_t dst_opnd = opnd_create_reg(dst);
        instr_set_dst(instr, i, dst_opnd);
    }

    /* Compute next instruction's pc as: current pc + encoded instruction size.
     */
    byte *next_pc = encoded_instr + encoding_size + num_srcs + num_dsts;

    return;
}
