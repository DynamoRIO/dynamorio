/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
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

/* AArch64 decoder and encoder functions.
 * This file is rather large and should perhaps be split up, but there are many
 * opportunities for inlining which could be lost if it were split into separate
 * translation units, and it is helpful to have the per-operand-type decode/encode
 * functions next to each other.
 */

#include <stdint.h>
#include "../globals.h"
#include "arch.h"
#include "decode.h"
#include "disassemble.h"
#include "instr.h"
#include "instr_create_shared.h"

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
    /* pos starts at bit 0 and len includes pos bit as part of its length. */
    return enc >> pos & (((uint)1 << len) - 1);
}

/* Find the highest bit set in subfield, relative to the starting position. */
static inline uint
highest_bit_set(uint enc, int pos, int len, int *highest_bit)
{
    for (int i = pos + len - 1; i >= pos; i--) {
        if (enc & (1 << i)) {
            *highest_bit = i - pos;
            return true;
        }
    }
    return false;
}

/* Find the lowest bit set in subfield, relative to the starting position. */
static inline uint
lowest_bit_set(uint enc, int pos, int len, int *lowest_bit)
{
    for (int i = pos; i < pos + len; i++) {
        if (enc & (1 << i)) {
            *lowest_bit = i - pos;
            return true;
        }
    }
    return false;
}

