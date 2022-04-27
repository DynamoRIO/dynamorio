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
 * encoder functionality to help describe the mapping between the two for
 * auto-generation purposes.
 *
 * The format of the code is intended to be human readable and will include
 * auto-generated comments extracted from the MRS. The prefix 'MRSC:' is used
 * to distinguish these examples from manual comments in this file.
 */

/* Each instruction's decode and encode function name is built from literal and
 * type data from the MRS. As an example:
 * enc_       Encode function, (dec_ for decode function, see decode_v81.h).
 * SQRDMLAH   Instruction name.
 * VVV        Operands signature:
 *            R  General purpose register.
 *            I  Immediate.
 *            V  Vector register (size specification follows).
 * 16         Vector size and type, e.g. 16 bit scalar (halfword). For vector
 *            elements, <number of elements>x<element size>, e.g. 4x16.
 */
/* MRSC: SQRDMLAH <V><d>, <V><n>, <V><m> */
static uint
enc_SQRDMLAH_VVV_16(instr_t *instr)
{
    /* Fixed (non-operand) bits extracted from the MRS uniquely identifying
     * this instruction.
     */
    uint32 enc = 0x7e408400;

    /* Sanity check based on name of instruction extracted from MRS. */
    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    /* Encode operands based on number, type and size data, and bit positions
     * extracted from MRS.
     */
    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 2)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0;
    opnd_size_t half = OPSZ_2;
    if (!encode_vreg(&half, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&half, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&half, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;

    return enc |= (Vm << 16) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <V><d>, <V><n>, <V><m> */
static uint
enc_SQRDMLAH_VVV_32(instr_t *instr)
{
    uint32 enc = 0x7e808400;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 2)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0;
    opnd_size_t single = OPSZ_4;
    if (!encode_vreg(&single, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&single, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&single, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;

    return enc |= (Vm << 16) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
static uint
enc_SQRDMLAH_VVV_4x16(instr_t *instr)
{
    uint32 enc = 0x2e408400;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 3)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0;
    opnd_size_t double_ = OPSZ_8;
    if (!encode_vreg(&double_, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&double_, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&double_, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!get_el_hs_sz(&elsz, instr_get_src(instr, 2)))
        return ENCFAIL;
    if (elsz != VECTOR_ELEM_WIDTH_HALF)
        return ENCFAIL;

    return enc |= (elsz << 22) | (Vm << 16) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
static uint
enc_SQRDMLAH_VVV_8x16(instr_t *instr)
{
    uint32 enc = 0x6e408400;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 3)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0;
    opnd_size_t quad = OPSZ_16;
    if (!encode_vreg(&quad, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!get_el_hs_sz(&elsz, instr_get_src(instr, 2)))
        return ENCFAIL;
    if (elsz != VECTOR_ELEM_WIDTH_HALF)
        return ENCFAIL;

    return enc |= (elsz << 22) | (Vm << 16) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
static uint
enc_SQRDMLAH_VVV_2x32(instr_t *instr)
{
    uint32 enc = 0x2e808400;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 3)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0;
    opnd_size_t double_ = OPSZ_8;
    if (!encode_vreg(&double_, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&double_, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&double_, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!get_el_hs_sz(&elsz, instr_get_src(instr, 2)))
        return ENCFAIL;
    if (elsz != VECTOR_ELEM_WIDTH_SINGLE)
        return ENCFAIL;

    return enc |= (elsz << 22) | (Vm << 16) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
static uint
enc_SQRDMLAH_VVV_4x32(instr_t *instr)
{
    uint32 enc = 0x6e808400;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 3)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0;
    opnd_size_t quad = OPSZ_16;
    if (!encode_vreg(&quad, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!get_el_hs_sz(&elsz, instr_get_src(instr, 2)))
        return ENCFAIL;
    if (elsz != VECTOR_ELEM_WIDTH_SINGLE)
        return ENCFAIL;

    return enc |= (elsz << 22) | (Vm << 16) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <V><d>, <V><n>, <Vm>.<Ts>[<index>] */
static uint
enc_SQRDMLAH_VVVI_16(instr_t *instr)
{
    uint32 enc = 0x7f40d000;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 4)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0, idx = 0;
    opnd_size_t half = OPSZ_2, quad = OPSZ_16;
    if (!encode_vreg(&half, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&half, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!encode_opnd_vindex_H(0, 0, 0, instr_get_src(instr, 2), &idx))
        return ENCFAIL;
    if (!encode_opnd_h_sz(0, 0, 0, instr_get_src(instr, 3), &elsz))
        return ENCFAIL;

    return enc |=
        (elsz << 22) | (idx & 0x300000) | (Vm << 16) | (idx & 0x800) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <V><d>, <V><n>, <Vm>.<Ts>[<index>] */
static uint
enc_SQRDMLAH_VVVI_32(instr_t *instr)
{
    uint32 enc = 0x7f80d000;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 4)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0, idx = 0;
    opnd_size_t single = OPSZ_4, quad = OPSZ_16;
    if (!encode_vreg(&single, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&single, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!encode_opnd_vindex_SD(0, 0, 0, instr_get_src(instr, 2), &idx))
        return ENCFAIL;
    if (!encode_opnd_sd_sz(0, 0, 0, instr_get_src(instr, 3), &elsz))
        return ENCFAIL;

    return enc |=
        (elsz << 22) | (idx & 0x200000) | (Vm << 16) | (idx & 0x800) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
static uint
enc_SQRDMLAH_VVVI_4x16_1x16(instr_t *instr)
{
    uint32 enc = 0x2f40d000;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 4)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0, idx = 0;
    opnd_size_t double_ = OPSZ_8;
    if (!encode_vreg(&double_, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&double_, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&double_, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!encode_opnd_vindex_H(0, 0, 0, instr_get_src(instr, 2), &idx))
        return ENCFAIL;
    if (!encode_opnd_h_sz(0, 0, 0, instr_get_src(instr, 3), &elsz))
        return ENCFAIL;

    return enc |=
        (elsz << 22) | (idx & 0x300000) | (Vm << 16) | (idx & 0x800) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
static uint
enc_SQRDMLAH_VVVI_8x16_1x16(instr_t *instr)
{
    uint32 enc = 0x6f40d000;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 4)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0, idx = 0;
    opnd_size_t quad = OPSZ_16;
    if (!encode_vreg(&quad, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!encode_opnd_vindex_H(0, 0, 0, instr_get_src(instr, 2), &idx))
        return ENCFAIL;
    if (!encode_opnd_h_sz(0, 0, 0, instr_get_src(instr, 3), &elsz))
        return ENCFAIL;

    return enc |=
        (elsz << 22) | (idx & 0x300000) | (Vm << 16) | (idx & 0x800) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
static uint
enc_SQRDMLAH_VVVI_2x32_1x32(instr_t *instr)
{
    uint32 enc = 0x2f80d000;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 4)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0, idx = 0;
    opnd_size_t double_ = OPSZ_8;
    if (!encode_vreg(&double_, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&double_, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&double_, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!encode_opnd_vindex_SD(0, 0, 0, instr_get_src(instr, 2), &idx))
        return ENCFAIL;
    if (!encode_opnd_sd_sz(0, 0, 0, instr_get_src(instr, 3), &elsz))
        return ENCFAIL;

    return enc |=
        (elsz << 22) | (idx & 0x200000) | (Vm << 16) | (idx & 0x800) | (Vn << 5) | Vd;
}

/* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
static uint
enc_SQRDMLAH_VVVI_4x32_1x32(instr_t *instr)
{
    uint32 enc = 0x6f80d000;

    if (instr_get_opcode(instr) != OP_sqrdmlah)
        return ENCFAIL;

    if (instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 4)
        return ENCFAIL;

    uint Vd = 0, Vn = 0, Vm = 0, elsz = 0, idx = 0;
    opnd_size_t quad = OPSZ_16;
    if (!encode_vreg(&quad, &Vd, instr_get_dst(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vn, instr_get_src(instr, 0)))
        return ENCFAIL;
    if (!encode_vreg(&quad, &Vm, instr_get_src(instr, 1)))
        return ENCFAIL;
    if (!encode_opnd_vindex_SD(0, 0, 0, instr_get_src(instr, 2), &idx))
        return ENCFAIL;
    if (!encode_opnd_sd_sz(0, 0, 0, instr_get_src(instr, 3), &elsz))
        return ENCFAIL;

    return enc |=
        (elsz << 22) | (idx & 0x200000) | (Vm << 16) | (idx & 0x800) | (Vn << 5) | Vd;
}

static uint
encode_v81(byte *pc, instr_t *instr, decode_info_t *di)
{
    /* The encoder for each version of the architecture is a switch statement
     * which selects encoder function(s) for each instruction and its variants
     * based on the instruction name extracted from the MRS.
     */
    uint enc;
    (void)enc;
    switch (instr->opcode) {
    case OP_sqrdmlah:
        /* MRSC: SQRDMLAH <V><d>, <V><n>, <V><m> */
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVV_16);
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVV_32);
        /* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<T> */
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVV_4x16);
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVV_8x16);
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVV_2x32);
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVV_4x32);
        /* MRSC: SQRDMLAH <V><d>, <V><n>, <Vm>.<Ts>[<index>] */
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVVI_16);
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVVI_32);
        /* MRSC: SQRDMLAH <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>] */
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVVI_4x16_1x16);
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVVI_8x16_1x16);
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVVI_2x32_1x32);
        ENCODE_IF_MATCH(enc_SQRDMLAH_VVVI_4x32_1x32);
        break;
    }
    return encoder_v82(pc, instr, di);
}
