/* **********************************************************
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

#include "../globals.h"
#include "arch.h"
#include "decode.h"
#include "disassemble.h"
#include "instr.h"
#include "instr_create.h"

#include "codec.h"

/* The functions instr_set_0dst_0src, etc. could perhaps be moved to instr.h
 * where instr_create_0dst_0src, etc. are declared.
 */

static inline void
instr_set_0dst_0src(dcontext_t *dc, instr_t *instr, int op)
{
    instr_set_opcode(instr, op);
    instr_set_num_opnds(dc, instr, 0, 0);
}

static inline void
instr_set_0dst_1src(dcontext_t *dc, instr_t *instr, int op,
                    opnd_t src0)
{
    instr_set_opcode(instr, op);
    instr_set_num_opnds(dc, instr, 0, 1);
    instr_set_src(instr, 0, src0);
}

static inline void
instr_set_0dst_2src(dcontext_t *dc, instr_t *instr, int op,
                    opnd_t src0, opnd_t src1)
{
    instr_set_opcode(instr, op);
    instr_set_num_opnds(dc, instr, 0, 2);
    instr_set_src(instr, 0, src0);
    instr_set_src(instr, 1, src1);
}

static inline void
instr_set_0dst_3src(dcontext_t *dc, instr_t *instr, int op,
                    opnd_t src0, opnd_t src1, opnd_t src2)
{
    instr_set_opcode(instr, op);
    instr_set_num_opnds(dc, instr, 0, 3);
    instr_set_src(instr, 0, src0);
    instr_set_src(instr, 1, src1);
    instr_set_src(instr, 2, src2);
}

static inline void
instr_set_1dst_0src(dcontext_t *dc, instr_t *instr, int op,
                    opnd_t dst0)
{
    instr_set_opcode(instr, op);
    instr_set_num_opnds(dc, instr, 1, 0);
    instr_set_dst(instr, 0, dst0);
}

static inline void
instr_set_1dst_1src(dcontext_t *dc, instr_t *instr, int op,
                    opnd_t dst0, opnd_t src0)
{
    instr_set_opcode(instr, op);
    instr_set_num_opnds(dc, instr, 1, 1);
    instr_set_dst(instr, 0, dst0);
    instr_set_src(instr, 0, src0);
}

static inline void
instr_set_1dst_2src(dcontext_t *dc, instr_t *instr, int op,
                    opnd_t dst0, opnd_t src0, opnd_t src1)
{
    instr_set_opcode(instr, op);
    instr_set_num_opnds(dc, instr, 1, 2);
    instr_set_dst(instr, 0, dst0);
    instr_set_src(instr, 0, src0);
    instr_set_src(instr, 1, src1);
}

static inline void
instr_set_1dst_3src(dcontext_t *dc, instr_t *instr, int op,
                    opnd_t dst0, opnd_t src0, opnd_t src1, opnd_t src2)
{
    instr_set_opcode(instr, op);
    instr_set_num_opnds(dc, instr, 1, 3);
    instr_set_dst(instr, 0, dst0);
    instr_set_src(instr, 0, src0);
    instr_set_src(instr, 1, src1);
    instr_set_src(instr, 2, src2);
}

static inline void
instr_set_1dst_4src(dcontext_t *dc, instr_t *instr, int op,
                    opnd_t dst0, opnd_t src0, opnd_t src1, opnd_t src2, opnd_t src3)
{
    instr_set_opcode(instr, op);
    instr_set_num_opnds(dc, instr, 1, 4);
    instr_set_dst(instr, 0, dst0);
    instr_set_src(instr, 0, src0);
    instr_set_src(instr, 1, src1);
    instr_set_src(instr, 2, src2);
    instr_set_src(instr, 3, src3);
}

static inline ptr_int_t
extract_int(uint enc, int pos, int len)
{
    uint u = ((enc >> pos & (((uint)1 << (len - 1)) - 1)) -
              (enc >> pos & ((uint)1 << (len - 1))));
    return u << 1 < u ? -(ptr_int_t)~u - 1 : u;
}

static inline ptr_uint_t
extract_uint(uint enc, int pos, int len)
{
    return enc >> pos & (((uint)1 << len) - 1);
}

