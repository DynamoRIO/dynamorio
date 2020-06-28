/* **********************************************************
 * Copyright (c) 2017-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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

/* AArch64 decoder and encoder functions.
 * This file is rather large and should perhaps be split up, but there are many
 * opportunities for inlining which could be lost if it were split into separate
 * translation units, and it is helpful to have the per-operand-type decode/encode
 * functions next to each other.
 */

#include "../globals.h"
#include "arch.h"
#include "decode.h"
#include "disassemble.h"
#include "instr.h"
#include "instr_create.h"

#include "codec.h"

/* Decode immediate argument of bitwise operations.
 * Returns zero if the encoding is invalid.
 */
static ptr_uint_t
decode_bitmask(uint enc)
{
    uint pos = enc >> 6 & 63;
    uint len = enc & 63;
    ptr_uint_t x;

    if (TEST(1U << 12, enc)) {
        if (len == 63)
            return 0;
        x = ((ptr_uint_t)1 << (len + 1)) - 1;
        return x >> pos | x << 1 << (63 - pos);
    } else {
        uint i, t = 32;

        while ((t & len) != 0)
            t >>= 1;
        if (t < 2)
            return 0;
        x = len & (t - 1);
        if (x == t - 1)
            return 0;
        x = ((ptr_uint_t)1 << (x + 1)) - 1;
        pos &= t - 1;
        x = x >> pos | x << (t - pos);
        for (i = 2; i < 64; i *= 2) {
            if (t <= i)
                x |= x << i;
        }
        return x;
    }
}

/* Encode immediate argument of bitwise operations.
 * Returns -1 if the value cannot be encoded.
 */
static int
encode_bitmask(ptr_uint_t x)
{
    int neg, rep, pos, len;

    neg = 0;
    if ((x & 1) != 0)
        neg = 1, x = ~x;
    if (x == 0)
        return -1;

    if (x >> 2 == (x & (((ptr_uint_t)1 << (64 - 2)) - 1)))
        rep = 2, x &= ((ptr_uint_t)1 << 2) - 1;
    else if (x >> 4 == (x & (((ptr_uint_t)1 << (64 - 4)) - 1)))
        rep = 4, x &= ((ptr_uint_t)1 << 4) - 1;
    else if (x >> 8 == (x & (((ptr_uint_t)1 << (64 - 8)) - 1)))
        rep = 8, x &= ((ptr_uint_t)1 << 8) - 1;
    else if (x >> 16 == (x & (((ptr_uint_t)1 << (64 - 16)) - 1)))
        rep = 16, x &= ((ptr_uint_t)1 << 16) - 1;
    else if (x >> 32 == (x & (((ptr_uint_t)1 << (64 - 32)) - 1)))
        rep = 32, x &= ((ptr_uint_t)1 << 32) - 1;
    else
        rep = 64;

    pos = 0;
    (x & (((ptr_uint_t)1 << 32) - 1)) != 0 ? 0 : (x >>= 32, pos += 32);
    (x & (((ptr_uint_t)1 << 16) - 1)) != 0 ? 0 : (x >>= 16, pos += 16);
    (x & (((ptr_uint_t)1 << 8) - 1)) != 0 ? 0 : (x >>= 8, pos += 8);
    (x & (((ptr_uint_t)1 << 4) - 1)) != 0 ? 0 : (x >>= 4, pos += 4);
    (x & (((ptr_uint_t)1 << 2) - 1)) != 0 ? 0 : (x >>= 2, pos += 2);
    (x & (((ptr_uint_t)1 << 1) - 1)) != 0 ? 0 : (x >>= 1, pos += 1);

    len = 0;
    (~x & (((ptr_uint_t)1 << 32) - 1)) != 0 ? 0 : (x >>= 32, len += 32);
    (~x & (((ptr_uint_t)1 << 16) - 1)) != 0 ? 0 : (x >>= 16, len += 16);
    (~x & (((ptr_uint_t)1 << 8) - 1)) != 0 ? 0 : (x >>= 8, len += 8);
    (~x & (((ptr_uint_t)1 << 4) - 1)) != 0 ? 0 : (x >>= 4, len += 4);
    (~x & (((ptr_uint_t)1 << 2) - 1)) != 0 ? 0 : (x >>= 2, len += 2);
    (~x & (((ptr_uint_t)1 << 1) - 1)) != 0 ? 0 : (x >>= 1, len += 1);

    if (x != 0)
        return -1;
    if (neg) {
        pos = (pos + len) & (rep - 1);
        len = rep - len;
    }
    return (0x1000 & rep << 6) | (((rep - 1) ^ 31) << 1 & 63) |
        ((rep - pos) & (rep - 1)) << 6 | (len - 1);
}

/* Extract signed integer from subfield of word. */
static inline ptr_int_t
extract_int(uint enc, int pos, int len)
{
    uint u = ((enc >> pos & (((uint)1 << (len - 1)) - 1)) -
              (enc >> pos & ((uint)1 << (len - 1))));
    return u << 1 < u ? -(ptr_int_t)~u - 1 : u;
}

/* Extract unsigned integer from subfield of word. */
static inline ptr_uint_t
extract_uint(uint enc, int pos, int len)
{
    return enc >> pos & (((uint)1 << len) - 1);
}

static inline bool
try_encode_int(OUT uint *bits, int len, int scale, ptr_int_t val)
{
    /* If any of lowest 'scale' bits are set, or 'val' is out of range, fail. */
    if (((ptr_uint_t)val & ((1U << scale) - 1)) != 0 ||
        val < -((ptr_int_t)1 << (len + scale - 1)) ||
        val >= (ptr_int_t)1 << (len + scale - 1))
        return false;
    *bits = (ptr_uint_t)val >> scale & ((1U << len) - 1);
    return true;
}

static inline bool
try_encode_imm(OUT uint *imm, int bits, opnd_t opnd)
{
    ptr_int_t value;
    if (!opnd_is_immed_int(opnd))
        return false;
    value = opnd_get_immed_int(opnd);
    if (!(0 <= value && value < (uint)1 << bits))
        return false;
    *imm = value;
    return true;
}

static inline bool
encode_pc_off(OUT uint *poff, int bits, byte *pc, instr_t *instr, opnd_t opnd,
              decode_info_t *di)
{
    ptr_uint_t off, range;
    ASSERT(0 < bits && bits <= 32);
    if (opnd.kind == PC_kind)
        off = opnd.value.pc - pc;
    else if (opnd.kind == INSTR_kind)
        off = (byte *)opnd_get_instr(opnd)->note - (byte *)instr->note;
    else
        return false;
    range = (ptr_uint_t)1 << bits;
    if (!TEST(~((range - 1) << 2), off + (range << 1))) {
        *poff = off >> 2 & (range - 1);
        return true;
    }
    /* If !di->check_reachable we still require correct alignment. */
    if (!di->check_reachable && ALIGNED(off, 4)) {
        *poff = 0;
        return true;
    }
    return false;
}

static inline opnd_t
decode_sysreg(uint imm15)
{
    reg_t sysreg;
    switch (imm15) {
    case 0x5a10: sysreg = DR_REG_NZCV; break;
    case 0x5a20: sysreg = DR_REG_FPCR; break;
    case 0x5a21: sysreg = DR_REG_FPSR; break;
    case 0x5e82: sysreg = DR_REG_TPIDR_EL0; break;
    default: return opnd_create_immed_uint(imm15, OPSZ_2);
    }
    return opnd_create_reg(sysreg);
}

static inline bool
encode_sysreg(OUT uint *imm15, opnd_t opnd)
{
    if (opnd_is_reg(opnd)) {
        switch (opnd_get_reg(opnd)) {
        case DR_REG_NZCV: *imm15 = 0x5a10; break;
        case DR_REG_FPCR: *imm15 = 0x5a20; break;
        case DR_REG_FPSR: *imm15 = 0x5a21; break;
        case DR_REG_TPIDR_EL0: *imm15 = 0x5e82; break;
        default: return false;
        }
        return true;
    }
    if (opnd_is_immed_int(opnd)) {
        uint imm;
        if (try_encode_imm(&imm, 15, opnd) && !opnd_is_reg(decode_sysreg(imm))) {
            *imm15 = imm;
            return true;
        }
        return false;
    }
    return false;
}

/* Decode integer register. Input 'n' is number from 0 to 31, where
 * 31 can mean stack pointer or zero register, depending on 'is_sp'.
 */
static inline reg_id_t
decode_reg(uint n, bool is_x, bool is_sp)
{
    return (n < 31 ? (is_x ? DR_REG_X0 : DR_REG_W0) + n
                   : is_sp ? (is_x ? DR_REG_XSP : DR_REG_WSP)
                           : (is_x ? DR_REG_XZR : DR_REG_WZR));
}

/* Encode integer register. */
static inline bool
encode_reg(OUT uint *num, OUT bool *is_x, reg_id_t reg, bool is_sp)
{
    if (DR_REG_X0 <= reg && reg <= DR_REG_X30) {
        *num = reg - DR_REG_X0;
        *is_x = true;
        return true;
    }
    if (DR_REG_W0 <= reg && reg <= DR_REG_W30) {
        *num = reg - DR_REG_W0;
        *is_x = false;
        return true;
    }
    if (is_sp && (reg == DR_REG_XSP || reg == DR_REG_WSP)) {
        *num = 31;
        *is_x = (reg == DR_REG_XSP);
        return true;
    }
    if (!is_sp && (reg == DR_REG_XZR || reg == DR_REG_WZR)) {
        *num = 31;
        *is_x = (reg == DR_REG_XZR);
        return true;
    }
    return false;
}

/* Decode SIMD/FP register. */
static inline opnd_t
decode_vreg(uint scale, uint n)
{
    reg_id_t reg = DR_REG_NULL;
    ASSERT(n < 32 && scale < 5);
    switch (scale) {
    case 0: reg = DR_REG_B0 + n; break;
    case 1: reg = DR_REG_H0 + n; break;
    case 2: reg = DR_REG_S0 + n; break;
    case 3: reg = DR_REG_D0 + n; break;
    case 4: reg = DR_REG_Q0 + n; break;
    }
    return opnd_create_reg(reg);
}

/* Encode SIMD/FP register. */
static inline bool
encode_vreg(INOUT opnd_size_t *x, OUT uint *r, opnd_t opnd)
{
    reg_id_t reg;
    opnd_size_t sz;
    uint n;
    if (!opnd_is_reg(opnd))
        return false;
    reg = opnd_get_reg(opnd);
    if ((uint)(reg - DR_REG_B0) < 32) {
        n = reg - DR_REG_B0;
        sz = OPSZ_1;
    } else if ((uint)(reg - DR_REG_H0) < 32) {
        n = reg - DR_REG_H0;
        sz = OPSZ_2;
    } else if ((uint)(reg - DR_REG_S0) < 32) {
        n = reg - DR_REG_S0;
        sz = OPSZ_4;
    } else if ((uint)(reg - DR_REG_D0) < 32) {
        n = reg - DR_REG_D0;
        sz = OPSZ_8;
    } else if ((uint)(reg - DR_REG_Q0) < 32) {
        n = reg - DR_REG_Q0;
        sz = OPSZ_16;
    } else
        return false;
    if (*x == OPSZ_NA)
        *x = sz;
    else if (*x != sz)
        return false;
    *r = n;
    return true;
}

