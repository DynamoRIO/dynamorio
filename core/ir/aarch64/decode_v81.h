/* **********************************************************
 * Copyright (c) 2016-2022 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* File created by codec_gen.py on 07-04-22 from MRS version v8.1. */

/* This file is an example of the code which will be generated from a machine
 * readable specification (MRS) for all AArch64 instruction from v8.1 onwards.
 *
 * The comments in this file bind instruction specification data in the MRS to
 * decoder functionality to help describe the mapping between the two for
 * auto-generation purposes.
 *
 * The format of the code is intended to be human readable and will include
 * auto-generated comments extracted from the MRS. The prefix 'MRSC:' is used
 * to distinguish these examples from manual comments in this file.
 */

/* Each instruction's decode and encode function name is built from literal and
 * type data from the MRS. As an example:
 * dec_       Decode function, (enc_ for encode function, see encode_v81.h).
 * SQRDMLAH   Instruction name.
 * VVV        Operands signature:
 *            R  General purpose register.
 *            I  Immediate.
 *            V  Vector register (size specification follows).
 * 16         Vector size and type, e.g. 16 bit scalar (halfword). For vector
 *            elements, <number of elements>x<element size>, e.g. 4x16.
 */
/* MRSC: SQRDMLAH <V><d>, <V><n>, <V><m> */
static bool
dec_SQRDMLAH_VVV_16(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    /* Sanity check to ensure correct key/value entry has been generated in
     * decode_map.
     */
    ASSERT(BITS(enc, 15, 10) == 0b100001 && BITS(enc, 31, 21) == 0b01111110010);

    /* Decode operands based on type and size data, and bit positions extracted
     * from MRS.
     */
    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_HALF, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_HALF, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_HALF, BITS(enc, 20, 16));

    /* Instruction name and operand type data from MRS. */
    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 2);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);

    return true;
}

/* MRSC: SQRDMLAH <V><d>, <V><n>, <V><m> */
static bool
dec_SQRDMLAH_VVV_32(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 15, 10) == 0b100001 && BITS(enc, 31, 21) == 0b01111110100);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_SINGLE, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_SINGLE, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_SINGLE, BITS(enc, 20, 16));

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 2);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);

    return true;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
static bool
dec_SQRDMLAH_VVV_4x16(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 15, 10) == 0b100001 && BITS(enc, 31, 21) == 0b00101110010);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 20, 16));
    opnd_t elsz = opnd_create_immed_uint(VECTOR_ELEM_WIDTH_HALF, OPSZ_2b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 3);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, elsz);

    return true;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
static bool
dec_SQRDMLAH_VVV_8x16(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 15, 10) == 0b100001 && BITS(enc, 31, 21) == 0b01101110010);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 20, 16));
    opnd_t elsz = opnd_create_immed_uint(VECTOR_ELEM_WIDTH_HALF, OPSZ_2b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 3);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, elsz);

    return true;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
static bool
dec_SQRDMLAH_VVV_2x32(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 15, 10) == 0b100001 && BITS(enc, 31, 21) == 0b00101110100);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 20, 16));
    opnd_t elsz = opnd_create_immed_uint(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_2b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 3);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, elsz);

    return true;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
static bool
dec_SQRDMLAH_VVV_4x32(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 15, 10) == 0b100001 && BITS(enc, 31, 21) == 0b01101110100);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 20, 16));
    opnd_t elsz = opnd_create_immed_int(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_2b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 3);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, elsz);

    return true;
}

/* Translation from a 32 bit encoding to the decode function is based on a
 * collection of decode_map structures which use key/value mappings. The key is
 * a unique set of up to 32 fixed (non-operand) bits. The value is a pointer to
 * the decode function.
 *
 * The name of each decode_map struct is created from the bit positions of the
 * fixed (non-operand) bits extracted from the MRS. The keys are encodings for
 * these bit positions extracted from the MRS. The function pointer name is
 * created from the MRS as described by dec_SQRDMLAH_VVV_16() above.
 */