static inline aarch64_reg_offset
get_reg_offset(reg_t reg)
{
    if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31)
        return QUAD_REG;
    else if (reg >= DR_REG_D0 && reg <= DR_REG_D31)
        return DOUBLE_REG;
    else if (reg >= DR_REG_S0 && reg <= DR_REG_S31)
        return SINGLE_REG;
    else if (reg >= DR_REG_H0 && reg <= DR_REG_H31)
        return HALF_REG;
    else if (reg >= DR_REG_B0 && reg <= DR_REG_B31)
        return BYTE_REG;
    else
        return NOT_A_REG;
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
    /* If !di->check_reachable we do not require correct alignment for instr operands as
     * there is a common use case of a label instruction operand whose note value holds
     * an identifier used in instrumentation (i#5297).  For pc operands, we do require
     * correct alignment even if !di->check_reachable.
     */
    if (!di->check_reachable && (opnd.kind != PC_kind || ALIGNED(off, 4))) {
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
    case 0x1808: sysreg = DR_REG_MDCCSR_EL0; break;
    case 0x1820: sysreg = DR_REG_DBGDTR_EL0; break;
    case 0x1828: sysreg = DR_REG_DBGDTRRX_EL0; break;
    case 0x4208: sysreg = DR_REG_SP_EL0; break;
    case 0x4210: sysreg = DR_REG_SPSEL; break;
    case 0x4212: sysreg = DR_REG_CURRENTEL; break;
    case 0x4213: sysreg = DR_REG_PAN; break;
    case 0x4214: sysreg = DR_REG_UAO; break;
    case 0x5801: sysreg = DR_REG_CTR_EL0; break;
    case 0x5807: sysreg = DR_REG_DCZID_EL0; break;
    case 0x5920: sysreg = DR_REG_RNDR; break;
    case 0x5921: sysreg = DR_REG_RNDRRS; break;
    case 0x5a11: sysreg = DR_REG_DAIF; break;
    case 0x5a15: sysreg = DR_REG_DIT; break;
    case 0x5a16: sysreg = DR_REG_SSBS; break;
    case 0x5a17: sysreg = DR_REG_TCO; break;
    case 0x5a28: sysreg = DR_REG_DSPSR_EL0; break;
    case 0x5a29: sysreg = DR_REG_DLR_EL0; break;
    case 0x5ce0: sysreg = DR_REG_PMCR_EL0; break;
    case 0x5ce1: sysreg = DR_REG_PMCNTENSET_EL0; break;
    case 0x5ce2: sysreg = DR_REG_PMCNTENCLR_EL0; break;
    case 0x5ce3: sysreg = DR_REG_PMOVSCLR_EL0; break;
    case 0x5ce4: sysreg = DR_REG_PMSWINC_EL0; break;
    case 0x5ce5: sysreg = DR_REG_PMSELR_EL0; break;
    case 0x5ce6: sysreg = DR_REG_PMCEID0_EL0; break;
    case 0x5ce7: sysreg = DR_REG_PMCEID1_EL0; break;
    case 0x5ce8: sysreg = DR_REG_PMCCNTR_EL0; break;
    case 0x5ce9: sysreg = DR_REG_PMXEVTYPER_EL0; break;
    case 0x5cea: sysreg = DR_REG_PMXEVCNTR_EL0; break;
    case 0x5cf0: sysreg = DR_REG_PMUSERENR_EL0; break;
    case 0x5cf3: sysreg = DR_REG_PMOVSSET_EL0; break;
    case 0x5e82: sysreg = DR_REG_TPIDR_EL0; break;
    case 0x5e83: sysreg = DR_REG_TPIDRRO_EL0; break;
    case 0x5e87: sysreg = DR_REG_SCXTNUM_EL0; break;
    case 0x5f00: sysreg = DR_REG_CNTFRQ_EL0; break;
    case 0x5f01: sysreg = DR_REG_CNTPCT_EL0; break;
    case 0x5f02: sysreg = DR_REG_CNTVCT_EL0; break;
    case 0x5f10: sysreg = DR_REG_CNTP_TVAL_EL0; break;
    case 0x5f11: sysreg = DR_REG_CNTP_CTL_EL0; break;
    case 0x5f12: sysreg = DR_REG_CNTP_CVAL_EL0; break;
    case 0x5f18: sysreg = DR_REG_CNTV_TVAL_EL0; break;
    case 0x5f19: sysreg = DR_REG_CNTV_CTL_EL0; break;
    case 0x5f1a: sysreg = DR_REG_CNTV_CVAL_EL0; break;
    case 0x5f40: sysreg = DR_REG_PMEVCNTR0_EL0; break;
    case 0x5f41: sysreg = DR_REG_PMEVCNTR1_EL0; break;
    case 0x5f42: sysreg = DR_REG_PMEVCNTR2_EL0; break;
    case 0x5f43: sysreg = DR_REG_PMEVCNTR3_EL0; break;
    case 0x5f44: sysreg = DR_REG_PMEVCNTR4_EL0; break;
    case 0x5f45: sysreg = DR_REG_PMEVCNTR5_EL0; break;
    case 0x5f46: sysreg = DR_REG_PMEVCNTR6_EL0; break;
    case 0x5f47: sysreg = DR_REG_PMEVCNTR7_EL0; break;
    case 0x5f48: sysreg = DR_REG_PMEVCNTR8_EL0; break;
    case 0x5f49: sysreg = DR_REG_PMEVCNTR9_EL0; break;
    case 0x5f4a: sysreg = DR_REG_PMEVCNTR10_EL0; break;
    case 0x5f4b: sysreg = DR_REG_PMEVCNTR11_EL0; break;
    case 0x5f4c: sysreg = DR_REG_PMEVCNTR12_EL0; break;
    case 0x5f4d: sysreg = DR_REG_PMEVCNTR13_EL0; break;
    case 0x5f4e: sysreg = DR_REG_PMEVCNTR14_EL0; break;
    case 0x5f4f: sysreg = DR_REG_PMEVCNTR15_EL0; break;
    case 0x5f50: sysreg = DR_REG_PMEVCNTR16_EL0; break;
    case 0x5f51: sysreg = DR_REG_PMEVCNTR17_EL0; break;
    case 0x5f52: sysreg = DR_REG_PMEVCNTR18_EL0; break;
    case 0x5f53: sysreg = DR_REG_PMEVCNTR19_EL0; break;
    case 0x5f54: sysreg = DR_REG_PMEVCNTR20_EL0; break;
    case 0x5f55: sysreg = DR_REG_PMEVCNTR21_EL0; break;
    case 0x5f56: sysreg = DR_REG_PMEVCNTR22_EL0; break;
    case 0x5f57: sysreg = DR_REG_PMEVCNTR23_EL0; break;
    case 0x5f58: sysreg = DR_REG_PMEVCNTR24_EL0; break;
    case 0x5f59: sysreg = DR_REG_PMEVCNTR25_EL0; break;
    case 0x5f5a: sysreg = DR_REG_PMEVCNTR26_EL0; break;
    case 0x5f5b: sysreg = DR_REG_PMEVCNTR27_EL0; break;
    case 0x5f5c: sysreg = DR_REG_PMEVCNTR28_EL0; break;
    case 0x5f5d: sysreg = DR_REG_PMEVCNTR29_EL0; break;
    case 0x5f5e: sysreg = DR_REG_PMEVCNTR30_EL0; break;
    case 0x5f60: sysreg = DR_REG_PMEVTYPER0_EL0; break;
    case 0x5f61: sysreg = DR_REG_PMEVTYPER1_EL0; break;
    case 0x5f62: sysreg = DR_REG_PMEVTYPER2_EL0; break;
    case 0x5f63: sysreg = DR_REG_PMEVTYPER3_EL0; break;
    case 0x5f64: sysreg = DR_REG_PMEVTYPER4_EL0; break;
    case 0x5f65: sysreg = DR_REG_PMEVTYPER5_EL0; break;
    case 0x5f66: sysreg = DR_REG_PMEVTYPER6_EL0; break;
    case 0x5f67: sysreg = DR_REG_PMEVTYPER7_EL0; break;
    case 0x5f68: sysreg = DR_REG_PMEVTYPER8_EL0; break;
    case 0x5f69: sysreg = DR_REG_PMEVTYPER9_EL0; break;
    case 0x5f6a: sysreg = DR_REG_PMEVTYPER10_EL0; break;
    case 0x5f6b: sysreg = DR_REG_PMEVTYPER11_EL0; break;
    case 0x5f6c: sysreg = DR_REG_PMEVTYPER12_EL0; break;
    case 0x5f6d: sysreg = DR_REG_PMEVTYPER13_EL0; break;
    case 0x5f6e: sysreg = DR_REG_PMEVTYPER14_EL0; break;
    case 0x5f6f: sysreg = DR_REG_PMEVTYPER15_EL0; break;
    case 0x5f70: sysreg = DR_REG_PMEVTYPER16_EL0; break;
    case 0x5f71: sysreg = DR_REG_PMEVTYPER17_EL0; break;
    case 0x5f72: sysreg = DR_REG_PMEVTYPER18_EL0; break;
    case 0x5f73: sysreg = DR_REG_PMEVTYPER19_EL0; break;
    case 0x5f74: sysreg = DR_REG_PMEVTYPER20_EL0; break;
    case 0x5f75: sysreg = DR_REG_PMEVTYPER21_EL0; break;
    case 0x5f76: sysreg = DR_REG_PMEVTYPER22_EL0; break;
    case 0x5f77: sysreg = DR_REG_PMEVTYPER23_EL0; break;
    case 0x5f78: sysreg = DR_REG_PMEVTYPER24_EL0; break;
    case 0x5f79: sysreg = DR_REG_PMEVTYPER25_EL0; break;
    case 0x5f7a: sysreg = DR_REG_PMEVTYPER26_EL0; break;
    case 0x5f7b: sysreg = DR_REG_PMEVTYPER27_EL0; break;
    case 0x5f7c: sysreg = DR_REG_PMEVTYPER28_EL0; break;
    case 0x5f7d: sysreg = DR_REG_PMEVTYPER29_EL0; break;
    case 0x5f7e: sysreg = DR_REG_PMEVTYPER30_EL0; break;
    case 0x5f7f: sysreg = DR_REG_PMCCFILTR_EL0; break;
    case 0x6218: sysreg = DR_REG_SPSR_IRQ; break;
    case 0x6219: sysreg = DR_REG_SPSR_ABT; break;
    case 0x621a: sysreg = DR_REG_SPSR_UND; break;
    case 0x621b: sysreg = DR_REG_SPSR_FIQ; break;
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
        case DR_REG_MDCCSR_EL0: *imm15 = 0x1808; break;
        case DR_REG_DBGDTR_EL0: *imm15 = 0x1820; break;
        case DR_REG_DBGDTRRX_EL0: *imm15 = 0x1828; break;
        case DR_REG_SP_EL0: *imm15 = 0x4208; break;
        case DR_REG_SPSEL: *imm15 = 0x4210; break;
        case DR_REG_CURRENTEL: *imm15 = 0x4212; break;
        case DR_REG_PAN: *imm15 = 0x4213; break;
        case DR_REG_UAO: *imm15 = 0x4214; break;
        case DR_REG_CTR_EL0: *imm15 = 0x5801; break;
        case DR_REG_DCZID_EL0: *imm15 = 0x5807; break;
        case DR_REG_RNDR: *imm15 = 0x5920; break;
        case DR_REG_RNDRRS: *imm15 = 0x5921; break;
        case DR_REG_DAIF: *imm15 = 0x5a11; break;
        case DR_REG_DIT: *imm15 = 0x5a15; break;
        case DR_REG_SSBS: *imm15 = 0x5a16; break;
        case DR_REG_TCO: *imm15 = 0x5a17; break;
        case DR_REG_DSPSR_EL0: *imm15 = 0x5a28; break;
        case DR_REG_DLR_EL0: *imm15 = 0x5a29; break;
        case DR_REG_PMCR_EL0: *imm15 = 0x5ce0; break;
        case DR_REG_PMCNTENSET_EL0: *imm15 = 0x5ce1; break;
        case DR_REG_PMCNTENCLR_EL0: *imm15 = 0x5ce2; break;
        case DR_REG_PMOVSCLR_EL0: *imm15 = 0x5ce3; break;
        case DR_REG_PMSWINC_EL0: *imm15 = 0x5ce4; break;
        case DR_REG_PMSELR_EL0: *imm15 = 0x5ce5; break;
        case DR_REG_PMCEID0_EL0: *imm15 = 0x5ce6; break;
        case DR_REG_PMCEID1_EL0: *imm15 = 0x5ce7; break;
        case DR_REG_PMCCNTR_EL0: *imm15 = 0x5ce8; break;
        case DR_REG_PMXEVTYPER_EL0: *imm15 = 0x5ce9; break;
        case DR_REG_PMXEVCNTR_EL0: *imm15 = 0x5cea; break;
        case DR_REG_PMUSERENR_EL0: *imm15 = 0x5cf0; break;
        case DR_REG_PMOVSSET_EL0: *imm15 = 0x5cf3; break;
        case DR_REG_TPIDR_EL0: *imm15 = 0x5e82; break;
        case DR_REG_TPIDRRO_EL0: *imm15 = 0x5e83; break;
        case DR_REG_SCXTNUM_EL0: *imm15 = 0x5e87; break;
        case DR_REG_CNTFRQ_EL0: *imm15 = 0x5f00; break;
        case DR_REG_CNTPCT_EL0: *imm15 = 0x5f01; break;
        case DR_REG_CNTVCT_EL0: *imm15 = 0x5f02; break;
        case DR_REG_CNTP_TVAL_EL0: *imm15 = 0x5f10; break;
        case DR_REG_CNTP_CTL_EL0: *imm15 = 0x5f11; break;
        case DR_REG_CNTP_CVAL_EL0: *imm15 = 0x5f12; break;
        case DR_REG_CNTV_TVAL_EL0: *imm15 = 0x5f18; break;
        case DR_REG_CNTV_CTL_EL0: *imm15 = 0x5f19; break;
        case DR_REG_CNTV_CVAL_EL0: *imm15 = 0x5f1a; break;
        case DR_REG_PMEVCNTR0_EL0: *imm15 = 0x5f40; break;
        case DR_REG_PMEVCNTR1_EL0: *imm15 = 0x5f41; break;
        case DR_REG_PMEVCNTR2_EL0: *imm15 = 0x5f42; break;
        case DR_REG_PMEVCNTR3_EL0: *imm15 = 0x5f43; break;
        case DR_REG_PMEVCNTR4_EL0: *imm15 = 0x5f44; break;
        case DR_REG_PMEVCNTR5_EL0: *imm15 = 0x5f45; break;
        case DR_REG_PMEVCNTR6_EL0: *imm15 = 0x5f46; break;
        case DR_REG_PMEVCNTR7_EL0: *imm15 = 0x5f47; break;
        case DR_REG_PMEVCNTR8_EL0: *imm15 = 0x5f48; break;
        case DR_REG_PMEVCNTR9_EL0: *imm15 = 0x5f49; break;
        case DR_REG_PMEVCNTR10_EL0: *imm15 = 0x5f4a; break;
        case DR_REG_PMEVCNTR11_EL0: *imm15 = 0x5f4b; break;
        case DR_REG_PMEVCNTR12_EL0: *imm15 = 0x5f4c; break;
        case DR_REG_PMEVCNTR13_EL0: *imm15 = 0x5f4d; break;
        case DR_REG_PMEVCNTR14_EL0: *imm15 = 0x5f4e; break;
        case DR_REG_PMEVCNTR15_EL0: *imm15 = 0x5f4f; break;
        case DR_REG_PMEVCNTR16_EL0: *imm15 = 0x5f50; break;
        case DR_REG_PMEVCNTR17_EL0: *imm15 = 0x5f51; break;
        case DR_REG_PMEVCNTR18_EL0: *imm15 = 0x5f52; break;
        case DR_REG_PMEVCNTR19_EL0: *imm15 = 0x5f53; break;
        case DR_REG_PMEVCNTR20_EL0: *imm15 = 0x5f54; break;
        case DR_REG_PMEVCNTR21_EL0: *imm15 = 0x5f55; break;
        case DR_REG_PMEVCNTR22_EL0: *imm15 = 0x5f56; break;
        case DR_REG_PMEVCNTR23_EL0: *imm15 = 0x5f57; break;
        case DR_REG_PMEVCNTR24_EL0: *imm15 = 0x5f58; break;
        case DR_REG_PMEVCNTR25_EL0: *imm15 = 0x5f59; break;
        case DR_REG_PMEVCNTR26_EL0: *imm15 = 0x5f5a; break;
        case DR_REG_PMEVCNTR27_EL0: *imm15 = 0x5f5b; break;
        case DR_REG_PMEVCNTR28_EL0: *imm15 = 0x5f5c; break;
        case DR_REG_PMEVCNTR29_EL0: *imm15 = 0x5f5d; break;
        case DR_REG_PMEVCNTR30_EL0: *imm15 = 0x5f5e; break;
        case DR_REG_PMEVTYPER0_EL0: *imm15 = 0x5f60; break;
        case DR_REG_PMEVTYPER1_EL0: *imm15 = 0x5f61; break;
        case DR_REG_PMEVTYPER2_EL0: *imm15 = 0x5f62; break;
        case DR_REG_PMEVTYPER3_EL0: *imm15 = 0x5f63; break;
        case DR_REG_PMEVTYPER4_EL0: *imm15 = 0x5f64; break;
        case DR_REG_PMEVTYPER5_EL0: *imm15 = 0x5f65; break;
        case DR_REG_PMEVTYPER6_EL0: *imm15 = 0x5f66; break;
        case DR_REG_PMEVTYPER7_EL0: *imm15 = 0x5f67; break;
        case DR_REG_PMEVTYPER8_EL0: *imm15 = 0x5f68; break;
        case DR_REG_PMEVTYPER9_EL0: *imm15 = 0x5f69; break;
        case DR_REG_PMEVTYPER10_EL0: *imm15 = 0x5f6a; break;
        case DR_REG_PMEVTYPER11_EL0: *imm15 = 0x5f6b; break;
        case DR_REG_PMEVTYPER12_EL0: *imm15 = 0x5f6c; break;
        case DR_REG_PMEVTYPER13_EL0: *imm15 = 0x5f6d; break;
        case DR_REG_PMEVTYPER14_EL0: *imm15 = 0x5f6e; break;
        case DR_REG_PMEVTYPER15_EL0: *imm15 = 0x5f6f; break;
        case DR_REG_PMEVTYPER16_EL0: *imm15 = 0x5f70; break;
        case DR_REG_PMEVTYPER17_EL0: *imm15 = 0x5f71; break;
        case DR_REG_PMEVTYPER18_EL0: *imm15 = 0x5f72; break;
        case DR_REG_PMEVTYPER19_EL0: *imm15 = 0x5f73; break;
        case DR_REG_PMEVTYPER20_EL0: *imm15 = 0x5f74; break;
        case DR_REG_PMEVTYPER21_EL0: *imm15 = 0x5f75; break;
        case DR_REG_PMEVTYPER22_EL0: *imm15 = 0x5f76; break;
        case DR_REG_PMEVTYPER23_EL0: *imm15 = 0x5f77; break;
        case DR_REG_PMEVTYPER24_EL0: *imm15 = 0x5f78; break;
        case DR_REG_PMEVTYPER25_EL0: *imm15 = 0x5f79; break;
        case DR_REG_PMEVTYPER26_EL0: *imm15 = 0x5f7a; break;
        case DR_REG_PMEVTYPER27_EL0: *imm15 = 0x5f7b; break;
        case DR_REG_PMEVTYPER28_EL0: *imm15 = 0x5f7c; break;
        case DR_REG_PMEVTYPER29_EL0: *imm15 = 0x5f7d; break;
        case DR_REG_PMEVTYPER30_EL0: *imm15 = 0x5f7e; break;
        case DR_REG_PMCCFILTR_EL0: *imm15 = 0x5f7f; break;
        case DR_REG_SPSR_IRQ: *imm15 = 0x6218; break;
        case DR_REG_SPSR_ABT: *imm15 = 0x6219; break;
        case DR_REG_SPSR_UND: *imm15 = 0x621a; break;
        case DR_REG_SPSR_FIQ: *imm15 = 0x621b; break;
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
    dr_extend_type_t extend;
    switch (enc >> 13 & 7) {
    case 0b010: extend = DR_EXTEND_UXTW; break;
    // Alias for LSL. LSL preferred in disassembly.
    case 0b011: extend = DR_EXTEND_UXTX; break;
    case 0b110: extend = DR_EXTEND_SXTW; break;
    case 0b111: extend = DR_EXTEND_SXTX; break;
    default: return false;
    }

    *opnd = opnd_create_base_disp_aarch64(
        decode_reg(enc >> 5 & 31, true, true),
        decode_reg(enc >> 16 & 31, TEST(1U << 13, enc), false), extend,
        TEST(1U << 12, enc), 0, 0, size);
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
        !encode_reg(&rm, &xm, opnd_get_index(opnd), false) || (!xm && (option & 1) != 0))
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
    *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_HALF, OPSZ_1);
    return true;
}