static inline bool
encode_imm(uint *imm, int bits, opnd_t opnd)
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
encode_opnums(instr_t *i, int dsts, int srcs)
{
    return instr_num_dsts(i) == dsts && instr_num_srcs(i) == srcs;
}

static inline bool
encode_pc_off(uint *poff, int bits, byte *pc, instr_t *instr, opnd_t opnd)
{
    ptr_uint_t off, range;
    ASSERT(0 < bits && bits <= 32);
    if (opnd.kind == PC_kind)
        off = opnd.value.pc - pc;
    else if (opnd.kind == INSTR_kind)
        off = opnd_get_instr(opnd)->note - instr->note;
    else
        return false;
    range = (ptr_uint_t)1 << bits;
    if (TEST(~((range - 1) << 2), off + (range << 1)))
        return false;
    *poff = off >> 2 & (range - 1);
    return true;
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
    default:
        return opnd_create_immed_uint(imm15, OPSZ_2);
    }
    return opnd_create_reg(sysreg);
}

static inline bool
encode_sysreg(uint *imm15, opnd_t opnd)
{
    if (opnd_is_reg(opnd)) {
        switch (opnd_get_reg(opnd)) {
        case DR_REG_NZCV: *imm15 = 0x5a10; break;
        case DR_REG_FPCR: *imm15 = 0x5a20; break;
        case DR_REG_FPSR: *imm15 = 0x5a21; break;
        case DR_REG_TPIDR_EL0: *imm15 = 0x5e82; break;
        default:
            return false;
        }
        return true;
    }
    if (opnd_is_immed_int(opnd)) {
        uint imm;
        if (encode_imm(&imm, 15, opnd) && !opnd_is_reg(decode_sysreg(imm))) {
            *imm15 = imm;
            return true;
        }
        return false;
    }
    return false;
}

static inline opnd_t
decode_rreg(bool x, uint n, reg_t w31, reg_t x31)
{
    ASSERT(n < 32);
    return opnd_create_reg(x ?
                           (n < 31 ? DR_REG_X0 + n : x31) :
                           (n < 31 ? DR_REG_W0 + n : w31));
}

static inline bool
encode_rreg(opnd_size_t *x, uint *r, opnd_t opnd, reg_t w31, reg_t x31)
{
    reg_id_t reg;
    if (!opnd_is_reg(opnd))
        return false;
    reg = opnd_get_reg(opnd);
    if ((DR_REG_W0 <= reg && reg <= DR_REG_W30) || reg == w31) {
        if (*x == OPSZ_NA)
            *x = OPSZ_4;
        else if (*x != OPSZ_4)
            return false;
        *r = (reg == w31) ? 31 : (reg - DR_REG_W0);
        return true;
    }
    if ((DR_REG_X0 <= reg && reg <= DR_REG_X30) || reg == x31) {
        if (*x == OPSZ_NA)
            *x = OPSZ_8;
        else if (*x != OPSZ_8)
            return false;
        *r = (reg == x31) ? 31 : (reg - DR_REG_X0);
        return true;
    }
    return false;
}

static inline opnd_t
decode_rregsp(bool x, uint n)
{
    return decode_rreg(x, n, DR_REG_WSP, DR_REG_XSP);
}

static inline bool
encode_rregsp(opnd_size_t *x, uint *r, opnd_t opnd)
{
    return encode_rreg(x, r, opnd, DR_REG_WSP, DR_REG_XSP);
}

static inline opnd_t
decode_rregz(bool x, uint n)
{
    return decode_rreg(x, n, DR_REG_WZR, DR_REG_XZR);
}

static inline bool
encode_rregz(opnd_size_t *x, uint *r, opnd_t opnd)
{
    return encode_rreg(x, r, opnd, DR_REG_WZR, DR_REG_XZR);
}

static inline opnd_t
decode_shift(uint sh)
{
    dr_shift_type_t type;
    switch (sh) {
    default:
        ASSERT(false);
    case 0: type = DR_SHIFT_LSL; break;
    case 1: type = DR_SHIFT_LSR; break;
    case 2: type = DR_SHIFT_ASR; break;
    case 3: type = DR_SHIFT_ROR; break;
    }
    return opnd_create_immed_uint(type, OPSZ_2b);
}