static opnd_t
create_base_imm(uint enc, int disp, int bytes)
{
    /* The base register number comes from bits 5 to 9. It may be SP. */
    return opnd_create_base_disp(decode_reg(extract_uint(enc, 5, 5), true, true),
                                 DR_REG_NULL, 0, disp, opnd_size_from_bytes(bytes));
}

static bool
is_base_imm(opnd_t opnd, OUT uint *regnum)
{
    uint n;
    bool is_x;
    if (!opnd_is_base_disp(opnd) || opnd_get_index(opnd) != DR_REG_NULL ||
        !encode_reg(&n, &is_x, opnd_get_base(opnd), true) || !is_x)
        return false;
    *regnum = n;
    return true;
}

/* Used for mem7* operand types, which have a 7-bit offset and are used by
 * load/store (pair) instructions. Returns the scale (log base 2 of number
 * of bytes) of the memory argument, a function of bits 26, 30 and 31.
 */
static int
mem7_scale(uint enc)
{
    return 2 +
        (TEST(1U << 26, enc) ? extract_uint(enc, 30, 2) : extract_uint(enc, 31, 1));
}

/* Used for memlit operand type, used by load (literal). Returns the size
 * of the memory operand, a function of bits 26, 30 and 31.
 */
static opnd_size_t
memlit_size(uint enc)
{
    opnd_size_t size = OPSZ_0;
    switch (extract_uint(enc, 30, 2)) {
    case 0: size = OPSZ_4; break;
    case 1: size = OPSZ_8; break;
    case 2: size = TEST(1U << 26, enc) ? OPSZ_16 : OPSZ_4;
    }
    return size;
}

/* Returns the number of registers accessed by SIMD load structure and replicate,
 * a function of bits 13 and 21.
 */
static int
memvr_regcount(uint enc)
{
    return ((enc >> 13 & 1) << 1 | (enc >> 21 & 1)) + 1;
}

/* Used for memvs operand type, used by SIMD load/store single structure.
 * Returns the number of bytes read or written, which is a function of
 * bits 10, 11, 13, 14, 15 and 21.
 */
static int
memvs_size(uint enc)
{
    int scale = extract_uint(enc, 14, 2);
    /* Number of elements in structure, 1 to 4. */
    int elems = memvr_regcount(enc);
    int size = extract_uint(enc, 10, 2);
    if (scale == 2 && size == 1)
        scale = 3;
    return elems * (1 << scale);
}

/* Returns the number of registers accessed by SIMD load/store multiple structures,
 * a function of bits 12-15.
 */
static int
multistruct_regcount(uint enc)
{
    switch (extract_uint(enc, 12, 4)) {
    case 0: return 4;
    case 2: return 4;
    case 4: return 3;
    case 6: return 3;
    case 7: return 1;
    case 8: return 2;
    case 10: return 2;
    }
    ASSERT(false);
    return 0;
}

/*******************************************************************************
 * Pairs of functions for decoding and encoding a generalised type of operand.
 */

/* adr_page: used for adr, adrp */

static bool
decode_opnd_adr_page(int scale, uint enc, byte *pc, OUT opnd_t *opnd)
{
    uint bits = (enc >> 3 & 0x1ffffc) | (enc >> 29 & 3);
    byte *addr = ((byte *)((ptr_uint_t)pc >> scale << scale) +
                  extract_int(bits, 0, 21) * ((ptr_int_t)1 << scale));
    *opnd = opnd_create_rel_addr(addr, OPSZ_0);
    return true;
}

static bool
encode_opnd_adr_page(int scale, byte *pc, opnd_t opnd, OUT uint *enc_out, instr_t *instr,
                     decode_info_t *di)
{
    ptr_int_t offset;
    uint bits;
    if (opnd_is_rel_addr(opnd)) {
        offset = (ptr_int_t)opnd_get_addr(opnd) -
            (ptr_int_t)((ptr_uint_t)pc >> scale << scale);
    } else if (opnd_is_instr(opnd)) {
        offset = (ptr_int_t)((byte *)opnd_get_instr(opnd)->note - (byte *)instr->note);
    } else
        return false;

    if (try_encode_int(&bits, 21, scale, offset)) {
        *enc_out = (bits & 3) << 29 | (bits & 0x1ffffc) << 3;
        return true;
    }
    /* If !di->check_reachable we still require correct alignment. */
    if (!di->check_reachable && ALIGNED(offset, 1ULL << scale)) {
        *enc_out = 0;
        return true;
    }
    return false;
}

/* dq_plus: used for dq0, dq5, dq16, dq0p1, dq0p2, dq0p3 */

static inline bool
decode_opnd_dq_plus(int add, int rpos, int qpos, uint enc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_reg((TEST(1U << qpos, enc) ? DR_REG_Q0 : DR_REG_D0) +
                            (extract_uint(enc, rpos, rpos + 5) + add) % 32);
    return true;
}

static inline bool
encode_opnd_dq_plus(int add, int rpos, int qpos, opnd_t opnd, OUT uint *enc_out)
{
    uint num;
    bool q;
    if (!opnd_is_reg(opnd))
        return false;
    q = (uint)(opnd_get_reg(opnd) - DR_REG_Q0) < 32;
    num = opnd_get_reg(opnd) - (q ? DR_REG_Q0 : DR_REG_D0);
    if (num >= 32)
        return false;
    *enc_out = ((num - add) % 32) << rpos | (uint)q << qpos;
    return true;
}

/* index: used for opnd_index0, ..., opnd_index3 */

static bool
decode_opnd_index(int n, uint enc, OUT opnd_t *opnd)
{
    uint bits = (enc >> 30 & 1) << 3 | (enc >> 10 & 7);
    *opnd = opnd_create_immed_int(bits >> n, OPSZ_4b);
    return true;
}

static bool
encode_opnd_index(int n, opnd_t opnd, OUT uint *enc_out)
{
    ptr_int_t val;
    uint bits;
    if (!opnd_is_immed_int(opnd))
        return false;
    val = opnd_get_immed_int(opnd);
    if (val < 0 || val >= 16 >> n)
        return false;
    bits = val << n;
    *enc_out = (bits >> 3 & 1) << 30 | (bits & 7) << 10;
    return true;
}

/* int: used for almost every operand type that is an immediate integer */

static bool
decode_opnd_int(int pos, int len, bool signd, int scale, opnd_size_t size,
                dr_opnd_flags_t flags, uint enc, OUT opnd_t *opnd)
{
    ptr_int_t val = signd ? extract_int(enc, pos, len) : extract_uint(enc, pos, len);
    *opnd =
        opnd_add_flags(opnd_create_immed_int(val * ((ptr_int_t)1 << scale), size), flags);
    return true;
}

static bool
encode_opnd_int(int pos, int len, bool signd, int scale, dr_opnd_flags_t flags,
                opnd_t opnd, OUT uint *enc_out)
{
    ptr_uint_t val;
    if (!opnd_is_immed_int(opnd) || (opnd_get_flags(opnd) & flags) != flags)
        return false;
    val = opnd_get_immed_int(opnd);
    if ((val & (((ptr_uint_t)1 << scale) - 1)) != 0)
        return false;
    if ((val + (signd ? ((ptr_uint_t)1 << (len + scale - 1)) : 0)) >> (len + scale) != 0)
        return false;
    *enc_out = (val >> scale & (((ptr_uint_t)1 << (len - 1)) * 2 - 1)) << pos;
    return true;
}

/* imm_bf: used for bitfield immediate operands  */

static bool
decode_opnd_imm_bf(int pos, uint enc, OUT opnd_t *opnd)
{
    if (!TEST(1U << 31, enc) && extract_uint(enc, pos, 6) >= 32)
        return false;
    return decode_opnd_int(pos, 6, false, 0, OPSZ_6b, 0, enc, opnd);
}

static bool
encode_opnd_imm_bf(int pos, uint enc, opnd_t opnd, uint *enc_out)
{
    if (!TEST(1U << 31, enc) && extract_uint(enc, pos, 6) >= 32)
        return false;
    return encode_opnd_int(pos, 6, false, 0, 0, opnd, enc_out);
}

/* mem0_scale: used for mem0, mem0p */

static inline bool
decode_opnd_mem0_scale(int scale, uint enc, OUT opnd_t *opnd)
{
    *opnd = create_base_imm(enc, 0, 1 << scale);
    return true;
}

static inline bool
encode_opnd_mem0_scale(int scale, opnd_t opnd, OUT uint *enc_out)
{
    uint xn;
    if (!is_base_imm(opnd, &xn) ||
        opnd_get_size(opnd) != opnd_size_from_bytes(1 << scale) ||
        opnd_get_disp(opnd) != 0)
        return false;
    *enc_out = xn << 5;
    return true;
}

/* mem12_scale: used for mem12, mem12q, prf12 */

static inline bool
decode_opnd_mem12_scale(int scale, bool prfm, uint enc, OUT opnd_t *opnd)
{
    *opnd =
        create_base_imm(enc, extract_uint(enc, 10, 12) << scale, prfm ? 0 : 1 << scale);
    return true;
}

static inline bool
encode_opnd_mem12_scale(int scale, bool prfm, opnd_t opnd, OUT uint *enc_out)
{
    int disp;
    uint xn;
    if (!is_base_imm(opnd, &xn) ||
        opnd_get_size(opnd) != (prfm ? OPSZ_0 : opnd_size_from_bytes(1 << scale)))
        return false;
    disp = opnd_get_disp(opnd);
    if (disp < 0 || disp >> scale > 0xfff || disp >> scale << scale != disp)
        return false;
    *enc_out = xn << 5 | (uint)disp >> scale << 10;
    return true;
}

/* mem7_postindex: used for mem7, mem7post */

static inline bool
decode_opnd_mem7_postindex(bool post, uint enc, OUT opnd_t *opnd)
{
    int scale = mem7_scale(enc);
    *opnd = create_base_imm(enc, post ? 0 : extract_int(enc, 15, 7) * (1 << scale),
                            2 << scale);
    opnd->value.base_disp.pre_index = !post;
    return true;
}

static inline bool
encode_opnd_mem7_postindex(bool post, uint enc, opnd_t opnd, OUT uint *enc_out)
{
    int scale = mem7_scale(enc);
    int disp;
    uint xn;
    if (!is_base_imm(opnd, &xn) ||
        opnd_get_size(opnd) != opnd_size_from_bytes(2 << scale))
        return false;
    disp = opnd_get_disp(opnd);
    if (disp == 0 && opnd.value.base_disp.pre_index == post)
        return false;
    if (post ? disp != 0
             : ((uint)disp & ((1 << scale) - 1)) != 0 ||
                (uint)disp + (0x40 << scale) >= (0x80 << scale))
        return false;
    *enc_out = xn << 5 | ((uint)disp >> scale & 0x7f) << 15;
    return true;
}

