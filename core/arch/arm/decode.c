/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

#include "../globals.h"
#include "instr.h"
#include "decode.h"
#include "decode_private.h"
#include <string.h> /* for memcpy */

/* ARM decoder.
 * General strategy:
 * + We use a data-driven table-based approach, as we need to both encode and
 *   decode and a central source of data lets us move in both directions.
 */
/* FIXME i#1551: add Thumb support: for now just A32 */
/* FIXME i#1551: add A64 support: for now just A32 */

/* With register lists we can see quite long operand lists */
#define MAX_OPNDS IF_X64_ELSE(8, 22)

bool
is_isa_mode_legal(dr_isa_mode_t mode)
{
#ifdef X64
    return (mode == DR_ISA_ARM_A64);
#else
    return (mode == DR_ISA_ARM_THUMB || DR_ISA_ARM_A32);
#endif
}

static bool
reg_is_past_last_simd(reg_id_t reg, uint add)
{
    if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31)
        return reg+add > IF_X64_ELSE(DR_REG_Q31, DR_REG_Q15);
    if (reg >= DR_REG_D0 && reg <= DR_REG_D31)
        return reg+add > DR_REG_D31;
    if (reg >= DR_REG_S0 && reg <= DR_REG_S31)
        return reg+add > DR_REG_S31;
    if (reg >= DR_REG_H0 && reg <= DR_REG_H31)
        return reg+add > DR_REG_H31;
    if (reg >= DR_REG_B0 && reg <= DR_REG_B31)
        return reg+add > DR_REG_B31;
    ASSERT_NOT_REACHED();
    return true;
}

/* We assume little-endian */
static inline int
decode_predicate(uint instr_word)
{
    return instr_word >> 28; /* bits 31:28 */
}

/* We often take bits 27:20 as an 8-bit opcode */
static inline int
decode_opc8(uint instr_word)
{
    return (instr_word >> 20) & 0xff;
}

/* We often take bits 7:4 as a 4-bit auxiliary opcode */
static inline int
decode_opc4(uint instr_word)
{
    return (instr_word >> 4) & 0xf;
}

static reg_id_t
decode_regA(decode_info_t *di)
{
    /* A32 = 19:16 */
    return DR_REG_START_GPR + ((di->instr_word >> 16) & 0xf);
}

static reg_id_t
decode_regB(decode_info_t *di)
{
    /* A32 = 15:12 */
    return DR_REG_START_GPR + ((di->instr_word >> 12) & 0xf);
}

static reg_id_t
decode_regC(decode_info_t *di)
{
    /* A32 = 11:8 */
    return DR_REG_START_GPR + ((di->instr_word >> 8) & 0xf);
}

static reg_id_t
decode_regD(decode_info_t *di)
{
    /* A32 = 3:0 */
    return DR_REG_START_GPR + (di->instr_word & 0xf);
}

static inline reg_id_t
decode_simd_start(opnd_size_t opsize)
{
    if (opsize == OPSZ_1)
        return DR_REG_B0;
    else if (opsize == OPSZ_2)
        return DR_REG_H0;
    else if (opsize == OPSZ_4)
        return DR_REG_S0;
    else if (opsize == OPSZ_8)
        return DR_REG_D0;
    else if (opsize == OPSZ_16)
        return DR_REG_Q0;
    else
        CLIENT_ASSERT(false, "invalid SIMD reg size");
    return DR_REG_D0;
}

static reg_id_t
decode_vregA(decode_info_t *di, opnd_size_t opsize)
{
    /* A32 = 7,19:16 */
    return decode_simd_start(opsize) +
        (((di->instr_word & 0x00000080) >> 3) | ((di->instr_word >> 16) & 0xf));
}

static reg_id_t
decode_vregB(decode_info_t *di, opnd_size_t opsize)
{
    /* A32 = 22,15:12 */
    return decode_simd_start(opsize) +
        (((di->instr_word & 0x00400000) >> 18) | ((di->instr_word >> 12) & 0xf));
}

static reg_id_t
decode_vregC(decode_info_t *di, opnd_size_t opsize)
{
    /* A32 = 5,3:0 */
    return decode_simd_start(opsize) +
        (((di->instr_word & 0x00000020) >> 1) | (di->instr_word & 0xf));
}

static reg_id_t
decode_wregA(decode_info_t *di, opnd_size_t opsize)
{
    /* A32 = 19:16,7 */
    return decode_simd_start(opsize) +
        (((di->instr_word & 0x000f0000) >> 15) | ((di->instr_word >> 19) & 0x1));
}

static reg_id_t
decode_wregB(decode_info_t *di, opnd_size_t opsize)
{
    /* A32 = 15:12,22 */
    return decode_simd_start(opsize) +
        (((di->instr_word & 0x0000f000) >> 11) | ((di->instr_word >> 22) & 0x1));
}

static reg_id_t
decode_wregC(decode_info_t *di, opnd_size_t opsize)
{
    /* A32 = 3:0,5 */
    return decode_simd_start(opsize) +
        (((di->instr_word & 0x0000000f) << 1) | ((di->instr_word >> 5) & 0x1));
}

static ptr_int_t
decode_immed(decode_info_t *di, uint start_bit, opnd_size_t opsize, bool is_signed)
{
    ptr_int_t val;
    uint mask = ((1 << opnd_size_in_bits(opsize)) - 1);
    if (is_signed) {
        val = (ptr_int_t)(int)((di->instr_word >> start_bit) & mask) | (~mask);
    } else
        val = (ptr_int_t)(ptr_uint_t)((di->instr_word >> start_bit) & mask);
    return val;
}

static bool
decode_float_reglist(decode_info_t *di, opnd_size_t downsz, opnd_size_t upsz,
                     opnd_t *array, uint *counter INOUT)
{
    uint i;
    uint count = (uint) decode_immed(di, 0, OPSZ_1, false/*unsigned*/);
    reg_id_t first_reg;
    if (upsz == OPSZ_8) {
        /* XXX i#1551: if immed is odd, supposed to be (deprecated) OP_fldmx */
        count /= 2;
    } else
        CLIENT_ASSERT(upsz == OPSZ_4, "invalid opsz for TYPE_L_CONSEC");
    /* There must be an immediately prior simd reg */
    CLIENT_ASSERT(*counter > 0 && opnd_is_reg(array[*counter-1]),
                  "invalid instr template");
    count--; /* The prior was already added */
    first_reg = opnd_get_reg(array[*counter-1]);
    di->reglist_sz = 0;
    for (i = 0; i < count; i++) {
        print_file(STDERR, "reglist: first=%s, new=%s\n", reg_names[first_reg],
                   reg_names[first_reg + i]);
        if ((upsz == OPSZ_8 && first_reg + i > DR_REG_D31) ||
            (upsz == OPSZ_4 && first_reg + i > DR_REG_S31))
            return false; /* invalid */
        array[(*counter)++] = opnd_create_reg_ex(first_reg + i, downsz, 0);
        di->reglist_sz += opnd_size_in_bytes(downsz);
    }
    if (di->mem_needs_reglist_sz != NULL)
        opnd_set_size(di->mem_needs_reglist_sz, opnd_size_from_bytes(di->reglist_sz));
    return true;
}