static inline bool
encode_opnd_h_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    if ((opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_HALF) &&
        (opnd_get_size(opnd) == OPSZ_1))
        return true;
    return false;
}

/* b_const_sz: Operand size for byte elements
 */
static inline bool
decode_opnd_b_const_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_BYTE, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_b_const_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    if (opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_BYTE)
        return true;
    return false;
}

/* s_const_sz: Operand size for single (32-bit) element
 */
static inline bool
decode_opnd_s_const_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_s_const_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    if (opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_SINGLE)
        return true;
    return false;
}

/* d_const_sz: Operand size for double elements
 */
static inline bool
decode_opnd_d_const_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_DOUBLE, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_d_const_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    if (opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_DOUBLE)
        return true;
    return false;
}

/* vindex_D1: implicit index, always 1 */

static inline bool
decode_opnd_vindex_D1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_immed_int(1, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_vindex_D1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    if (opnd_get_immed_int(opnd) == 1)
        return true;
    return false;
}

/* Zero_const: implicit imm, always 0 */

static inline bool
decode_opnd_zero_fp_const(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_immed_float(0);
    return true;
}

static inline bool
encode_opnd_zero_fp_const(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_float(opnd))
        return false;

    if (opnd_get_immed_float(opnd) == 0)
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

/* memx0: memory operand with no offset used as memref for SYS */

static inline bool
decode_opnd_memx0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    *opnd = opnd_create_base_disp(decode_reg(extract_uint(enc, 0, 5), true, false),
                                  DR_REG_NULL, 0, 0, OPSZ_sys);
    return true;
}

static inline bool
encode_opnd_memx0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    uint xn;
    bool is_x;
    /* Only a base address in X reg is valid */
    if (!opnd_is_base_disp(opnd) || !encode_reg(&xn, &is_x, opnd_get_base(opnd), false) ||
        !is_x || opnd_get_size(opnd) != OPSZ_sys || opnd_get_scale(opnd) != 0 ||
        opnd_get_disp(opnd) != 0 || opnd_get_index(opnd) != DR_REG_NULL)
        return false;
    *enc_out = xn;
    return true;
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

/* op2: 3-bit immediate from bits 5-7 */

static inline bool
decode_opnd_op2(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(5, 3, false, 0, OPSZ_3b, 0, enc, opnd);
}

static inline bool
encode_opnd_op2(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(5, 3, false, 0, 0, opnd, enc_out);
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

/* b5: B register at bit position 5 */
static inline bool
decode_opnd_b5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(5, 0, enc, opnd);
}

static inline bool
encode_opnd_b5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(5, 0, opnd, enc_out);
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

#define CMODE_MSL_BIT 28

/* cmode4_s_sz_msl: Operand for 32 bit elements' shift amount (shifting ones) */

static inline bool
decode_opnd_cmode4_s_sz_msl(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    /* cmode size shift amounts
     * 110x  32   8,16
     * This is an MSL (Modified Shift Left). Unlike an LSL (Logical Shift
     * Left), this left shift shifts ones instead of zeros into the low order
     * bits.
     *
     * The element size and shift amount are stored as two 32 bit numbers in
     * sz_shft. This is a workaround until issue i#4393 is addressed.
     */
    const int cmode4 = extract_uint(enc, 12, 1);
    const int size = 32;
    const int shift = ((cmode4 == 0) ? 8 : 16) | (1U << CMODE_MSL_BIT);
    uint64 sz_shft = ((uint64)size << 32) | shift;
    *opnd = opnd_create_immed_int(sz_shft, OPSZ_8);
    return true;
}

static inline bool
encode_opnd_cmode4_s_sz_msl(uint enc, int opcode, byte *pc, opnd_t opnd,
                            OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    int64 sz_shft = opnd_get_immed_int(opnd);
    int shift = (int)(sz_shft & 0xffffffff);
    if (!TEST(1U << CMODE_MSL_BIT, shift)) // MSL bit should be set
        return false;
    shift &= 0xff;
    const int size = (int)(sz_shft >> 32);

    if (size != 32)
        return false;

    int cmode4;
    if (shift == 8)
        cmode4 = 0;
    else if (shift == 16)
        cmode4 = 1;
    else
        return false;

    opnd = opnd_create_immed_uint(cmode4, OPSZ_1b);
    encode_opnd_int(12, 1, false, false, 0, opnd, enc_out);
    return true;
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

/* cmode_h_sz: Operand for 16 bit elements' shift amount */

static inline bool
decode_opnd_cmode_h_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    /* cmode size amounts
     * 10x0  16   0,8
     *
     * The element size and shift amount are stored as two 32 bit numbers in
     * sz_shft. This is a workaround until issue i#4393 is addressed.
     */
    const int cmode = extract_uint(enc, 13, 1);
    int size = 16;
    const int shift = (cmode == 0) ? 0 : 8;
    const uint64 sz_shft = ((uint64)size << 32) | shift;
    *opnd = opnd_create_immed_int(sz_shft, OPSZ_8);
    return true;
}

static inline bool
encode_opnd_cmode_h_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    const int64 sz_shft = opnd_get_immed_int(opnd);
    const int shift = (int)(sz_shft & 0xFF);
    int size = (int)(sz_shft >> 32);

    if (size != 16)
        return false;

    int cmode;
    if (shift == 0)
        cmode = 0;
    else if (shift == 8)
        cmode = 1;
    else
        return false;

    opnd = opnd_create_immed_uint(cmode, OPSZ_1b);
    encode_opnd_int(13, 1, false, false, 0, opnd, enc_out);
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

/* cmode_s_sz: Operand for 32 bit elements' shift amount */

static inline bool
decode_opnd_cmode_s_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    /* cmode size amounts
     * 0xx0  32   0,8,16,24
     *
     * The element size and shift amount are stored as two 32 bit numbers in
     * sz_shft. This is a workaround until issue i#4393 is addressed.
     */
    const int cmode = extract_uint(enc, 13, 2);
    const int size = 32;
    int shift;
    switch (cmode) {
    case 0: shift = 0; break;
    case 1: shift = 8; break;
    case 2: shift = 16; break;
    case 3: shift = 24; break;
    default: return false;
    }
    const uint64 sz_shft = ((uint64)size << 32) | shift;
    *opnd = opnd_create_immed_int(sz_shft, OPSZ_8);
    return true;
}

static inline bool
encode_opnd_cmode_s_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    const int64 sz_shft = opnd_get_immed_int(opnd);
    const int shift = (int)(sz_shft & 0xffffffff);
    if (TEST(1U << CMODE_MSL_BIT, shift)) // MSL bit should not be set as this is LSL
        return false;
    const int size = (int)(sz_shft >> 32);

    if (size != 32)
        return false;

    int cmode;
    switch (shift) {
    case 0: cmode = 0; break;
    case 8: cmode = 1; break;
    case 16: cmode = 2; break;
    case 24: cmode = 3; break;
    default: return false;
    }

    opnd = opnd_create_immed_uint(cmode, OPSZ_2b);
    encode_opnd_int(13, 2, false, false, 0, opnd, enc_out);
    return true;
}

/* len: imm2 at bits 13 & 14 */

static inline bool
decode_opnd_len(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(13, 2, false, 0, OPSZ_2b, 0, enc, opnd);
}