/* mem9_bytes: used for mem9, mem9post, mem9q, mem9qpost, prf9 */

static inline bool
decode_opnd_mem9_bytes(int bytes, bool post, uint enc, OUT opnd_t *opnd)
{
    *opnd = create_base_imm(enc, post ? 0 : extract_int(enc, 12, 9), bytes);
    opnd->value.base_disp.pre_index = !post;
    return true;
}

static inline bool
encode_opnd_mem9_bytes(int bytes, bool post, opnd_t opnd, OUT uint *enc_out)
{
    int disp;
    uint xn;
    if (!is_base_imm(opnd, &xn) || opnd_get_size(opnd) != opnd_size_from_bytes(bytes))
        return false;
    disp = opnd_get_disp(opnd);
    if (disp == 0 && opnd.value.base_disp.pre_index == post)
        return false;
    if (post ? (disp != 0) : (disp < -256 || disp > 255))
        return false;
    *enc_out = xn << 5 | ((uint)disp & 0x1ff) << 12;
    return true;
}

/* memreg_size: used for memreg, memregq, prfreg */

static inline bool
decode_opnd_memreg_size(opnd_size_t size, uint enc, OUT opnd_t *opnd)
{
    if (!TEST(1U << 14, enc))
        return false;
    *opnd = opnd_create_base_disp_aarch64(decode_reg(enc >> 5 & 31, true, true),
                                          decode_reg(enc >> 16 & 31, true, false),
                                          enc >> 13 & 7, TEST(1U << 12, enc), 0, 0, size);
    return true;
}

static inline bool
encode_opnd_memreg_size(opnd_size_t size, opnd_t opnd, OUT uint *enc_out)
{
    uint rn, rm, option;
    bool xn, xm, scaled;
    if (!opnd_is_base_disp(opnd) || opnd_get_size(opnd) != size ||
        opnd_get_disp(opnd) != 0)
        return false;
    option = opnd_get_index_extend(opnd, &scaled, NULL);
    if (!TEST(2, option))
        return false;
    if (!encode_reg(&rn, &xn, opnd_get_base(opnd), true) || !xn ||
        !encode_reg(&rm, &xm, opnd_get_index(opnd), false) || !xm)
        return false;
    *enc_out = rn << 5 | rm << 16 | option << 13 | (uint)scaled << 12;
    return true;
}

/* q0p: used for q0p1, q0p2, q0p3 */

static bool
decode_opnd_q0p(int add, uint enc, OUT opnd_t *opnd)
{
    *opnd = decode_vreg(4, (extract_uint(enc, 0, 5) + add) % 32);
    return true;
}

static bool
encode_opnd_q0p(int add, opnd_t opnd, OUT uint *enc_out)
{
    opnd_size_t size = OPSZ_NA;
    uint r;
    if (!encode_vreg(&size, &r, opnd) || size != OPSZ_16)
        return false;
    *enc_out = (r - add) % 32;
    return true;
}

/* rn: used for many integer register operands where bit 31 specifies W or X */

static inline bool
decode_opnd_rn(bool is_sp, int pos, uint enc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_reg(
        decode_reg(extract_uint(enc, pos, 5), TEST(1U << 31, enc), is_sp));
    return true;
}

static inline bool
encode_opnd_rn(bool is_sp, int pos, opnd_t opnd, OUT uint *enc_out)
{
    uint num;
    bool is_x;
    if (!opnd_is_reg(opnd) || !encode_reg(&num, &is_x, opnd_get_reg(opnd), is_sp))
        return false;
    *enc_out = (uint)is_x << 31 | num << pos;
    return true;
}

/* vector_reg: used for many FP/SIMD register operands */

static bool
decode_opnd_vector_reg(int pos, int scale, uint enc, OUT opnd_t *opnd)
{
    *opnd = decode_vreg(scale, extract_uint(enc, pos, 5));
    return true;
}

static bool
encode_opnd_vector_reg(int pos, int scale, opnd_t opnd, OUT uint *enc_out)
{
    opnd_size_t size = OPSZ_NA;
    uint r;
    if (!encode_vreg(&size, &r, opnd) || size != opnd_size_from_bytes(1 << scale))
        return false;
    *enc_out = r << pos;
    return true;
}

/* vtn: used for vt0, ..., vt3 */

static bool
decode_opnd_vtn(int add, uint enc, OUT opnd_t *opnd)
{
    if (extract_uint(enc, 10, 2) == 3 && extract_uint(enc, 30, 1) == 0)
        return false;
    *opnd = opnd_create_reg((TEST(1U << 30, enc) ? DR_REG_Q0 : DR_REG_D0) +
                            ((extract_uint(enc, 0, 5) + add) % 32));
    return true;
}

static bool
encode_opnd_vtn(int add, uint enc, opnd_t opnd, OUT uint *enc_out)
{
    reg_t reg;
    uint num;
    bool q;
    if (!opnd_is_reg(opnd))
        return false;
    reg = opnd_get_reg(opnd);
    q = (uint)(reg - DR_REG_Q0) < 32;
    if (extract_uint(enc, 10, 2) == 3 && !q)
        return false;
    num = reg - (q ? DR_REG_Q0 : DR_REG_D0);
    if (num >= 32)
        return false;
    *enc_out = (num - add) % 32 | (uint)q << 30;
    return true;
}

/* wxn: used for many integer register operands with fixed size (W or X) */

static bool
decode_opnd_wxn(bool is_x, bool is_sp, int pos, uint enc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_reg(decode_reg(enc >> pos & 31, is_x, is_sp));
    return true;
}

static bool
encode_opnd_wxn(bool is_x, bool is_sp, int pos, opnd_t opnd, OUT uint *enc_out)
{
    reg_id_t reg;
    uint n;
    if (!opnd_is_reg(opnd))
        return false;
    reg = opnd_get_reg(opnd);
    n = reg - (is_x ? DR_REG_X0 : DR_REG_W0);
    if (n < 31) {
        *enc_out = n << pos;
        return true;
    }
    if (reg ==
        (is_sp ? (is_x ? DR_REG_XSP : DR_REG_WSP) : (is_x ? DR_REG_XZR : DR_REG_WZR))) {
        *enc_out = (uint)31 << pos;
        return true;
    }
    return false;
}

/* wxnp: used for CASP, even/odd register pairs */

static bool
decode_opnd_wxnp(bool is_x, int plus, int pos, uint enc, OUT opnd_t *opnd)
{
    if ((enc >> pos & 1) != 0)
        return false;
    *opnd = opnd_create_reg(decode_reg(((enc >> pos) + plus) & 31, is_x, false));
    return true;
}

static bool
encode_opnd_wxnp(bool is_x, int plus, int pos, opnd_t opnd, OUT uint *enc_out)
{
    reg_id_t reg;
    uint n;
    if (!opnd_is_reg(opnd))
        return false;
    reg = opnd_get_reg(opnd);
    n = reg - (is_x ? DR_REG_X0 : DR_REG_W0);
    if (n < 31 && (n - plus) % 2 == 0) {
        *enc_out = ((n - plus) & 31) << pos;
        return true;
    }
    if (reg == (is_x ? DR_REG_XZR : DR_REG_WZR) && ((uint)31 - plus) % 2 == 0) {
        *enc_out = (((uint)31 - plus) & 31) << pos;
        return true;
    }
    return false;
}

static inline reg_id_t
decode_float_reg(uint n, uint type, reg_id_t *reg)
{
    switch (type) {
    case 3:
        /* Half precision operands are only supported in Armv8.2+. */
        *reg = DR_REG_H0 + n;
        return true;
    case 0: *reg = DR_REG_S0 + n; return true;
    case 1: *reg = DR_REG_D0 + n; return true;
    default: return false;
    }
}

static inline bool
decode_opnd_float_reg(int pos, uint enc, OUT opnd_t *opnd)
{
    reg_id_t reg;
    if (!decode_float_reg(extract_uint(enc, pos, 5), extract_uint(enc, 22, 2), &reg))
        return false;
    *opnd = opnd_create_reg(reg);
    return true;
}

static inline bool
encode_opnd_float_reg(int pos, opnd_t opnd, OUT uint *enc_out)
{
    uint num;
    uint type;

    opnd_size_t size = OPSZ_NA;

    if (!encode_vreg(&size, &num, opnd))
        return false;

    switch (size) {
    case OPSZ_2:
        /* Half precision operands are only supported in Armv8.2+. */
        type = 3;
        break;
    case OPSZ_4: type = 0; break;
    case OPSZ_8: type = 1; break;
    default: return false;
    }

    *enc_out = type << 22 | num << pos;
    return true;
}

/* Used to encode a SVE predicate register (P register). */

static inline bool
encode_opnd_p(uint pos_start, uint max_reg_num, opnd_t opnd, OUT uint *enc_out)
{
    uint num;
    if (!opnd_is_reg(opnd))
        return false;
    num = opnd_get_reg(opnd) - DR_REG_P0;
    if (num > max_reg_num)
        return false;
    *enc_out = num << pos_start;
    return true;
}

/* Used to encode a SVE vector register (Z registers). */

static inline bool
encode_opnd_z(uint pos_start, opnd_t opnd, OUT uint *enc_out)
{
    uint num;
    if (!opnd_is_reg(opnd))
        return false;
    num = opnd_get_reg(opnd) - DR_REG_Z0;
    if (num >= 32)
        return false;
    *enc_out = num << pos_start;
    return true;
}

/*******************************************************************************
 * Pairs of functions for decoding and encoding each type of operand, as listed in
 * "codec.txt". Try to keep these short: perhaps a tail call to a function in the
 * previous section.
 */

/* impx30: implicit X30 operand, used by BLR */

static inline bool
decode_opnd_impx30(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_reg(DR_REG_X30);
    return true;
}

static inline bool
encode_opnd_impx30(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_reg(opnd) || opnd_get_reg(opnd) != DR_REG_X30)
        return false;
    *enc_out = 0;
    return true;
}

/* lsl: constant LSL for ADD/MOV, no encoding bits */

static inline bool
decode_opnd_lsl(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint t = DR_SHIFT_LSL;
    return decode_opnd_int(0, 2, false, 0, OPSZ_2b, DR_OPND_IS_SHIFT, t, opnd);
}

static inline bool
encode_opnd_lsl(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    uint t;
    if (!encode_opnd_int(0, 2, false, 0, DR_OPND_IS_SHIFT, opnd, &t) || t != DR_SHIFT_LSL)
        return false;
    *enc_out = 0;
    return true;
}

/* h_sz: Operand size for half precision encoding of floating point vector
 * instructions. We need to convert the generic size operand to the right
 * encoding bits. It only supports ISZ_HALF.
 */