static dr_shift_type_t
decode_index_shift_values(ptr_int_t sh2, ptr_int_t val, uint *amount OUT)
{
    if (sh2 == 0 && val == 0) {
        *amount = 0;
        return DR_SHIFT_NONE;
    } else if (sh2 == SHIFT_ENCODING_LSL) {
        *amount = val;
        return DR_SHIFT_LSL;
    } else if (sh2 == SHIFT_ENCODING_LSR) {
        *amount = (val == 0) ? 32 : val;
        return DR_SHIFT_LSR;
    } else if (sh2 == SHIFT_ENCODING_ASR) {
        *amount = (val == 0) ? 32 : val;
        return DR_SHIFT_ASR;
    } else if (sh2 == SHIFT_ENCODING_RRX && val == 0) {
        *amount = 1;
        return DR_SHIFT_RRX;
    } else {
        *amount = val;
        return DR_SHIFT_ROR;
    }
}

static dr_shift_type_t
decode_index_shift(decode_info_t *di, uint *amount OUT)
{
    ptr_int_t sh2 = decode_immed(di, DECODE_INDEX_SHIFT_TYPE_BITPOS,
                                 DECODE_INDEX_SHIFT_TYPE_SIZE, false);
    ptr_int_t val = decode_immed(di, DECODE_INDEX_SHIFT_AMOUNT_BITPOS,
                                 DECODE_INDEX_SHIFT_AMOUNT_SIZE, false);
    return decode_index_shift_values(sh2, val, amount);
}

static void
decode_register_shift(decode_info_t *di, opnd_t *array, uint *counter IN)
{
    if (di->shift_type_idx == *counter - 2 &&
        /* We only need to do this for shifts whose amount is an immed.
         * When the amount is in a reg, only the low 4 DR_SHIFT_* are valid,
         * and they match the encoded values.
         */
        opnd_is_immed_int(array[*counter - 1])) {
        /* Mark the register as shifted and move the two immediates to a
         * higher abstraction layer.  Note that b/c we map the lower 4
         * DR_SHIFT_* values to the encoded values, we can handle either
         * raw or higher-layer values at encode time.
         */
        ptr_int_t sh2 = opnd_get_immed_int(array[*counter - 2]);
        ptr_int_t val = opnd_get_immed_int(array[*counter - 1]);
        uint amount;
        dr_shift_type_t type = decode_index_shift_values(sh2, val, &amount);
        array[*counter - 2] = opnd_create_immed_int(type, OPSZ_2b);
        array[*counter - 1] = opnd_create_immed_int(amount, OPSZ_5b);
        CLIENT_ASSERT(*counter >= 3 && opnd_is_reg(array[*counter - 3]),
                      "invalid shift sequence");
        array[*counter - 3] = opnd_create_reg_ex(opnd_get_reg(array[*counter - 3]),
                                                 opnd_get_size(array[*counter - 3]),
                                                 DR_OPND_SHIFTED);
    }
}

static void
decode_update_mem_for_reglist(decode_info_t *di)
{
    if (di->mem_needs_reglist_sz != NULL) {
        opnd_set_size(di->mem_needs_reglist_sz, opnd_size_from_bytes(di->reglist_sz));
        if (di->mem_adjust_disp_for_reglist) {
            opnd_set_disp(di->mem_needs_reglist_sz,
                          opnd_get_disp(*di->mem_needs_reglist_sz) - di->reglist_sz);
        }
    }
}

static opnd_size_t
decode_mem_reglist_size(decode_info_t *di, opnd_t *memop,
                        opnd_size_t opsize, bool adjust_disp)
{
    if (opsize == OPSZ_VAR_REGLIST) {
        if (di->reglist_sz == -1) {
            /* Have not yet seen the reglist opnd yet */
            di->mem_needs_reglist_sz = memop;
            di->mem_adjust_disp_for_reglist = adjust_disp;
            return OPSZ_0;
        } else
            return opnd_size_from_bytes(di->reglist_sz);
    }
    return opsize;
}

