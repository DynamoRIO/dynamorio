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
    NOT_A_REG = DR_REG_INVALID
} aarch64_reg_offset;

byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr);
uint
encode_common(byte *pc, instr_t *i, decode_info_t *di);

/* Types and macros used by new codec. */

/* Bit extraction macro used extensively by automatically generated decoder and
 * encoder functions.
 */
#define BITS(_enc, bitmax, bitmin)    \
    ((((uint32)(_enc)) >> (bitmin)) & \
     (uint32)((1ULL << ((bitmax) - (bitmin) + 1)) - 1ULL))

/* Decoding is based on a key/value mapping (decode_map) where the key
 * (enc_bits) is a unique set of up to 32 bits representing an instruction
 * which is decoded by a function (decode_fn).
 */
typedef bool(decode_func_ptr)(dcontext_t *dcontext, uint enc, instr_t *instr);

typedef struct dmap {
    uint32 enc_bits;
    decode_func_ptr *decode_fn;
} decode_map;

/* Encoding function call-and-check macro used extensively by automatically
 * generated encoder switch/case clauses.
 */
#define ENCODE_IF_MATCH(ENCODE_FUNCTION) \
    enc = ENCODE_FUNCTION(instr);        \
    if (enc != ENCFAIL)                  \
        return enc;

#endif /* CODEC_H */