static inline bool
decode_opnd_h_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_HALF, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_h_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_HALF)
        return true;
    return false;
}

/* nzcv: flag bit specifier for conditional compare */

static inline bool
decode_opnd_nzcv(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(0, 4, false, 0, OPSZ_4b, 0, enc, opnd);
}

static inline bool
encode_opnd_nzcv(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(0, 4, false, 0, 0, opnd, enc_out);
}

/* w0: W register or WZR at bit position 0 */

static inline bool
decode_opnd_w0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxn(false, false, 0, enc, opnd);
}

static inline bool
encode_opnd_w0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxn(false, false, 0, opnd, enc_out);
}

/* w0p0: even-numbered W register or WZR at bit position 0 */

static inline bool
decode_opnd_w0p0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxnp(false, 0, 0, enc, opnd);
}

static inline bool
encode_opnd_w0p0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxnp(false, 0, 0, opnd, enc_out);
}

/* w0p1: even-numbered W register or WZR at bit position 0, add 1 */

static inline bool
decode_opnd_w0p1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxnp(false, 1, 0, enc, opnd);
}

static inline bool
encode_opnd_w0p1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxnp(false, 1, 0, opnd, enc_out);
}

/* x0: X register or XZR at bit position 0 */

static inline bool
decode_opnd_x0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxn(true, false, 0, enc, opnd);
}

static inline bool
encode_opnd_x0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxn(true, false, 0, opnd, enc_out);
}

/* x0p0: even-numbered X register or XZR at bit position 0 */

static inline bool
decode_opnd_x0p0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxnp(true, 0, 0, enc, opnd);
}

static inline bool
encode_opnd_x0p0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxnp(true, 0, 0, opnd, enc_out);
}

/* x0p1: even-numbered X register or XZR at bit position 0, add 1 */

static inline bool
decode_opnd_x0p1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxnp(true, 1, 0, enc, opnd);
}

static inline bool
encode_opnd_x0p1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxnp(true, 1, 0, opnd, enc_out);
}

/* b0: B register at bit position 0 */

static inline bool
decode_opnd_b0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(0, 0, enc, opnd);
}

static inline bool
encode_opnd_b0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(0, 0, opnd, enc_out);
}

/* h0: H register at bit position 0 */

static inline bool
decode_opnd_h0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(0, 1, enc, opnd);
}

static inline bool
encode_opnd_h0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(0, 1, opnd, enc_out);
}

/* s0: S register at bit position 0 */

static inline bool
decode_opnd_s0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(0, 2, enc, opnd);
}

static inline bool
encode_opnd_s0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(0, 2, opnd, enc_out);
}

/* d0: D register at bit position 0 */

static inline bool
decode_opnd_d0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(0, 3, enc, opnd);
}

static inline bool
encode_opnd_d0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(0, 3, opnd, enc_out);
}

/* q0: Q register at bit position 0 */

static inline bool
decode_opnd_q0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(0, 4, enc, opnd);
}

static inline bool
encode_opnd_q0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(0, 4, opnd, enc_out);
}

/* z0: Z register at bit position 0. */

static inline bool
decode_opnd_z0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_reg(DR_REG_Z0 + extract_uint(enc, 0, 5));
    return true;
}

static inline bool
encode_opnd_z0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_z(0, opnd, enc_out);
}

/* q0p1: as q0 but add 1 mod 32 to reg number */

static inline bool
decode_opnd_q0p1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_q0p(1, enc, opnd);
}

static inline bool
encode_opnd_q0p1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_q0p(1, opnd, enc_out);
}

/* q0p2: as q0 but add 2 mod 32 to reg number */

static inline bool
decode_opnd_q0p2(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_q0p(2, enc, opnd);
}

static inline bool
encode_opnd_q0p2(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_q0p(2, opnd, enc_out);
}

/* q0p3: as q0 but add 3 mod 32 to reg number */

static inline bool
decode_opnd_q0p3(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_q0p(3, enc, opnd);
}

static inline bool
encode_opnd_q0p3(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_q0p(3, opnd, enc_out);
}

/* prfop: prefetch operation, such as PLDL1KEEP */

static inline bool
decode_opnd_prfop(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(0, 5, false, 0, OPSZ_5b, 0, enc, opnd);
}

static inline bool
encode_opnd_prfop(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(0, 5, false, 0, 0, opnd, enc_out);
}

/* w5: W register or WZR at bit position 5 */

static inline bool
decode_opnd_w5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxn(false, false, 5, enc, opnd);
}

static inline bool
encode_opnd_w5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxn(false, false, 5, opnd, enc_out);
}

/* x5: X register or XZR at position 5 */

static inline bool
decode_opnd_x5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxn(true, false, 5, enc, opnd);
}

static inline bool
encode_opnd_x5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxn(true, false, 5, opnd, enc_out);
}

/* x5: X register or XSP at position 5 */

static inline bool
decode_opnd_x5sp(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxn(true, true, 5, enc, opnd);
}

static inline bool
encode_opnd_x5sp(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxn(true, true, 5, opnd, enc_out);
}

/* h5: H register at bit position 5 */

static inline bool
decode_opnd_h5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(5, 1, enc, opnd);
}

static inline bool
encode_opnd_h5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(5, 1, opnd, enc_out);
}

/* s5: S register at bit position 5 */

static inline bool
decode_opnd_s5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(5, 2, enc, opnd);
}

static inline bool
encode_opnd_s5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(5, 2, opnd, enc_out);
}

/* d5: D register at bit position 5 */

static inline bool
decode_opnd_d5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(5, 3, enc, opnd);
}

static inline bool
encode_opnd_d5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(5, 3, opnd, enc_out);
}

/* q5: Q register at bit position 5 */

static inline bool
decode_opnd_q5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(5, 4, enc, opnd);
}

static inline bool
encode_opnd_q5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(5, 4, opnd, enc_out);
}

/* z5: Z register at bit position 5. */

static inline bool
decode_opnd_z5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_reg(DR_REG_Z0 + extract_uint(enc, 5, 5));
    return true;
}

static inline bool
encode_opnd_z5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_z(5, opnd, enc_out);
}

/* mem9qpost: post-indexed mem9q, so offset is zero */

static inline bool
decode_opnd_mem9qpost(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem9_bytes(16, true, enc, opnd);
}

static inline bool
encode_opnd_mem9qpost(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem9_bytes(16, true, opnd, enc_out);
}

/* vmsz: B/H/S/D for load/store multiple structures */

static inline bool
decode_opnd_vmsz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(10, 2, false, 0, OPSZ_2b, 0, enc, opnd);
}

static inline bool
encode_opnd_vmsz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(10, 2, false, 0, 0, opnd, enc_out);
}

/* imm4: immediate operand for some system instructions */

static inline bool
decode_opnd_imm4(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(8, 4, false, 0, OPSZ_4b, 0, enc, opnd);
}

static inline bool
encode_opnd_imm4(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(8, 4, false, 0, 0, opnd, enc_out);
}

/* extam: extend amount, a left shift from 0 to 4 */

static inline bool
decode_opnd_extam(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    if (extract_uint(enc, 10, 3) > 4) /* shift amount must be <= 4 */
        return false;
    return decode_opnd_int(10, 3, false, 0, OPSZ_3b, 0, enc, opnd);
}

static inline bool
encode_opnd_extam(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    uint t;
    if (!encode_opnd_int(10, 3, false, 0, 0, opnd, &t) ||
        extract_uint(t, 10, 3) > 4) /* shift amount must be <= 4 */
        return false;
    *enc_out = t;
    return true;
}

/* p10_low: P register at bit position 10; P0-P7 */

static inline bool
decode_opnd_p10_low(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_reg(DR_REG_P0 + extract_uint(enc, 10, 3));
    return true;
}

static inline bool
encode_opnd_p10_low(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_p(10, 7, opnd, enc_out);
}

/* ign10: ignored register field at bit position 10 in load/store exclusive */

static inline bool
decode_opnd_ign10(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(10, 5, false, 0, OPSZ_5b, 0, enc, opnd);
}

static inline bool
encode_opnd_ign10(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(10, 5, false, 0, 0, opnd, enc_out);
}

/* w10: W register or WZR at bit position 10 */

static inline bool
decode_opnd_w10(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxn(false, false, 10, enc, opnd);
}

static inline bool
encode_opnd_w10(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxn(false, false, 10, opnd, enc_out);
}

/* x10: X register or XZR at bit position 10 */

static inline bool
decode_opnd_x10(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxn(true, false, 10, enc, opnd);
}

static inline bool
encode_opnd_x10(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxn(true, false, 10, opnd, enc_out);
}

/* s10: S register at bit position 10 */

static inline bool
decode_opnd_s10(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(10, 2, enc, opnd);
}

static inline bool
encode_opnd_s10(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(10, 2, opnd, enc_out);
}

/* d10: D register at bit position 10 */

static inline bool
decode_opnd_d10(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(10, 3, enc, opnd);
}

static inline bool
encode_opnd_d10(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(10, 3, opnd, enc_out);
}

/* q10: Q register at bit position 10 */

static inline bool
decode_opnd_q10(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(10, 4, enc, opnd);
}

static inline bool
encode_opnd_q10(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(10, 4, opnd, enc_out);
}

/* ext: extend type, dr_extend_type_t */

static inline bool
decode_opnd_ext(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(13, 3, false, 0, OPSZ_3b, DR_OPND_IS_EXTEND, enc, opnd);
}

static inline bool
encode_opnd_ext(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(13, 3, false, 0, DR_OPND_IS_EXTEND, opnd, enc_out);
}

/* cond: condition operand for conditional compare */

static inline bool
decode_opnd_cond(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(12, 4, false, 0, OPSZ_4b, DR_OPND_IS_CONDITION, enc, opnd);
}

static inline bool
encode_opnd_cond(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(12, 4, false, 0, 0, opnd, enc_out);
}

/* sysops: immediate operand for SYS instruction */

static inline bool
decode_opnd_sysops(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(5, 14, false, 0, OPSZ_2, 0, enc, opnd);
}

static inline bool
encode_opnd_sysops(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(5, 14, false, 0, 0, opnd, enc_out);
}

/* sysreg: system register, operand of MRS/MSR */

static inline bool
decode_opnd_sysreg(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = decode_sysreg(extract_uint(enc, 5, 15));
    return true;
}

static inline bool
encode_opnd_sysreg(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    uint t;
    if (!encode_sysreg(&t, opnd))
        return false;
    *enc_out = t << 5;
    return true;
}

/* ign16: ignored register field at bit position 16 in load/store exclusive */

static inline bool
decode_opnd_ign16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(16, 5, false, 0, OPSZ_5b, 0, enc, opnd);
}

static inline bool
encode_opnd_ign16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(16, 5, false, 0, 0, opnd, enc_out);
}

/* imm5: immediate operand for conditional compare (immediate) */