static bool
decode_operand(decode_info_t *di, byte optype, opnd_size_t opsize, opnd_t *array,
               uint *counter INOUT)
{
    uint i;
    ptr_int_t val = 0;
    opnd_size_t downsz = resolve_size_downward(opsize);
    opnd_size_t upsz = resolve_size_upward(opsize);

    switch (optype) {
    case TYPE_NONE:
        array[(*counter)++] = opnd_create_null();
        return true;

    /* Registers */
    case TYPE_R_A:
    case TYPE_R_A_TOP: /* we aren't storing whether top in our IR */
        array[(*counter)++] = opnd_create_reg_ex(decode_regA(di), downsz, 0);
        return true;
    case TYPE_R_B:
    case TYPE_R_B_TOP: /* we aren't storing whether top in our IR */
        array[(*counter)++] = opnd_create_reg_ex(decode_regB(di), downsz, 0);
        return true;
    case TYPE_R_C:
    case TYPE_R_C_TOP: /* we aren't storing whether top in our IR */
        array[(*counter)++] = opnd_create_reg_ex(decode_regC(di), downsz, 0);
        return true;
    case TYPE_R_D:
    case TYPE_R_D_TOP: /* we aren't storing whether top in our IR */
        array[(*counter)++] = opnd_create_reg_ex(decode_regD(di), downsz, 0);
        return true;
    case TYPE_R_D_NEGATED:
        array[(*counter)++] = opnd_create_reg_ex(decode_regD(di), downsz,
                                                 DR_OPND_NEGATED);
        return true;
    case TYPE_R_B_EVEN:
    case TYPE_R_D_EVEN: {
        reg_id_t reg = (optype == TYPE_R_B_EVEN) ? decode_regB(di) : decode_regD(di);
        if ((reg - DR_REG_START_GPR) % 2 == 1)
            return false;
        array[(*counter)++] = opnd_create_reg_ex(reg, downsz, 0);
        return true;
    }
    case TYPE_R_B_PLUS1:
    case TYPE_R_D_PLUS1: {
        reg_id_t reg;
        if (*counter <= 0 || !opnd_is_reg(array[(*counter)-1]))
            return false;
        reg = opnd_get_reg(array[(*counter)-1]);
        if (reg == DR_REG_STOP_32 || reg == DR_REG_STOP_64)
            return false;
        array[(*counter)++] = opnd_create_reg_ex(reg + 1, downsz, 0);
        return true;
    }
    case TYPE_CR_A:
        array[(*counter)++] = opnd_create_reg_ex(decode_regA(di) - DR_REG_START_GPR +
                                                 DR_REG_CR0, downsz, 0);
        return true;
    case TYPE_CR_B:
        array[(*counter)++] = opnd_create_reg_ex(decode_regB(di) - DR_REG_START_GPR +
                                                 DR_REG_CR0, downsz, 0);
        return true;
    case TYPE_CR_C:
        array[(*counter)++] = opnd_create_reg_ex(decode_regC(di) - DR_REG_START_GPR +
                                                 DR_REG_CR0, downsz, 0);
        return true;
    case TYPE_CR_D:
        array[(*counter)++] = opnd_create_reg_ex(decode_regD(di) - DR_REG_START_GPR +
                                                 DR_REG_CR0, downsz, 0);
        return true;
    case TYPE_V_A:
        array[(*counter)++] = opnd_create_reg_ex(decode_vregA(di, upsz), downsz, 0);
        return true;
    case TYPE_V_B:
        array[(*counter)++] = opnd_create_reg_ex(decode_vregB(di, upsz), downsz, 0);
        return true;
    case TYPE_V_C:
        array[(*counter)++] = opnd_create_reg_ex(decode_vregC(di, upsz), downsz, 0);
        return true;
    case TYPE_W_A:
        array[(*counter)++] = opnd_create_reg_ex(decode_wregA(di, upsz), downsz, 0);
        return true;
    case TYPE_W_B:
        array[(*counter)++] = opnd_create_reg_ex(decode_wregB(di, upsz), downsz, 0);
        return true;
    case TYPE_W_C:
        array[(*counter)++] = opnd_create_reg_ex(decode_wregC(di, upsz), downsz, 0);
        return true;
    case TYPE_V_C_3b: {
        reg_id_t reg = decode_simd_start(upsz) + (di->instr_word & 0x00000007);
        array[(*counter)++] = opnd_create_reg_ex(reg, downsz, 0);
        return true;
    }
    case TYPE_V_C_4b: {
        reg_id_t reg = decode_simd_start(upsz) + (di->instr_word & 0x0000000f);
        array[(*counter)++] = opnd_create_reg_ex(reg, downsz, 0);
        return true;
    }
    case TYPE_W_C_PLUS1: {
        reg_id_t reg;
        if (*counter <= 0 || !opnd_is_reg(array[(*counter)-1]))
            return false;
        reg = opnd_get_reg(array[(*counter)-1]);
        if (reg_is_past_last_simd(reg, 1))
            return false;
        array[(*counter)++] = opnd_create_reg_ex(reg + 1, downsz, 0);
        return true;
    }
    case TYPE_SPSR:
        array[(*counter)++] = opnd_create_reg_ex(DR_REG_SPSR, downsz, 0);
        return true;
    case TYPE_CPSR:
        array[(*counter)++] = opnd_create_reg_ex(DR_REG_CPSR, downsz, 0);
        return true;
    case TYPE_FPSCR:
        array[(*counter)++] = opnd_create_reg_ex(DR_REG_FPSCR, downsz, 0);
        return true;
    case TYPE_LR:
        array[(*counter)++] = opnd_create_reg_ex(DR_REG_LR, downsz, 0);
        return true;
    case TYPE_SP:
        array[(*counter)++] = opnd_create_reg_ex(DR_REG_SP, downsz, 0);
        return true;

    /* Register lists */
    case TYPE_L_8b:
    case TYPE_L_13b:
    case TYPE_L_16b: {
        uint num = (optype == TYPE_L_8b ? 8 : (optype == TYPE_L_13b ? 13 : 16));
        di->reglist_sz = 0;
        for (i = 0; i < num; i++) {
            if ((di->instr_word & (1 << i)) != 0) {
                array[(*counter)++] = opnd_create_reg_ex(DR_REG_START_GPR + i, downsz, 0);
                di->reglist_sz += opnd_size_in_bytes(downsz);
            }
        }
        /* These 3 var-size reg lists need to update a corresponding mem opnd */
        decode_update_mem_for_reglist(di);
        return true;
    }
    case TYPE_L_CONSEC:
        return decode_float_reglist(di, downsz, upsz, array, counter);
    case TYPE_L_VBx2:
    case TYPE_L_VBx3:
    case TYPE_L_VBx4:
    case TYPE_L_VBx2D:
    case TYPE_L_VBx3D:
    case TYPE_L_VBx4D: {
        reg_id_t start = decode_vregB(di, upsz);
        uint inc = 1;
        if (optype == TYPE_L_VBx2D || optype == TYPE_L_VBx3D || optype == TYPE_L_VBx4D)
            inc = 2;
        array[(*counter)++] = opnd_create_reg_ex(start, downsz, 0);
        if (reg_is_past_last_simd(start, inc))
            return false;
        array[(*counter)++] = opnd_create_reg_ex(start + inc, downsz, 0);
        if (optype == TYPE_L_VBx2 || optype == TYPE_L_VBx2D)
            return true;
        if (reg_is_past_last_simd(start, 2*inc))
            return false;
        array[(*counter)++] = opnd_create_reg_ex(start + 2*inc, downsz, 0);
        if (optype == TYPE_L_VBx3 || optype == TYPE_L_VBx3D)
            return true;
        if (reg_is_past_last_simd(start, 3*inc))
            return false;
        array[(*counter)++] = opnd_create_reg_ex(start + 3*inc, downsz, 0);
        return true;
    }
    case TYPE_L_VAx2:
    case TYPE_L_VAx3:
    case TYPE_L_VAx4: {
        reg_id_t start = decode_vregA(di, upsz);
        uint inc = 1;
        array[(*counter)++] = opnd_create_reg_ex(start, downsz, 0);
        if (reg_is_past_last_simd(start, inc))
            return false;
        array[(*counter)++] = opnd_create_reg_ex(start + inc, downsz, 0);
        if (optype == TYPE_L_VBx2)
            return true;
        if (reg_is_past_last_simd(start, 2*inc))
            return false;
        array[(*counter)++] = opnd_create_reg_ex(start + 2*inc, downsz, 0);
        if (optype == TYPE_L_VBx3)
            return true;
        if (reg_is_past_last_simd(start, 3*inc))
            return false;
        array[(*counter)++] = opnd_create_reg_ex(start + 3*inc, downsz, 0);
        return true;
    }

    /* Immeds */
    case TYPE_I_b0:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 0, opsize, false/*unsigned*/), opsize);
        return true;
    case TYPE_NI_b0:
        array[(*counter)++] =
            opnd_create_immed_int(-decode_immed(di, 0, opsize, false/*unsigned*/),
                                  opsize);
        return true;
    case TYPE_I_b3:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 3, opsize, false/*unsigned*/), opsize);
        return true;
    case TYPE_I_b4:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 4, opsize, false/*unsigned*/), opsize);
        return true;
    case TYPE_I_b5:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 5, opsize, false/*unsigned*/), opsize);
        return true;
    case TYPE_I_b6:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 6, opsize, false/*unsigned*/), opsize);
        return true;
    case TYPE_I_b7:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 7, opsize, false/*unsigned*/), opsize);
        if (opsize == OPSZ_5b) {
            decode_register_shift(di, array, counter);
        }
        return true;
    case TYPE_I_b8:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 8, opsize, false/*unsigned*/), opsize);
        return true;
    case TYPE_I_b9:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 9, opsize, false/*unsigned*/), opsize);
        return true;
    case TYPE_I_b10:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 10, opsize, false/*unsign*/), opsize);
        return true;
    case TYPE_I_b16:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 16, opsize, false/*unsign*/), opsize);
        return true;
    case TYPE_I_b17:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 17, opsize, false/*unsign*/), opsize);
        return true;
    case TYPE_I_b18:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 18, opsize, false/*unsign*/), opsize);
        return true;
    case TYPE_I_b19:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 19, opsize, false/*unsign*/), opsize);
        return true;
    case TYPE_I_b20:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 20, opsize, false/*unsign*/), opsize);
        return true;
    case TYPE_I_b21:
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 21, opsize, false/*unsign*/), opsize);
        return true;
    case TYPE_NI_b8_b0:
    case TYPE_I_b8_b0: {
        if (opsize == OPSZ_2) {
            val = decode_immed(di, 0, OPSZ_4b, false/*unsigned*/);
            val |= (decode_immed(di, 8, OPSZ_12b, false/*unsigned*/) << 12);
        } else if (opsize == OPSZ_1) {
            val = decode_immed(di, 0, OPSZ_4b, false/*unsigned*/);
            val |= (decode_immed(di, 8, OPSZ_4b, false/*unsigned*/) << 4);
        } else
            CLIENT_ASSERT(false, "unsupported 8-0 split immed size");
        if (optype == TYPE_NI_b8_b0)
            val = -val;
        array[(*counter)++] = opnd_create_immed_int(val, opsize);
        return true;
    }
    case TYPE_I_b16_b0: {
        if (opsize == OPSZ_2) {
            val = decode_immed(di, 0, OPSZ_12b, false/*unsigned*/);
            val |= (decode_immed(di, 16, OPSZ_4b, false/*unsigned*/) << 12);
        } else if (opsize == OPSZ_1) {
            val = decode_immed(di, 0, OPSZ_4b, false/*unsigned*/);
            val |= (decode_immed(di, 16, OPSZ_4b, false/*unsigned*/) << 4);
        } else
            CLIENT_ASSERT(false, "unsupported 16-0 split immed size");
        array[(*counter)++] = opnd_create_immed_int(val, opsize);
        return true;
    }
    case TYPE_I_b0_b5: {
        if (opsize == OPSZ_5b) {
            val = decode_immed(di, 5, OPSZ_1b, false/*unsigned*/);
            val |= (decode_immed(di, 0, OPSZ_4b, false/*unsigned*/) << 1);
        } else
            CLIENT_ASSERT(false, "unsupported 0-5 split immed size");
        array[(*counter)++] = opnd_create_immed_int(val, opsize);
        return true;
    }
    case TYPE_I_b5_b3: { /* OP_vmla scalar: M:Vm<3> */
        if (opsize == OPSZ_2b) {
            val = decode_immed(di, 3, OPSZ_1b, false/*unsigned*/);
            val |= (decode_immed(di, 5, OPSZ_1b, false/*unsigned*/) << 1);
        } else
            CLIENT_ASSERT(false, "unsupported 5-3 split immed size");
        array[(*counter)++] = opnd_create_immed_int(val, opsize);
        return true;
    }
    case TYPE_I_b8_b16: { /* OP_msr */
        if (opsize == OPSZ_5b) {
            val = decode_immed(di, 16, OPSZ_4b, false/*unsigned*/);
            val |= (decode_immed(di, 8, OPSZ_1b, false/*unsigned*/) << 4);
        } else
            CLIENT_ASSERT(false, "unsupported 8-16 split immed size");
        array[(*counter)++] = opnd_create_immed_int(val, opsize);
        return true;
    }
    case TYPE_I_b21_b5: { /* OP_vmov: 21,6:5 */
        if (opsize == OPSZ_3b) {
            val = decode_immed(di, 5, OPSZ_2b, false/*unsigned*/);
            val |= (decode_immed(di, 21, OPSZ_1b, false/*unsigned*/) << 2);
        } else
            CLIENT_ASSERT(false, "unsupported 21-5 split immed size");
        array[(*counter)++] = opnd_create_immed_int(val, opsize);
        return true;
    }
    case TYPE_I_b21_b6: { /* OP_vmov: 21,6 */
        if (opsize == OPSZ_2b) {
            val = decode_immed(di, 6, OPSZ_1b, false/*unsigned*/);
            val |= (decode_immed(di, 21, OPSZ_1b, false/*unsigned*/) << 1);
        } else
            CLIENT_ASSERT(false, "unsupported 21-6 split immed size");
        array[(*counter)++] = opnd_create_immed_int(val, opsize);
        return true;
    }
    case TYPE_I_b24_b16_b0: { /* OP_vbic, OP_vmov: 24,18:16,3:0 */
        if (opsize == OPSZ_1) {
            val = decode_immed(di, 0, OPSZ_4b, false/*unsigned*/);
            val |= (decode_immed(di, 16, OPSZ_3b, false/*unsigned*/) << 4);
            val |= (decode_immed(di, 24, OPSZ_1b, false/*unsigned*/) << 7);
        } else
            CLIENT_ASSERT(false, "unsupported 24-16-0 split immed size");
        array[(*counter)++] = opnd_create_immed_int(val, opsize);
        return true;
    }
    case TYPE_J_x4_b0: /* OP_b, OP_bl */
        array[(*counter)++] =
            /* For A32, "cur pc" is PC + 8 */
            opnd_create_pc(di->start_pc + 8 +
                           (decode_immed(di, 0, opsize, true/*signed*/) << 2));
        return true;
    case TYPE_J_b0_b24: { /* OP_blx imm24:H:0 */
        if (opsize == OPSZ_25b) {
            val = decode_immed(di, 24, OPSZ_1b, false/*unsigned*/) << 1;
            val |= (decode_immed(di, 0, OPSZ_3, false/*unsigned*/) << 2);
        } else
            CLIENT_ASSERT(false, "unsupported 0-24 split immed size");
        /* For A32, "cur pc" is PC + 8 */
        array[(*counter)++] = opnd_create_pc(di->start_pc + 8 + val);
        return true;
    }
    case TYPE_SHIFT_b5:
        di->shift_type_idx = *counter;
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 5, opsize, false/*unsigned*/),
                                  opsize);
        return true;
    case TYPE_SHIFT_b6: /* value is :0 */
        di->shift_type_idx = *counter;
        array[(*counter)++] =
            opnd_create_immed_int(decode_immed(di, 5, opsize, false/*unsigned*/) << 1,
                                  OPSZ_2b);
        return true;
    case TYPE_SHIFT_LSL:
        array[(*counter)++] = opnd_create_immed_int(SHIFT_ENCODING_LSL, opsize);
        return true;
    case TYPE_SHIFT_ASR:
        array[(*counter)++] = opnd_create_immed_int(SHIFT_ENCODING_ASR, opsize);
        return true;
    case TYPE_K:
        array[(*counter)++] = opnd_create_immed_int(opsize, OPSZ_0);
        return true;

    /* Memory */
    /* Only some types are ever used with register lists. */
    case TYPE_M:
        opsize = decode_mem_reglist_size(di, &array[*counter], opsize, false/*just sz*/);
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0, 0, opsize);
        return true;
    case TYPE_M_POS_I12:
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0,
                                  decode_immed(di, 0, OPSZ_12b, false/*unsigned*/),
                                  opsize);
        return true;
    case TYPE_M_NEG_I12:
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0,
                                  -decode_immed(di, 0, OPSZ_12b, false/*unsigned*/),
                                  opsize);
        return true;
    case TYPE_M_POS_REG:
    case TYPE_M_NEG_REG:
        array[(*counter)++] =
            opnd_create_base_disp_arm(decode_regA(di), decode_regD(di), DR_SHIFT_NONE, 0,
                                      0, optype == TYPE_M_NEG_REG ?
                                      DR_OPND_NEGATED : 0, opsize);
        return true;
    case TYPE_M_POS_SHREG:
    case TYPE_M_NEG_SHREG: {
        uint amount;
        dr_shift_type_t shift = decode_index_shift(di, &amount);
        array[(*counter)++] =
            opnd_create_base_disp_arm(decode_regA(di), decode_regD(di), shift, amount,
                                      0, optype == TYPE_M_NEG_SHREG ?
                                      DR_OPND_NEGATED : 0, opsize);
        return true;
    }
    case TYPE_M_SI9:
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0,
                                  /* 9-bit signed immed @ 20:12 */
                                  decode_immed(di, 12, OPSZ_9b, true/*signed*/),
                                  opsize);
        return true;
    case TYPE_M_SI7:
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0,
                                  decode_immed(di, 0, OPSZ_7b, true/*signed*/),
                                  opsize);
        return true;
    case TYPE_M_POS_I8:
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0,
                                  4*decode_immed(di, 0, OPSZ_1, false/*unsigned*/),
                                  opsize);
        return true;
    case TYPE_M_NEG_I8:
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0,
                                  -4*decode_immed(di, 0, OPSZ_1, false/*unsigned*/),
                                  opsize);
        return true;
    case TYPE_M_POS_I4_4: {
        ptr_int_t val =
            (decode_immed(di, 8, OPSZ_4b, false/*unsigned*/) << 4) |
            decode_immed(di, 0, OPSZ_4b, false/*unsigned*/);
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0, val, opsize);
        return true;
    }
    case TYPE_M_NEG_I4_4: {
        ptr_int_t val =
            (decode_immed(di, 8, OPSZ_4b, false/*unsigned*/) << 4) |
            decode_immed(di, 0, OPSZ_4b, false/*unsigned*/);
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0, -val, opsize);
        return true;
    }
    case TYPE_M_POS_I5:
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0,
                                  decode_immed(di, 0, OPSZ_5b, false/*unsigned*/),
                                  opsize);
        return true;
    case TYPE_M_UP_OFFS:
        opsize = decode_mem_reglist_size(di, &array[*counter], opsize, false/*just sz*/);
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0, sizeof(void*), opsize);
        return true;
    case TYPE_M_DOWN:
        opsize = decode_mem_reglist_size(di, &array[*counter], opsize, true/*disp*/);
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0, 0, opsize);
        return true;
    case TYPE_M_DOWN_OFFS:
        opsize = decode_mem_reglist_size(di, &array[*counter], opsize, true/*disp*/);
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0, -sizeof(void*), opsize);
        return true;

    default:
        array[(*counter)++] = opnd_create_null();
        /* ok to assert, types coming only from instr_info_t */
        SYSLOG_INTERNAL_ERROR("unknown operand type %s\n", type_names[optype]);
        CLIENT_ASSERT(false, "decode error: unknown operand type");
    }
    return false;
}