/* MRSC: SQRDMLAH <V><d>, <V><n>, <V><m> */
/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
const static decode_map decode_map__31_21__15_10[] = {
    { 0b01111110010100001, dec_SQRDMLAH_VVV_16 },
    { 0b01111110100100001, dec_SQRDMLAH_VVV_32 },
    { 0b00101110010100001, dec_SQRDMLAH_VVV_4x16 },
    { 0b01101110010100001, dec_SQRDMLAH_VVV_8x16 },
    { 0b00101110100100001, dec_SQRDMLAH_VVV_2x32 },
    { 0b01101110100100001, dec_SQRDMLAH_VVV_4x32 }
};

/* MRSC: SQRDMLAH <V><d>, <V><n>, <Vm>.<Ts>[<index>] */
static bool
dec_SQRDMLAH_VVVI_16(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 10, 10) == 0b0 && BITS(enc, 15, 12) == 0b1101 &&
           BITS(enc, 31, 22) == 0b0111111101);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_HALF, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_HALF, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 19, 16));
    opnd_t elsz = opnd_create_immed_uint(VECTOR_ELEM_WIDTH_HALF, OPSZ_1);
    uint hlm = (BITS(enc, 11, 11) << 2) | BITS(enc, 21, 20);
    opnd_t idx = opnd_create_immed_uint(hlm, OPSZ_3b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 4);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, idx);
    instr_set_src(instr, 3, elsz);

    return true;
}

/* MRSC: SQRDMLAH <V><d>, <V><n>, <Vm>.<Ts>[<index>] */
static bool
dec_SQRDMLAH_VVVI_32(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 10, 10) == 0b0 && BITS(enc, 15, 12) == 0b1101 &&
           BITS(enc, 31, 22) == 0b0111111110);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_SINGLE, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_SINGLE, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(4, BITS(enc, 19, 16));
    opnd_t elsz = opnd_create_immed_uint(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_1);
    uint hl = (BITS(enc, 11, 11) << 1) | BITS(enc, 21, 21);
    opnd_t idx = opnd_create_immed_uint(hl, OPSZ_3b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 4);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, idx);
    instr_set_src(instr, 3, elsz);

    return true;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
static bool
dec_SQRDMLAH_VVVI_4x16_1x16(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 10, 10) == 0b0 && BITS(enc, 15, 12) == 0b1101 &&
           BITS(enc, 31, 22) == 0b0010111101);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 19, 16));
    opnd_t elsz = opnd_create_immed_uint(VECTOR_ELEM_WIDTH_HALF, OPSZ_1);
    uint hlm = (BITS(enc, 11, 11) << 2) | BITS(enc, 21, 20);
    opnd_t idx = opnd_create_immed_uint(hlm, OPSZ_3b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 4);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, idx);
    instr_set_src(instr, 3, elsz);

    return true;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
static bool
dec_SQRDMLAH_VVVI_8x16_1x16(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 10, 10) == 0b0 && BITS(enc, 15, 12) == 0b1101 &&
           BITS(enc, 31, 22) == 0b0110111101);

    opnd_t Vd = decode_vreg(4, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(4, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(4, BITS(enc, 19, 16));
    opnd_t elsz = opnd_create_immed_uint(VECTOR_ELEM_WIDTH_HALF, OPSZ_1);
    uint hlm = (BITS(enc, 11, 11) << 2) | BITS(enc, 21, 20);
    opnd_t idx = opnd_create_immed_uint(hlm, OPSZ_3b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 4);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, idx);
    instr_set_src(instr, 3, elsz);

    return true;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
static bool
dec_SQRDMLAH_VVVI_2x32_1x32(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 10, 10) == 0b0 && BITS(enc, 15, 12) == 0b1101 &&
           BITS(enc, 31, 22) == 0b0010111110);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_DOUBLE, BITS(enc, 20, 16));
    opnd_t elsz = opnd_create_immed_uint(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_1);
    uint hl = (BITS(enc, 11, 11) << 1) | BITS(enc, 21, 21);
    opnd_t idx = opnd_create_immed_uint(hl, OPSZ_3b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 4);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, idx);
    instr_set_src(instr, 3, elsz);

    return true;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