static inline bool
decode_opnd_imm5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(16, 5, false, 0, OPSZ_6b, 0, enc, opnd);
}

static inline bool
encode_opnd_imm5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(16, 5, false, 0, 0, opnd, enc_out);
}

/* w16: W register or WZR at bit position 16 */

static inline bool
decode_opnd_w16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxn(false, false, 16, enc, opnd);
}

static inline bool
encode_opnd_w16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxn(false, false, 16, opnd, enc_out);
}

/* w16p0: even-numbered W register or WZR at bit position 16 */

static inline bool
decode_opnd_w16p0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxnp(false, 0, 16, enc, opnd);
}

static inline bool
encode_opnd_w16p0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxnp(false, 0, 16, opnd, enc_out);
}

/* w16p1: even-numbered W register or WZR at bit position 16, add 1 */

static inline bool
decode_opnd_w16p1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxnp(false, 1, 16, enc, opnd);
}

static inline bool
encode_opnd_w16p1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxnp(false, 1, 16, opnd, enc_out);
}

/* x16: X register or XZR at bit position 16 */

static inline bool
decode_opnd_x16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxn(true, false, 16, enc, opnd);
}

static inline bool
encode_opnd_x16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxn(true, false, 16, opnd, enc_out);
}

/* x16p0: even-numbered X register or XZR at bit position 16 */

static inline bool
decode_opnd_x16p0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxnp(true, 0, 16, enc, opnd);
}

static inline bool
encode_opnd_x16p0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxnp(true, 0, 16, opnd, enc_out);
}

/* x16p1: even-numbered X register or XZR at bit position 16, add 1 */

static inline bool
decode_opnd_x16p1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_wxnp(true, 1, 16, enc, opnd);
}

static inline bool
encode_opnd_x16p1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_wxnp(true, 1, 16, opnd, enc_out);
}

/* d16: D register at bit position 16 */

static inline bool
decode_opnd_d16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(16, 3, enc, opnd);
}

static inline bool
encode_opnd_d16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(16, 3, opnd, enc_out);
}

/* q16: Q register at bit position 16 */

static inline bool
decode_opnd_q16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(16, 4, enc, opnd);
}

static inline bool
encode_opnd_q16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(16, 4, opnd, enc_out);
}

/* z16: Z register at bit position 16. */

static inline bool
decode_opnd_z16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_reg(DR_REG_Z0 + extract_uint(enc, 16, 5));
    return true;
}

static inline bool
encode_opnd_z16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_z(16, opnd, enc_out);
}

/* mem9off: just the 9-bit offset from mem9 */

static inline bool
decode_opnd_mem9off(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(12, 9, true, 0, OPSZ_PTR, 0, enc, opnd);
}

static inline bool
encode_opnd_mem9off(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(12, 9, true, 0, 0, opnd, enc_out);
}

/* mem9q: memory operand with 9-bit offset; size is 16 bytes */

static inline bool
decode_opnd_mem9q(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem9_bytes(16, false, enc, opnd);
}

static inline bool
encode_opnd_mem9q(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem9_bytes(16, false, opnd, enc_out);
}

/* prf9: prefetch variant of mem9 */

static inline bool
decode_opnd_prf9(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem9_bytes(0, false, enc, opnd);
}

static inline bool
encode_opnd_prf9(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem9_bytes(0, false, opnd, enc_out);
}

/* memreqq: memory operand with register offset; size is 16 bytes */

static inline bool
decode_opnd_memregq(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_memreg_size(OPSZ_16, enc, opnd);
}

static inline bool
encode_opnd_memregq(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_memreg_size(OPSZ_16, opnd, enc_out);
}

/* prfreg: prefetch variant of memreg */

static inline bool
decode_opnd_prfreg(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_memreg_size(OPSZ_0, enc, opnd);
}

static inline bool
encode_opnd_prfreg(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_memreg_size(OPSZ_0, opnd, enc_out);
}

/* imm16: 16-bit immediate operand of MOVK/MOVN/MOVZ/SVC */

static inline bool
decode_opnd_imm16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(5, 16, false, 0, OPSZ_12b, 0, enc, opnd);
}

static inline bool
encode_opnd_imm16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(5, 16, false, 0, 0, opnd, enc_out);
}

/* memvr: memory operand for SIMD load structure and replicate */

static inline bool
decode_opnd_memvr(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int bytes = memvr_regcount(enc) << extract_uint(enc, 10, 2);
    *opnd = create_base_imm(enc, 0, bytes);
    return true;
}

static inline bool
encode_opnd_memvr(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    int regcount;
    uint bytes, rn;
    if (!is_base_imm(opnd, &rn) || opnd_get_disp(opnd) != 0)
        return false;
    bytes = opnd_size_in_bytes(opnd_get_size(opnd));
    regcount = memvr_regcount(enc);
    if (bytes % regcount != 0)
        return false;
    bytes /= regcount;
    if (bytes < 1 || bytes > 8 || (bytes & (bytes - 1)) != 0 ||
        opnd_size_from_bytes(bytes * regcount) != opnd_get_size(opnd))
        return false;
    *enc_out = (rn << 5 | (bytes == 1 ? 0 : bytes == 2 ? 1 : bytes == 4 ? 2 : 3) << 10);
    return true;
}

/* memvs: memory operand for SIMD load/store single structure */

static inline bool
decode_opnd_memvs(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int bytes = memvs_size(enc);
    *opnd = create_base_imm(enc, 0, bytes);
    return true;
}

static inline bool
encode_opnd_memvs(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    uint rn;
    if (!is_base_imm(opnd, &rn) || opnd_get_disp(opnd) != 0)
        return false;
    if (opnd_get_size(opnd) != opnd_size_from_bytes(memvs_size(enc)))
        return false;
    *enc_out = rn << 5;
    return true;
}

/* x16immvr: immediate operand for SIMD load structure and replicate (post-indexed) */

static inline bool
decode_opnd_x16immvr(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int num = extract_uint(enc, 16, 5);
    if (num < 31)
        *opnd = opnd_create_reg(DR_REG_X0 + num);
    else {
        int bytes = memvr_regcount(enc) << extract_uint(enc, 10, 2);
        *opnd = opnd_create_immed_int(bytes, OPSZ_1);
    }
    return true;
}

static inline bool
encode_opnd_x16immvr(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (opnd_is_reg(opnd)) {
        uint num = opnd_get_reg(opnd) - DR_REG_X0;
        if (num == 31)
            return false;
        *enc_out = num << 16;
        return true;
    } else if (opnd_is_immed_int(opnd)) {
        ptr_int_t bytes = opnd_get_immed_int(opnd);
        if (bytes != memvr_regcount(enc) << extract_uint(enc, 10, 2))
            return false;
        *enc_out = 31U << 16;
        return true;
    }
    return false;
}

/* x16immvs: immediate operand for SIMD load/store single structure (post-indexed) */

static inline bool
decode_opnd_x16immvs(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int num = extract_uint(enc, 16, 5);
    if (num < 31)
        *opnd = opnd_create_reg(DR_REG_X0 + num);
    else {
        int bytes = memvs_size(enc);
        *opnd = opnd_create_immed_int(bytes, OPSZ_1);
    }
    return true;
}

static inline bool
encode_opnd_x16immvs(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (opnd_is_reg(opnd)) {
        uint num = opnd_get_reg(opnd) - DR_REG_X0;
        if (num == 31)
            return false;
        *enc_out = num << 16;
        return true;
    } else if (opnd_is_immed_int(opnd)) {
        ptr_int_t bytes = opnd_get_immed_int(opnd);
        if (bytes != memvs_size(enc))
            return false;
        *enc_out = 31U << 16;
        return true;
    }
    return false;
}

/* vindex_H: Index for vector with half elements (0-7). */

static inline bool
decode_opnd_vindex_H(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint bits = (enc >> 11 & 1) << 2 | (enc >> 21 & 1) << 1 | (enc >> 20 & 1);
    *opnd = opnd_create_immed_int(bits, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_vindex_H(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    ptr_int_t val;
    if (!opnd_is_immed_int(opnd))
        return false;
    val = opnd_get_immed_int(opnd);
    if (val < 0 || val >= 8)
        return false;
    *enc_out = (val >> 2 & 1) << 11 | (val >> 1 & 1) << 21 | (val & 1) << 20;
    return true;
}

/* imm12: 12-bit immediate operand of ADD/SUB */

static inline bool
decode_opnd_imm12(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(10, 12, false, 0, OPSZ_12b, 0, enc, opnd);
}

static inline bool
encode_opnd_imm12(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(10, 12, false, 0, 0, opnd, enc_out);
}

/* mem12q: memory operand with 12-bit offset; size is 16 bytes */

static inline bool
decode_opnd_mem12q(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem12_scale(4, false, enc, opnd);
}

static inline bool
encode_opnd_mem12q(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem12_scale(4, false, opnd, enc_out);
}

/* prf12: prefetch variant of mem12 */

static inline bool
decode_opnd_prf12(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem12_scale(3, true, enc, opnd);
}

static inline bool
encode_opnd_prf12(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem12_scale(3, true, opnd, enc_out);
}

/* vindex_SD: Index for vector with single or double elements. */

static inline bool
decode_opnd_vindex_SD(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint bits;
    if ((enc >> 22 & 1) == 0) {
        bits = (enc >> 11 & 1) << 1 | (enc >> 21 & 1);
    } else {
        if ((enc >> 21 & 1) != 0) {
            return false;
        }
        bits = enc >> 11 & 1;
    }
    *opnd = opnd_create_immed_int(bits, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_vindex_SD(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    ptr_int_t val;
    if (!opnd_is_immed_int(opnd))
        return false;
    val = opnd_get_immed_int(opnd);
    if ((enc >> 22 & 1) == 0) {
        if (val < 0 || val >= 4)
            return false;
        *enc_out = (val & 1) << 21 | (val >> 1 & 1) << 11;
    } else {
        if (val < 0 || val >= 2)
            return false;
        *enc_out = (val & 1) << 11;
    }
    return true;
}

/* imm12sh: shift amount for 12-bit immediate of ADD/SUB, 0 or 16 */

static inline bool
decode_opnd_imm12sh(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(22, 1, false, 4, OPSZ_5b, 0, enc, opnd);
}

static inline bool
encode_opnd_imm12sh(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(22, 1, false, 4, 0, opnd, enc_out);
}

/* sd_sz: Operand size for single and double precision encoding of floating point
 * vector instructions. We need to convert the generic size operand to the right
 * encoding bits. It only supports VECTOR_ELEM_WIDTH_SINGLE and VECTOR_ELEM_WIDTH_DOUBLE.
 */
static inline bool
decode_opnd_sd_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    if (((enc >> 22) & 1) == 0) {
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_2b);
        return true;
    }
    if (((enc >> 22) & 1) == 1) {
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_DOUBLE, OPSZ_2b);
        return true;
    }
    return false;
}

static inline bool
encode_opnd_sd_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_SINGLE) {
        *enc_out = 0;
        return true;
    }
    if (opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_DOUBLE) {
        *enc_out = 1 << 22;
        return true;
    }
    return false;
}