/* Disassembles the instruction at pc into the data structures ret_info
 * and di.  Returns a pointer to the pc of the next instruction.
 * Returns NULL on an invalid instruction.
 * Caller should set di->isa_mode.
 */
static byte *
read_instruction(byte *pc, byte *orig_pc,
                 const instr_info_t **ret_info, decode_info_t *di
                 _IF_DEBUG(bool report_invalid))
{
    uint instr_word;
    const instr_info_t *info;
    uint idx;

    /* Read instr bytes and initialize di */
    di->start_pc = pc;
    di->orig_pc = orig_pc;
    instr_word = *(uint *)pc;
    pc += sizeof(instr_word);
    di->instr_word = instr_word;
    di->mem_needs_reglist_sz = NULL;
    di->reglist_sz = -1;

    di->predicate = decode_predicate(instr_word) + DR_PRED_EQ;
    if (di->predicate == DR_PRED_OP) {
        uint opc7 = /* remove bit 22 */
            ((instr_word >> 21) & 0x7c) | ((instr_word >> 20) & 0x3);
        info = &A32_unpred_opc7[opc7];
    } else {
        uint opc8 = decode_opc8(instr_word);
        info = &A32_pred_opc8[opc8];
    }

    /* If an extension, discard the old info and get a new one */
    while (info->type > INVALID) {
        if (info->type == EXT_OPC4X) {
            if ((instr_word & 0x10 /*bit 4*/) == 0)
                idx = 0;
            else if ((instr_word & 0x80 /*bit 7*/) == 0)
                idx = 1;
            else
                idx = 2 + ((instr_word >> 5) & 0x3) /*bits 6:5*/;
            info = &A32_ext_opc4x[info->code][idx];
        } else if (info->type == EXT_OPC4Y) {
            if ((instr_word & 0x10 /*bit 4*/) == 0)
                idx = 0;
            else
                idx = 1 + ((instr_word >> 5) & 0x7) /*bits 7:5*/;
            info = &A32_ext_opc4y[info->code][idx];
        } else if (info->type == EXT_OPC4) {
            idx = decode_opc4(instr_word);
            info = &A32_ext_opc4[info->code][idx];
        } else if (info->type == EXT_IMM1916) {
            idx = (((instr_word >> 16) & 0xf) /*bits 19:16*/ == 0) ? 0 : 1;
            info = &A32_ext_imm1916[info->code][idx];
        } else if (info->type == EXT_BIT4) {
            idx = (instr_word >> 4) & 0x1 /*bit 4*/;
            info = &A32_ext_bit4[info->code][idx];
        } else if (info->type == EXT_BIT5) {
            idx = (instr_word >> 5) & 0x1 /*bit 5*/;
            info = &A32_ext_bit5[info->code][idx];
        } else if (info->type == EXT_BIT9) {
            idx = (instr_word >> 9) & 0x1 /*bit 9*/;
            info = &A32_ext_bit9[info->code][idx];
        } else if (info->type == EXT_BITS8) {
            idx = (instr_word >> 8) & 0x3 /*bits 9:8*/;
            info = &A32_ext_bits8[info->code][idx];
        } else if (info->type == EXT_BITS0) {
            idx = instr_word & 0x7 /*bits 2:0*/;
            info = &A32_ext_bits0[info->code][idx];
        } else if (info->type == EXT_IMM5) {
            idx = (((instr_word >> 7) & 0x1f) /*bits 11:7*/ == 0) ? 0 : 1;
            info = &A32_ext_imm5[info->code][idx];
        } else if (info->type == EXT_FP) {
            idx = ((instr_word >> 8) & 0xf) /*bits 11:8*/;
            idx = (idx == 0xa ? 0 : (idx == 0xb ? 1 : 2));
            info = &A32_ext_fp[info->code][idx];
        } else if (info->type == EXT_FPA) {
            idx = ((instr_word >> 4) & 0x7) /*bits 6:4*/;
            idx = (idx == 0 ? 0 : (idx == 1 ? 1 : (idx == 4 ? 2 : 3)));
            if (idx == 3)
                info = &invalid_instr;
            else
                info = &A32_ext_opc4fpA[info->code][idx];
        } else if (info->type == EXT_FPB) {
            idx = ((instr_word >> 4) & 0x7) /*bits 6:4*/;
            info = &A32_ext_opc4fpB[info->code][idx];
        } else if (info->type == EXT_BITS16) {
            idx = ((instr_word >> 16) & 0xf) /*bits 19:16*/;
            info = &A32_ext_bits16[info->code][idx];
        } else if (info->type == EXT_RBPC) {
            idx = (((instr_word >> 12) & 0xf) /*bits 19:16*/ != 0xf) ? 0 : 1;
            info = &A32_ext_RBPC[info->code][idx];
        } else if (info->type == EXT_RDPC) {
            idx = ((instr_word & 0xf) /*bits 3:0*/ == 0xf) ? 1 : 0;
            info = &A32_ext_RDPC[info->code][idx];
        } else if (info->type == EXT_BIT6) {
            idx = ((instr_word >> 6) & 0x1) /*bit 6 */;
            info = &A32_ext_bit6[info->code][idx];
        } else if (info->type == EXT_BIT7) {
            idx = ((instr_word >> 7) & 0x1) /*bit 7 */;
            info = &A32_ext_bit7[info->code][idx];
        } else if (info->type == EXT_BIT19) {
            idx = ((instr_word >> 19) & 0x1) /*bit 19 */;
            info = &A32_ext_bit19[info->code][idx];
        } else if (info->type == EXT_BIT22) {
            idx = ((instr_word >> 22) & 0x1) /*bit 22 */;
            info = &A32_ext_bit22[info->code][idx];
        } else if (info->type == EXT_BITS20) {
            idx = ((instr_word >> 20) & 0xf) /*bits 23:20 */;
            info = &A32_ext_bits20[info->code][idx];
        } else if (info->type == EXT_IMM1816) {
            idx = (((instr_word >> 16) & 0x7) /*bits 18:16*/ == 0) ? 0 : 1;
            info = &A32_ext_imm1816[info->code][idx];
        } else if (info->type == EXT_IMM2016) {
            idx = (((instr_word >> 16) & 0x1f) /*bits 20:16*/ == 0) ? 0 : 1;
            info = &A32_ext_imm2016[info->code][idx];
        } else if (info->type == EXT_SIMD6) {
            idx =  /*6 bits 11:8,6,4 */
                ((instr_word >> 6) & 0x3c) |
                ((instr_word >> 5) & 0x2) |
                ((instr_word >> 4) & 0x1);
            info = &A32_ext_simd6[info->code][idx];
        } else if (info->type == EXT_SIMD5) {
            idx =  /*5 bits 11:8,5 */
                ((instr_word >> 7) & 0x1e) | ((instr_word >> 5) & 0x1);
            info = &A32_ext_simd5[info->code][idx];
        } else if (info->type == EXT_SIMD5B) {
            idx = /*bits 18:16,8:7 */
                ((instr_word >> 14) & 0x1c) | ((instr_word >> 7) & 0x3);
            info = &A32_ext_simd5b[info->code][idx];
        } else if (info->type == EXT_SIMD8) {
            /* Odds + 0 == 9 entries each */
            idx = 9 * ((instr_word >> 8) & 0xf) /*bits 11:8*/;
            if (((instr_word >> 4) & 0x1) != 0)
                idx += 1 + ((instr_word >> 5) & 0x7) /*bits 7:5*/;
            info = &A32_ext_simd8[info->code][idx];
        } else if (info->type == EXT_SIMD6B) {
            idx = /*bits 11:8,7:6 */
                ((instr_word >> 6) & 0x3c) | ((instr_word >> 6) & 0x3);
            info = &A32_ext_simd6b[info->code][idx];
        } else if (info->type == EXT_SIMD6C) {
            /* bits 10:8,7:6 + extra set of 7:6 for bit 11 being set */
            if (((instr_word >> 11) & 0x1) != 0)
                idx = 32 + ((instr_word >> 6) & 0x3);
            else
                idx = ((instr_word >> 6) & 0x1c) | ((instr_word >> 6) & 0x3);
            info = &A32_ext_simd6c[info->code][idx];
        } else if (info->type == EXT_SIMD2) {
            idx = /*11,6 */
                ((instr_word >> 10) & 0x2) | ((instr_word >> 6) & 0x1);
            info = &A32_ext_simd2[info->code][idx];
        } else if (info->type == EXT_VLDA) {
            int reg = (instr_word & 0xf);
            idx = /*bits (11:8,7:6)*3+X where X based on value of 3:0 */
                3 * (((instr_word >> 6) & 0x3c) | ((instr_word >> 6) & 0x3));
            idx += (reg == 0xd ? 0 : (reg == 0xf ? 1 : 2));
            /* this table stops at 0xa in top bits, to save space */
            if (((instr_word >> 8) & 0xf) > 0xa)
                info = &invalid_instr;
            else
                info = &A32_ext_vldA[info->code][idx];
        } else if (info->type == EXT_VLDB) {
            int reg = (instr_word & 0xf);
            idx = /*bits (11:8,Y)*3+X where X based on value of 3:0 */
                ((instr_word >> 7) & 0x1e);
            /* Y is bit 6 if bit 11 is set; else, bit 5 */
            if (((instr_word >> 11) & 0x1) != 0)
                idx |= ((instr_word >> 6) & 0x1);
            else
                idx |= ((instr_word >> 5) & 0x1);
            idx *= 3;
            idx += (reg == 0xd ? 0 : (reg == 0xf ? 1 : 2));
            info = &A32_ext_vldB[info->code][idx];
        } else if (info->type == EXT_VLDC) {
            int reg = (instr_word & 0xf);
            idx = /*bits (9:8,7:5)*3+X where X based on value of 3:0 */
                3 * (((instr_word >> 5) & 0x18) | ((instr_word >> 5) & 0x7));
            idx += (reg == 0xd ? 0 : (reg == 0xf ? 1 : 2));
            info = &A32_ext_vldC[info->code][idx];
        } else if (info->type == EXT_VTB) {
            idx = ((instr_word >> 10) & 0x3) /*bits 11:10 */;
            if (idx != 2)
                idx = 0;
            else {
                idx = 1 +   /*3 bits 9:8,6 */
                    (((instr_word >> 7) & 0x6) |
                     ((instr_word >> 6) & 0x1));
            }
            info = &A32_ext_vtb[info->code][idx];
        }
    }
    CLIENT_ASSERT(info->type <= INVALID, "decoding table error");

    /* All required bits should be set */
    if ((instr_word & info->opcode) != info->opcode && info->type != INVALID)
        info = &invalid_instr;

    /* We should now have either a valid OP_ opcode or an invalid opcode */
    if (info == &invalid_instr || info->type < OP_FIRST || info->type > OP_LAST) {
        DODEBUG({
            /* PR 605161: don't report on DR addresses */
            if (report_invalid && !is_dynamo_address(di->start_pc)) {
                SYSLOG_INTERNAL_WARNING_ONCE("Invalid opcode encountered");
                LOG(THREAD_GET, LOG_ALL, 1, "Invalid opcode @"PFX": 0x%016x\n",
                    di->start_pc, instr_word);
            }
        });
        *ret_info = &invalid_instr;
        return NULL;
    }

    /* Unlike x86, we have a fixed size, so we're done */
    *ret_info = info;
    return pc;
}