static inline bool
encode_shift(uint *sh, opnd_t opnd)
{
    ptr_int_t value;
    if (!opnd_is_immed_int(opnd))
        return false;
    value = opnd_get_immed_int(opnd);
    switch (value) {
    default:
        return false;
    case DR_SHIFT_LSL: *sh = 0; break;
    case DR_SHIFT_LSR: *sh = 1; break;
    case DR_SHIFT_ASR: *sh = 2; break;
    case DR_SHIFT_ROR: *sh = 3; break;
    }
    return true;
}

static inline bool
encode_vreg(opnd_size_t *x, uint *r, opnd_t opnd)
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

static inline bool
encode_xregsp_reg(uint *r, reg_t reg)
{
    if (DR_REG_X0 <= reg && reg <= DR_REG_X30)
        *r = reg - DR_REG_X0;
    else if (reg == DR_REG_XSP)
        *r = 31;
    else
        return false;
    return true;
}

static inline opnd_t
decode_xregz(uint n)
{
    ASSERT(n < 32);
    return opnd_create_reg(n < 31 ? DR_REG_X0 + n : DR_REG_XZR);
}

static inline bool
encode_xregz(uint *r, opnd_t opnd)
{
    reg_id_t reg;
    if (!opnd_is_reg(opnd))
        return false;
    reg = opnd_get_reg(opnd);
    if ((DR_REG_X0 <= reg && reg <= DR_REG_X30) || reg == DR_REG_XZR) {
        *r = (reg == DR_REG_XZR) ? 31 : (reg - DR_REG_X0);
        return true;
    }
    return false;
}

static inline bool
encode_base_imm(opnd_size_t *x, uint *rn, uint *imm, int bits, bool signd, opnd_t opnd)
{
    uint reg;
    if (opnd.kind == BASE_DISP_kind &&
        opnd.value.base_disp.index_reg == DR_REG_NULL &&
        encode_xregsp_reg(&reg, opnd.value.base_disp.base_reg) &&
        !TEST(opnd.value.base_disp.disp + (signd ? (ptr_uint_t)1 << (bits - 1) : 0),
              ~(ptr_uint_t)0 << (bits - 1) << 1)) {
        *x = opnd.size;
        *rn = reg;
        *imm = opnd.value.base_disp.disp;
        return true;
    }
    return false;
}

/*******************************************************************************
 * Functions for decoding and encoding each "type" of instruction.
 */

#define ENCFAIL (uint)0 /* a value that is not a valid instruction */

static inline bool
decode_add_imm(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    bool x = TEST(1U << 31, enc);
    instr_set_1dst_4src(dc, instr, op,
                        decode_rregsp(x, enc & 31),
                        decode_rregsp(x, enc >> 5 & 31),
                        opnd_create_immed_uint(extract_uint(enc, 10, 12), OPSZ_4),
                        decode_shift(0),
                        opnd_create_immed_uint(extract_uint(enc, 22, 2) * 16, OPSZ_4));
    return true;
}

static inline uint
encode_add_imm(byte *pc, instr_t *i, uint enc)
{
    uint rd, rn, imm12, shift_type, shift_amount;
    opnd_size_t x = OPSZ_NA;
    if (encode_opnums(i, 1, 4) &&
        encode_rregsp(&x, &rd, instr_get_dst(i, 0)) &&
        encode_rregsp(&x, &rn, instr_get_src(i, 0)) &&
        encode_imm(&imm12, 12, instr_get_src(i, 1)) &&
        encode_shift(&shift_type, instr_get_src(i, 2)) &&
        encode_imm(&shift_amount, 5, instr_get_src(i, 3)) &&
        shift_type == 0 && (shift_amount & 15) == 0)
        return (enc | (uint)(x == OPSZ_8) << 31 |
                rd | rn << 5 | imm12 << 10 | shift_amount >> 4 << 22);
    return ENCFAIL;
}