/* b_sz: Vector element width for SIMD instructions. */

static inline bool
decode_opnd_b_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint bits = enc >> 22 & 3;
    if (bits != 0)
        return false;
    *opnd = opnd_create_immed_int(bits, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_b_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    ptr_int_t val = opnd_get_immed_int(opnd);
    if (val != 0)
        return false;
    *enc_out = val << 22;
    return true;
}

/* hs_sz: Vector element width for SIMD instructions. */

static inline bool
decode_opnd_hs_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint bits = enc >> 22 & 3;
    if (bits != 1 && bits != 2)
        return false;
    *opnd = opnd_create_immed_int(bits, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_hs_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    ptr_int_t val = opnd_get_immed_int(opnd);
    if (val < 1 || val > 2)
        return false;
    *enc_out = val << 22;
    return true;
}

/* bhs_sz: Vector element width for SIMD instructions. */

static inline bool
decode_opnd_bhs_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint bits = enc >> 22 & 3;
    if (bits != 0 && bits != 1 && bits != 2)
        return false;
    *opnd = opnd_create_immed_int(bits, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_bhs_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    ptr_int_t val = opnd_get_immed_int(opnd);
    if (val < 0 || val > 2)
        return false;
    *enc_out = val << 22;
    return true;
}

/* bhsd_sz: Vector element width for SIMD instructions. */

static inline bool
decode_opnd_bhsd_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint bits = enc >> 22 & 3;
    *opnd = opnd_create_immed_int(bits, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_bhsd_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    ptr_int_t val = opnd_get_immed_int(opnd);
    if (val < 0 || val > 3)
        return false;
    *enc_out = val << 22;
    return true;
}

/* bd_sz: Vector element width for SIMD instructions. */

static inline bool
decode_opnd_bd_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint bits = enc >> 22 & 3;
    if (bits != 0 && bits != 3)
        return false;
    *opnd = opnd_create_immed_int(bits, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_bd_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    ptr_int_t val = opnd_get_immed_int(opnd);
    if (val != 0 && val != 3)
        return false;
    *enc_out = val << 22;
    return true;
}

/* shift3: shift type for ADD/SUB: LSL, LSR or ASR */

static inline bool
decode_opnd_shift3(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    if (extract_uint(enc, 22, 2) == 3)
        return false;
    return decode_opnd_int(22, 2, false, 0, OPSZ_3b, DR_OPND_IS_SHIFT, enc, opnd);
}

static inline bool
encode_opnd_shift3(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    uint t;
    if (!encode_opnd_int(22, 2, false, 0, DR_OPND_IS_SHIFT, opnd, &t) ||
        extract_uint(t, 22, 2) == 3)
        return false;
    *enc_out = t;
    return true;
}

/* shift4: shift type for logical operation: LSL, LSR, ASR or ROR */

static inline bool
decode_opnd_shift4(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(22, 2, false, 0, OPSZ_3b, DR_OPND_IS_SHIFT, enc, opnd);
}

static inline bool
encode_opnd_shift4(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(22, 2, false, 0, DR_OPND_IS_SHIFT, opnd, enc_out);
}

static inline bool
decode_opnd_float_reg0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_float_reg(0, enc, opnd);
}

static inline bool
encode_opnd_float_reg0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_float_reg(0, opnd, enc_out);
}

static inline bool
decode_opnd_float_reg5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_float_reg(5, enc, opnd);
}

static inline bool
encode_opnd_float_reg5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_float_reg(5, opnd, enc_out);
}

static inline bool
decode_opnd_float_reg10(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_float_reg(10, enc, opnd);
}

static inline bool
encode_opnd_float_reg10(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_float_reg(10, opnd, enc_out);
}

static inline bool
decode_opnd_float_reg16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_float_reg(16, enc, opnd);
}

static inline bool
encode_opnd_float_reg16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_float_reg(16, opnd, enc_out);
}

/* mem0p: as mem0, but a pair of registers, so double size */

static inline bool
decode_opnd_mem0p(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem0_scale(extract_uint(enc, 30, 1) + 3, enc, opnd);
}

static inline bool
encode_opnd_mem0p(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem0_scale(extract_uint(enc, 30, 1) + 3, opnd, enc_out);
}

/* x16imm: immediate operand for SIMD load/store multiple structures (post-indexed) */

static inline bool
decode_opnd_x16imm(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int num = extract_uint(enc, 16, 5);
    if (num < 31)
        *opnd = opnd_create_reg(DR_REG_X0 + num);
    else {
        int bytes = (8 << extract_uint(enc, 30, 1)) * multistruct_regcount(enc);
        *opnd = opnd_create_immed_int(bytes, OPSZ_1);
    }
    return true;
}

static inline bool
encode_opnd_x16imm(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (opnd_is_reg(opnd)) {
        uint num = opnd_get_reg(opnd) - DR_REG_X0;
        if (num == 31)
            return false;
        *enc_out = num << 16;
        return true;
    } else if (opnd_is_immed_int(opnd)) {
        ptr_int_t bytes = opnd_get_immed_int(opnd);
        if (bytes != (8 << extract_uint(enc, 30, 1)) * multistruct_regcount(enc))
            return false;
        *enc_out = 31U << 16;
        return true;
    }
    return false;
}

/* index3: index of D subreg in Q register: 0-1 */

static inline bool
decode_opnd_index3(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_index(3, enc, opnd);
}

static inline bool
encode_opnd_index3(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_index(3, opnd, enc_out);
}

/* dq0: D/Q register at bit position 0; bit 30 selects Q reg */

static inline bool
decode_opnd_dq0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_dq_plus(0, 0, 30, enc, opnd);
}

static inline bool
encode_opnd_dq0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_dq_plus(0, 0, 30, opnd, enc_out);
}

/* dq0p1: as dq0 but add 1 mod 32 to reg number */

static inline bool
decode_opnd_dq0p1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_dq_plus(1, 0, 30, enc, opnd);
}

static inline bool
encode_opnd_dq0p1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_dq_plus(1, 0, 30, opnd, enc_out);
}

/* dq0p2: as dq0 but add 2 mod 32 to reg number */

static inline bool
decode_opnd_dq0p2(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_dq_plus(2, 0, 30, enc, opnd);
}

static inline bool
encode_opnd_dq0p2(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_dq_plus(2, 0, 30, opnd, enc_out);
}

/* dq0p3: as dq0 but add 3 mod 32 to reg number */

static inline bool
decode_opnd_dq0p3(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_dq_plus(3, 0, 30, enc, opnd);
}

static inline bool
encode_opnd_dq0p3(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_dq_plus(3, 0, 30, opnd, enc_out);
}

/* vt0: first register operand of SIMD load/store multiple structures */

static inline bool
decode_opnd_vt0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vtn(0, enc, opnd);
}

static inline bool
encode_opnd_vt0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vtn(0, enc, opnd, enc_out);
}

/* vt1: second register operand of SIMD load/store multiple structures */

static inline bool
decode_opnd_vt1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vtn(1, enc, opnd);
}

static inline bool
encode_opnd_vt1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vtn(1, enc, opnd, enc_out);
}

/* vt2: third register operand of SIMD load/store multiple structures */

static inline bool
decode_opnd_vt2(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vtn(2, enc, opnd);
}

static inline bool
encode_opnd_vt2(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vtn(2, enc, opnd, enc_out);
}

/* vt3: fourth register operand of SIMD load/store multiple structures */

static inline bool
decode_opnd_vt3(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vtn(3, enc, opnd);
}

static inline bool
encode_opnd_vt3(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vtn(3, enc, opnd, enc_out);
}

/* dq5: D/Q register at bit position 5; bit 30 selects Q reg */

static inline bool
decode_opnd_dq5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_dq_plus(0, 5, 30, enc, opnd);
}

static inline bool
encode_opnd_dq5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_dq_plus(0, 5, 30, opnd, enc_out);
}

/* index2: index of S subreg in Q register: 0-3 */

static inline bool
decode_opnd_index2(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_index(2, enc, opnd);
}

static inline bool
encode_opnd_index2(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_index(2, opnd, enc_out);
}

/* index1: index of H subreg in Q register: 0-7 */

static inline bool
decode_opnd_index1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_index(1, enc, opnd);
}

static inline bool
encode_opnd_index1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_index(1, opnd, enc_out);
}

/* index0: index of B subreg in Q register: 0-15 */

static inline bool
decode_opnd_index0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_index(0, enc, opnd);
}

static inline bool
encode_opnd_index0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_index(0, opnd, enc_out);
}

/* memvm: memory operand for SIMD load/store multiple structures */

static inline bool
decode_opnd_memvm(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int bytes = (8 << extract_uint(enc, 30, 1)) * multistruct_regcount(enc);
    *opnd = create_base_imm(enc, 0, bytes);
    return true;
}

static inline bool
encode_opnd_memvm(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    int regs = multistruct_regcount(enc);
    opnd_size_t size;
    uint rn;
    if (!is_base_imm(opnd, &rn) || opnd_get_disp(opnd) != 0)
        return false;
    size = opnd_get_size(opnd);
    if (size != opnd_size_from_bytes(regs * 8) && size != opnd_size_from_bytes(regs * 16))
        return false;
    *enc_out = rn << 5 | (uint)(size == opnd_size_from_bytes(regs * 16)) << 30;
    return true;
}

/* dq16_h_sz: D/Q register at bit position 16 with 4 bits only, for the FP16
 *             by-element encoding; bit 30 selects Q reg
 */

static inline bool
decode_opnd_dq16_h_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_reg((TEST(1U << 30, enc) ? DR_REG_Q0 : DR_REG_D0) +
                            extract_uint(enc, 16, 4));
    return true;
}

static inline bool
encode_opnd_dq16_h_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    uint num;
    bool q;
    if (!opnd_is_reg(opnd))
        return false;
    q = (uint)(opnd_get_reg(opnd) - DR_REG_Q0) < 16;
    num = opnd_get_reg(opnd) - (q ? DR_REG_Q0 : DR_REG_D0);
    if (num >= 16)
        return false;
    *enc_out = num << 16 | (uint)q << 30;
    return true;
}

/* dq16: D/Q register at bit position 16; bit 30 selects Q reg */

static inline bool
decode_opnd_dq16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_dq_plus(0, 16, 30, enc, opnd);
}

static inline bool
encode_opnd_dq16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_dq_plus(0, 16, 30, opnd, enc_out);
}

/* imm6: shift amount for logical and arithmetical instructions */

static inline bool
decode_opnd_imm6(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    if (!TEST(1U << 31, enc) && TEST(1U << 15, enc))
        return false;
    return decode_opnd_int(10, 6, false, 0, OPSZ_6b, 0, enc, opnd);
}