byte *
decode_eflags_usage(dcontext_t *dcontext, byte *pc, uint *usage,
                    dr_opnd_query_flags_t flags)
{
    const instr_info_t *info;
    decode_info_t di;
    di.isa_mode = dr_get_isa_mode(dcontext);
    pc = read_instruction(pc, pc, &info, &di _IF_DEBUG(true));
    *usage = instr_eflags_conditionally(info->eflags, di.predicate, flags);
    /* we're fine returning NULL on failure */
    return pc;
}

byte *
decode_opcode(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    const instr_info_t *info;
    decode_info_t di;
    di.isa_mode = dr_get_isa_mode(dcontext);
    pc = read_instruction(pc, pc, &info, &di _IF_DEBUG(true));
    instr_set_isa_mode(instr, di.isa_mode);
    instr_set_opcode(instr, info->type);
    if (!instr_valid(instr)) {
        CLIENT_ASSERT(!instr_valid(instr), "decode_opcode: invalid instr");
        return NULL;
    }
    instr->eflags = info->eflags;
    instr_set_eflags_valid(instr, true);
    instr_set_operands_valid(instr, false);
    instr_set_raw_bits(instr, pc, pc - di.orig_pc);
    return pc;
}

/* XXX: some of this code could be shared with x86/decode.c */
static byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr)
{
    const instr_info_t *info;
    decode_info_t di;
    byte *next_pc;
    uint num_dsts = 0, num_srcs = 0;
    opnd_t dsts[MAX_OPNDS];
    opnd_t srcs[MAX_OPNDS];

    CLIENT_ASSERT(instr->opcode == OP_INVALID || instr->opcode == OP_UNDECODED,
                  "decode: instr is already decoded, may need to call instr_reset()");

    di.isa_mode = dr_get_isa_mode(dcontext);
    next_pc = read_instruction(pc, orig_pc, &info, &di
                               _IF_DEBUG(!TEST(INSTR_IGNORE_INVALID, instr->flags)));
    instr_set_isa_mode(instr, di.isa_mode);
    instr_set_opcode(instr, info->type);
    /* failure up to this point handled fine -- we set opcode to OP_INVALID */
    if (next_pc == NULL) {
        LOG(THREAD, LOG_INTERP, 3, "decode: invalid instr at "PFX"\n", pc);
        CLIENT_ASSERT(!instr_valid(instr), "decode: invalid instr");
        return NULL;
    }
    instr->eflags = info->eflags;
    instr_set_eflags_valid(instr, true);
    /* since we don't use set_src/set_dst we must explicitly say they're valid */
    instr_set_operands_valid(instr, true);

    if (di.predicate != DR_PRED_OP) {
        /* XXX: not bothering to mark invalid for DECODE_PREDICATE_AL */
        instr_set_predicate(instr, di.predicate);
    }

    /* operands */
    do {
        if (info->dst1_type != TYPE_NONE) {
            if (!decode_operand(&di, info->dst1_type, info->dst1_size, dsts, &num_dsts))
                goto decode_invalid;
        }
        if (info->dst2_type != TYPE_NONE) {
            if (!decode_operand(&di, info->dst2_type, info->dst2_size,
                                TEST(DECODE_4_SRCS, info->flags) ? srcs : dsts,
                                TEST(DECODE_4_SRCS, info->flags) ? &num_srcs : &num_dsts))
                goto decode_invalid;
        }
        if (info->src1_type != TYPE_NONE) {
            if (!decode_operand(&di, info->src1_type, info->src1_size,
                                TEST(DECODE_3_DSTS, info->flags) ? dsts : srcs,
                                TEST(DECODE_3_DSTS, info->flags) ? &num_dsts : &num_srcs))
                goto decode_invalid;
        }
        if (info->src2_type != TYPE_NONE) {
            if (!decode_operand(&di, info->src2_type, info->src2_size, srcs, &num_srcs))
                goto decode_invalid;
        }
        if (info->src3_type != TYPE_NONE) {
            if (!decode_operand(&di, info->src3_type, info->src3_size, srcs, &num_srcs))
                goto decode_invalid;
        }
        info = instr_info_extra_opnds(info);
    } while (info != NULL);

    CLIENT_ASSERT(num_srcs < sizeof(srcs)/sizeof(srcs[0]), "internal decode error");
    CLIENT_ASSERT(num_dsts < sizeof(dsts)/sizeof(dsts[0]), "internal decode error");

    /* now copy operands into their real slots */
    instr_set_num_opnds(dcontext, instr, num_dsts, num_srcs);
    if (num_dsts > 0) {
        memcpy(instr->dsts, dsts, num_dsts*sizeof(opnd_t));
    }
    if (num_srcs > 0) {
        instr->src0 = srcs[0];
        if (num_srcs > 1) {
            memcpy(instr->srcs, &(srcs[1]), (num_srcs-1)*sizeof(opnd_t));
        }
    }

    if (orig_pc != pc) {
        /* We do not want to copy when encoding and condone an invalid
         * relative target
         */
        instr_set_raw_bits_valid(instr, false);
        instr_set_translation(instr, orig_pc);
    } else {
        /* we set raw bits AFTER setting all srcs and dsts b/c setting
         * a src or dst marks instr as having invalid raw bits
         */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(next_pc - pc)));
        instr_set_raw_bits(instr, pc, (uint)(next_pc - pc));
    }

    return next_pc;

 decode_invalid:
    instr_set_operands_valid(instr, false);
    instr_set_opcode(instr, OP_INVALID);
    return NULL;
}