static inline bool
encode_opnd_len(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(13, 2, false, 0, 0, opnd, enc_out);
}

/* imm4 encoded in bits 11-14 */
static inline bool
decode_opnd_imm4idx(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint value = extract_uint(enc, 11, 4);
    *opnd = opnd_create_immed_uint(value, OPSZ_4b);
    return true;
}

static inline bool
encode_opnd_imm4idx(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;
    return encode_opnd_int(11, 4, false, 0, 0, opnd, enc_out);
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

/* cmode4_b_sz : Operand for byte elements' shift amount
 */
static inline bool
decode_opnd_cmode4_b_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    /* cmode size shift amount
     * 1110  8    0
     *
     * The element size and shift amount are stored as two 32 bit numbers in
     * sz_shft. This is a workaround until issue i#4393 is addressed.
     */
    if ((enc & 0xf000) != 0xe000)
        return false;
    const int size = 8;
    const uint64 sz_shft = (uint64)size << 32;
    *opnd = opnd_create_immed_int(sz_shft, OPSZ_8);
    return true;
}

static inline bool
encode_opnd_cmode4_b_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    const int size = 8;
    if (opnd_is_immed_int(opnd) && opnd_get_immed_int(opnd) == ((uint64)size << 32))
        return true;
    return false;
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

/* crn: 4-bit immediate from bits 12-15 */

static inline bool
decode_opnd_crn(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(12, 4, false, 0, OPSZ_4b, 0, enc, opnd);
}

static inline bool
encode_opnd_crn(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(12, 4, false, 0, 0, opnd, enc_out);
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

/* scale: The scalar encoding of #fbits operand. This is the number of bits
 * after the decimal point for fixed-point values.
 */
static inline bool
decode_opnd_scale(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint scale = extract_uint(enc, 10, 6);
    *opnd = opnd_create_immed_int(64 - scale, OPSZ_6b);
    return true;
}

static inline bool
encode_opnd_scale(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    ptr_int_t fbits;

    if (!opnd_is_immed_int(opnd))
        return false;

    fbits = opnd_get_immed_int(opnd);

    if (fbits < 1 || fbits > 64)
        return false;

    *enc_out = (64 - fbits) << 10; /* 'scale' bitfield in encoding */

    return true;
}

static inline bool
decode_opnd_imm16_0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    uint value = extract_uint(enc, 0, 16);
    *opnd = opnd_create_immed_int(value, OPSZ_2);
    return true;
}

static inline bool
encode_opnd_imm16_0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    uint value;

    if (!opnd_is_immed_int(opnd))
        return false;

    value = opnd_get_immed_int(opnd);

    opnd = opnd_create_immed_uint(value, OPSZ_2);
    uint enc_value;
    encode_opnd_int(0, 16, false, false, 0, opnd, &enc_value);
    *enc_out = enc_value;
    return true;
}

/* op1: 3-bit immediate from bits 16-18 */

static inline bool
decode_opnd_op1(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(16, 3, false, 0, OPSZ_3b, 0, enc, opnd);
}

static inline bool
encode_opnd_op1(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(16, 3, false, 0, 0, opnd, enc_out);
}

/* pstate: decode pstate from 5-7 and 16-18 */

static inline bool
decode_opnd_pstate(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int lower = enc >> 5 & 0b111;
    int upper = enc >> 16 & 0b111;
    int both = lower | upper << 3;

    reg_t pstate;
    switch (both) {
    case 0b000101: pstate = DR_REG_SPSEL; break;
    case 0b011110: pstate = DR_REG_DAIFSET; break;
    case 0b011111: pstate = DR_REG_DAIFCLR; break;
    default: return false;
    }

    *opnd = opnd_create_reg(pstate);
    return true;
}

static inline bool
encode_opnd_pstate(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    int upper, lower;
    if (!opnd_is_reg(opnd))
        return false;

    switch (opnd_get_reg(opnd)) {
    case DR_REG_SPSEL:
        upper = 0b000;
        lower = 0b101;
        break;
    case DR_REG_DAIFSET:
        upper = 0b011;
        lower = 0b110;
        break;
    case DR_REG_DAIFCLR:
        upper = 0b011;
        lower = 0b111;
        break;
    default: return false;
    }

    *enc_out = upper << 16 | lower << 5;

    return true;
}

/* fpimm8: immediate operand for SIMD fmov */

static inline bool
decode_opnd_fpimm8(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    /* See Arm Architecture Reference Manual
     *
     * Immediate is encoded as 8 bits. Bits 5->9 and 16->18. LSB is bit 5:
     * imm8 = a:b:c:d:e:f:g:h (LSB)
     *
     * Half-precision (v8.2)
     * --------------
     *
     * imm16 = imm8<7>:NOT(imm8<6>):Replicate(imm8<6>,2):imm8<5:0>:Zeros(6);
     *         a:~b:bb:cdefgh:000000
     *
     * datasize = if Q == '1' then 128 else 64;
     * imm = Replicate(imm16, datasize DIV 16);
     *     = imm16:imm16:imm16:imm16                         (Q=0 -> 64)
     *     = imm16:imm16:imm16:imm16:imm16:imm16:imm16:imm16 (Q=1 -> 128)
     *
     * Single-precision (TODO)
     * ----------------
     * Assume cmode = 1111 and op = 0
     *
     * imm32 = imm8<7>:NOT(imm8<6>):Replicate(imm8<6>,5):imm8<5:0>:Zeros(19);
     *         a:~b:bbbbb:cdefgh:0000000000000000000
     *
     * imm64 = Replicate(imm32, 2);
     *       = a:~b:bbbbb:cdefgh:0000000000000000000 a:~b:bbbbb:cdefgh:0000000000000000000
     *
     * datasize = if Q == '1' then 128 else 64;
     * imm = Replicate(imm64, datasize DIV 64);
     *     = imm64       (Q=0)
     *     = imm64:imm64 (Q=1)
     */
    union {
#ifdef HAVE_HALF_FLOAT
        __fp16 f;
        uint16_t i;
#else
        /* For platforms on which 16 bit (half-precision) FP is not yet available. */
        float f;
        uint32_t i;
#endif
    } fpv;

    int abc = extract_uint(enc, 16, 3);
    int defgh = extract_uint(enc, 5, 5);

    uint a = (abc & 0x4);
    uint b = (abc & 0x2);
    uint not_b = b == 0 ? 1 : 0;

#ifdef HAVE_HALF_FLOAT
    uint bb = ((b == 0) ? 0 : 0x3);
#else
    uint bbbbb = ((b == 0) ? 0 : 0x1f);
#endif

    uint cdefgh = ((abc & 0x1) << 5) | (defgh & 0x1f);

#ifdef HAVE_HALF_FLOAT
    uint16_t imm16 = (a << 13) | (not_b << 14) | (bb << 12) | (cdefgh << 6);
    fpv.i = imm16;
#else
    uint32_t imm32 = (a << 29) | (not_b << 30) | (bbbbb << 25) | (cdefgh << 19);
    fpv.i = imm32;
#endif
    *opnd = opnd_create_immed_float(fpv.f);

    return true;
}

static inline bool
encode_opnd_fpimm8(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    /* Based on the IEEE 754-2008 standard but with Arm-specific details that
     * are left open by the standard. See Arm Architecture Reference Manual.
     *
     * Half-precision example
     *   __   ________
     * S/exp\/fraction\
     *  _
     * abbbcdefgh000000
     * 0011110000000000 = 1.0
     *    _
     *   abbb cdef gh00 0000
     * 0x8    0    0    0     a
     * 0x1    0    0    0     b
     * 0x0    8    0    0     c
     * 0x0    7    c    0     defgh
     */
    union {
#ifdef HAVE_HALF_FLOAT
        __fp16 f;
        uint16_t i;
#else
        /* For platforms on which 16 bit (half-precision) FP is not yet available. */
        float f;
        uint32_t i;
#endif
    } fpv;

    if (!opnd_is_immed_float(opnd))
        return false;

    fpv.f = opnd_get_immed_float(opnd);
#ifdef HAVE_HALF_FLOAT
    uint16_t imm = fpv.i;
    uint a = (imm & 0x8000);
    uint b = (imm & 0x1000);
    uint c = (imm & 0x800);
    uint defgh = (imm & 0x7c0);

    /* 3332 2222 2222 1111 1111 11
     * 1098 7654 3210 9876 5432 1098 7654 3210
     * ---- ---- ---- -abc ---- --de fgh- ----   immediate encoding
     *          0x8000 |<-3|  | ||
     *          0x1000  |<-5--| ||
     *           0x800   |<--5--||
     *           0x7c0           |>
     */
    *enc_out = (a << 3) | (b << 5) | (c << 5) | (defgh >> 1);
#else
    /* 3332 2222 2222 1111 1111 11
     * 1098 7654 3210 9876 5432 1098 7654 3210
     *  _
     * abbb bbbc defg h000 0000 0000 0000 0000
     */
    uint32_t imm = fpv.i;
    uint a = (imm & 0x80000000);
    uint b = (imm & 0x10000000);
    uint c = (imm & 0x1000000);
    uint defgh = (imm & 0xf80000);
    *enc_out = (a >> 13) | (b >> 11) | (c >> 8) | (defgh >> 14);
#endif
    return true;
}

/* imm8: an 8 bit uint stitched together from 2 parts of bits 16-18 and 5-9*/

static inline bool
decode_opnd_imm8(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int value_0 = extract_uint(enc, 16, 3);
    int value_1 = extract_uint(enc, 5, 5);
    int value = (value_0 << 5) | value_1;
    *opnd = opnd_create_immed_uint(value, OPSZ_1);
    return true;
}

static inline bool
encode_opnd_imm8(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;
    uint eight_bits = opnd_get_immed_int(opnd);

    uint enc_top = 0;
    opnd = opnd_create_immed_uint((eight_bits >> 5) & 0b111, OPSZ_3b);
    encode_opnd_int(16, 3, false, false, 0, opnd, &enc_top);

    uint enc_bottom = 0;
    opnd = opnd_create_immed_uint(eight_bits & 0b11111, OPSZ_5b);
    encode_opnd_int(5, 5, false, false, 0, opnd, &enc_bottom);

    *enc_out = enc_top | enc_bottom;
    return true;
}