static inline bool
encode_opnd_imm6(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!TEST(1U << 31, enc) && TEST(1U << 15, enc))
        return false;
    return encode_opnd_int(10, 6, false, 0, 0, opnd, enc_out);
}

/* imms: second immediate operand for bitfield operation */

static inline bool
decode_opnd_imms(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_imm_bf(10, enc, opnd);
}

static inline bool
encode_opnd_imms(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_imm_bf(10, enc, opnd, enc_out);
}

/* immr: first immediate operand for bitfield operation */

static inline bool
decode_opnd_immr(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_imm_bf(16, enc, opnd);
}

static inline bool
encode_opnd_immr(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_imm_bf(16, enc, opnd, enc_out);
}

/* imm16sh: shift amount for 16-bit immediate of MOVK/MOVN/MOVZ/SVC */

static inline bool
decode_opnd_imm16sh(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    if (!TEST(1U << 31, enc) && TEST(1U << 22, enc))
        return false;
    return decode_opnd_int(21, 2, false, 4, OPSZ_6b, 0, enc, opnd);
}

static inline bool
encode_opnd_imm16sh(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    uint t;
    if (!encode_opnd_int(21, 2, false, 4, 0, opnd, &t) ||
        (!TEST(1U << 31, enc) && TEST(1U << 22, t)))
        return false;
    *enc_out = t;
    return true;
}

/* mem0: memory operand with no offset, gets size from bits 30 and 31 */

static inline bool
decode_opnd_mem0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem0_scale(extract_uint(enc, 30, 2), enc, opnd);
}

static inline bool
encode_opnd_mem0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem0_scale(extract_uint(enc, 30, 2), opnd, enc_out);
}

/* mem9post: post-indexed mem9, so offset is zero */

static inline bool
decode_opnd_mem9post(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem9_bytes(1 << extract_uint(enc, 30, 2), true, enc, opnd);
}

static inline bool
encode_opnd_mem9post(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem9_bytes(1 << extract_uint(enc, 30, 2), true, opnd, enc_out);
}

/* mem9: memory operand with 9-bit offset; gets size from bits 30 and 31 */

static inline bool
decode_opnd_mem9(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem9_bytes(1 << extract_uint(enc, 30, 2), false, enc, opnd);
}

static inline bool
encode_opnd_mem9(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem9_bytes(1 << extract_uint(enc, 30, 2), false, opnd, enc_out);
}

/* memreg: memory operand with register offset; gets size from bits 30 and 31 */

static inline bool
decode_opnd_memreg(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_memreg_size(opnd_size_from_bytes(1 << extract_uint(enc, 30, 2)),
                                   enc, opnd);
}

static inline bool
encode_opnd_memreg(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_memreg_size(opnd_size_from_bytes(1 << extract_uint(enc, 30, 2)),
                                   opnd, enc_out);
}

/* mem12: memory operand with 12-bit offset; gets size from bits 30 and 31 */

static inline bool
decode_opnd_mem12(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem12_scale(extract_uint(enc, 30, 2), false, enc, opnd);
}

static inline bool
encode_opnd_mem12(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem12_scale(extract_uint(enc, 30, 2), false, opnd, enc_out);
}

/* mem7post: post-indexed mem7, so offset is zero */

static inline bool
decode_opnd_mem7post(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem7_postindex(true, enc, opnd);
}

static inline bool
encode_opnd_mem7post(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem7_postindex(true, enc, opnd, enc_out);
}

/* mem7off: just the 7-bit offset from mem7 */

static inline bool
decode_opnd_mem7off(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(15, 7, true, mem7_scale(enc), OPSZ_PTR, 0, enc, opnd);
}

static inline bool
encode_opnd_mem7off(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(15, 7, true, mem7_scale(enc), 0, opnd, enc_out);
}

/* mem7: memory operand with 7-bit offset; gets size from bits 26, 30 and 31 */

static inline bool
decode_opnd_mem7(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_mem7_postindex(false, enc, opnd);
}

static inline bool
encode_opnd_mem7(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_mem7_postindex(false, enc, opnd, enc_out);
}

/* memlit: memory operand for literal load; gets size from bits 26, 30 and 31 */

static inline bool
decode_opnd_memlit(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_rel_addr(pc + 4 * extract_int(enc, 5, 19), memlit_size(enc));
    return true;
}

static inline bool
encode_opnd_memlit(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    ptr_uint_t off;
    if (!opnd_is_rel_addr(opnd) || opnd_get_size(opnd) != memlit_size(enc))
        return false;
    off = (byte *)opnd_get_addr(opnd) - pc;
    if ((off & 3) != 0 || off + (1U << 20) >= 1U << 21)
        return false;
    *enc_out = (off >> 2 & 0x7ffff) << 5;
    return true;
}

/* wx0: W/X register or WZR/XZR at bit position 0; bit 31 selects X reg */

static inline bool
decode_opnd_wx0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_rn(false, 0, enc, opnd);
}

static inline bool
encode_opnd_wx0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_rn(false, 0, opnd, enc_out);
}

/* wx0sp: W/X register or WSP/XSP at bit position 0; bit 31 selects X reg */

static inline bool
decode_opnd_wx0sp(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_rn(true, 0, enc, opnd);
}

static inline bool
encode_opnd_wx0sp(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_rn(true, 0, opnd, enc_out);
}

/* wx5: W/X register or WZR/XZR at bit position 5; bit 31 selects X reg */

static inline bool
decode_opnd_wx5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_rn(false, 5, enc, opnd);
}

static inline bool
encode_opnd_wx5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_rn(false, 5, opnd, enc_out);
}

/* wx5sp: W/X register or WSP/XSP at bit position 5; bit 31 selects X reg */

static inline bool
decode_opnd_wx5sp(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_rn(true, 5, enc, opnd);
}

static inline bool
encode_opnd_wx5sp(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_rn(true, 5, opnd, enc_out);
}

/* wx10: W/X register or WZR/XZR at bit position 10; bit 31 selects X reg */

static inline bool
decode_opnd_wx10(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_rn(false, 10, enc, opnd);
}

static inline bool
encode_opnd_wx10(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_rn(false, 10, opnd, enc_out);
}

/* wx16: W/X register or WZR/XZR at bit position 16; bit 31 selects X reg */

static inline bool
decode_opnd_wx16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_rn(false, 16, enc, opnd);
}

static inline bool
encode_opnd_wx16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_rn(false, 16, opnd, enc_out);
}

/*******************************************************************************
 * Pairs of functions for decoding and encoding opndsets, as listed in "codec.txt".
 * Currently all branch instructions are handled in this way.
 */

/* adr: used for ADR and ADRP */

static inline bool
decode_opnds_adr(uint enc, dcontext_t *dcontext, byte *pc, instr_t *instr, int opcode)
{
    opnd_t opnd;
    if (!decode_opnd_adr_page(opcode == OP_adrp ? 12 : 0, enc, pc, &opnd))
        return false;
    instr_set_opcode(instr, opcode);
    instr_set_num_opnds(dcontext, instr, 1, 1);
    instr_set_dst(instr, 0,
                  opnd_create_reg(decode_reg(extract_uint(enc, 0, 5), true, false)));
    instr_set_src(instr, 0, opnd);
    return true;
}

static inline uint
encode_opnds_adr(byte *pc, instr_t *instr, uint enc, decode_info_t *di)
{
    int opcode = instr_get_opcode(instr);
    uint rd, adr;
    if (instr_num_dsts(instr) == 1 && instr_num_srcs(instr) == 1 &&
        encode_opnd_adr_page(opcode == OP_adrp ? 12 : 0, pc, instr_get_src(instr, 0),
                             &adr, instr, di) &&
        encode_opnd_wxn(true, false, 0, instr_get_dst(instr, 0), &rd))
        return (enc | adr | rd);
    return ENCFAIL;
}

/* b: used for B and BL */

static inline bool
decode_opnds_b(uint enc, dcontext_t *dcontext, byte *pc, instr_t *instr, int opcode)
{
    instr_set_opcode(instr, opcode);
    if (opcode == OP_bl) {
        instr_set_num_opnds(dcontext, instr, 1, 1);
        instr_set_dst(instr, 0, opnd_create_reg(DR_REG_X30));
    } else
        instr_set_num_opnds(dcontext, instr, 0, 1);
    instr_set_src(instr, 0, opnd_create_pc(pc + extract_int(enc, 0, 26) * 4));
    return true;
}

static inline uint
encode_opnds_b(byte *pc, instr_t *instr, uint enc, decode_info_t *di)
{
    int opcode = instr_get_opcode(instr);
    bool is_bl = (opcode == OP_bl);
    uint off, x30;
    if (instr_num_dsts(instr) == (is_bl ? 1 : 0) && instr_num_srcs(instr) == 1 &&
        (!is_bl || encode_opnd_impx30(enc, opcode, pc, instr_get_dst(instr, 0), &x30)) &&
        encode_pc_off(&off, 26, pc, instr, instr_get_src(instr, 0), di))
        return (enc | off);
    return ENCFAIL;
}

/* bcond: used for B.cond */

static inline bool
decode_opnds_bcond(uint enc, dcontext_t *dcontext, byte *pc, instr_t *instr, int opcode)
{
    instr_set_opcode(instr, opcode);
    instr_set_num_opnds(dcontext, instr, 0, 1);
    instr_set_src(instr, 0, opnd_create_pc(pc + extract_int(enc, 5, 19) * 4));
    instr_set_predicate(instr, DR_PRED_EQ + (enc & 15));
    return true;
}

static inline uint
encode_opnds_bcond(byte *pc, instr_t *instr, uint enc, decode_info_t *di)
{
    uint off;
    if (instr_num_dsts(instr) == 0 && instr_num_srcs(instr) == 1 &&
        encode_pc_off(&off, 19, pc, instr, instr_get_src(instr, 0), di) &&
        (uint)(instr_get_predicate(instr) - DR_PRED_EQ) < 16)
        return (enc | off << 5 | (instr_get_predicate(instr) - DR_PRED_EQ));
    return ENCFAIL;
}

/* cbz: used for CBNZ and CBZ */

static inline bool
decode_opnds_cbz(uint enc, dcontext_t *dcontext, byte *pc, instr_t *instr, int opcode)
{
    instr_set_opcode(instr, opcode);
    instr_set_num_opnds(dcontext, instr, 0, 2);
    instr_set_src(instr, 0, opnd_create_pc(pc + extract_int(enc, 5, 19) * 4));
    instr_set_src(
        instr, 1,
        opnd_create_reg(decode_reg(extract_uint(enc, 0, 5), TEST(1U << 31, enc), false)));
    return true;
}

static inline uint
encode_opnds_cbz(byte *pc, instr_t *instr, uint enc, decode_info_t *di)
{
    uint rt, off;
    if (instr_num_dsts(instr) == 0 && instr_num_srcs(instr) == 2 &&
        encode_pc_off(&off, 19, pc, instr, instr_get_src(instr, 0), di) &&
        encode_opnd_rn(false, 0, instr_get_src(instr, 1), &rt))
        return (enc | off << 5 | rt);
    return ENCFAIL;
}