byte *
decode(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    return decode_common(dcontext, pc, pc, instr);
}

byte *
decode_from_copy(dcontext_t *dcontext, byte *copy_pc, byte *orig_pc, instr_t *instr)
{
    return decode_common(dcontext, copy_pc, orig_pc, instr);
}

byte *
decode_cti(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    /* XXX i#1551: build a fast decoder for branches -- though it may not make
     * sense for 32-bit where many instrs can write to the pc.
     */
    return decode(dcontext, pc, instr);
}

byte *
decode_next_pc(dcontext_t *dcontext, byte *pc)
{
    /* FIXME i#1551: check for invalid opcodes */
    /* FIXME i#1551: add Thumb support */
    return pc + 4;
}

int
decode_sizeof(dcontext_t *dcontext, byte *pc, int *num_prefixes
              _IF_X64(uint *rip_rel_pos))
{
    /* FIXME i#1551: check for invalid opcodes */
    /* FIXME i#1551: add Thumb support */
    return 4;
}

/* XXX: share this with x86 */
byte *
decode_raw(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    /* XXX i#1551: set isa_mode of instr once we add that feature */
    int sz = decode_sizeof(dcontext, pc, NULL _IF_X64(NULL));
    if (sz == 0) {
        /* invalid instruction! */
        instr_set_opcode(instr, OP_INVALID);
        return NULL;
    }
    instr_set_opcode(instr, OP_UNDECODED);
    instr_set_raw_bits(instr, pc, sz);
    /* assumption: operands are already marked invalid (instr was reset) */
    return (pc + sz);
}

