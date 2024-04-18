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

/* We need a seprate enum from the DR_REG_ in opnd_api.h so we can start counting virtual
 * registers from 0.  Otherwise, the DR_REG_ enum values in opnd_api.h won't fit in the
 * 1 byte operand size of DR_ISA_REGDEPS encoding.
 */
enum {
    DR_REG_V0 = 0,
    DR_REG_V1,
    DR_REG_V2,
    DR_REG_V3,
    DR_REG_V4,
    DR_REG_V5,
    DR_REG_V6,
    DR_REG_V7,
    DR_REG_V8,
    DR_REG_V9,
    DR_REG_V10,
    DR_REG_V11,
    DR_REG_V12,
    DR_REG_V13,
    DR_REG_V14,
    DR_REG_V15,
    DR_REG_V16,
    DR_REG_V17,
    DR_REG_V18,
    DR_REG_V19,
    DR_REG_V20,
    DR_REG_V21,
    DR_REG_V22,
    DR_REG_V23,
    DR_REG_V24,
    DR_REG_V25,
    DR_REG_V26,
    DR_REG_V27,
    DR_REG_V28,
    DR_REG_V29,
    DR_REG_V30,
    DR_REG_V31,
    DR_REG_V32,
    DR_REG_V33,
    DR_REG_V34,
    DR_REG_V35,
    DR_REG_V36,
    DR_REG_V37,
    DR_REG_V38,
    DR_REG_V39,
    DR_REG_V40,
    DR_REG_V41,
    DR_REG_V42,
    DR_REG_V43,
    DR_REG_V44,
    DR_REG_V45,
    DR_REG_V46,
    DR_REG_V47,
    DR_REG_V48,
    DR_REG_V49,
    DR_REG_V50,
    DR_REG_V51,
    DR_REG_V52,
    DR_REG_V53,
    DR_REG_V54,
    DR_REG_V55,
    DR_REG_V56,
    DR_REG_V57,
    DR_REG_V58,
    DR_REG_V59,
    DR_REG_V60,
    DR_REG_V61,
    DR_REG_V62,
    DR_REG_V63,
    DR_REG_V64,
    DR_REG_V65,
    DR_REG_V66,
    DR_REG_V67,
    DR_REG_V68,
    DR_REG_V69,
    DR_REG_V70,
    DR_REG_V71,
    DR_REG_V72,
    DR_REG_V73,
    DR_REG_V74,
    DR_REG_V75,
    DR_REG_V76,
    DR_REG_V77,
    DR_REG_V78,
    DR_REG_V79,
    DR_REG_V80,
    DR_REG_V81,
    DR_REG_V82,
    DR_REG_V83,
    DR_REG_V84,
    DR_REG_V85,
    DR_REG_V86,
    DR_REG_V87,
    DR_REG_V88,
    DR_REG_V89,
    DR_REG_V90,
    DR_REG_V91,
    DR_REG_V92,
    DR_REG_V93,
    DR_REG_V94,
    DR_REG_V95,
    DR_REG_V96,
    DR_REG_V97,
    DR_REG_V98,
    DR_REG_V99,
    DR_REG_V100,
    DR_REG_V101,
    DR_REG_V102,
    DR_REG_V103,
    DR_REG_V104,
    DR_REG_V105,
    DR_REG_V106,
    DR_REG_V107,
    DR_REG_V108,
    DR_REG_V109,
    DR_REG_V110,
    DR_REG_V111,
    DR_REG_V112,
    DR_REG_V113,
    DR_REG_V114,
    DR_REG_V115,
    DR_REG_V116,
    DR_REG_V117,
    DR_REG_V118,
    DR_REG_V119,
    DR_REG_V120,
    DR_REG_V121,
    DR_REG_V122,
    DR_REG_V123,
    DR_REG_V124,
    DR_REG_V125,
    DR_REG_V126,
    DR_REG_V127,
    DR_REG_V128,
    DR_REG_V129,
    DR_REG_V130,
    DR_REG_V131,
    DR_REG_V132,
    DR_REG_V133,
    DR_REG_V134,
    DR_REG_V135,
    DR_REG_V136,
    DR_REG_V137,
    DR_REG_V138,
    DR_REG_V139,
    DR_REG_V140,
    DR_REG_V141,
    DR_REG_V142,
    DR_REG_V143,
    DR_REG_V144,
    DR_REG_V145,
    DR_REG_V146,
    DR_REG_V147,
    DR_REG_V148,
    DR_REG_V149,
    DR_REG_V150,
    DR_REG_V151,
    DR_REG_V152,
    DR_REG_V153,
    DR_REG_V154,
    DR_REG_V155,
    DR_REG_V156,
    DR_REG_V157,
    DR_REG_V158,
    DR_REG_V159,
    DR_REG_V160,
    DR_REG_V161,
    DR_REG_V162,
    DR_REG_V163,
    DR_REG_V164,
    DR_REG_V165,
    DR_REG_V166,
    DR_REG_V167,
    DR_REG_V168,
    DR_REG_V169,
    DR_REG_V170,
    DR_REG_V171,
    DR_REG_V172,
    DR_REG_V173,
    DR_REG_V174,
    DR_REG_V175,
    DR_REG_V176,
    DR_REG_V177,
    DR_REG_V178,
    DR_REG_V179,
    DR_REG_V180,
    DR_REG_V181,
    DR_REG_V182,
    DR_REG_V183,
    DR_REG_V184,
    DR_REG_V185,
    DR_REG_V186,
    DR_REG_V187,
    DR_REG_V188,
    DR_REG_V189,
    DR_REG_V190,
    DR_REG_V191,
    DR_REG_V192,
    DR_REG_V193,
    DR_REG_V194,
    DR_REG_V195,
    DR_REG_V196,
    DR_REG_V197,
    DR_REG_V198,
    DR_REG_V199,
    DR_REG_V200,
    DR_REG_V201,
    DR_REG_V202,
    DR_REG_V203,
    DR_REG_V204,
    DR_REG_V205,
    DR_REG_V206,
    DR_REG_V207,
    DR_REG_V208,
    DR_REG_V209,
    DR_REG_V210,
    DR_REG_V211,
    DR_REG_V212,
    DR_REG_V213,
    DR_REG_V214,
    DR_REG_V215,
    DR_REG_V216,
    DR_REG_V217,
    DR_REG_V218,
    DR_REG_V219,
    DR_REG_V220,
    DR_REG_V221,
    DR_REG_V222,
    DR_REG_V223,
    DR_REG_V224,
    DR_REG_V225,
    DR_REG_V226,
    DR_REG_V227,
    DR_REG_V228,
    DR_REG_V229,
    DR_REG_V230,
    DR_REG_V231,
    DR_REG_V232,
    DR_REG_V233,
    DR_REG_V234,
    DR_REG_V235,
    DR_REG_V236,
    DR_REG_V237,
    DR_REG_V238,
    DR_REG_V239,
    DR_REG_V240,
    DR_REG_V241,
    DR_REG_V242,
    DR_REG_V243,
    DR_REG_V244,
    DR_REG_V245,
    DR_REG_V246,
    DR_REG_V247,
    DR_REG_V248,
    DR_REG_V249,
    DR_REG_V250,
    DR_REG_V251,
    DR_REG_V252,
    DR_REG_V253,
    DR_REG_V254,
    DR_REG_V255,
};

#endif // _REGDEPS_ENCODING_COMMON_H_