/* logic_imm: used for AND, ANDS, EOR and ORR.
 * Logical (immediate) instructions are awkward because there are sometimes
 * many ways of representing the same immediate value. We add the raw encoding
 * as an additional operand when the encoding is not the canonical one.
 */

static inline bool
decode_opnds_logic_imm(uint enc, dcontext_t *dcontext, byte *pc, instr_t *instr,
                       int opcode)
{
    bool is_x = TEST(1U << 31, enc);
    uint imm_enc = extract_uint(enc, 10, 13);     /* encoding of bitmask */
    ptr_uint_t imm_val = decode_bitmask(imm_enc); /* value of bitmask */
    bool canonical = encode_bitmask(imm_val) == imm_enc;
    if (imm_val == 0 || (!is_x && TEST(1U << 12, imm_enc)))
        return false;
    if (!is_x)
        imm_val &= 0xffffffff;
    instr_set_opcode(instr, opcode);
    instr_set_num_opnds(dcontext, instr, 1, 2 + (canonical ? 0 : 1));
    instr_set_dst(
        instr, 0,
        opnd_create_reg(decode_reg(extract_uint(enc, 0, 5), is_x, opcode != OP_ands)));
    instr_set_src(instr, 0,
                  opnd_create_reg(decode_reg(extract_uint(enc, 5, 5), is_x, false)));
    instr_set_src(instr, 1, opnd_create_immed_uint(imm_val, is_x ? OPSZ_8 : OPSZ_4));
    if (!canonical)
        instr_set_src(instr, 2, opnd_create_immed_uint(imm_enc, OPSZ_2));
    return true;
}

static inline uint
encode_opnds_logic_imm(byte *pc, instr_t *instr, uint enc, decode_info_t *di)
{
    int opcode = instr_get_opcode(instr);
    int srcs = instr_num_srcs(instr);
    opnd_t opnd_val;
    ptr_uint_t imm_val;
    uint rd, rn;
    if (srcs < 2 || srcs > 3 || instr_num_dsts(instr) != 1)
        return ENCFAIL;
    opnd_val = instr_get_src(instr, 1);
    if (!encode_opnd_rn(opcode != OP_ands, 0, instr_get_dst(instr, 0), &rd) ||
        !encode_opnd_rn(false, 5, instr_get_src(instr, 0), &rn) ||
        TEST(1U << 31, rd ^ rn) || !opnd_is_immed_int(opnd_val))
        return ENCFAIL;
    imm_val = opnd_get_immed_int(opnd_val);
    if (!TEST(1U << 31, rd)) {
        if ((imm_val >> 32) != 0)
            return ENCFAIL;
        imm_val |= imm_val << 32;
    }
    if (srcs == 3) {
        opnd_t opnd_enc = instr_get_src(instr, 2);
        ptr_int_t imm_enc;
        if (!opnd_is_immed_int(opnd_enc))
            return ENCFAIL;
        imm_enc = opnd_get_immed_int(opnd_enc);
        if (imm_enc < 0 || imm_enc > 0x1fff || decode_bitmask(imm_enc) != imm_val)
            return ENCFAIL;
        return (enc | rd | rn | (uint)imm_enc << 10);
    } else {
        int imm_enc = encode_bitmask(imm_val);
        if (imm_enc < 0)
            return ENCFAIL;
        return (enc | rd | rn | (uint)imm_enc << 10);
    }
}

/* mst: used for MSR.
 * With MSR the destination register may or may not be one of the system registers
 * that we recognise.
 */

static inline bool
decode_opnds_msr(uint enc, dcontext_t *dcontext, byte *pc, instr_t *instr, int opcode)
{
    opnd_t opnd = decode_sysreg(extract_uint(enc, 5, 15));
    instr_set_opcode(instr, opcode);
    if (opnd_is_reg(opnd)) {
        instr_set_num_opnds(dcontext, instr, 1, 1);
        instr_set_dst(instr, 0, opnd);
    } else {
        instr_set_num_opnds(dcontext, instr, 0, 2);
        instr_set_src(instr, 1, opnd);
    }
    instr_set_src(instr, 0,
                  opnd_create_reg(decode_reg(extract_uint(enc, 0, 5), true, false)));
    return true;
}

static inline uint
encode_opnds_msr(byte *pc, instr_t *instr, uint enc, decode_info_t *di)
{
    uint imm15, xt;
    if (instr_num_dsts(instr) == 1 && instr_num_srcs(instr) == 1 &&
        opnd_is_reg(instr_get_dst(instr, 0)) &&
        encode_sysreg(&imm15, instr_get_dst(instr, 0)) &&
        encode_opnd_wxn(true, false, 0, instr_get_src(instr, 0), &xt))
        return (enc | xt | imm15 << 5);
    if (instr_num_dsts(instr) == 0 && instr_num_srcs(instr) == 2 &&
        opnd_is_immed_int(instr_get_src(instr, 1)) &&
        encode_opnd_wxn(true, false, 0, instr_get_src(instr, 0), &xt) &&
        encode_sysreg(&imm15, instr_get_src(instr, 1)))
        return (enc | xt | imm15 << 5);
    return ENCFAIL;
}

/* tbz: used for TBNZ and TBZ */

static inline bool
decode_opnds_tbz(uint enc, dcontext_t *dcontext, byte *pc, instr_t *instr, int opcode)
{
    instr_set_opcode(instr, opcode);
    instr_set_num_opnds(dcontext, instr, 0, 3);
    instr_set_src(instr, 0, opnd_create_pc(pc + extract_int(enc, 5, 14) * 4));
    instr_set_src(instr, 1,
                  opnd_create_reg(decode_reg(extract_uint(enc, 0, 5), true, false)));
    instr_set_src(instr, 2,
                  opnd_create_immed_int((enc >> 19 & 31) | (enc >> 26 & 32), OPSZ_5b));
    return true;
}

static inline uint
encode_opnds_tbz(byte *pc, instr_t *instr, uint enc, decode_info_t *di)
{
    uint xt, imm6, off;
    if (instr_num_dsts(instr) == 0 && instr_num_srcs(instr) == 3 &&
        encode_pc_off(&off, 14, pc, instr, instr_get_src(instr, 0), di) &&
        encode_opnd_wxn(true, false, 0, instr_get_src(instr, 1), &xt) &&
        encode_opnd_int(0, 6, false, 0, 0, instr_get_src(instr, 2), &imm6))
        return (enc | off << 5 | xt | (imm6 & 31) << 19 | (imm6 & 32) << 26);
    return ENCFAIL;
}

/******************************************************************************/

/* Include automatically generated decoder and encoder. */
#include "decode_gen.h"
#include "encode_gen.h"

/******************************************************************************/

byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr)
{
    byte *next_pc = pc + 4;
    uint enc = *(uint *)pc;
    uint eflags = 0;
    int opc;

    CLIENT_ASSERT(instr->opcode == OP_INVALID || instr->opcode == OP_UNDECODED,
                  "decode: instr is already decoded, may need to call instr_reset()");

    if (!decoder(enc, dcontext, orig_pc, instr)) {
        /* We use OP_xx for instructions not yet handled by the decoder.
         * If an A64 instruction accesses a general-purpose register
         * (except X30) then the number of that register appears in one
         * of four possible places in the instruction word, so we can
         * pessimistically assume that an unrecognised instruction reads
         * and writes all four of those registers, and this is
         * sufficient to enable correct (though often excessive) mangling.
         */
        instr_set_opcode(instr, OP_xx);
        instr_set_num_opnds(dcontext, instr, 4, 5);
        instr->src0 = OPND_CREATE_INT32(enc);
        instr->srcs[0] = opnd_create_reg(DR_REG_X0 + (enc & 31));
        instr->dsts[0] = opnd_create_reg(DR_REG_X0 + (enc & 31));
        instr->srcs[1] = opnd_create_reg(DR_REG_X0 + (enc >> 5 & 31));
        instr->dsts[1] = opnd_create_reg(DR_REG_X0 + (enc >> 5 & 31));
        instr->srcs[2] = opnd_create_reg(DR_REG_X0 + (enc >> 10 & 31));
        instr->dsts[2] = opnd_create_reg(DR_REG_X0 + (enc >> 10 & 31));
        instr->srcs[3] = opnd_create_reg(DR_REG_X0 + (enc >> 16 & 31));
        instr->dsts[3] = opnd_create_reg(DR_REG_X0 + (enc >> 16 & 31));
    }

    /* XXX i#2374: This determination of flag usage should be separate from the decoding
     * of operands. Also, we should perhaps add flag information in codec.txt instead of
     * listing all the opcodes, although the list is short and unlikely to change.
     */
    opc = instr_get_opcode(instr);
    if ((opc == OP_mrs && instr_num_srcs(instr) == 1 &&
         opnd_is_reg(instr_get_src(instr, 0)) &&
         opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_NZCV) ||
        opc == OP_bcond || opc == OP_adc || opc == OP_adcs || opc == OP_sbc ||
        opc == OP_sbcs || opc == OP_csel || opc == OP_csinc || opc == OP_csinv ||
        opc == OP_csneg || opc == OP_ccmn || opc == OP_ccmp) {
        /* FIXME i#1569: When handled by decoder, add:
         * opc == OP_fcsel
         */
        eflags |= EFLAGS_READ_NZCV;
    }
    if ((opc == OP_msr && instr_num_dsts(instr) == 1 &&
         opnd_is_reg(instr_get_dst(instr, 0)) &&
         opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_NZCV) ||
        opc == OP_adcs || opc == OP_adds || opc == OP_sbcs || opc == OP_subs ||
        opc == OP_ands || opc == OP_bics || opc == OP_ccmn || opc == OP_ccmp) {
        /* FIXME i#1569: When handled by decoder, add:
         * opc == OP_fccmp || opc == OP_fccmpe || opc == OP_fcmp || opc == OP_fcmpe
         */
        eflags |= EFLAGS_WRITE_NZCV;
    }
    instr->eflags = eflags;
    instr_set_eflags_valid(instr, true);

    instr_set_operands_valid(instr, true);

    if (orig_pc != pc) {
        /* We do not want to copy when encoding and condone an invalid
         * relative target.
         * TODO i#4016: Add re-relativization support without having to re-encode.
         */
        instr_set_raw_bits_valid(instr, false);
        instr_set_translation(instr, orig_pc);
    } else {
        /* We set raw bits AFTER setting all srcs and dsts because setting
         * a src or dst marks instr as having invalid raw bits.
         */
        ASSERT(CHECK_TRUNCATE_TYPE_uint(next_pc - pc));
        instr_set_raw_bits(instr, pc, (uint)(next_pc - pc));
    }

    return next_pc;
}

uint
encode_common(byte *pc, instr_t *i, decode_info_t *di)
{
    ASSERT(((ptr_int_t)pc & 3) == 0);
    return encoder(pc, i, di);
}