static inline bool
decode_adr(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    ptr_int_t off = (((enc >> 3 & 0xffffc) | (enc >> 29 & 3)) -
                     (ptr_int_t)(enc >> 3 & 0x100000));
    ptr_int_t x = op == OP_adrp ?
        ((ptr_int_t)pc >> 12 << 12) + (off << 12) :
        (ptr_int_t)pc + off;
    instr_set_1dst_1src(dc, instr, op,
                        decode_xregz(enc & 31),
                        opnd_create_rel_addr((void *)x, OPSZ_8));
    return true;
}

static inline uint
encode_adr(byte *pc, instr_t *i, uint enc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return ENCFAIL;
}

static inline bool
decode_and_reg(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    bool x = TEST(1U << 31, enc);
    uint imm6 = enc >> 10 & 63;
    if (!x && imm6 >= 32)
        return false;
    instr_set_1dst_4src(dc, instr, op,
                        decode_rregz(x, enc & 31),
                        decode_rregz(x, enc >> 5 & 31),
                        decode_rregz(x, enc >> 16 & 31),
                        decode_shift(enc >> 22 & 3),
                        opnd_create_immed_uint(imm6, OPSZ_4));
    return true;
}

static inline uint
encode_and_reg(byte *pc, instr_t *i, uint enc)
{
    uint rd, rn, rm, sh, imm6;
    opnd_size_t x = OPSZ_NA;
    if (encode_opnums(i, 1, 4) &&
        encode_rregz(&x, &rd, instr_get_dst(i, 0)) &&
        encode_rregz(&x, &rn, instr_get_src(i, 0)) &&
        encode_rregz(&x, &rm, instr_get_src(i, 1)) &&
        encode_shift(&sh, instr_get_src(i, 2)) &&
        encode_imm(&imm6, (x ? 6 : 5), instr_get_src(i, 3)))
        return (enc | (uint)(x == OPSZ_8) << 31 |
                rd | rn << 5 | rm << 16 | sh << 22 | imm6 << 10);
    return ENCFAIL;
}

static inline bool
decode_b(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    instr_set_0dst_1src(dc, instr, op,
                        opnd_create_pc(pc + extract_int(enc, 0, 26) * 4));
    return true;
}

static inline uint
encode_b(byte *pc, instr_t *i, uint enc)
{
    uint off;
    if (encode_opnums(i, 0, 1) &&
        encode_pc_off(&off, 26, pc, i, instr_get_src(i, 0)))
        return (enc | (uint)(instr_get_opcode(i) == OP_bl) << 31 | off);
    return ENCFAIL;
}

static inline bool
decode_bcond(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    instr_set_0dst_1src(dc, instr, op,
                        opnd_create_pc(pc + extract_int(enc, 5, 19) * 4));
    instr_set_predicate(instr, enc & 15);
    return true;
}

static inline uint
encode_bcond(byte *pc, instr_t *i, uint enc)
{
    uint off;
    if (encode_opnums(i, 0, 1) &&
        encode_pc_off(&off, 19, pc, i, instr_get_src(i, 0)))
        return (enc | off << 5 | (instr_get_predicate(i) & 15));
    return ENCFAIL;
}

static inline bool
decode_br(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    instr_set_0dst_1src(dc, instr, op,
                        decode_xregz(enc >> 5 & 31));
    return true;
}

static inline uint
encode_br(byte *pc, instr_t *i, uint enc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return ENCFAIL;
}

static inline bool
decode_cbz(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    instr_set_0dst_2src(dc, instr, op,
                        opnd_create_pc(pc + extract_int(enc, 5, 19) * 4),
                        decode_rregz(TEST(1U << 31, enc), enc & 31));
    return true;
}

static inline uint
encode_cbz(byte *pc, instr_t *i, uint enc)
{
    opnd_size_t x = OPSZ_NA;
    uint rt, off;
    if (encode_opnums(i, 0, 2) &&
        encode_pc_off(&off, 19, pc, i, instr_get_src(i, 0)) &&
        encode_rregz(&x, &rt, instr_get_src(i, 1))) {
        return (enc | (uint)(x == OPSZ_8) << 31 | off << 5 | rt);
    }
    return ENCFAIL;
}

static inline bool
decode_ldr_imm(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    /* FIXME i#1569: NYI */
    return false;
}

