/* **********************************************************
 * Copyright (c) 2016-2021 ARM Limited. All rights reserved.
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

#ifndef CODEC_H
#define CODEC_H 1

#include "decode_private.h"

#define ENCFAIL (uint)0xFFFFFFFF /* a value that is not a valid instruction */

typedef enum {
    BYTE_REG = 0,
    HALF_REG = 1,
    SINGLE_REG = 2,
    DOUBLE_REG = 3,
    QUAD_REG = 4,
    Z_REG = 5,
    NOT_A_REG = 255
} aarch64_reg_offset;

byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr);
uint
encode_common(byte *pc, instr_t *i, decode_info_t *di);

#define MASK(size) ((1ULL << (size)) - 1ULL)

#define BITS(_enc, bitmax, bitmin) \
    ((((uint32)(_enc)) >> (bitmin)) & (uint32)MASK((bitmax) - (bitmin) + 1))

#if !defined(DR_HOST_NOT_TARGET) && !defined(STANDALONE_DECODER)
#    define OPSZ_SVE_VL_BYTES opnd_size_from_bytes(proc_get_vector_length_bytes())
#    define OPSZ_SVE_PL_BYTES opnd_size_from_bytes(proc_get_vector_length_bytes() / 8)
#else
/* SVE vector length for off-line decoder set using -vl option with drdisas,
 * e.g.
 * $ drdisas -vl 256 e58057a1 85865e6b
 *  e58057a1   str    %z1 -> +0x05(%x29)[32byte]
 *  85865e6b   ldr    +0x37(%x19)[32byte] -> %z11
 * $
 */
#    define OPSZ_SVE_VL_BYTES opnd_size_from_bytes(dr_get_sve_vector_length() / 8)
#    define OPSZ_SVE_PL_BYTES opnd_size_from_bytes((dr_get_sve_vector_length() / 8) / 8)
#endif

#define RETURN_FALSE                                               \
    do {                                                           \
        CLIENT_ASSERT(false, "Unexpected state in AArch64 codec"); \
        return false;                                              \
    } while (0);

// Frustratingly vera++ fails if RETURN_FALSE is referenced inside this macro
#define IF_RETURN_FALSE(condition)                                 \
    if (condition) {                                               \
        CLIENT_ASSERT(false, "Unexpected state in AArch64 codec"); \
        return false;                                              \
    }

#define EXCLUDE_ELEMENT(elsz)                                                \
    do {                                                                     \
        if (!opnd_is_element_vector_reg(opnd))                               \
            return false;                                                    \
                                                                             \
        const aarch64_reg_offset size = get_vector_element_reg_offset(opnd); \
        if (size == NOT_A_REG || size == elsz)                               \
            return false;                                                    \
    } while (0);

#endif /* CODEC_H */