/* exp_imm8 Encode and decode functions for the expanded imm format
   The expanded imm format takes the bits from 16-18 and 5-9 and expands
   them to a 64bit int.

   It does this by taking each bit in turn and repeating it 8 times so,
   abcdefgh
   becomes
   aaaaaaaabbbbbbbbccccccccddddddddeeeeeeeefffffffgggggggghhhhhhh
*/

static inline bool
decode_opnd_exp_imm8(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    const uint repeats = 8;
    uint upper_bits = extract_uint(enc, 16, 3);
    uint lower_bits = extract_uint(enc, 5, 5);
    uint bit_value = (upper_bits << 5) | lower_bits;
    uint64 value = 0;
    for (uint i = 0; i < repeats; i++) {
        uint64 bit = (bit_value & (1 << i)) >> i;
        if (bit == 1) /* bit = 0 is already set, don't do unnecessary work*/
            for (uint j = 0; j < repeats; j++)
                value |= bit << (i * repeats + j);
    }
    *opnd = opnd_create_immed_uint(value, OPSZ_8);
    return true;
}

static inline bool
encode_opnd_exp_imm8(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;
    uint64 value = opnd_get_immed_int(opnd);

    const uint first_top_bit = 5;
    const uint num_top_bits = 3;
    const uint first_bottom_bit = 0;
    const uint num_bottom_bits = 5;

    /*
    The below code recompresses the repeated bits by selecting the first
    bit of the group &(1 << (i * 8)) and then shifts it back to its
    original position (i *7 + offset)
    */

    uint top_bits = 0;
    uint enc_top = 0;
    for (uint i = first_top_bit; i < first_top_bit + num_top_bits; i++)
        top_bits |= (value & (uint64)1 << (i * 8)) >> (i * 7 + first_top_bit);
    opnd = opnd_create_immed_uint(top_bits, OPSZ_3b);
    encode_opnd_int(16, num_top_bits, false, false, 0, opnd, &enc_top);

    uint bottom_bits = 0;
    uint enc_bottom = 0;
    for (uint i = first_bottom_bit; i < first_bottom_bit + num_bottom_bits; i++)
        bottom_bits |= (value & (uint64)1 << (i * 8)) >> (i * 7 + first_bottom_bit);
    opnd = opnd_create_immed_uint(bottom_bits, OPSZ_5b);
    encode_opnd_int(5, num_bottom_bits, false, false, 0, opnd, &enc_bottom);

    *enc_out = enc_top | enc_bottom;
    return true;
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

static inline bool
imm5_sz_decode(uint max_size, uint enc, OUT opnd_t *opnd)
{
    int lowest_bit;
    if (!lowest_bit_set(enc, 16, 5, &lowest_bit))
        return false;

    if (lowest_bit > max_size)
        return false;

    switch (lowest_bit) {
    case BYTE_REG: *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_BYTE, OPSZ_2b); break;
    case HALF_REG: *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_HALF, OPSZ_2b); break;
    case SINGLE_REG:
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_2b);
        break;
    case DOUBLE_REG:
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_DOUBLE, OPSZ_2b);
        break;
    default: return false;
    }
    return true;
}

static inline bool
imm5_sz_encode(ptr_int_t max_size, bool write_out, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    ptr_int_t size = opnd_get_immed_int(opnd);

    if (size > max_size)
        return false;

    uint imm;
    switch (size) {
    case VECTOR_ELEM_WIDTH_BYTE: imm = 0b00001; break;
    case VECTOR_ELEM_WIDTH_HALF: imm = 0b00010; break;
    case VECTOR_ELEM_WIDTH_SINGLE: imm = 0b00100; break;
    case VECTOR_ELEM_WIDTH_DOUBLE: imm = 0b01000; break;
    default: return false;
    }

    if (write_out)
        *enc_out = imm << 16;

    return true;
}

/* bh_imm5_sz: The element size of a vector mediated by imm5 with possible values b or h
 */
static inline bool
decode_opnd_bh_imm5_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{

    return imm5_sz_decode(HALF_REG, enc, opnd);
}

static inline bool
encode_opnd_bh_imm5_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return imm5_sz_encode(VECTOR_ELEM_WIDTH_HALF, false, opnd, enc_out);
}

/* bhs_imm5_sz: The element size of a vector mediated by imm5 with possible values b, h
 * and s
 */
static inline bool
decode_opnd_bhs_imm5_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return imm5_sz_decode(SINGLE_REG, enc, opnd);
}

static inline bool
encode_opnd_bhs_imm5_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return imm5_sz_encode(VECTOR_ELEM_WIDTH_SINGLE, false, opnd, enc_out);
}

/* bhsd_imm5_sz: The element size of a vector mediated by imm5 with possible values b, h,
 * s and d
 */
static inline bool
decode_opnd_bhsd_imm5_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return imm5_sz_decode(DOUBLE_REG, enc, opnd);
}

static inline bool
encode_opnd_bhsd_imm5_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return imm5_sz_encode(VECTOR_ELEM_WIDTH_DOUBLE, false, opnd, enc_out);
}

/* wx5_imm5: bits 5-9 is a GPR whose width is dependent on information in
   an imm5 from bits 16-20
*/
static inline bool
decode_opnd_wx5_imm5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int lowest_bit;
    if (!lowest_bit_set(enc, 16, 5, &lowest_bit) || lowest_bit == 5)
        return false;

    bool is_x_register = lowest_bit == 3;
    *opnd = opnd_create_reg(decode_reg(extract_uint(enc, 5, 5), is_x_register, false));

    return true;
}

static inline bool
encode_opnd_wx5_imm5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_reg(opnd))
        ASSERT(false);
    uint num;
    bool is_x;
    if (!encode_reg(&num, &is_x, opnd_get_reg(opnd), false))
        ASSERT(false);
    *enc_out = num << 5;
    return true;
}

/* imm5: immediate operand for conditional compare (immediate) */

static inline bool
decode_opnd_imm5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_int(16, 5, false, 0, OPSZ_5b, 0, enc, opnd);
}

static inline bool
encode_opnd_imm5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_int(16, 5, false, 0, 0, opnd, enc_out);
}

/* bhs_imm5_sz_s: The element size of a vector mediated by imm5 with possible values b, h,
 * and s. Some instructions don't use the value space in the imm5 structure, so the
 * usual strategy of allowing them to handle writing of the encoding don't work here
 * and we have to explicitly do the encoding.
 */
static inline bool
decode_opnd_bhs_imm5_sz_s(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return imm5_sz_decode(SINGLE_REG, enc, opnd);
}

static inline bool
encode_opnd_bhs_imm5_sz_s(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return imm5_sz_encode(VECTOR_ELEM_WIDTH_SINGLE, true, opnd, enc_out);
}

/* bhsd_imm5_sz_s: The element size of a vector mediated by imm5 with possible values b,
 * h, s and d and writing out the encoding
 */
static inline bool
decode_opnd_bhsd_imm5_sz_s(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return imm5_sz_decode(DOUBLE_REG, enc, opnd);
}

static inline bool
encode_opnd_bhsd_imm5_sz_s(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return imm5_sz_encode(VECTOR_ELEM_WIDTH_DOUBLE, true, opnd, enc_out);
}

/* imm5_idx: Extract the index portion from the imm5 field
 */
static inline bool
decode_opnd_imm5_idx(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int lowest_bit;
    if (!lowest_bit_set(enc, 16, 5, &lowest_bit))
        return false;

    uint imm5_index = extract_uint(enc, 16 + lowest_bit + 1, 4 - lowest_bit);
    opnd_size_t index_size;
    switch (lowest_bit) {
    case 0: index_size = OPSZ_4b; break;
    case 1: index_size = OPSZ_3b; break;
    case 2: index_size = OPSZ_2b; break;
    case 3: index_size = OPSZ_1b; break;
    default: return false;
    }

    *opnd = opnd_create_immed_int(imm5_index, index_size);

    return true;
}

static inline bool
encode_opnd_imm5_idx(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    opnd_size_t index_size = opnd_get_size(opnd);
    uint lowest_bit;
    switch (index_size) {
    case OPSZ_4b: lowest_bit = 0; break;
    case OPSZ_3b: lowest_bit = 1; break;
    case OPSZ_2b: lowest_bit = 2; break;
    case OPSZ_1b: lowest_bit = 3; break;
    default: return false;
    }
    ptr_int_t index;

    if (!opnd_is_immed_int(opnd))
        return false;

    index = opnd_get_immed_int(opnd);
    uint min_index = 0;
    uint max_index = (1 << opnd_size_in_bits(index_size)) - 1;

    if (index < min_index || index > max_index)
        return false;

    uint index_encoding = index << (lowest_bit + 1) | 1 << lowest_bit;

    *enc_out = (index_encoding << 16);

    return true;
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

/* b16: B register at bit position 16. */

static inline bool
decode_opnd_b16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(16, 0, enc, opnd);
}

static inline bool
encode_opnd_b16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(16, 0, opnd, enc_out);
}

/* h16: H register at bit position 16. */

static inline bool
decode_opnd_h16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(16, 1, enc, opnd);
}

static inline bool
encode_opnd_h16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(16, 1, opnd, enc_out);
}

/* s16: S register at bit position 16. */

static inline bool
decode_opnd_s16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_vector_reg(16, 2, enc, opnd);
}

static inline bool
encode_opnd_s16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_vector_reg(16, 2, opnd, enc_out);
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