static bool
dec_SQRDMLAH_VVVI_4x32_1x32(dcontext_t *dcontext, uint enc, instr_t *instr)
{
    ASSERT(BITS(enc, 10, 10) == 0b0 && BITS(enc, 15, 12) == 0b1101 &&
           BITS(enc, 31, 22) == 0b0110111110);

    opnd_t Vd = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 4, 0));
    opnd_t Vn = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 9, 5));
    opnd_t Vm = decode_vreg(VECTOR_ELEM_WIDTH_QUAD, BITS(enc, 20, 16));
    opnd_t elsz = opnd_create_immed_uint(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_1);
    uint hl = (BITS(enc, 11, 11) << 1) | BITS(enc, 21, 21);
    opnd_t idx = opnd_create_immed_uint(hl, OPSZ_3b);

    instr_set_opcode(instr, OP_sqrdmlah);
    instr_set_num_opnds(dcontext, instr, 1, 4);

    instr_set_dst(instr, 0, Vd);
    instr_set_src(instr, 0, Vn);
    instr_set_src(instr, 1, Vm);
    instr_set_src(instr, 2, idx);
    instr_set_src(instr, 3, elsz);

    return true;
}

/* MRSC: SQRDMLAH <V><d>, <V><n>, <Vm>.<Ts>[<index>] */
/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
const static decode_map decode_map__31_22__15_12__10[] = {
    { 0b011111110111010, dec_SQRDMLAH_VVVI_16 },
    { 0b011111111011010, dec_SQRDMLAH_VVVI_32 },
    { 0b001011110111010, dec_SQRDMLAH_VVVI_4x16_1x16 },
    { 0b011011110111010, dec_SQRDMLAH_VVVI_8x16_1x16 },
    { 0b001011111011010, dec_SQRDMLAH_VVVI_2x32_1x32 },
    { 0b011011111011010, dec_SQRDMLAH_VVVI_4x32_1x32 }
};

static bool
decode_v81(uint enc, dcontext_t *dc, byte *pc, instr_t *instr)
{
    int map_size;

    /* The decoder for each version of the architecture is a set of loops over
     * the decode_map structs defined for supported instructions. When a fixed
     * (non-operand) bit match is found from the 32 bit input encoding, a
     * decode function is called, constructing an instr_t object.
     *
     * This example is the lowest level (leaf) of the decode tree. The fully
     * generated decoder will start at the top level (root) working its way
     * down to the leaves by decoding successive fixed opcode fields of the
     * encoding spec: a chain of decode_map objects with pointers to lower
     * level decode loops and instruction decode functions.
     */
    {
        /* Extract bit values from input encoding for instructions with fixed
         * (non-operand) bits specified in MRS at:
         * 31        21    15   10
         * |         |     |    |
         * 01111110sz0Vm---100001Vn---Vd---
         * Note: the size field (22,23) is a fixed property of the instruction
         * variant, not an operand.
         */
        /* MRSC: SQRDMLAH <V><d>, <V><n>, <V><m> */
        /* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
        uint bits_15_10 = BITS(enc, 15, 10);
        uint bits_31_21 = BITS(enc, 31, 21);
        uint bits_15_10_len = (15 - 10) + 1;
        uint bitmap = (bits_31_21 << bits_15_10_len) | bits_15_10;
        map_size = sizeof(decode_map__31_21__15_10) / sizeof(decode_map);
        for (int m = 0; m < map_size; m++) {
            if (decode_map__31_21__15_10[m].enc_bits == bitmap)
                return decode_map__31_21__15_10[m].decode_fn(dc, enc, instr);
        }
    }
    {
        /* MRSC: SQRDMLAH <V><d>, <V><n>, <Vm>.<Ts>[<index>] */
        /* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
        uint bit_10 = BITS(enc, 10, 10);
        uint bits_15_12 = BITS(enc, 15, 12);
        uint bits_31_22 = BITS(enc, 31, 22);
        uint bit_10_len = 1;
        uint bits_15_12_len = (15 - 12) + 1;
        uint bitmap = (bits_31_22 << (bit_10_len + bits_15_12_len)) |
            (bits_15_12 << bit_10_len) | bit_10;
        map_size = sizeof(decode_map__31_22__15_12__10) / sizeof(decode_map);
        for (int m = 0; m < map_size; m++) {
            if (decode_map__31_22__15_12__10[m].enc_bits == bitmap)
                return decode_map__31_22__15_12__10[m].decode_fn(dc, enc, instr);
        }
    }
    return decoder_v82(enc, dc, pc, instr);
}
