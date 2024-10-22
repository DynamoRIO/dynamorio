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

/* decode.c -- a decoder for DR_ISA_REGDEPS instructions */

#include "decode.h"

#include "../globals.h"
#include "encode_api.h"
#include "encoding_common.h"
#include "instr_api.h"
#include "opnd_api.h"

/* Decodes the raw bytes of an encoded instruction \p encoded_instr into DR instruction
 * representation \p instr.
 * Returns the next instruction's PC.
 * The encoding scheme followed is described in #core/ir/isa_regdeps/encoding_common.h.
 */
byte *
decode_isa_regdeps(dcontext_t *dcontext, byte *encoded_instr, instr_t *instr)
{
    /* Interpret the first 4 bytes of encoded_instr (which are always present) as a uint
     * for easier retrieving of category, eflags, #src, and #dst values.
     * We can do this safely because encoded_instr is 4 bytes aligned.
     */
    ASSERT(ALIGNED(encoded_instr, REGDEPS_ALIGN_BYTES));
    uint encoding_header = *((uint *)encoded_instr);

    /* Decode number of register destination operands.
     */
    uint num_dsts = encoding_header & REGDEPS_DST_OPND_MASK;

    /* Decode number of register source operands.
     */
    uint num_srcs = (encoding_header & REGDEPS_SRC_OPND_MASK) >> REGDEPS_SRC_OPND_SHIFT;

    instr_set_num_opnds(dcontext, instr, num_dsts, num_srcs);

    /* Decode arithmetic flags.
     */
    uint eflags = (encoding_header & REGDEPS_FLAGS_MASK) >> REGDEPS_FLAGS_SHIFT;
    uint eflags_instr = 0;
    if (TESTANY(REGDEPS_INSTR_WRITES_ARITH, eflags))
        eflags_instr |= EFLAGS_WRITE_ARITH;
    if (TESTANY(REGDEPS_INSTR_READS_ARITH, eflags))
        eflags_instr |= EFLAGS_READ_ARITH;
    instr->eflags = eflags_instr;

    /* Declare the eflags to be valid.
     * This is needed in order to retrieve their value without trying to compute it again.
     */
    instr_set_arith_flags_valid(instr, true);

    /* Decode instruction category.
     */
    uint category = (encoding_header & REGDEPS_CATEGORY_MASK) >> REGDEPS_CATEGORY_SHIFT;
    instr_set_category(instr, category);

    /* Decode operation size, if there are any operands.
     */
    uint num_opnds = num_dsts + num_srcs;
    opnd_size_t max_opnd_size = OPSZ_0;
    if (num_opnds > 0)
        max_opnd_size = (opnd_size_t)encoded_instr[REGDEPS_OP_SIZE_INDEX];
    instr->operation_size = max_opnd_size;

    /* Decode register destination operands, if present.
     */
    for (uint i = 0; i < num_dsts; ++i) {
        reg_id_t dst = (reg_id_t)encoded_instr[i + REGDEPS_OPND_INDEX];
        opnd_t dst_opnd = opnd_create_reg((reg_id_t)dst);
        /* Virtual registers don't have a fixed size like real ISA registers do.
         * So, it can happen that the same virtual register in two different
         * instructions may have different sizes.
         *
         * Even though querying the size of a virtual register is not supported on
         * purpose (a user should query the instr_t.operation_size), we set the
         * opnd_t.size field to be the same as instr_t.operation_size (i.e.,
         * max_opnd_size), so that reg_get_size() can return some meaningful
         * information without triggering a CLIENT_ASSERT error because the
         * virtual register ID is not supported (e.g., is one of the "reserved"
         * register IDs).
         *
         * We do the same for both src and dst register operands of DR_ISA_REGDEPS
         * instructions.
         */
        opnd_set_size(&dst_opnd, max_opnd_size);
        instr_set_dst(instr, i, dst_opnd);
    }

    /* Decode register source operands, if present.
     */
    for (uint i = 0; i < num_srcs; ++i) {
        reg_id_t src = (reg_id_t)encoded_instr[i + REGDEPS_OPND_INDEX + num_dsts];
        opnd_t src_opnd = opnd_create_reg((reg_id_t)src);
        opnd_set_size(&src_opnd, max_opnd_size);
        instr_set_src(instr, i, src_opnd);
    }

    /* Compute instruction length including bytes for padding to reach 4 bytes alignment.
     * Account for 1 additional byte containing max register operand size, if there are
     * any operands.
     */
    uint num_opnd_bytes = num_opnds > 0 ? num_opnds + 1 : 0;
    uint length =
        ALIGN_FORWARD(REGDEPS_HEADER_BYTES + num_opnd_bytes, REGDEPS_ALIGN_BYTES);
    instr->length = length;

    /* Allocate space to save encoding in the bytes field of instr_t.  We use it to avoid
     * unnecessary encoding.
     */
    instr_allocate_raw_bits(dcontext, instr, length);

    /* Declare the operands to be valid.
     */
    instr_set_operands_valid(instr, true);

    /* Set opcode as OP_UNDECODED, so routines like instr_valid() can still work.
     * We can't use instr_set_opcode() because of its CLIENT_ASSERT when setting the
     * opcode to OP_UNDECODED or OP_INVALID.
     */
    instr->opcode = OP_UNDECODED;

    /* Set decoded instruction ISA mode to be synthetic.
     */
    instr_set_isa_mode(instr, DR_ISA_REGDEPS);

    /* Copy encoding to bytes field of instr_t.
     */
    instr_set_raw_bytes(instr, encoded_instr, length);

    /* Compute next instruction's PC as: current PC + instruction length.
     */
    return encoded_instr + length;
}