static bool
encode_opnd_instr(int bit_pos, opnd_t opnd, byte *start_pc, instr_t *containing_instr,
                  OUT uint *enc_out)
{
    if (!opnd_is_instr(opnd)) {
        return false;
    }
    ptr_uint_t val =
        ((ptr_uint_t)instr_get_note(opnd_get_instr(opnd)) -
         (ptr_uint_t)instr_get_note(containing_instr) + (ptr_uint_t)start_pc) >>
        opnd_get_shift(opnd);

    uint bits = opnd_size_in_bits(opnd_get_size(opnd));
    // We expect truncation; instrlist_insert_mov_instr_addr splits the instr's
    // encoded address into INSTR_kind operands in multiple mov instructions in the
    // ilist, each representing a 2-byte portion of the complete address.
    val &= ((1 << bits) - 1);

    ASSERT((*enc_out & (val << bit_pos)) == 0);
    *enc_out |= (val << bit_pos);
    return true;
}

static inline bool
encode_opnd_imm16(uint enc, int opcode, byte *start_pc, opnd_t opnd,
                  instr_t *containing_instr, OUT uint *enc_out)
{
    if (opnd_is_immed_int(opnd))
        return encode_opnd_int(5, 16, false, 0, 0, opnd, enc_out);
    else if (opnd_is_instr(opnd))
        return encode_opnd_instr(5, opnd, start_pc, containing_instr, enc_out);
    ASSERT_NOT_REACHED();
    return false;
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
        *opnd = opnd_create_immed_int(bytes, OPSZ_PTR);
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
        *opnd = opnd_create_immed_int(bytes, OPSZ_PTR);
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
    /* Example encoding:
     * FMLA <Vd>.<T>, <Vn>.<T>, <Vm>.H[<index>]
     * 3322222222221111111111
     * 10987654321098765432109876543210
     * 0Q00111100LMRm--0001H0Rn---Rd---
     */
    int H = 11;
    int L = 21;
    int M = 20;
    // index=H:L:M
    uint bits = (enc >> H & 1) << 2 | (enc >> L & 1) << 1 | (enc >> M & 1);
    *opnd = opnd_create_immed_int(bits, OPSZ_3b);
    return true;
}

static inline bool
encode_opnd_vindex_H(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    /* Example encoding:
     * FMLA <Vd>.<T>, <Vn>.<T>, <Vm>.H[<index>]
     * 3322222222221111111111
     * 10987654321098765432109876543210
     * 0Q00111100LMRm--0001H0Rn---Rd---
     */
    int H = 11;
    int L = 21;
    int M = 20;
    ptr_int_t val;
    if (!opnd_is_immed_int(opnd))
        return false;
    val = opnd_get_immed_int(opnd);
    if (val < 0 || val >= 8)
        return false;
    // index=H:L:M
    *enc_out = (val >> 2 & 1) << H | (val >> 1 & 1) << L | (val & 1) << M;
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

/* hsd_immh_sz: The element size of a vector mediated by immh with possible values h, s
 * and d
 */
static inline bool
decode_opnd_hsd_immh_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int highest_bit;
    if (!highest_bit_set(enc, 19, 4, &highest_bit))
        return false;

    switch (highest_bit) {
    case 0: *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_HALF, OPSZ_2b); break;
    case 1: *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_2b); break;
    case 2: *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_DOUBLE, OPSZ_2b); break;
    default: return false;
    }
    return true;
}

static inline bool
encode_opnd_hsd_immh_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return true;
}

/* bhsd_immh_sz: The element size of a vector mediated by immh with possible values b, h,
 * s and d
 */
static inline bool
decode_opnd_bhsd_immh_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int highest_bit;
    if (!highest_bit_set(enc, 19, 4, &highest_bit))
        return false;

    switch (highest_bit) {
    case BYTE_REG: *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_BYTE, OPSZ_2b); break;
    case HALF_REG: *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_HALF, OPSZ_2b); break;
    case SINGLE_REG:
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_2b);
        break;
    case DOUBLE_REG:
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_DOUBLE, OPSZ_2b);
        break;
    default: return false;
    }

    return true;
}

static inline bool
encode_opnd_bhsd_immh_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return true;
}

static inline bool
decode_hsd_immh_regx(int rpos, uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int offset;
    if (!highest_bit_set(enc, 19, 4, &offset))
        return false;

    /* The binary representation starts at HALF_BIT=0, so shift to align with the normal
       offset */
    offset += 1;

    if (offset < HALF_REG || offset > DOUBLE_REG)
        return false;

    return decode_opnd_vector_reg(rpos, offset, enc, opnd);
}

static inline bool
encode_hsd_immh_regx(int rpos, uint enc, int opcode, byte *pc, opnd_t opnd,
                     OUT uint *enc_out)
{
    if (!opnd_is_reg(opnd))
        return false;
    reg_t reg = opnd_get_reg(opnd);
    aarch64_reg_offset offset = get_reg_offset(reg);
    if (offset == BYTE_REG || offset > DOUBLE_REG)
        return false;

    return encode_opnd_vector_reg(rpos, offset, opnd, enc_out);
}

static inline bool
decode_bhsd_immh_regx(int rpos, uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    int highest_bit;
    if (!highest_bit_set(enc, 19, 4, &highest_bit))
        return false;

    if (highest_bit < 0 || highest_bit > 3)
        return false;

    return decode_opnd_vector_reg(rpos, highest_bit, enc, opnd);
}

static inline bool
encode_bhsd_immh_regx(int rpos, uint enc, int opcode, byte *pc, opnd_t opnd,
                      OUT uint *enc_out)
{
    if (!opnd_is_reg(opnd))
        return false;
    reg_t reg = opnd_get_reg(opnd);

    aarch64_reg_offset offset = get_reg_offset(reg);
    if (offset > DOUBLE_REG)
        return false;

    return encode_opnd_vector_reg(rpos, offset, opnd, enc_out);
}

static inline bool
decode_opnd_hsd_immh_reg0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_hsd_immh_regx(0, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_hsd_immh_reg0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_hsd_immh_regx(0, enc, opcode, pc, opnd, enc_out);
}

static inline bool
decode_opnd_bhsd_immh_reg0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_bhsd_immh_regx(0, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_bhsd_immh_reg0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_bhsd_immh_regx(0, enc, opcode, pc, opnd, enc_out);
}

static inline bool
decode_opnd_hsd_immh_reg5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_hsd_immh_regx(5, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_hsd_immh_reg5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_hsd_immh_regx(5, enc, opcode, pc, opnd, enc_out);
}

static inline bool
decode_opnd_bhsd_immh_reg5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_bhsd_immh_regx(5, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_bhsd_immh_reg5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_bhsd_immh_regx(5, enc, opcode, pc, opnd, enc_out);
}

/* vindex_SD: Index for vector with single or double elements. */

static inline bool
decode_opnd_vindex_SD(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    /* Example encoding:
     * FMLA <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>]
     * 3322222222221111111111
     * 10987654321098765432109876543210
     * 0Q0011111sLMRm--0001H0Rn---Rd---
     *          z
     */
    int sz = 22;
    int H = 11;
    int L = 21;
    uint bits;
    if ((enc >> sz & 1) == 0) {                      // Single
        bits = (enc >> H & 1) << 1 | (enc >> L & 1); // index=H:L
    } else {                                         // Double
        if ((enc >> L & 1) != 0) {
            return false;
        }
        bits = enc >> H & 1; // index=H
    }
    *opnd = opnd_create_immed_int(bits, OPSZ_2b);
    return true;
}

static inline bool
encode_opnd_vindex_SD(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    /* Example encoding:
     * FMLA <Vd>.<T>, <Vn>.<T>, <Vm>.<Ts>[<index>]
     * 3322222222221111111111
     * 10987654321098765432109876543210
     * 0Q0011111sLMRm--0001H0Rn---Rd---
     *          z
     */
    int sz = 22;
    int H = 11;
    int L = 21;
    ptr_int_t val;
    if (!opnd_is_immed_int(opnd))
        return false;
    val = opnd_get_immed_int(opnd);
    if ((enc >> sz & 1) == 0) { // Single
        if (val < 0 || val >= 4)
            return false;
        *enc_out = (val & 1) << L | (val >> 1 & 1) << H; // index=H:L
    } else {                                             // Double
        if (val < 0 || val >= 2)
            return false;
        *enc_out = (val & 1) << H; // index=H
    }
    return true;
}

/* imm12sh: shift amount for 12-bit immediate of ADD/SUB, 0 or 12 */

static inline bool
decode_opnd_imm12sh(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{

    uint shift_bits = extract_uint(enc, 22, 2);
    if (shift_bits > 1)
        return false; /* 1x is reserved */

    *opnd = opnd_create_immed_int(shift_bits * 12, OPSZ_5b);
    return true;
}

static inline bool
encode_opnd_imm12sh(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    uint value = opnd_get_immed_int(opnd);
    if (value != 0 && value != 12)
        return false;

    *enc_out = value / 12 << 22;
    return true;
}

/* sd_sz: Operand size for single and double precision encoding of floating point
 * vector instructions. We need to convert the generic size operand to the right
 * encoding bits. It only supports VECTOR_ELEM_WIDTH_SINGLE and VECTOR_ELEM_WIDTH_DOUBLE.
 */
static inline bool
decode_opnd_sd_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    if (((enc >> 22) & 1) == 0) {
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_1);
        return true;
    }
    if (((enc >> 22) & 1) == 1) {
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_DOUBLE, OPSZ_1);
        return true;
    }
    return false;
}

static inline bool
encode_opnd_sd_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    if ((opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_SINGLE) &&
        (opnd_get_size(opnd) == OPSZ_1)) {
        *enc_out = 0;
        return true;
    }
    if ((opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_DOUBLE) &&
        (opnd_get_size(opnd) == OPSZ_1)) {
        *enc_out = 1 << 22;
        return true;
    }
    return false;
}

/* hs_fsz: Operand size for half and single precision encoding of floating point
 * vector instructions. We need to convert the generic size operand to the right
 * encoding bits. It only supports VECTOR_ELEM_WIDTH_HALF and VECTOR_ELEM_WIDTH_SINGLE.
 */