static inline uint
encode_ldr_imm(byte *pc, instr_t *i, uint enc)
{
    opnd_size_t x = OPSZ_NA;
    opnd_size_t m = OPSZ_NA;
    uint rt, xn, imm12;
    if (encode_opnums(i, 1, 1) &&
        encode_rregz(&x, &rt, instr_get_dst(i, 0)) &&
        encode_base_imm(&m, &xn, &imm12, 12, false, instr_get_src(i, 0)) &&
        x == m && !TEST(opnd_size_in_bytes(m) - 1, imm12)) {
        return (enc | (uint)(m == OPSZ_8) << 30 |
                rt | xn << 5 | imm12 / opnd_size_in_bytes(m) << 10);
    }
    return ENCFAIL;
}

static inline bool
decode_ldr_imm_simd(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    /* FIXME i#1569: NYI */
    return false;
}

static inline uint
encode_ldr_imm_simd(byte *pc, instr_t *i, uint enc)
{
    opnd_size_t x = OPSZ_NA;
    opnd_size_t m = OPSZ_NA;
    uint vt, xn, imm12;
    if (encode_opnums(i, 1, 1) &&
        encode_vreg(&x, &vt, instr_get_dst(i, 0)) &&
        opnd_size_in_bytes(x) >= 4 &&
        encode_base_imm(&m, &xn, &imm12, 12, false, instr_get_src(i, 0)) &&
        x == m && !TEST(opnd_size_in_bytes(m) - 1, imm12)) {
        return (enc | (m == OPSZ_4 ? 0x80000000 :
                       m == OPSZ_8 ? 0xc0000000 : 0x00800000) |
                vt | xn | imm12 / opnd_size_in_bytes(m) << 10);
    }
    return ENCFAIL;
}

static inline bool
decode_ldr_literal(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    int offs = (enc >> 3 & 0xffffc) - (enc >> 3 & 0x100000);
    instr_set_1dst_1src(dc, instr, op,
                        decode_rregz(TEST(1 << 30, enc), enc & 31),
                        opnd_create_rel_addr(pc + offs, OPSZ_8));
    return true;
}

static inline uint
encode_ldr_literal(byte *pc, instr_t *i, uint enc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return ENCFAIL;
}

static inline bool
decode_ldr_literal_simd(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    int opc = enc >> 30 & 3;
    int offs = (enc >> 3 & 0xffffc) - (enc >> 3 & 0x100000);
    if (opc == 3)
        return false;
    instr_set_1dst_1src(dc, instr, op,
                        opnd_create_reg((opc == 0 ? DR_REG_S0 :
                                         opc == 1 ? DR_REG_D0 : DR_REG_Q0) +
                                        (enc & 31)),
                        opnd_create_rel_addr(pc + offs,
                                             opc == 0 ? OPSZ_4 :
                                             opc == 1 ? OPSZ_8 : OPSZ_16));
    return true;
}

static inline uint
encode_ldr_literal_simd(byte *pc, instr_t *i, uint enc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return ENCFAIL;
}

static inline bool
decode_mrs(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    instr_set_1dst_1src(dc, instr, op,
                        decode_xregz(enc & 31),
                        decode_sysreg(enc >> 5 & 0x7fff));
    return true;
}

static inline uint
encode_mrs(byte *pc, instr_t *i, uint enc)
{
    uint xt, imm15;
    if (encode_opnums(i, 1, 1) &&
        encode_xregz(&xt, instr_get_dst(i, 0)) &&
        encode_sysreg(&imm15, instr_get_src(i, 0)))
        return (enc | xt | imm15 << 5);
    return ENCFAIL;
}

static inline bool
decode_msr(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    opnd_t opnd = decode_sysreg(enc >> 5 & 0x7fff);
    if (opnd_is_reg(opnd)) {
        instr_set_1dst_1src(dc, instr, op, opnd, decode_xregz(enc & 31));
        return true;
    } else {
        instr_set_0dst_2src(dc, instr, op, decode_xregz(enc & 31), opnd);
        return true;
    }
    return false;
}