const instr_info_t *
instr_info_extra_opnds(const instr_info_t *info)
{
    /* XXX i#1551: pick proper *_extra_operands table */
    if (TEST(DECODE_EXTRA_SHIFT, info->flags))
        return &A32_extra_operands[0];
    else if (TEST(DECODE_EXTRA_WRITEBACK, info->flags))
        return &A32_extra_operands[1];
    else if (TEST(DECODE_EXTRA_WRITEBACK2, info->flags))
        return &A32_extra_operands[2];
    else if (TEST(DECODE_EXTRA_OPERANDS, info->flags))
        return (const instr_info_t *)(info->code);
    else
        return NULL;
}

/* num is 0-based */
byte
instr_info_opnd_type(const instr_info_t *info, bool src, int num)
{
    int i = 0;
    while (info != NULL) {
        if (!src && i++ == num)
            return info->dst1_type;
        if (TEST(DECODE_4_SRCS, info->flags)) {
            if (src && i++ == num)
                return info->dst2_type;
        } else {
            if (!src && i++ == num)
                return info->dst2_type;
        }
        if (TEST(DECODE_3_DSTS, info->flags)) {
            if (!src && i++ == num)
                return info->src1_type;
        } else {
            if (src && i++ == num)
                return info->src1_type;
        }
        if (src && i++ == num)
            return info->src2_type;
        if (src && i++ == num)
            return info->src3_type;
        info = instr_info_extra_opnds(info);
    }
    CLIENT_ASSERT(false, "internal decode error");
    return TYPE_NONE;
}