static inline bool
decode_opnd_hs_fsz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    if (((enc >> 22) & 1) == 0) {
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_HALF, OPSZ_2b);
        return true;
    }
    if (((enc >> 22) & 1) == 1) {
        *opnd = opnd_create_immed_int(VECTOR_ELEM_WIDTH_SINGLE, OPSZ_2b);
        return true;
    }
    return false;
}

static inline bool
encode_opnd_hs_fsz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_immed_int(opnd))
        return false;

    if (opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_HALF) {
        *enc_out = 0;
        return true;
    }
    if (opnd_get_immed_int(opnd) == VECTOR_ELEM_WIDTH_SINGLE) {
        *enc_out = 1 << 22;
        return true;
    }
    return false;
}

/* dq5_sz: D/Q register at bit position 5; bit 22 selects Q reg */

static inline bool
decode_opnd_dq5_sz(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_opnd_dq_plus(0, 5, 22, enc, opnd);
}

static inline bool
encode_opnd_dq5_sz(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_opnd_dq_plus(0, 5, 22, opnd, enc_out);
}

static inline bool
immhb_shf_decode(uint enc, int opcode, byte *pc, OUT opnd_t *opnd, uint min_shift)
{
    int highest_bit;
    if (!highest_bit_set(enc, 19, 4, &highest_bit))
        return false;

    uint esize = 8 << highest_bit;
    uint immhb_shf = extract_uint(enc, 16, 4 + highest_bit);
    opnd_size_t shift_size;
    switch (highest_bit) {
    case 0: shift_size = OPSZ_3b; break;
    case 1: shift_size = OPSZ_4b; break;
    case 2: shift_size = OPSZ_5b; break;
    case 3: shift_size = OPSZ_6b; break;
    default: return false;
    }

    if (min_shift == 1)
        *opnd = opnd_create_immed_int((2 * esize) - immhb_shf, shift_size);
    else if (min_shift == 0)
        *opnd = opnd_create_immed_int(immhb_shf - esize, shift_size);
    else
        return false;

    return true;
}

static inline bool
immhb_shf_encode(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out,
                 uint min_shift)
{

    if (!opnd_is_immed_int(opnd))
        return false;

    opnd_size_t shift_size = opnd_get_size(opnd);

    uint highest_bit;
    switch (shift_size) {
    case OPSZ_3b: highest_bit = 0; break;
    case OPSZ_4b: highest_bit = 1; break;
    case OPSZ_5b: highest_bit = 2; break;
    case OPSZ_6b: highest_bit = 3; break;
    default: return false;
    }
    ptr_int_t shift_amount;
    uint esize = 8 << highest_bit;

    shift_amount = opnd_get_immed_int(opnd);

    uint shift_encoding, max_shift;
    if (min_shift == 0) {
        shift_encoding = shift_amount + esize;
        max_shift = esize - 1;
    } else if (min_shift == 1) {
        shift_encoding = esize * 2 - shift_amount;
        max_shift = esize;
    } else
        return false;

    if (shift_amount < min_shift || shift_amount > max_shift)
        return false;

    *enc_out = (shift_encoding << 16);

    return true;
}

/* immhb_shf: The vector encoding of #shift operand.
 */
static inline bool
decode_opnd_immhb_shf(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return immhb_shf_decode(enc, opcode, pc, opnd, 1);
}

static inline bool
encode_opnd_immhb_shf(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return immhb_shf_encode(enc, opcode, pc, opnd, enc_out, 1);
}

/* immhb_shf2: The vector encoding of #shift operand.
 */
static inline bool
decode_opnd_immhb_0shf(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return immhb_shf_decode(enc, opcode, pc, opnd, 0);
}

static inline bool
encode_opnd_immhb_0shf(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return immhb_shf_encode(enc, opcode, pc, opnd, enc_out, 0);
}

/* immhb_fxp: The vector encoding of #fbits operand. This is the number of bits
 * after the decimal point for fixed-point values.
 */
static inline bool
decode_opnd_immhb_fxp(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return immhb_shf_decode(enc, opcode, pc, opnd, 1);
}

static inline bool
encode_opnd_immhb_fxp(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return immhb_shf_encode(enc, opcode, pc, opnd, enc_out, 1);
}

/* fpimm13: floating-point immediate for scalar fmov */

static inline bool
decode_opnd_fpimm13(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    /*
     * From the Architecture Reference Manual, 8 bit immediate abcdefgh maps to
     * floats:
     *
     * 3332 2222 2222 1111 1111 11
     * 1098 7654 3210 9876 5432 1098 7654 3210
     *  _                            abcd efgh <- 8 bit immediate mapped to
     * abbb bbbc defg h000 0000 0000 0000 0000 <- 32 bit float
     *
     *   abcd efgh  Masks
     * 0x1    0     a
     * 0x4    0     b
     * 0x2    0     c
     * 0x1    F     defgh
     */
    if (extract_uint(enc, 22, 1) == 0) { /* 32 bits */
        union {
            float f;
            uint32_t i;
        } fpv;

        uint32_t imm = extract_uint(enc, 13, 8);

        uint32_t a = imm & 0x80;
        uint32_t b = imm & 0x40;
        uint32_t not_b = ((b == 0) ? 1 : 0);
        uint32_t bbbbb = ((b == 0) ? 0 : 0x1f);
        uint32_t c = imm & 0x20;
        uint32_t defgh = imm & 0x1f;

        uint32_t imm32 =
            (a << 24) | (not_b << 30) | (bbbbb << 25) | (c << 19) | (defgh << 19);

        fpv.i = imm32;
        *opnd = opnd_create_immed_float(fpv.f);
    } else { /* 64 bits */
        /* 6666 5555 5555 5544 44444444 33333333 33322222 22221111 111111
         * 3210 9876 5432 1098 76543210 98765432 10987654 32109876 54321098 76543210
         *  _                                                               abcdefgh
         * abbb bbbb bbcd efgh 00000000 00000000 00000000 00000000 00000000 00000000
         */
        union {
            double d;
            uint64_t i;
        } fpv;

        uint64_t imm = extract_uint(enc, 13, 8);

        uint64_t a = imm & 0x80;
        uint64_t b = imm & 0x40;
        uint64_t not_b = ((b == 0) ? 1 : 0);
        uint64_t bbbbbbbb = ((b == 0) ? 0 : 0xff);
        uint64_t c = imm & 0x20;
        uint64_t defgh = imm & 0x1f;

        uint64_t imm64 =
            (a << 56) | (not_b << 62) | (bbbbbbbb << 54) | (c << 48) | (defgh << 48);

        fpv.i = imm64;
        *opnd = opnd_create_immed_double(fpv.d);
    }
    return true;
}