static inline uint
encode_msr(byte *pc, instr_t *i, uint enc)
{
    uint imm15, xt;
    if (encode_opnums(i, 1, 1) &&
        opnd_is_reg(instr_get_dst(i, 0)) &&
        encode_sysreg(&imm15, instr_get_dst(i, 0)) &&
        encode_xregz(&xt, instr_get_src(i, 0)))
        return (enc | xt | imm15 << 5);
    if (encode_opnums(i, 0, 2) &&
        opnd_is_immed_int(instr_get_src(i, 1)) &&
        encode_xregz(&xt, instr_get_src(i, 0)) &&
        encode_sysreg(&imm15, instr_get_src(i, 1)))
        return (enc | xt | imm15 << 5);
    return ENCFAIL;
}

static inline bool
decode_nop(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    instr_set_0dst_0src(dc, instr, op);
    return true;
}

static inline uint
encode_nop(byte *pc, instr_t *i, uint enc)
{
    if (encode_opnums(i, 0, 0))
        return enc;
    return ENCFAIL;
}

static inline bool
decode_str_imm(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    /* FIXME i#1569: NYI */
    return false;
}

static inline uint
encode_str_imm(byte *pc, instr_t *i, uint enc)
{
    opnd_size_t x = OPSZ_NA;
    opnd_size_t m = OPSZ_NA;
    uint rt, xn, imm12;
    if (encode_opnums(i, 1, 1) &&
        encode_base_imm(&m, &xn, &imm12, 12, false, instr_get_dst(i, 0)) &&
        encode_rregz(&x, &rt, instr_get_src(i, 0)) &&
        x == m && !TEST(opnd_size_in_bytes(m) - 1, imm12))
        return (enc | (uint)(m == OPSZ_8) << 30 |
                rt | xn << 5 | imm12 / opnd_size_in_bytes(m) << 10);
    return ENCFAIL;
}

static inline bool
decode_strb_imm(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    /* FIXME i#1569: NYI */
    return false;
}

static inline uint
encode_strb_imm(byte *pc, instr_t *i, uint enc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return ENCFAIL;
}

static inline bool
decode_svc(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    instr_set_0dst_1src(dc, instr, op,
                        OPND_CREATE_INT16(enc >> 5 & 0xffff));
    return true;
}

static inline uint
encode_svc(byte *pc, instr_t *i, uint enc)
{
    uint imm16;
    if (encode_opnums(i, 0, 1) &&
        encode_imm(&imm16, 16, instr_get_src(i, 0)))
        return (enc | imm16 << 5);
    return ENCFAIL;
}

static inline bool
decode_tbz(uint enc, dcontext_t *dc, byte *pc, instr_t *instr, int op)
{
    instr_set_0dst_3src(dc, instr, op,
                        opnd_create_pc(pc + extract_int(enc, 5, 14) * 4),
                        decode_xregz(enc & 31),
                        OPND_CREATE_INT8((enc >> 19 & 31) | (enc >> 26 & 32)));
    return true;
}

static inline uint
encode_tbz(byte *pc, instr_t *i, uint enc)
{
    uint xt, imm6, off;
    if (encode_opnums(i, 0, 3) &&
        encode_pc_off(&off, 14, pc, i, instr_get_src(i, 0)) &&
        encode_xregz(&xt, instr_get_src(i, 1)) &&
        encode_imm(&imm6, 6, instr_get_src(i, 2)))
        return (enc | (uint)(instr_get_opcode(i) == OP_tbnz) << 24 | off << 5 | xt |
                (imm6 & 31) << 19 | (imm6 & 32) << 26);
    return ENCFAIL;
}

/******************************************************************************/

#include "codec_gen.h"

/******************************************************************************/

byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr)
{
    byte *next_pc = pc + 4;
    uint enc = *(uint *)pc;

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

    instr_set_operands_valid(instr, true);

    if (orig_pc != pc) {
        /* We do not want to copy when encoding and condone an invalid
         * relative target.
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

uint encode_common(byte *pc, instr_t *i)
{
    uint enc;
    ASSERT(((ptr_int_t)pc & 3) == 0);
    enc = encoder(pc, i);
    if (enc != ENCFAIL)
        return enc;
    switch (instr_get_opcode(i)) {
    case OP_xx:
        ASSERT(instr_num_srcs(i) >= 1 && opnd_is_immed_int(instr_get_src(i, 0)));
        return opnd_get_immed_int(instr_get_src(i, 0));
    }
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return enc;
}
