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

#ifndef _SYNTHETIC_ENCODING_COMMON_H_
#define _SYNTHETIC_ENCODING_COMMON_H_

/* This synthetic ISA is a made up ISA that has the purpose of preserving register
 * dependencies and giving hints on the type of operations each instruction is performing.
 * For this reason the majority of operations that would normally work on instructions
 * coming from an actual ISA are not supported.
 * The only operations that we support are instr_encode() and decode() when DynamoRIO's
 * host is an x86 architecture.
 * Also note that in DynamoRIO we use the #instr_t ISA mode to determine what type of
 * encoding to perform, and #dr_context_t ISA mode to determine what type of decoding to
 * perform.
 * Currently the only exception to this rule happens for decoding of synthetic
 * instructions, where #instr_t ISA mode takes precedence and #dr_context_t ISA mode is
 * ignored if #instr_t ISA mode is DR_ISA_SYNTHETIC.
 * XXX i#1684: Note that this is part of a larger issue, where lack of cross-arch support
 * in the same build of DynamoRIO is limiting us.
 */

/* Here we describe the encoding scheme for the Synthetic ISA that is enforced in decode.c
 * and encode.c.
 *
 * Encoded instructions are 4 bytes aligned.
 *
 * All instruction encodings begin with the following 4 bytes, which follow this scheme:
 * |----------------------| |--| |----| |----|
 * 31..               ..10  9,8   7..4   3..0
 *       category          eflags #src   #dst
 *
 * 22 bits, category: it's a high level representation of the opcode of an instruction.
 * Each bit represents one category following #dr_instr_category_t.
 * Note that an instruction can belong to more than one category, hence multiple bits can
 * be set.
 * 2 bits, eflags: most significant bit set to 1 indicates the instruction reads at least
 * one arithmetic flag, least significant bit set to 1 indicates the instruction writes
 * at least one arithmetic flag.
 * 4 bits, #src: number of source operands (read) that are registers.
 * 4 bits, #dst: number of destination operands (written) that are registers.
 * (Note that we are only interested in register dependencies, hence operands that are
 * not registers, such as immediates or memory references, are not present.)
 *
 * Following the 4 instruction-related bytes are the bytes for encoding register operands.
 * Each operand is 1 byte.
 * The destination operands go first, followed by the source operands.
 * An instruction can have up to 8 operands (sources + destinations).
 * Note that, because of 4 bytes alignment, instructions with 1 to 4 (included) operands
 * will have a size of 8 bytes (4 instruction-related bytes + 4 operand-related bytes),
 * while instructions with 5 to 8 (included) operands will have a size of 12 bytes
 * (4 instruction-related bytes + 8 operand-related bytes).
 * Instructions with no operands only have 4 bytes (the 4 instruction-related bytes).
 * For example, an instruction with 3 operands (1 dst, 2 src) has 4 additional bytes that
 * are encoded following this scheme:
 * |--------| |--------| |--------| |--------|
 * 31.. ..24  23.. ..16  15..  ..8  7..   ..0
 *            src_opnd1  src_opnd0  dst_opnd0
 *
 * Because of 4 bytes alignment, the last byte (31..  ..24) is padding and it's undefined
 * (i.e., it cannot be assumed that it has been zeroed-out or contains any meaningful
 * value).
 *
 * We assume all encoded values to be little-endian.
 */

#define CATEGORY_BITS 22
#define FLAGS_BITS 2
#define NUM_OPND_BITS 4

#define SRC_OPND_SHIFT NUM_OPND_BITS
#define FLAGS_SHIFT (2 * NUM_OPND_BITS)
#define CATEGORY_SHIFT (2 * NUM_OPND_BITS + FLAGS_BITS)

#define DST_OPND_MASK ((1U << NUM_OPND_BITS) - 1)
#define SRC_OPND_MASK (((1U << NUM_OPND_BITS) - 1) << SRC_OPND_SHIFT)
#define FLAGS_MASK (((1U << FLAGS_BITS) - 1) << FLAGS_SHIFT)
#define CATEGORY_MASK (((1U << CATEGORY_BITS) - 1) << CATEGORY_SHIFT)

#define SYNTHETIC_INSTR_WRITES_ARITH 0x1
#define SYNTHETIC_INSTR_READS_ARITH 0x2

#define INSTRUCTION_BYTES 4

#define MAX_NUM_REGS 256

#endif // _SYNTHETIC_ENCODING_COMMON_H_