static inline bool
encode_opnd_fpimm13(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    /*
     * From the Architecture Reference Manual, 8 bit immediate abcdefgh maps to
     * floats:
     *
     *   3332 2222 2222 1111 1111 11
     *   1098 7654 3210 9876 5432 1098 7654 3210
     *    _
     *   abbb bbbc defg h000 0000 0000 0000 0000
     * 0x8    0    0    0    0    0    0    0    a
     * 0x1    0    0    0    0    0    0    0    b
     * 0x0    1    0    0    0    0    0    0    c
     * 0x0    0    f    8    0    0    0    0    defgh
     */
    if (opnd_is_immed_float(opnd)) {
        ASSERT(extract_uint(enc, 22, 1) == 0); /* 32 bit floating point */
        union {
            float f;
            uint32_t i;
        } fpv;
        fpv.f = opnd_get_immed_float(opnd);
        uint32_t imm = fpv.i;

        uint a = (imm & 0x80000000);
        uint b = (imm & 0x10000000);
        uint c = (imm & 0x01000000);
        uint defgh = (imm & 0x00f80000);

        /* 3332 2222 2222 1111 1111 11
         * 1098 7654 3210 9876 5432 1098 7654 3210
         * ---- ---- ---a bcde fgh- ---- ---- ----   immediate encoding
         * |-----11---->|           0x80000000 a
         *    |-----9---->|         0x10000000 b
         *         |---6-->|        0x01000000 c
         *           |--6-->|       0x00f80000 defgh
         */
        *enc_out = (a >> 11) | (b >> 9) | (c >> 6) | (defgh >> 6);
    } else if (opnd_is_immed_double(opnd)) {
        ASSERT(extract_uint(enc, 22, 1) == 1); /* 64 bit floating point */
        /* 6666 5555 5555 5544 44444444 33333333 33322222 22221111 111111
         * 3210 9876 5432 1098 76543210 98765432 10987654 32109876 54321098 76543210
         *  _
         * abbb bbbb bbcd efgh 00000000 00000000 00000000 00000000 00000000 00000000
         *
         * ---- ---- ---a bcde fgh----- -------- immediate encoding
         */
        union {
            double d;
            uint64_t i;
        } fpv;
        fpv.d = opnd_get_immed_double(opnd);
        uint64_t imm = fpv.i;

        uint64_t a = (imm & 0x8000000000000000);
        uint64_t b = (imm & 0x1000000000000000);
        uint64_t c = (imm & 0x0020000000000000);
        uint64_t defgh = (imm & 0x001f000000000000);

        *enc_out =
            (((a >> 11) | (b >> 9) | (c >> 3) | (defgh >> 3)) & 0xffffffff00000000) >> 32;
    } else
        return false;

    return true;
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
    if (!opnd_is_immed_int(opnd))
        return false;
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
decode_scalar_size_regx(uint size_offset, int rpos, uint enc, int opcode, byte *pc,
                        OUT opnd_t *opnd)
{
    uint size = extract_uint(enc, 22, 2);

    if (size < 0 || size > (3 - size_offset))
        return false;

    return decode_opnd_vector_reg(rpos, size + size_offset, enc, opnd);
}

static inline bool
encode_scalar_size_regx(uint size_offset, int rpos, uint enc, int opcode, byte *pc,
                        opnd_t opnd, OUT uint *enc_out)
{
    if (!opnd_is_reg(opnd))
        return false;

    reg_t reg = opnd_get_reg(opnd);

    aarch64_reg_offset offset = get_reg_offset(reg);
    if (offset > DOUBLE_REG) {
        return false;
    }
    bool reg_written = encode_opnd_vector_reg(rpos, offset, opnd, enc_out);
    *enc_out |= (offset - size_offset) << 22;

    return reg_written;
}

static inline bool
decode_hsd_size_regx(int rpos, uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_scalar_size_regx(1, rpos, enc, opcode, pc, opnd);
}

static inline bool
encode_hsd_size_regx(int rpos, uint enc, int opcode, byte *pc, opnd_t opnd,
                     OUT uint *enc_out)
{
    return encode_scalar_size_regx(1, rpos, enc, opcode, pc, opnd, enc_out);
}

static inline bool
decode_bhsd_size_regx(int rpos, uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_scalar_size_regx(0, rpos, enc, opcode, pc, opnd);
}

static inline bool
encode_bhsd_size_regx(int rpos, uint enc, int opcode, byte *pc, opnd_t opnd,
                      OUT uint *enc_out)
{
    return encode_scalar_size_regx(0, rpos, enc, opcode, pc, opnd, enc_out);
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
decode_opnd_hsd_size_reg0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_hsd_size_regx(0, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_hsd_size_reg0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_hsd_size_regx(0, enc, opcode, pc, opnd, enc_out);
}

static inline bool
decode_opnd_bhsd_size_reg0(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_bhsd_size_regx(0, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_bhsd_size_reg0(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_bhsd_size_regx(0, enc, opcode, pc, opnd, enc_out);
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
decode_opnd_hsd_size_reg5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_hsd_size_regx(5, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_hsd_size_reg5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_hsd_size_regx(5, enc, opcode, pc, opnd, enc_out);
}

static inline bool
decode_opnd_bhsd_size_reg5(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_bhsd_size_regx(5, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_bhsd_size_reg5(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_bhsd_size_regx(5, enc, opcode, pc, opnd, enc_out);
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

static inline bool
decode_opnd_hsd_size_reg16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_hsd_size_regx(16, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_hsd_size_reg16(uint enc, int opcode, byte *pc, opnd_t opnd, OUT uint *enc_out)
{
    return encode_hsd_size_regx(16, enc, opcode, pc, opnd, enc_out);
}

static inline bool
decode_opnd_bhsd_size_reg16(uint enc, int opcode, byte *pc, OUT opnd_t *opnd)
{
    return decode_bhsd_size_regx(16, enc, opcode, pc, opnd);
}

static inline bool
encode_opnd_bhsd_size_reg16(uint enc, int opcode, byte *pc, opnd_t opnd,
                            OUT uint *enc_out)
{
    return encode_bhsd_size_regx(16, enc, opcode, pc, opnd, enc_out);
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
        *opnd = opnd_create_immed_int(bytes, OPSZ_PTR);
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

/* ccm: operands for conditional compare instructions */

static inline bool
decode_opnds_ccm(uint enc, dcontext_t *dcontext, byte *pc, instr_t *instr, int opcode)
{
    instr_set_opcode(instr, opcode);
    instr_set_num_opnds(dcontext, instr, 0, 3);

    /* Rn */
    opnd_t rn;
    if (!decode_opnd_rn(false, 5, enc, &rn))
        return false;
    instr_set_src(instr, 0, rn);

    opnd_t rm;
    if (TEST(1U << 11, enc)) /* imm5 */
        instr_set_src(instr, 1, opnd_create_immed_int(extract_uint(enc, 16, 5), OPSZ_5b));
    else if (!decode_opnd_rn(false, 16, enc, &rm)) /* Rm */
        return false;
    else
        instr_set_src(instr, 1, rm);

    /* nzcv */
    instr_set_src(instr, 2, opnd_create_immed_int(extract_uint(enc, 0, 4), OPSZ_4b));
    /* cond */
    instr_set_predicate(instr, DR_PRED_EQ + extract_uint(enc, 12, 4));

    return true;
}

static inline uint
encode_opnds_ccm(byte *pc, instr_t *instr, uint enc, decode_info_t *di)
{
    uint rn;
    uint rm_imm5 = 0;
    uint imm5_flag = 0;
    if (instr_num_dsts(instr) == 0 && instr_num_srcs(instr) == 3 &&
        encode_opnd_rn(false, 5, instr_get_src(instr, 0), &rn) && /* Rn */
        opnd_is_immed_int(instr_get_src(instr, 2)) &&             /* nzcv */
        (uint)(instr_get_predicate(instr) - DR_PRED_EQ) < 16) {   /* cond */
        uint nzcv = opnd_get_immed_int(instr_get_src(instr, 2));
        uint cond = instr_get_predicate(instr) - DR_PRED_EQ;
        if (opnd_is_immed_int(instr_get_src(instr, 1))) { /* imm5 */
            rm_imm5 = opnd_get_immed_int(instr_get_src(instr, 1)) << 16;
            imm5_flag = 1;
        } else if (opnd_is_reg(instr_get_src(instr, 1))) { /* Rm */
            encode_opnd_rn(false, 16, instr_get_src(instr, 1), &rm_imm5);
        } else
            return ENCFAIL;
        return (enc | nzcv | rn | (imm5_flag << 11) | rm_imm5 | (cond << 12));
    }

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
                  opnd_create_reg(decode_reg(extract_uint(enc, 0, 5),
                                             TEST(1U << 31, enc), /* true if x, else w*/
                                             false)));
    instr_set_src(instr, 2,
                  opnd_create_immed_int((enc >> 19 & 31) | (enc >> 26 & 32), OPSZ_5b));
    return true;
}

static inline uint
encode_opnds_tbz(byte *pc, instr_t *instr, uint enc, decode_info_t *di)
{
    uint xt, imm6, off;
    reg_id_t reg = opnd_get_reg(instr_get_src(instr, 1));
    /* TBZ accepts a x register in all cases, but will decode it
     * to a w register when imm6 is less than 32 */
    bool is_x_register = reg >= DR_REG_X0 && reg <= DR_REG_X30;
    if (instr_num_dsts(instr) == 0 && instr_num_srcs(instr) == 3 &&
        encode_pc_off(&off, 14, pc, instr, instr_get_src(instr, 0), di) &&
        encode_opnd_int(0, 6, false, 0, 0, instr_get_src(instr, 2), &imm6) &&
        encode_opnd_wxn((imm6 > 31) || is_x_register, false, 0, instr_get_src(instr, 1),
                        &xt))
        return (enc | off << 5 | xt | (imm6 & 31) << 19 | (imm6 & 32) << 26);
    return ENCFAIL;
}

/* Function(s) used by new codec. */

/* Halfword and short element size. This is the same as encode_opnd_hs_sz()
 * except that the shift of the size value to place it in the correct part of
 * the encoding is not done here. That placement is instruction specific and so
 * is done by each instruction's enc_*() function. In general the positioning
 * of operand encodings in instruction encodings should be done by each
 * instruction's encoding function, rather than an operand's encoding function.
 */
static inline bool
get_el_hs_sz(OUT uint *elsz_out, opnd_t opnd)
{
    ptr_int_t val = opnd_get_immed_int(opnd);
    if (val < 1 || val > 2)
        return false;
    *elsz_out = val;
    return true;
}

/******************************************************************************/

/* Include automatically generated decoder and encoder files. Decode and encode
 * code is partitioned into versions of the AArch64 architecture starting with
 * v8.0. The decode/encode logic is chained together into a pipeline with v8.0
 * calling v8.1, which calls v8.2 and so on, returning from the decode/encode
 * functions as soon as a match is found.
 */
#include "opnd_decode_funcs.h"
#include "opnd_encode_funcs.h"
#include "decode_gen_sve.h"
#include "decode_gen_v82.h"
#include "decode_v81.h"
#include "decode_gen_v81.h" /* Redirects decoding to decode_v81.h */
#include "decode_gen_v80.h"
#include "encode_gen_sve.h"
#include "encode_gen_v82.h"
#include "encode_v81.h"
#include "encode_gen_v81.h" /* Redirects encoding to encode_v81.h */
#include "encode_gen_v80.h"

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

    if (!decoder_v80(enc, dcontext, orig_pc, instr)) {
        /* This clause handles undefined HINT instructions. See the comment
         * 'Notes on specific instructions' in codec.txt for details. If the
         * decoder reads an undefined hint, a message with the unallocated
         * CRm:op2 field value is output and the encoding converted into a NOP
         * instruction.
         */
        if ((enc & 0xfffff01f) == 0xd503201f) {
            SYSLOG_INTERNAL_WARNING("Undefined HINT instruction found: "
                                    "encoding 0x%x (CRm:op2 0x%x)\n",
                                    enc, (enc & 0xfe0) >> 5);
            instr_set_opcode(instr, OP_nop);
            instr_set_num_opnds(dcontext, instr, 0, 0);
        } else {
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
    }

    /* XXX i#2374: This determination of flag usage should be separate from the
     * decoding of operands.
     *
     * Apart from explicit read/write from/to flags register using MRS and MSR,
     * a field in codec.txt specifies whether instructions read/write from/to
     * flags register.
     */
    opc = instr_get_opcode(instr);
    if (opc == OP_mrs && instr_num_srcs(instr) == 1 &&
        opnd_is_reg(instr_get_src(instr, 0)) &&
        opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_NZCV) {
        eflags |= EFLAGS_READ_NZCV;
    }
    if (opc == OP_msr && instr_num_dsts(instr) == 1 &&
        opnd_is_reg(instr_get_dst(instr, 0)) &&
        opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_NZCV) {
        eflags |= EFLAGS_WRITE_NZCV;
    }

    /* XXX i#2626: Until the decoder for AArch64 covers all the instructions that
     * read/write aflags, as a workaround conservatively assume that all OP_xx
     * instructions (i.e., unrecognized instructions) may read/write aflags.
     */
    if (opc == OP_xx) {
        eflags |= EFLAGS_READ_ARITH;
        eflags |= EFLAGS_WRITE_ARITH;
    }

    instr->eflags |= eflags;
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
    return encoder_v80(pc, i, di);
}