const instr_info_t *
get_next_instr_info(const instr_info_t * info)
{
    return (const instr_info_t *)(info->code);
}

byte
decode_first_opcode_byte(int opcode)
{
    CLIENT_ASSERT(false, "should not be used on ARM");
    return 0;
}

const instr_info_t *
opcode_to_encoding_info(uint opc, dr_isa_mode_t isa_mode)
{
    if (isa_mode == DR_ISA_ARM_A32)
        return op_instr_A32[opc];
    CLIENT_ASSERT(false, "NYI i#1551");
    return &invalid_instr;
}

DR_API
const char *
decode_opcode_name(int opcode)
{
    const instr_info_t * info =
        opcode_to_encoding_info(opcode, dr_get_isa_mode(get_thread_private_dcontext()));
    if (info != NULL)
        return info->name;
    else
        return "<unknown>";
}

opnd_size_t
resolve_variable_size(decode_info_t *di, opnd_size_t sz, bool is_reg)
{
    return sz;
}

bool
optype_is_indir_reg(int optype)
{
    return false;
}

bool
optype_is_reg(int optype)
{
    switch (optype) {
    case TYPE_R_A:
    case TYPE_R_B:
    case TYPE_R_C:
    case TYPE_R_D:
    case TYPE_R_A_TOP:
    case TYPE_R_B_TOP:
    case TYPE_R_C_TOP:
    case TYPE_R_D_TOP:
    case TYPE_R_D_NEGATED:
    case TYPE_R_B_EVEN:
    case TYPE_R_B_PLUS1:
    case TYPE_R_D_EVEN:
    case TYPE_R_D_PLUS1:
    case TYPE_CR_A:
    case TYPE_CR_B:
    case TYPE_CR_C:
    case TYPE_CR_D:
    case TYPE_V_A:
    case TYPE_V_B:
    case TYPE_V_C:
    case TYPE_V_C_3b:
    case TYPE_V_C_4b:
    case TYPE_W_A:
    case TYPE_W_B:
    case TYPE_W_C:
    case TYPE_W_C_PLUS1:
    case TYPE_SPSR:
    case TYPE_CPSR:
    case TYPE_FPSCR:
    case TYPE_LR:
    case TYPE_SP:
        return true;
    }
    return false;
}

#ifdef DEBUG
# ifndef STANDALONE_DECODER
static bool
optype_is_reglist(int optype)
{
    switch (optype) {
    case TYPE_L_8b:
    case TYPE_L_13b:
    case TYPE_L_16b:
    case TYPE_L_CONSEC:
    case TYPE_L_VBx2:
    case TYPE_L_VBx3:
    case TYPE_L_VBx4:
    case TYPE_L_VBx2D:
    case TYPE_L_VBx3D:
    case TYPE_L_VBx4D:
    case TYPE_L_VAx2:
    case TYPE_L_VAx3:
    case TYPE_L_VAx4:
        return true;
    }
    return false;
}

static void
decode_check_opnds(int optype[], uint num_types)
{
    /* Ensure at most 1 reglist, and at most 1 reg after a reglist */
    uint i, num_reglist = 0, reglist_idx;
    bool post_reglist = false;
    for (i = 0; i < num_types; i++) {
        if (optype_is_reglist(optype[i])) {
            num_reglist++;
            reglist_idx = i;
            post_reglist = true;
        } else if (post_reglist) {
            if (optype_is_reg(optype[i]))
                ASSERT(reglist_idx == i - 1);
            else
                post_reglist = false;
        }
    }
    ASSERT(num_reglist <= 1);
}
# endif /* STANDALONE_DECODER */

void
decode_debug_checks_arch(void)
{
#   define MAX_TYPES 8
    DOCHECK(2, {
        uint opc;
        for (opc = OP_FIRST; opc < OP_AFTER_LAST; opc++) {
            const instr_info_t *info = opcode_to_encoding_info(opc, DR_ISA_ARM_A32);
            while (info != NULL && info != &invalid_instr && info->type != OP_CONTD) {
                const instr_info_t *ops = info;
                uint num_srcs = 0;
                uint num_dsts = 0;
                /* XXX: perhaps we should make an iterator and use it everywhere.
                 * For now, for simplicity here we use two passes.
                 */
                int src_type[MAX_TYPES];
                int dst_type[MAX_TYPES];
                while (ops != NULL) {
                    dst_type[num_dsts++] = ops->dst1_type;
                    if (TEST(DECODE_4_SRCS, ops->flags))
                        src_type[num_srcs++] = ops->dst2_type;
                    else
                        dst_type[num_dsts++] = ops->dst2_type;
                    if (TEST(DECODE_3_DSTS, ops->flags))
                        dst_type[num_dsts++] = ops->src1_type;
                    else
                        src_type[num_srcs++] = ops->src1_type;
                    src_type[num_srcs++] = ops->src2_type;
                    src_type[num_srcs++] = ops->src3_type;
                    ops = instr_info_extra_opnds(ops);
                }
                ASSERT(num_dsts <= MAX_TYPES);
                ASSERT(num_srcs <= MAX_TYPES);

                /* Sanity-check encoding chain */
                ASSERT(info->type == opc);

                decode_check_opnds(dst_type, num_dsts);
                decode_check_opnds(src_type, num_srcs);

                info = get_next_instr_info(info);
            }
        }
    });
}
#endif

#ifdef DECODE_UNIT_TEST
/* FIXME i#1551: add unit tests here.  How divide vs suite/tests/api/ tests? */
# include "instr_create.h"

int main()
{
    bool res = true;
    standalone_init();
    return res;
}

#endif /* DECODE_UNIT_TEST */
