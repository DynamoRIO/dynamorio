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

#ifndef _REGDEPS_ENCODING_COMMON_H_
#define _REGDEPS_ENCODING_COMMON_H_

/* Here we describe the encoding scheme for the Synthetic ISA that is enforced in decode.c
 * and encode.c.
 *
 * Encoded instructions are 4 byte aligned.
 *
 * All instruction encodings begin with the following 4 header bytes, which follow this
 * scheme:
 * |----------------------| |--| |----| |----|
 * 31..               ..10  9,8   7..4   3..0
 *        category         eflags #src   #dst
 *
 * - 22 bits, category: it's a high level representation of the opcode of an instruction.
 *   Each bit represents one category following #dr_instr_category_t.  Note that an
 *   instruction can belong to more than one category, hence multiple bits can be set;
 * - 2 bits, eflags: most significant bit set to 1 indicates the instruction reads at
 *   least one arithmetic flag; least significant bit set to 1 indicates the instruction
 *   writes at least one arithmetic flag;
 * - 4 bits, #src: number of source operands (read) that are registers.  Registers used in
 *   memory reference operands of the instruction we are encoding (regardless of whether
 *   they are source or destination operands) are considered as source operands in the
 *   encoded instruction because they are always read;
 * - 4 bits, #dst: number of destination operands (written) that are registers.
 *
 * We assume these encoded values to be little-endian.  Note that we are only interested
 * in register dependencies, hence operands that are not registers, such as immediates or
 * memory references, are not present.
 *
 * Following the 4 header bytes are the bytes for the operation size and for the encoding
 * of register operands, if any are present.
 *
 * The first byte contains the operation size encoded as an OPSZ_ enum value.  The
 * operation size is the size of the largest source operand, regardless of it being a
 * register, a memory reference, or an immediate.
 *
 * Following the operation size are the register operand IDs.  Each register operand is 1
 * byte.  The destination operands go first, followed by the source operands.  An
 * instruction can have up to 8 operands (sources + destinations).  Note that, because of
 * 4 byte alignment, the length of encoded instructions will include padding and is as
 * follows:
 * - instructions with no operands have only the 4 header bytes (no operation size byte
 *   nor operand-related bytes);
 * - instructions with 1 to 3 operands have a length of 8 bytes (4 header bytes + 1 byte
 *   for operation size + 3 operand-related/padding bytes);
 * - instructions with 4 to 7 operands have a length of 12 bytes;
 * - instructions with 8 operands have the maximum length of 16 bytes.
 *
 * For example, an instruction with 4 operands (1 dst, 3 src) has a length of 12 bytes and
 * would be encoded as:
 * |----------------------| |--| |----| |----|
 * 31..               ..10  9,8   7..4   3..0
 *        category         eflags #src   #dst
 * |--------| |--------| |--------| |--------|
 * 31.. ..24  23.. ..16  15..  ..8  7..   ..0
 *  src_op1    src_op0    dst_op0    op_size
 * |--------| |--------| |--------| |--------|
 * 31.. ..24  23.. ..16  15..  ..8  7..   ..0
 *  padding    padding    padding    src_op2
 *
 * Because of 4 byte alignment, the last 3 bytes [31.. ..8] are padding and are undefined
 * (i.e., it cannot be assumed that they have been zeroed-out or contain any meaningful
 * value).
 */

#define REGDEPS_CATEGORY_BITS 22
#define REGDEPS_FLAGS_BITS 2
#define REGDEPS_NUM_OPND_BITS 4

#define REGDEPS_SRC_OPND_SHIFT REGDEPS_NUM_OPND_BITS
#define REGDEPS_FLAGS_SHIFT (2 * REGDEPS_NUM_OPND_BITS)
#define REGDEPS_CATEGORY_SHIFT (2 * REGDEPS_NUM_OPND_BITS + REGDEPS_FLAGS_BITS)

#define REGDEPS_DST_OPND_MASK ((1U << REGDEPS_NUM_OPND_BITS) - 1)
#define REGDEPS_SRC_OPND_MASK \
    (((1U << REGDEPS_NUM_OPND_BITS) - 1) << REGDEPS_SRC_OPND_SHIFT)
#define REGDEPS_FLAGS_MASK (((1U << REGDEPS_FLAGS_BITS) - 1) << REGDEPS_FLAGS_SHIFT)
#define REGDEPS_CATEGORY_MASK \
    (((1U << REGDEPS_CATEGORY_BITS) - 1) << REGDEPS_CATEGORY_SHIFT)

#define REGDEPS_INSTR_WRITES_ARITH 0x1
#define REGDEPS_INSTR_READS_ARITH 0x2

#define REGDEPS_HEADER_BYTES 4
#define REGDEPS_OP_SIZE_INDEX REGDEPS_HEADER_BYTES
#define REGDEPS_OPND_INDEX REGDEPS_OP_SIZE_INDEX + 1

#define REGDEPS_ALIGN_BYTES 4

#define REGDEPS_MAX_NUM_OPNDS 8

/* Defines the maximum number of non-overlapping registers for any architecture we
 * currently support.  Currently AARCH64 has the highest number: 198.  We round it to 256.
 */
#define REGDEPS_MAX_NUM_REGS 256

#endif // _REGDEPS_ENCODING_COMMON_H_
