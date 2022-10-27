/* **********************************************************
 * Copyright (c) 2014-2022 Google, Inc.  All rights reserved.
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
#include "decode_fast.h" /* ensure we export decode_next_pc, decode_sizeof */
#include "instr_create_shared.h"
#include "disassemble.h"

/* ARM decoder.
 * General strategy:
 * + We use a data-driven table-based approach, as we need to both encode and
 *   decode and a central source of data lets us move in both directions.
 *
 * Potential shortcomings:
 * + i#1685: We do not bother to ensure that "reserved bits" (in
 *   parentheses in the manual: "(0)") set to 0 are in fact 0 as that
 *   would require a whole separate mask in our table entries.  Often
 *   the current processors execute these just fine when set to 1 and
 *   we would much rather err on the side of too permissive.
 * + Similarly (also i#1685), we are not currently modeling all the
 *   widely varying unpredictable conditions when pc or lr is used:
 *   xref notes at the top of table_a32_pred.c.
 */
/* FIXME i#1569: add A64 support: for now just A32 */

/* Global data structure to track the decode state,
 * it should be only used for drdecodelib or early init/late exit.
 * FIXME i#1595: add multi-dcontext support to drdecodelib.
 */
static decode_state_t global_decode_state;

static decode_state_t *
get_decode_state(dcontext_t *dcontext)
{
    ASSERT(sizeof(decode_state_t) <= sizeof(dcontext->decode_state));
    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT)
        return &global_decode_state;
    else
        return (decode_state_t *)&dcontext->decode_state;
}

static void
set_decode_state(dcontext_t *dcontext, decode_state_t *state)
{
    ASSERT(sizeof(*state) <= sizeof(dcontext->decode_state));
    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT)
        global_decode_state = *state;
    else {
        *(decode_state_t *)&dcontext->decode_state = *state;
    }
}

static void
decode_state_init(decode_state_t *state, decode_info_t *di, app_pc pc)
{
    LOG(THREAD_GET, LOG_EMIT, 5, "start IT block\n");
    it_block_info_init(&state->itb_info, di);
    state->pc = pc + THUMB_SHORT_INSTR_SIZE /*IT instr length*/;
}

static void
decode_state_reset(decode_state_t *state)
{
    LOG(THREAD_GET, LOG_EMIT, 5, "exited IT block\n");
    it_block_info_reset(&state->itb_info);
    state->pc = NULL;
}

/* Return current predicate and advance to next instr in the IT block.
 * Leaves the pc where it was if we are at the end of the current IT block,
 * so we can handle a duplicate call to the same pc in decode_in_it_block().
 */
static dr_pred_type_t
decode_state_advance(decode_state_t *state, decode_info_t *di)
{
    dr_pred_type_t pred;
    pred = it_block_instr_predicate(state->itb_info, state->itb_info.cur_instr);
    /* We don't want to point pc beyond the end of the IT block to avoid our
     * prior-pc-matching logic in decode_in_it_block().
     */
    if (it_block_info_advance(&state->itb_info))
        state->pc += (di->T32_16 ? THUMB_SHORT_INSTR_SIZE : THUMB_LONG_INSTR_SIZE);
    return pred;
}

static bool
decode_in_it_block(decode_state_t *state, app_pc pc, decode_info_t *di)
{
    if (state->itb_info.num_instrs != 0) {
        LOG(THREAD_GET, LOG_EMIT, 5, "in IT?: cur=%d/%d, " PFX " vs " PFX "\n",
            state->itb_info.cur_instr, state->itb_info.num_instrs, state->pc, pc);
        if (pc == state->pc) {
            /* Look for a duplicate call to the final instr in the block, where
             * we left pc where it was.
             */
            if (state->itb_info.cur_instr == state->itb_info.num_instrs) {
                /* Undo the advance */
                state->itb_info.cur_instr--;
                LOG(THREAD_GET, LOG_EMIT, 5, "in IT block 2x\n");
            } else /* Normal advance */
                LOG(THREAD_GET, LOG_EMIT, 5, "in IT block\n");
            return true;
        }
        /* Handle the caller invoking decode 2x in a row on the same
         * pc on the OP_it instr or a non-final instr in the block.
         */
        if ((di->T32_16 && pc == state->pc - THUMB_SHORT_INSTR_SIZE) ||
            (!di->T32_16 && pc == state->pc - THUMB_LONG_INSTR_SIZE)) {
            if (state->itb_info.cur_instr == 0 ||
                /* This is still fragile when crossing usage sequences.  The state is
                 * left in a final-IT-member state after bb building, and
                 * subsequently decoding the block again can result in incorrect
                 * advance-undoing which leads to incorrect predicate application.
                 *
                 * Our solution here is to do a raw byte check for OP_it, which is
                 * encoded as 0xbfXY where X is anything and Y is anything with at
                 * least 1 bit set.
                 */
                (di->T32_16 && *(pc + 1) == 0xbf && ((*pc) & 0x0f) != 0)) {
                ASSERT(pc == state->pc - THUMB_SHORT_INSTR_SIZE);
                return false; /* still on OP_it */
            } else {
                /* Undo the advance */
                state->pc = pc;
                state->itb_info.cur_instr--;
                LOG(THREAD_GET, LOG_EMIT, 5, "in IT block 2x\n");
                return true;
            }
        }
        /* pc not match, reset the state */
        decode_state_reset(state);
    }
    return false;
}

bool
is_isa_mode_legal(dr_isa_mode_t mode)
{
    return (mode == DR_ISA_ARM_THUMB || DR_ISA_ARM_A32);
}

/* We need to call canonicalize_pc_target() on all next_tag-writing
 * instances in initial takeover, signal handling, ibl, etc..
 * We can't put it in d_r_dispatch() b/c with our decision to store
 * tags and addresses as LSB=0, we can easily double-mode-switch.
 */
app_pc
canonicalize_pc_target(dcontext_t *dcontext, app_pc pc)
{
    if (TEST(0x1, (ptr_uint_t)pc)) {
        dr_isa_mode_t old_mode;
        dr_set_isa_mode(dcontext, DR_ISA_ARM_THUMB, &old_mode);
        DOLOG(2, LOG_TOP, {
            if (old_mode != DR_ISA_ARM_THUMB)
                LOG(THREAD, LOG_TOP, 2, "Switching to Thumb mode @" PFX "\n", pc);
        });
        return (app_pc)(((ptr_uint_t)pc) & ~0x1);
    } else {
        dr_isa_mode_t old_mode;
        dr_set_isa_mode(dcontext, DR_ISA_ARM_A32, &old_mode);
        DOLOG(2, LOG_TOP, {
            if (old_mode != DR_ISA_ARM_THUMB)
                LOG(THREAD, LOG_TOP, 2, "Switching to ARM mode @" PFX "\n", pc);
        });
        return pc;
    }
}

DR_API
app_pc
dr_app_pc_as_jump_target(dr_isa_mode_t isa_mode, app_pc pc)
{
    return PC_AS_JMP_TGT(isa_mode, pc);
}

DR_API
app_pc
dr_app_pc_as_load_target(dr_isa_mode_t isa_mode, app_pc pc)
{
    return PC_AS_LOAD_TGT(isa_mode, pc);
}

/* The "current" pc has an offset in pc-relative computations */
#define ARM_CUR_PC_OFFS 8
#define THUMB_CUR_PC_OFFS 4

app_pc
decode_cur_pc(app_pc instr_pc, dr_isa_mode_t mode, uint opcode, instr_t *instr)
{
    if (mode == DR_ISA_ARM_A32)
        return instr_pc + ARM_CUR_PC_OFFS;
    else if (mode == DR_ISA_ARM_THUMB) {
        /* The various sources of documentation are not very definitive on which
         * instructions align and which don't!
         */
        bool align = true;
        if (opcode == OP_b || opcode == OP_b_short || opcode == OP_bl ||
            opcode == OP_cbnz || opcode == OP_cbz || opcode == OP_tbb || opcode == OP_tbh)
            align = false;
        else {
            if (opcode == OP_add) {
                /* Amazingly, OP_add w/ an immed aligns, but all-register versions
                 * do not.  We could split into OP_add_imm to avoid this analysis here.
                 */
                ASSERT(instr != NULL);
                align = opnd_is_immed_int(instr_get_src(instr, 1)); /*always 2nd src*/
            } else {
                /* Certainly for OP_ldr* we have alignment */
                align = true;
            }
        }
        if (align) {
            return (app_pc)ALIGN_BACKWARD(instr_pc + THUMB_CUR_PC_OFFS,
                                          THUMB_CUR_PC_OFFS);
        } else
            return instr_pc + THUMB_CUR_PC_OFFS;
    } else {
        /* FIXME i#1569: A64 NYI */
        ASSERT_NOT_IMPLEMENTED(false);
        return instr_pc;
    }
}

static bool
reg_is_past_last_simd(reg_id_t reg, uint add)
{
    if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31)
        return reg + add > IF_X64_ELSE(DR_REG_Q31, DR_REG_Q15);
    if (reg >= DR_REG_D0 && reg <= DR_REG_D31)
        return reg + add > DR_REG_D31;
    if (reg >= DR_REG_S0 && reg <= DR_REG_S31)
        return reg + add > DR_REG_S31;
    if (reg >= DR_REG_H0 && reg <= DR_REG_H31)
        return reg + add > DR_REG_H31;
    if (reg >= DR_REG_B0 && reg <= DR_REG_B31)
        return reg + add > DR_REG_B31;
    ASSERT_NOT_REACHED();
    return true;
}

/* We assume little-endian */
static inline int
decode_predicate(uint instr_word, uint bit_pos)
{
    return (instr_word >> bit_pos) & 0xf;
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

static reg_id_t
decode_regU(decode_info_t *di)
{
    /* T32.16 = 6:3 */
    return DR_REG_START_GPR + ((di->instr_word >> 3) & 0xf);
}

static reg_id_t
decode_regV(decode_info_t *di)
{
    /* T32.16 = 7,2:0 */
    return DR_REG_START_GPR + (((di->instr_word & 0x80) >> 4) | (di->instr_word & 0x7));
}

static reg_id_t
decode_regW(decode_info_t *di)
{
    /* T32.16 = 10:8 */
    return DR_REG_START_GPR + ((di->instr_word >> 8) & 0x7);
}

static reg_id_t
decode_regX(decode_info_t *di)
{
    /* T32.16 = 8:6 */
    return DR_REG_START_GPR + ((di->instr_word >> 6) & 0x7);
}

static reg_id_t
decode_regY(decode_info_t *di)
{
    /* T32.16 = 5:3 */
    return DR_REG_START_GPR + ((di->instr_word >> 3) & 0x7);
}

static reg_id_t
decode_regZ(decode_info_t *di)
{
    /* T32.16 = 2:0 */
    return DR_REG_START_GPR + (di->instr_word & 0x7);
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
    /* A32/T32 = 7,19:16, but for Q regs 7,19:17 */
    return decode_simd_start(opsize) +
        (opsize == OPSZ_16
             ? (((di->instr_word & 0x00000080) >> 4) | ((di->instr_word >> 17) & 0x7))
             : (((di->instr_word & 0x00000080) >> 3) | ((di->instr_word >> 16) & 0xf)));
}

static reg_id_t
decode_vregB(decode_info_t *di, opnd_size_t opsize)
{
    /* A32/T32 = 22,15:12, but for Q regs 22,15:13 */
    return decode_simd_start(opsize) +
        (opsize == OPSZ_16
             ? (((di->instr_word & 0x00400000) >> 19) | ((di->instr_word >> 13) & 0x7))
             : (((di->instr_word & 0x00400000) >> 18) | ((di->instr_word >> 12) & 0xf)));
}

static reg_id_t
decode_vregC(decode_info_t *di, opnd_size_t opsize)
{
    /* A32/T32 = 5,3:0, but for Q regs 5,3:1 */
    return decode_simd_start(opsize) +
        (opsize == OPSZ_16
             ? (((di->instr_word & 0x00000020) >> 2) | ((di->instr_word >> 1) & 0x7))
             : (((di->instr_word & 0x00000020) >> 1) | (di->instr_word & 0xf)));
}

static reg_id_t
decode_wregA(decode_info_t *di, opnd_size_t opsize)
{
    /* A32/T32 = 19:16,7 */
    return decode_simd_start(opsize) +
        (((di->instr_word & 0x000f0000) >> 15) | ((di->instr_word >> 7) & 0x1));
}

static reg_id_t
decode_wregB(decode_info_t *di, opnd_size_t opsize)
{
    /* A32/T32 = 15:12,22 */
    return decode_simd_start(opsize) +
        (((di->instr_word & 0x0000f000) >> 11) | ((di->instr_word >> 22) & 0x1));
}

static reg_id_t
decode_wregC(decode_info_t *di, opnd_size_t opsize)
{
    /* A32/T32 = 3:0,5 */
    return decode_simd_start(opsize) +
        (((di->instr_word & 0x0000000f) << 1) | ((di->instr_word >> 5) & 0x1));
}

static ptr_int_t
decode_immed(decode_info_t *di, uint start_bit, opnd_size_t opsize, bool is_signed)
{
    ptr_int_t val;
    ptr_uint_t mask = ((1 << opnd_size_in_bits(opsize)) - 1);
    if (is_signed) {
        ptr_uint_t top_bit = (1 << (opnd_size_in_bits(opsize) - 1));
        val = (ptr_int_t)(int)((di->instr_word >> start_bit) & mask);
        if (TEST(top_bit, val))
            val |= (~mask);
    } else
        val = (ptr_int_t)(ptr_uint_t)((di->instr_word >> start_bit) & mask);
    return val;
}

/* This routine creates the decoded operand(s) itself */
static bool
decode_SIMD_modified_immed(decode_info_t *di, byte optype, opnd_t *array,
                           uint *counter INOUT)
{
    ptr_uint_t val; /* unsigned for logical shifts */
    /* This is a SIMD modified immedate: an 8-bit value with a 4-bit
     * "cmode" control which expands the value to 16 or 32 bits (from
     * there it is tiled into the target SIMD register).
     * We have the element size in the opcode.  We do not try to expand
     * to the SIMD size as that would require a 128-bit immed, and it
     * is a simple tiling.
     */
    uint cmode = decode_immed(di, 8, OPSZ_4b, false /*unsigned*/);
    /* XXX; we sometimes need the "op" bit too but I don't really want
     * to expand the immed name again and add OPSZ_13b just for this
     * expansion that we're special-casing anyway.
     */
    uint op = decode_immed(di, 5, OPSZ_1b, false /*unsigned*/);
    opnd_size_t sz = OPSZ_4;
    val = (ptr_uint_t)decode_immed(di, 0, OPSZ_4b, false /*unsigned*/);
    val |= (decode_immed(di, 16, OPSZ_3b, false /*unsigned*/) << 4);
    val |= (decode_immed(di, (optype == TYPE_I_b8_b28_b16_b0) ? 28 : 24, OPSZ_1b,
                         false /*unsigned*/)
            << 7);
    /* Val is "abcdefgh" for the following patterns: */
    if ((cmode & 0xe) == 0) {
        /* cmode = 000x => 00000000 00000000 00000000 abcdefgh */
        /* nothing to do */
    } else if ((cmode & 0xe) == 2) {
        /* cmode = 001x => 00000000 00000000 abcdefgh 00000000 */
        val = val << 8;
    } else if ((cmode & 0xe) == 4) {
        /* cmode = 010x => 00000000 abcdefgh 00000000 00000000 */
        val = val << 16;
    } else if ((cmode & 0xe) == 6) {
        /* cmode = 011x => abcdefgh 00000000 00000000 00000000 */
        val = val << 24;
    } else if ((cmode & 0xe) == 8) {
        /* cmode = 100x => 00000000 abcdefgh */
        sz = OPSZ_2;
    } else if ((cmode & 0xe) == 0xa) {
        /* cmode = 101x => abcdefgh 00000000 */
        val = val << 8;
        sz = OPSZ_2;
    } else if (cmode == 0xc) {
        /* cmode = 1100 => 00000000 00000000 abcdefgh 11111111 */
        val = (val << 8) | 0xff;
    } else if (cmode == 0xd) {
        /* cmode = 1101 => 00000000 abcdefgh 11111111 11111111 */
        val = (val << 16) | 0xffff;
    } else if (cmode == 0xe && op == 0) {
        /* cmode = 1110 => abcdefgh */
        sz = OPSZ_1;
    } else if (cmode == 0xf && op == 0) {
        /* cmode = 1111 => aBbbbbbc defgh000 00000000 00000000 */
        /* XXX: ARM assembly seems to not show this floating-point immed in its
         * expanded form, unlike the integer SIMD immediates: but it's a little
         * confusing what the assembler expects.
         */
        uint a = (val >> 7) & 0x1;
        uint b = (val >> 6) & 0x1;
        uint notb = ((~val) >> 6) & 0x1;
        val = (a << 31) | (notb << 30) | (b << 29) | (b << 28) | (b << 27) | (b << 26) |
            ((val << 19) & 0x03ff0000);
    } else if (cmode == 0xe && op == 1) {
        /* cmode = 1110 =>
         *   aaaaaaaa bbbbbbbb cccccccc dddddddd eeeeeeee ffffffff gggggggg hhhhhhhh
         */
        uint high = 0, low = 0;
        uint64 val64;
        high |= TEST(0x80, val) ? 0xff000000 : 0;
        high |= TEST(0x40, val) ? 0x00ff0000 : 0;
        high |= TEST(0x20, val) ? 0x0000ff00 : 0;
        high |= TEST(0x10, val) ? 0x000000ff : 0;
        low |= TEST(0x08, val) ? 0xff000000 : 0;
        low |= TEST(0x04, val) ? 0x00ff0000 : 0;
        low |= TEST(0x02, val) ? 0x0000ff00 : 0;
        low |= TEST(0x01, val) ? 0x000000ff : 0;
        val64 = ((uint64)high << 32) | low;
        array[(*counter)++] = opnd_create_immed_int64(val64, OPSZ_8);
        return true;
    } else {
        /* cmode = 1111, op = 1 => undefined */
        val = 0;
        return false;
    }
    array[(*counter)++] = opnd_create_immed_uint(val, sz);
    return true;
}

/* This routine creates the decoded operand(s) itself */
static bool
decode_VFP_modified_immed(decode_info_t *di, byte optype, opnd_t *array,
                          uint *counter INOUT)
{
    ptr_uint_t val; /* unsigned for logical shifts */
    /* This is a VFP modified immedate which is expanded.
     * Xref VFPIMDExpandImm in the manual.
     * XXX: ARM assembly seems to not show in its expanded form, unlike the
     * integer SIMD immediates: but it's a little confusing what the assembler
     * expects.
     */
    val = decode_immed(di, 0, OPSZ_4b, false /*unsigned*/);
    val |= (decode_immed(di, 16, OPSZ_4b, false /*unsigned*/) << 4);
    if (di->opcode == OP_vmov_f32) {
        /* aBbbbbbc defgh000 00000000 00000000 */
        uint a = (val >> 7) & 0x1;
        uint b = (val >> 6) & 0x1;
        uint notb = ((~val) >> 6) & 0x1;
        val = (a << 31) | (notb << 30) | (b << 29) | (b << 28) | (b << 27) | (b << 26) |
            ((val << 19) & 0x03ff0000);
        array[(*counter)++] = opnd_create_immed_uint(val, OPSZ_4);
    } else if (di->opcode == OP_vmov_f64) {
        /* aBbbbbbb bbcdefgh 00000000 00000000 00000000 00000000 00000000 00000000 */
        uint64 a = (val >> 7) & 0x1;
        uint64 b = (val >> 6) & 0x1;
        uint64 notb = ((~val) >> 6) & 0x1;
        uint64 val64 = (a << 63) | (notb << 62) | (b == 1 ? 0x3fc0000000000000 : 0) |
            (((uint64)val << 48) & 0x003f000000000000);
        array[(*counter)++] = opnd_create_immed_int64(val64, OPSZ_8);
    } else {
        CLIENT_ASSERT(false, "invalid opcode for VFPExpandImm");
        return false;
    }
    return true;
}

static bool
decode_float_reglist(decode_info_t *di, opnd_size_t downsz, opnd_size_t upsz,
                     opnd_t *array, uint *counter INOUT)
{
    uint i;
    uint count = (uint)decode_immed(di, 0, OPSZ_1, false /*unsigned*/);
    reg_id_t first_reg;
    /* Use a ceiling of 32 to match manual and avoid weird results from
     * opnd_size_from_bytes() returning OPSZ_NA.
     * XXX i#1685: or should we consider to be invalid?
     * Other decoders strangely are eager to mark invalid when PC as an
     * operand is officially "unpredictable", but while extra regs here
     * is also "unpredictable" they seem fine with it.
     */
    if (count > 32)
        count = 32;
    if (upsz == OPSZ_8) {
        /* If immed is odd, supposed to be (deprecated) OP_fldmx or OP_fstmx,
         * but they behave the same way so we treat them as just aliases.
         */
        count /= 2;
    } else
        CLIENT_ASSERT(upsz == OPSZ_4, "invalid opsz for TYPE_L_CONSEC");
    /* There must be an immediately prior simd reg */
    CLIENT_ASSERT(*counter > 0 && opnd_is_reg(array[*counter - 1]),
                  "invalid instr template");
    if (count > 0)
        count--; /* The prior was already added */
    first_reg = opnd_get_reg(array[*counter - 1]);
    array[(*counter) - 1] = opnd_add_flags(array[(*counter) - 1], DR_OPND_IN_LIST);
    di->reglist_sz = opnd_size_in_bytes(downsz);
    for (i = 0; i < count; i++) {
        LOG(THREAD_GET, LOG_INTERP, 5, "reglist: first=%s, new=%s\n",
            reg_names[first_reg], reg_names[first_reg + i]);
        if ((upsz == OPSZ_8 && first_reg + 1 + i > DR_REG_D31) ||
            (upsz == OPSZ_4 && first_reg + 1 + i > DR_REG_S31)) {
            /* Technically "unpredictable", but as we observe no SIGILL on our
             * processors, we just truncate and allow it according to our
             * general philosophy (i#1685).
             */
            break;
        }
        array[(*counter)++] =
            opnd_create_reg_ex(first_reg + 1 + i, downsz, DR_OPND_IN_LIST);
        di->reglist_sz += opnd_size_in_bytes(downsz);
    }
    if (di->mem_needs_reglist_sz != NULL)
        opnd_set_size(di->mem_needs_reglist_sz, opnd_size_from_bytes(di->reglist_sz));
    return true;
}

static dr_shift_type_t
decode_shift_values(ptr_int_t sh2, ptr_int_t val, uint *amount OUT)
{
    if (sh2 == SHIFT_ENCODING_LSL && val == 0) {
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
decode_index_shift(decode_info_t *di, ptr_int_t known_shift, uint *amount OUT)
{
    ptr_int_t sh2, val;
    if (di->isa_mode == DR_ISA_ARM_THUMB) {
        ASSERT(known_shift == SHIFT_ENCODING_LSL);
        /* index shift in T32 is a 2-bit immed at [5:4], which is different from
         * register shift (5-bit immed at [14:12] [7:6], and 2-bit type at [5:4])
         */
        val = decode_immed(di, DECODE_INDEX_SHIFT_AMOUNT_BITPOS_T32,
                           DECODE_INDEX_SHIFT_AMOUNT_SIZE_T32, false);
        sh2 = known_shift;
    } else {
        if (known_shift == SHIFT_ENCODING_DECODE) {
            sh2 = decode_immed(di, DECODE_INDEX_SHIFT_TYPE_BITPOS_A32,
                               DECODE_INDEX_SHIFT_TYPE_SIZE, false);
        } else
            sh2 = known_shift;
        /* index shift in A32 is a 5-bit immed at [11:7] */
        val = decode_immed(di, DECODE_INDEX_SHIFT_AMOUNT_BITPOS_A32,
                           DECODE_INDEX_SHIFT_AMOUNT_SIZE_A32, false);
    }
    return decode_shift_values(sh2, val, amount);
}

static void
decode_register_shift(decode_info_t *di, opnd_t *array, uint *counter IN)
{
    if (*counter > 2 && di->shift_type_idx == *counter - 2) {
        /* Mark the register as shifted for proper disassembly. */
        if (opnd_is_immed_int(array[*counter - 1])) {
            /* Move the two immediates to a higher abstraction layer.  Note that
             * b/c we map the lower 4 DR_SHIFT_* values to the encoded values,
             * we can handle either raw or higher-layer values at encode time.
             * We only need to do this for shifts whose amount is an immed.
             * When the amount is in a reg, only the low 4 DR_SHIFT_* are valid,
             * and they match the encoded values.
             */
            ptr_int_t sh2 = opnd_get_immed_int(array[*counter - 2]);
            ptr_int_t val = opnd_get_immed_int(array[*counter - 1]);
            uint amount;
            dr_shift_type_t type = decode_shift_values(sh2, val, &amount);
            array[*counter - 2] = opnd_create_immed_uint(type, OPSZ_2b);
            array[*counter - 1] = opnd_create_immed_uint(amount, OPSZ_5b);
        }
        array[*counter - 2] = opnd_add_flags(array[*counter - 2], DR_OPND_IS_SHIFT);
        CLIENT_ASSERT(*counter >= 3 && opnd_is_reg(array[*counter - 3]),
                      "invalid shift sequence");
        array[*counter - 3] = opnd_add_flags(array[*counter - 3], DR_OPND_SHIFTED);
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
decode_mem_reglist_size(decode_info_t *di, opnd_t *memop, opnd_size_t opsize,
                        bool adjust_disp)
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

static opnd_size_t
opnd_size_scale(opnd_size_t size, uint scale)
{
    /* only suppport OPSZ_* from 1-bit to 10-bit and only support x4 */
    ASSERT_NOT_IMPLEMENTED(scale == 4 && opnd_size_in_bits(size) >= 1 &&
                           opnd_size_in_bits(size) <= 10);
    switch (size) {
    case OPSZ_6b: return OPSZ_1;
    case OPSZ_7b: return OPSZ_9b;
    case OPSZ_1: return OPSZ_10b;
    default:
        /* assuming OPSZ_ includes every value from 1b to 12b (except 8b) in order */
        return (size + 2);
    }
    ASSERT_NOT_REACHED();
    return OPSZ_NA;
}

uint
gpr_list_num_bits(byte optype)
{
    switch (optype) {
    case TYPE_L_8b: return 8;
    case TYPE_L_9b_LR:
    case TYPE_L_9b_PC: return 9;
    case TYPE_L_16b:
    case TYPE_L_16b_NO_SP:
    case TYPE_L_16b_NO_SP_PC: return 16;
    default: ASSERT_NOT_REACHED();
    }
    return 0;
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
    case TYPE_NONE: array[(*counter)++] = opnd_create_null(); return true;

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
        if (di->shift_type_idx < UINT_MAX)
            decode_register_shift(di, array, counter);
        return true;
    case TYPE_R_D:
    case TYPE_R_D_TOP: /* we aren't storing whether top in our IR */
        array[(*counter)++] = opnd_create_reg_ex(decode_regD(di), downsz, 0);
        return true;
    case TYPE_R_D_NEGATED:
        array[(*counter)++] =
            opnd_create_reg_ex(decode_regD(di), downsz, DR_OPND_NEGATED);
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
        if (*counter <= 0 || !opnd_is_reg(array[(*counter) - 1]))
            return false;
        reg = opnd_get_reg(array[(*counter) - 1]);
        if (reg == DR_REG_STOP_32)
            return false;
        array[(*counter)++] = opnd_create_reg_ex(reg + 1, downsz, 0);
        return true;
    }
    case TYPE_R_A_EQ_D:
        if (decode_regA(di) != decode_regD(di))
            return false;
        /* This one is not its own opnd: just encoded 2x into different slots */
        return true;
    case TYPE_R_U:
        array[(*counter)++] = opnd_create_reg_ex(decode_regU(di), downsz, 0);
        return true;
    case TYPE_R_V:
    case TYPE_R_V_DUP:
        array[(*counter)++] = opnd_create_reg_ex(decode_regV(di), downsz, 0);
        return true;
    case TYPE_R_W:
    case TYPE_R_W_DUP:
        array[(*counter)++] = opnd_create_reg_ex(decode_regW(di), downsz, 0);
        return true;
    case TYPE_R_X:
        array[(*counter)++] = opnd_create_reg_ex(decode_regX(di), downsz, 0);
        return true;
    case TYPE_R_Y:
        array[(*counter)++] = opnd_create_reg_ex(decode_regY(di), downsz, 0);
        return true;
    case TYPE_R_Z:
    case TYPE_R_Z_DUP:
        array[(*counter)++] = opnd_create_reg_ex(decode_regZ(di), downsz, 0);
        return true;
    case TYPE_CR_A:
        array[(*counter)++] = opnd_create_reg_ex(
            decode_regA(di) - DR_REG_START_GPR + DR_REG_CR0, downsz, 0);
        return true;
    case TYPE_CR_B:
        array[(*counter)++] = opnd_create_reg_ex(
            decode_regB(di) - DR_REG_START_GPR + DR_REG_CR0, downsz, 0);
        return true;
    case TYPE_CR_C:
        array[(*counter)++] = opnd_create_reg_ex(
            decode_regC(di) - DR_REG_START_GPR + DR_REG_CR0, downsz, 0);
        return true;
    case TYPE_CR_D:
        array[(*counter)++] = opnd_create_reg_ex(
            decode_regD(di) - DR_REG_START_GPR + DR_REG_CR0, downsz, 0);
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
        if (*counter <= 0 || !opnd_is_reg(array[(*counter) - 1]))
            return false;
        reg = opnd_get_reg(array[(*counter) - 1]);
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
    case TYPE_PC:
        array[(*counter)++] = opnd_create_reg_ex(DR_REG_PC, downsz, 0);
        return true;

    /* Register lists */
    case TYPE_L_8b:
    case TYPE_L_9b_LR:
    case TYPE_L_9b_PC:
    case TYPE_L_16b_NO_SP:
    case TYPE_L_16b_NO_SP_PC:
    case TYPE_L_16b: {
        uint num = gpr_list_num_bits(optype);
        di->reglist_sz = 0;
        /* we must create regs in reglist in order for possible split in mangling */
        for (i = 0; i < num; i++) {
            if ((di->instr_word & (1 << i)) != 0) {
                if ((optype == TYPE_L_16b_NO_SP || optype == TYPE_L_16b_NO_SP_PC) &&
                    DR_REG_START_GPR + i == DR_REG_SP)
                    return false;
                if (optype == TYPE_L_16b_NO_SP_PC && DR_REG_START_GPR + i == DR_REG_PC)
                    return false;
                if (i == 8 /* 9th bit*/ &&
                    (optype == TYPE_L_9b_LR || optype == TYPE_L_9b_PC)) {
                    array[(*counter)++] =
                        opnd_create_reg_ex(optype == TYPE_L_9b_LR ? DR_REG_LR : DR_REG_PC,
                                           downsz, DR_OPND_IN_LIST);
                } else {
                    array[(*counter)++] =
                        opnd_create_reg_ex(DR_REG_START_GPR + i, downsz, DR_OPND_IN_LIST);
                }
                di->reglist_sz += opnd_size_in_bytes(downsz);
            }
        }
        /* These var-size reg lists need to update a corresponding mem opnd */
        decode_update_mem_for_reglist(di);
        return true;
    }
    case TYPE_L_CONSEC: return decode_float_reglist(di, downsz, upsz, array, counter);
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
        array[(*counter)++] = opnd_create_reg_ex(start, downsz, DR_OPND_IN_LIST);
        if (reg_is_past_last_simd(start, inc))
            return false;
        array[(*counter)++] = opnd_create_reg_ex(start + inc, downsz, DR_OPND_IN_LIST);
        if (optype == TYPE_L_VBx2 || optype == TYPE_L_VBx2D)
            return true;
        if (reg_is_past_last_simd(start, 2 * inc))
            return false;
        array[(*counter)++] =
            opnd_create_reg_ex(start + 2 * inc, downsz, DR_OPND_IN_LIST);
        if (optype == TYPE_L_VBx3 || optype == TYPE_L_VBx3D)
            return true;
        if (reg_is_past_last_simd(start, 3 * inc))
            return false;
        array[(*counter)++] =
            opnd_create_reg_ex(start + 3 * inc, downsz, DR_OPND_IN_LIST);
        return true;
    }
    case TYPE_L_VAx2:
    case TYPE_L_VAx3:
    case TYPE_L_VAx4: {
        reg_id_t start = decode_vregA(di, upsz);
        uint inc = 1;
        array[(*counter)++] = opnd_create_reg_ex(start, downsz, DR_OPND_IN_LIST);
        if (reg_is_past_last_simd(start, inc))
            return false;
        array[(*counter)++] = opnd_create_reg_ex(start + inc, downsz, DR_OPND_IN_LIST);
        if (optype == TYPE_L_VAx2)
            return true;
        if (reg_is_past_last_simd(start, 2 * inc))
            return false;
        array[(*counter)++] =
            opnd_create_reg_ex(start + 2 * inc, downsz, DR_OPND_IN_LIST);
        if (optype == TYPE_L_VAx3)
            return true;
        if (reg_is_past_last_simd(start, 3 * inc))
            return false;
        array[(*counter)++] =
            opnd_create_reg_ex(start + 3 * inc, downsz, DR_OPND_IN_LIST);
        return true;
    }

    /* Immeds */
    case TYPE_I_b0:
        array[(*counter)++] =
            opnd_create_immed_uint(decode_immed(di, 0, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_x4_b0:
        array[(*counter)++] =
            opnd_create_immed_uint(decode_immed(di, 0, opsize, false /*unsign*/) * 4,
                                   opnd_size_scale(opsize, 4));
        return true;
    case TYPE_I_SHIFTED_b0: {
        /* This is an A32 "modified immediate constant" (ARMExpandImm in the manual).
         * Top 4 bits x2 specify how much to right-rotate the bottom 8 bits.
         */
        int rot = 2 * decode_immed(di, 8, OPSZ_4b, false /*unsign*/);
        val = decode_immed(di, 0, OPSZ_1, false /*unsign*/);
        val = (val >> rot) | (val << (32 - rot));
        array[(*counter)++] = opnd_create_immed_uint(val, OPSZ_4 /*to fit rotations*/);
        return true;
    }
    case TYPE_NI_b0:
        array[(*counter)++] =
            opnd_create_immed_int(-decode_immed(di, 0, opsize, false /*unsign*/),
                                  OPSZ_4 /*could do opsize + 1 bit, but this is easier*/);
        return true;
    case TYPE_NI_x4_b0:
        array[(*counter)++] =
            opnd_create_immed_int(-decode_immed(di, 0, opsize, false /*unsign*/) * 4,
                                  opnd_size_scale(opsize, 4));
        return true;
    case TYPE_I_b3:
        array[(*counter)++] =
            opnd_create_immed_uint(decode_immed(di, 3, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b4:
        array[(*counter)++] =
            opnd_create_immed_uint(decode_immed(di, 4, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b5:
        array[(*counter)++] =
            opnd_create_immed_uint(decode_immed(di, 5, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b6:
        array[(*counter)++] =
            opnd_create_immed_uint(decode_immed(di, 6, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b7:
        array[(*counter)++] =
            opnd_create_immed_uint(decode_immed(di, 7, opsize, false /*unsign*/), opsize);
        if (opsize == OPSZ_5b) {
            decode_register_shift(di, array, counter);
        }
        return true;
    case TYPE_I_b8:
        array[(*counter)++] =
            opnd_create_immed_uint(decode_immed(di, 8, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b9:
        array[(*counter)++] =
            opnd_create_immed_uint(decode_immed(di, 9, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b10:
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 10, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b16:
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 16, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b17:
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 17, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b18:
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 18, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b19:
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 19, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b20:
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 20, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b21:
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 21, opsize, false /*unsign*/), opsize);
        return true;
    case TYPE_I_b0_b5: {
        if (opsize == OPSZ_5b) {
            val = decode_immed(di, 5, OPSZ_1b, false /*unsigned*/);
            val |= (decode_immed(di, 0, OPSZ_4b, false /*unsigned*/) << 1);
        } else
            CLIENT_ASSERT(false, "unsupported 0-5 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_I_b4_b8: { /* OP_msr_priv */
        if (opsize == OPSZ_5b) {
            val = decode_immed(di, 8, OPSZ_4b, false /*unsigned*/);
            val |= (decode_immed(di, 4, OPSZ_1b, false /*unsigned*/) << 4);
        } else
            CLIENT_ASSERT(false, "unsupported 4-8 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_I_b4_b16: { /* OP_mrs_priv */
        if (opsize == OPSZ_5b) {
            val = decode_immed(di, 16, OPSZ_4b, false /*unsigned*/);
            val |= (decode_immed(di, 4, OPSZ_1b, false /*unsigned*/) << 4);
        } else
            CLIENT_ASSERT(false, "unsupported 4-16 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_I_b5_b3: { /* OP_vmla scalar: M:Vm<3> */
        if (opsize == OPSZ_2b) {
            val = decode_immed(di, 3, OPSZ_1b, false /*unsigned*/);
            val |= (decode_immed(di, 5, OPSZ_1b, false /*unsigned*/) << 1);
        } else
            CLIENT_ASSERT(false, "unsupported 5-3 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_NI_b8_b0:
    case TYPE_I_b8_b0: {
        if (opsize == OPSZ_2) {
            val = decode_immed(di, 0, OPSZ_4b, false /*unsigned*/);
            val |= (decode_immed(di, 8, OPSZ_12b, false /*unsigned*/) << 4);
        } else if (opsize == OPSZ_1) {
            val = decode_immed(di, 0, OPSZ_4b, false /*unsigned*/);
            val |= (decode_immed(di, 8, OPSZ_4b, false /*unsigned*/) << 4);
        } else
            CLIENT_ASSERT(false, "unsupported 8-0 split immed size");
        if (optype == TYPE_NI_b8_b0) {
            /* We need an extra bit for the sign: easiest to just do OPSZ_4 */
            array[(*counter)++] = opnd_create_immed_int(-val, OPSZ_4);
        } else
            array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_I_b8_b16: { /* OP_msr */
        if (opsize == OPSZ_5b) {
            val = decode_immed(di, 16, OPSZ_4b, false /*unsigned*/);
            val |= (decode_immed(di, 8, OPSZ_1b, false /*unsigned*/) << 4);
        } else
            CLIENT_ASSERT(false, "unsupported 8-16 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_I_b8_b24_b16_b0:
    case TYPE_I_b8_b28_b16_b0: { /* OP_vbic, OP_vmov: 11:8,{24,28},18:16,3:0 */
        if (opsize == OPSZ_12b) {
            return decode_SIMD_modified_immed(di, optype, array, counter);
        } else
            CLIENT_ASSERT(false, "unsupported 8-24/28-16-0 split immed size");
        return true;
    }
    case TYPE_I_b12_b6: { /* T32.32: 14:12,7:6 */
        if (opsize == OPSZ_5b) {
            val = decode_immed(di, 6, OPSZ_2b, false /*unsigned*/);
            val |= (decode_immed(di, 12, OPSZ_3b, false /*unsigned*/) << 2);
        } else
            CLIENT_ASSERT(false, "unsupported 12-6 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        if (opsize == OPSZ_5b)
            decode_register_shift(di, array, counter);
        return true;
    }
    case TYPE_I_b16_b0: {
        if (opsize == OPSZ_2) {
            val = decode_immed(di, 0, OPSZ_12b, false /*unsigned*/);
            val |= (decode_immed(di, 16, OPSZ_4b, false /*unsigned*/) << 12);
        } else if (opsize == OPSZ_1) {
            return decode_VFP_modified_immed(di, optype, array, counter);
        } else
            CLIENT_ASSERT(false, "unsupported 16-0 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_I_b16_b26_b12_b0: { /* OP_movw T32-19:16,26,14:12,7:0 */
        if (opsize == OPSZ_2) {
            val = decode_immed(di, 0, OPSZ_1, false /*unsigned*/);
            val |= (decode_immed(di, 12, OPSZ_3b, false /*unsigned*/) << 8);
            val |= (decode_immed(di, 26, OPSZ_1b, false /*unsigned*/) << 11);
            val |= (decode_immed(di, 16, OPSZ_4b, false /*unsigned*/) << 12);
        } else
            CLIENT_ASSERT(false, "unsupported 16-26-12-0 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_I_b21_b5: { /* OP_vmov: 21,6:5 */
        if (opsize == OPSZ_3b) {
            val = decode_immed(di, 5, OPSZ_2b, false /*unsigned*/);
            val |= (decode_immed(di, 21, OPSZ_1b, false /*unsigned*/) << 2);
        } else
            CLIENT_ASSERT(false, "unsupported 21-5 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_I_b21_b6: { /* OP_vmov: 21,6 */
        if (opsize == OPSZ_2b) {
            val = decode_immed(di, 6, OPSZ_1b, false /*unsigned*/);
            val |= (decode_immed(di, 21, OPSZ_1b, false /*unsigned*/) << 1);
        } else
            CLIENT_ASSERT(false, "unsupported 21-6 split immed size");
        array[(*counter)++] = opnd_create_immed_uint(val, opsize);
        return true;
    }
    case TYPE_I_b26_b12_b0:
    case TYPE_I_b26_b12_b0_z: { /* T32-26,14:12,7:0 */
        if (opsize == OPSZ_12b) {
            val = decode_immed(di, 0, OPSZ_1, false /*unsigned*/);
            val |= (decode_immed(di, 12, OPSZ_3b, false /*unsigned*/) << 8);
            val |= (decode_immed(di, 26, OPSZ_1b, false /*unsigned*/) << 11);
        } else
            CLIENT_ASSERT(false, "unsupported 26-12-0 split immed size");
        if (optype == TYPE_I_b26_b12_b0) {
            /* This is a T32 "modified immediate constant" with complex rules
             * (ThumbExpandImm in the manual).
             * Bottom 8 bits are "abcdefgh" and the other bits indicate
             * whether to tile or rotate the bottom bits.
             */
            if (!TESTANY(0xc00, val)) {
                int code = (val >> 8) & 0x3;
                int val8 = (val & 0xff);
                if (code == 0) /* 00000000 00000000 00000000 abcdefgh */
                    val = val8;
                else if (code == 1) /* 00000000 abcdefgh 00000000 abcdefgh */
                    val = (val8 << 16) | val8;
                else if (code == 2) /* abcdefgh 00000000 abcdefgh 00000000 */
                    val = (val8 << 24) | (val8 << 8);
                else if (code == 3) /* abcdefgh abcdefgh abcdefgh abcdefgh */
                    val = (val8 << 24) | (val8 << 16) | (val8 << 8) | val8;
            } else {
                /* ROR of 1bcdefgh */
                int toror = 0x80 | (val & 0x7f);
                int amt = (val >> 7) & 0x1f;
                val = (toror >> amt) | (toror << (32 - amt));
            }
        }
        array[(*counter)++] = opnd_create_immed_uint(val, OPSZ_4 /*to fit tiling*/);
        return true;
    }
    case TYPE_J_b0: /* T32.16 OP_b, imm11 = 10:0, imm32 = SignExtend(imm11:'0', 32) */
        array[(*counter)++] =
            /* For A32, "cur pc" is PC + 8; for T32, PC + 4, sometimes aligned. */
            opnd_create_pc(decode_cur_pc(di->orig_pc, di->isa_mode, di->opcode, NULL) +
                           (decode_immed(di, 0, opsize, true /*signed*/) << 1));
        return true;
    case TYPE_J_x4_b0: /* OP_b, OP_bl */
        array[(*counter)++] =
            /* For A32, "cur pc" is PC + 8; for T32, PC + 4, sometimes aligned. */
            opnd_create_pc(decode_cur_pc(di->orig_pc, di->isa_mode, di->opcode, NULL) +
                           (decode_immed(di, 0, opsize, true /*signed*/) << 2));
        return true;
    case TYPE_J_b0_b24: { /* OP_blx imm24:H:0 */
        if (opsize == OPSZ_25b) {
            val = decode_immed(di, 24, OPSZ_1b, false /*unsigned*/) << 1; /*x2*/
            val |= (decode_immed(di, 0, OPSZ_3, true /*signed*/) << 2);
        } else
            CLIENT_ASSERT(false, "unsupported 0-24 split immed size");
        /* For A32, "cur pc" is PC + 8; for T32, PC + 4, sometimes aligned. */
        array[(*counter)++] = opnd_create_pc(
            decode_cur_pc(di->orig_pc, di->isa_mode, di->opcode, NULL) + val);
        return true;
    }
    case TYPE_J_b26_b11_b13_b16_b0: { /* OP_b T32-26,11,13,21:16,10:0 x2 */
        if (opsize == OPSZ_20b) {
            val = decode_immed(di, 0, OPSZ_11b, false /*unsigned*/) << 1; /*x2*/
            val |= (decode_immed(di, 16, OPSZ_6b, false /*unsigned*/) << 12);
            val |= (decode_immed(di, 13, OPSZ_1b, false /*unsigned*/) << 18);
            val |= (decode_immed(di, 11, OPSZ_1b, false /*unsigned*/) << 19);
            val |= (decode_immed(di, 26, OPSZ_1b, true /*signed*/) << 20);
        } else
            CLIENT_ASSERT(false, "unsupported 26-11-13-16-0 split immed size");
        /* For A32, "cur pc" is PC + 8; for T32, PC + 4, sometimes aligned. */
        array[(*counter)++] = opnd_create_pc(
            decode_cur_pc(di->orig_pc, di->isa_mode, di->opcode, NULL) + val);
        return true;
    }
    case TYPE_J_b26_b13_b11_b16_b0: {
        /* OP_b T32-26,13,11,25:16,10:0 x2, but bits 13 and 11 are flipped if
         * bit 26 is 0
         */
        if (opsize == OPSZ_3) {
            uint bit26 = decode_immed(di, 26, OPSZ_1b, true /*signed*/);
            uint bit13 = decode_immed(di, 13, OPSZ_1b, false /*unsigned*/);
            uint bit11 = decode_immed(di, 11, OPSZ_1b, false /*unsigned*/);
            val = decode_immed(di, 0, OPSZ_11b, false /*unsigned*/) << 1 /*x2*/;
            val |= (decode_immed(di, 16, OPSZ_10b, false /*unsigned*/) << 12);
            val |= ((bit26 == 0 ? (bit11 == 0 ? 1 : 0) : bit11) << 22);
            val |= ((bit26 == 0 ? (bit13 == 0 ? 1 : 0) : bit13) << 23);
            val |= bit26 << 24;
        } else
            CLIENT_ASSERT(false, "unsupported 26-13-11-16-0 split immed size");
        /* For A32, "cur pc" is PC + 8; for T32, PC + 4, sometimes aligned. */
        array[(*counter)++] = opnd_create_pc(
            decode_cur_pc(di->orig_pc, di->isa_mode, di->opcode, NULL) + val);
        return true;
    }
    case TYPE_J_b9_b3: { /* T32.16 OP_cb{n}z, ZeroExtend(i:imm5:0), i.e., [9,7:3]:0 */
        uint bit9 = decode_immed(di, 9, OPSZ_1b, false /*unsigned*/);
        val = decode_immed(di, 3, OPSZ_5b, false /*unsigned*/);
        val |= (bit9 << 5);
        val = val << 1 /* x2 */;
        /* For A32, "cur pc" is PC + 8; for T32, PC + 4, sometimes aligned. */
        array[(*counter)++] = opnd_create_pc(
            decode_cur_pc(di->orig_pc, di->isa_mode, di->opcode, NULL) + val);
        return true;
    }
    case TYPE_SHIFT_b4:
        di->shift_type_idx = *counter;
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 4, opsize, false /*unsigned*/), opsize);
        return true;
    case TYPE_SHIFT_b5:
        di->shift_type_idx = *counter;
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 5, opsize, false /*unsigned*/), opsize);
        return true;
    case TYPE_SHIFT_b6: /* value is :0 */
        di->shift_type_idx = *counter;
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 5, opsize, false /*unsigned*/) << 1, OPSZ_2b);
        return true;
    case TYPE_SHIFT_b21: /* value is :0 */
        di->shift_type_idx = *counter;
        array[(*counter)++] = opnd_create_immed_uint(
            decode_immed(di, 21, opsize, false /*unsigned*/) << 1, OPSZ_2b);
        return true;
    case TYPE_SHIFT_LSL:
        array[(*counter)++] = opnd_create_immed_uint(SHIFT_ENCODING_LSL, opsize);
        return true;
    case TYPE_SHIFT_ASR:
        array[(*counter)++] = opnd_create_immed_uint(SHIFT_ENCODING_ASR, opsize);
        return true;
    case TYPE_K:
        array[(*counter)++] = opnd_create_immed_uint(opsize, OPSZ_0);
        return true;

    /* Memory */
    /* Only some types are ever used with register lists. */
    /* We do not turn base-disp operands with PC bases into opnd_is_rel_addr opnds. */
    case TYPE_M:
        opsize = decode_mem_reglist_size(di, &array[*counter], opsize, false /*just sz*/);
        array[(*counter)++] = opnd_create_base_disp(
            di->T32_16 ? decode_regW(di) : decode_regA(di), REG_NULL, 0, 0, opsize);
        return true;
    case TYPE_M_SP:
        CLIENT_ASSERT(di->T32_16,
                      "32-bit instrs should use general types, not TYPE_M_SP");
        opsize = decode_mem_reglist_size(di, &array[*counter], opsize, false /*just sz*/);
        array[(*counter)++] = opnd_create_base_disp(DR_REG_SP, REG_NULL, 0, 0, opsize);
        return true;
    case TYPE_M_POS_I12:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] = opnd_create_base_disp(
            decode_regA(di), REG_NULL, 0,
            decode_immed(di, 0, OPSZ_12b, false /*unsigned*/), opsize);
        return true;
    case TYPE_M_NEG_I12:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] = opnd_create_base_disp(
            decode_regA(di), REG_NULL, 0,
            -decode_immed(di, 0, OPSZ_12b, false /*unsigned*/), opsize);
        return true;
    case TYPE_M_POS_REG:
    case TYPE_M_NEG_REG:
        array[(*counter)++] = opnd_create_base_disp_arm(
            di->T32_16 ? decode_regY(di) : decode_regA(di),
            di->T32_16 ? decode_regX(di) : decode_regD(di), DR_SHIFT_NONE, 0, 0,
            optype == TYPE_M_NEG_REG ? DR_OPND_NEGATED : 0, opsize);
        return true;
    case TYPE_M_POS_SHREG:
    case TYPE_M_NEG_SHREG: {
        uint amount;
        dr_shift_type_t shift = decode_index_shift(di, SHIFT_ENCODING_DECODE, &amount);
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] = opnd_create_base_disp_arm(
            decode_regA(di), decode_regD(di), shift, amount, 0,
            optype == TYPE_M_NEG_SHREG ? DR_OPND_NEGATED : 0, opsize);
        return true;
    }
    case TYPE_M_POS_LSHREG: {
        uint amount;
        dr_shift_type_t shift = decode_index_shift(di, SHIFT_ENCODING_LSL, &amount);
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] = opnd_create_base_disp_arm(decode_regA(di), decode_regD(di),
                                                        shift, amount, 0, 0, opsize);
        return true;
    }
    case TYPE_M_POS_LSH1REG:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] = opnd_create_base_disp_arm(decode_regA(di), decode_regD(di),
                                                        DR_SHIFT_LSL, 1, 0, 0, opsize);
        return true;
    case TYPE_M_SI9:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0,
                                  /* 9-bit signed immed @ 20:12 */
                                  decode_immed(di, 12, OPSZ_9b, true /*signed*/), opsize);
        return true;
    case TYPE_M_SI7:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0,
                                  decode_immed(di, 0, OPSZ_7b, true /*signed*/), opsize);
        return true;
    case TYPE_M_POS_I8:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] = opnd_create_base_disp(
            decode_regA(di), REG_NULL, 0, decode_immed(di, 0, OPSZ_1, false /*unsigned*/),
            opsize);
        return true;
    case TYPE_M_NEG_I8:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] = opnd_create_base_disp(
            decode_regA(di), REG_NULL, 0,
            -decode_immed(di, 0, OPSZ_1, false /*unsigned*/), opsize);
        return true;
    case TYPE_M_POS_I8x4:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] = opnd_create_base_disp(
            decode_regA(di), REG_NULL, 0,
            4 * decode_immed(di, 0, OPSZ_1, false /*unsigned*/), opsize);
        return true;
    case TYPE_M_NEG_I8x4:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] = opnd_create_base_disp(
            decode_regA(di), REG_NULL, 0,
            -4 * decode_immed(di, 0, OPSZ_1, false /*unsigned*/), opsize);
        return true;
    case TYPE_M_SP_POS_I8x4:
        CLIENT_ASSERT(di->T32_16,
                      "32-bit instrs should use general types, not TYPE_M_SP_POS_I8");
        array[(*counter)++] = opnd_create_base_disp(
            DR_REG_SP, REG_NULL, 0, 4 * decode_immed(di, 0, OPSZ_1, false /*unsigned*/),
            opsize);
        return true;
    case TYPE_M_POS_I4_4: {
        ptr_int_t val = (decode_immed(di, 8, OPSZ_4b, false /*unsigned*/) << 4) |
            decode_immed(di, 0, OPSZ_4b, false /*unsigned*/);
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0, val, opsize);
        return true;
    }
    case TYPE_M_NEG_I4_4: {
        ptr_int_t val = (decode_immed(di, 8, OPSZ_4b, false /*unsigned*/) << 4) |
            decode_immed(di, 0, OPSZ_4b, false /*unsigned*/);
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0, -val, opsize);
        return true;
    }
    case TYPE_M_POS_I5:
        CLIENT_ASSERT(di->T32_16, "supported in T32.16 only");
        array[(*counter)++] = opnd_create_base_disp(
            decode_regY(di), REG_NULL, 0,
            decode_immed(di, 6, OPSZ_5b, false /*unsigned*/), opsize);
        return true;
    case TYPE_M_POS_I5x2:
        CLIENT_ASSERT(di->T32_16, "supported in T32.16 only");
        array[(*counter)++] = opnd_create_base_disp(
            decode_regY(di), REG_NULL, 0,
            2 * decode_immed(di, 6, OPSZ_5b, false /*unsigned*/), opsize);
        return true;
    case TYPE_M_POS_I5x4:
        CLIENT_ASSERT(di->T32_16, "supported in T32.16 only");
        array[(*counter)++] = opnd_create_base_disp(
            decode_regY(di), REG_NULL, 0,
            4 * decode_immed(di, 6, OPSZ_5b, false /*unsigned*/), opsize);
        return true;
    case TYPE_M_UP_OFFS:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        opsize = decode_mem_reglist_size(di, &array[*counter], opsize, false /*just sz*/);
        array[(*counter)++] =
            opnd_create_base_disp(decode_regA(di), REG_NULL, 0, sizeof(void *), opsize);
        return true;
    case TYPE_M_DOWN:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        opsize = decode_mem_reglist_size(di, &array[*counter], opsize, true /*disp*/);
        array[(*counter)++] = opnd_create_base_disp(
            decode_regA(di), REG_NULL, 0,
            opsize == OPSZ_0 ? -sizeof(void *)
                             : -((int)opnd_size_in_bytes(opsize) - 1) * sizeof(void *),
            opsize);
        return true;
    case TYPE_M_DOWN_OFFS:
    case TYPE_M_SP_DOWN_OFFS:
        opsize = decode_mem_reglist_size(di, &array[*counter], opsize, true /*disp*/);
        array[(*counter)++] = opnd_create_base_disp(
            optype == TYPE_M_DOWN_OFFS ? decode_regA(di) : DR_REG_SP, REG_NULL, 0,
            -(int)opnd_size_in_bytes(opsize) * sizeof(void *), opsize);
        return true;
    case TYPE_M_PCREL_POS_I8x4: {
        ptr_int_t disp = decode_immed(di, 0, OPSZ_1, false /*unsigned*/) << 2;
        CLIENT_ASSERT(di->T32_16, "supported in T32.16 only");
        array[(*counter)++] = opnd_create_base_disp(DR_REG_PC, REG_NULL, 0, disp, opsize);
        return true;
    }
    case TYPE_M_PCREL_POS_I12:
    case TYPE_M_PCREL_NEG_I12: {
        ptr_int_t disp = decode_immed(di, 0, OPSZ_12b, false /*unsigned*/);
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        if (optype == TYPE_M_PCREL_NEG_I12)
            disp = -disp;
        array[(*counter)++] = opnd_create_base_disp(DR_REG_PC, REG_NULL, 0, disp, opsize);
        return true;
    }

    default:
        array[(*counter)++] = opnd_create_null();
        /* ok to assert, types coming only from instr_info_t */
        SYSLOG_INTERNAL_ERROR("unknown operand type %s\n", type_names[optype]);
        CLIENT_ASSERT(false, "decode error: unknown operand type");
    }
    return false;
}

/* Indexing shared between A32 and T32 SIMD decoding */

static inline uint
decode_ext_fp_idx(uint instr_word)
{
    uint idx = ((instr_word >> 8) & 0xf) /*bits 11:8*/;
    return (idx == 0xa ? 0 : (idx == 0xb ? 1 : 2));
}

static inline uint
decode_ext_fpa_idx(uint instr_word)
{
    return (((instr_word >> 5) & 0x2) | ((instr_word >> 4) & 0x1)) /*bits 6,4*/;
}

static inline uint
decode_ext_fpb_idx(uint instr_word)
{
    return ((instr_word >> 4) & 0x7) /*bits 6:4*/;
}

static inline uint
decode_ext_simd6_idx(uint instr_word)
{
    return ((instr_word >> 6) & 0x3c) | ((instr_word >> 5) & 0x2) |
        ((instr_word >> 4) & 0x1); /*6 bits 11:8,6,4 */
}

static inline uint
decode_ext_simd5_idx(uint instr_word)
{
    return ((instr_word >> 7) & 0x1e) | ((instr_word >> 6) & 0x1); /*5 bits 11:8,6 */
}

static inline uint
decode_ext_simd5b_idx(uint instr_word)
{
    return ((instr_word >> 14) & 0x1c) | ((instr_word >> 7) & 0x3); /*bits 18:16,8:7 */
}

static inline uint
decode_ext_simd8_idx(uint instr_word)
{
    /* Odds<8 + 0 == 5 entries each */
    uint idx = 5 * ((instr_word >> 8) & 0xf) /*bits 11:8*/;
    if (((instr_word >> 4) & 0x1) != 0)
        idx += 1 + ((instr_word >> 5) & 0x3) /*bits 6:5*/;
    return idx;
}

static inline uint
decode_ext_simd6b_idx(uint instr_word)
{
    /* bits 10:8,7:6 + extra set of 7:6 for bit 11 being set */
    if (((instr_word >> 11) & 0x1) != 0)
        return 32 + ((instr_word >> 6) & 0x3);
    else
        return ((instr_word >> 6) & 0x1c) | ((instr_word >> 6) & 0x3);
}

static inline uint
decode_ext_simd2_idx(uint instr_word)
{
    return ((instr_word >> 10) & 0x2) | ((instr_word >> 6) & 0x1); /*11,6 */
}

static inline uint
decode_ext_imm6l_idx(uint instr_word)
{
    return ((instr_word >> 7) & 0xe) | ((instr_word >> 6) & 0x1); /* 10:8,6 */
}

static inline uint
decode_ext_vlda_idx(uint instr_word)
{
    int reg = (instr_word & 0xf);
    uint idx = /*bits (11:8,7:6)*3+X where X based on value of 3:0 */
        3 * (((instr_word >> 6) & 0x3c) | ((instr_word >> 6) & 0x3));
    idx += (reg == 0xd ? 0 : (reg == 0xf ? 1 : 2));
    return idx;
}

static inline uint
decode_ext_vldb_idx(uint instr_word)
{
    int reg = (instr_word & 0xf);
    uint idx = /*bits (11:8,Y)*3+X where X based on value of 3:0 */
        ((instr_word >> 7) & 0x1e);
    /* Y is bit 6 if bit 11 is set; else, bit 5 */
    if (((instr_word >> 11) & 0x1) != 0)
        idx |= ((instr_word >> 6) & 0x1);
    else
        idx |= ((instr_word >> 5) & 0x1);
    idx *= 3;
    idx += (reg == 0xd ? 0 : (reg == 0xf ? 1 : 2));
    return idx;
}

static inline uint
decode_ext_vldc_idx(uint instr_word)
{
    int reg = (instr_word & 0xf);
    uint idx = /*bits (7:5)*3+X where X based on value of 3:0 */
        3 * ((instr_word >> 5) & 0x7);
    idx += (reg == 0xd ? 0 : (reg == 0xf ? 1 : 2));
    return idx;
}

static inline uint
decode_ext_vldd_idx(uint instr_word)
{
    int reg = (instr_word & 0xf);
    uint idx = /*bits (7:4)*3+X where X based on value of 3:0 */
        3 * ((instr_word >> 4) & 0xf);
    idx += (reg == 0xd ? 0 : (reg == 0xf ? 1 : 2));
    return idx;
}

static inline uint
decode_ext_vtb_idx(uint instr_word)
{
    uint idx = ((instr_word >> 10) & 0x3) /*bits 11:10 */;
    if (idx != 2)
        idx = 0;
    else {
        idx = 1 + /*3 bits 9:8,6 */
            (((instr_word >> 7) & 0x6) | ((instr_word >> 6) & 0x1));
    }
    return idx;
}

static inline uint
decode_T32_16_ext_bits_10_8_idx(uint instr_word)
{
    uint idx;
    /* check whether Rn is also listed in reglist */
    if (TEST((1 << ((instr_word >> 8) & 0x7) /*Rn*/), (instr_word & 0xff) /*reglist*/))
        idx = 0;
    else
        idx = 1;
    return idx;
}

static inline uint
decode_it_block_num_instrs(byte mask)
{
    if ((mask & 0xf) == 0x8)
        return 1;
    if ((mask & 0x7) == 0x4)
        return 2;
    if ((mask & 0x3) == 0x2)
        return 3;
    ASSERT((mask & 0x1) == 0x1);
    return 4;
}

void
it_block_info_init_immeds(it_block_info_t *info, byte mask, byte firstcond)
{
    byte i;
    info->firstcond = firstcond;
    info->num_instrs = decode_it_block_num_instrs(mask);
    info->cur_instr = 0;
    info->preds = 1; /* first instr use firstcond */
    /* mask[3..1] for predicate instr[1..3] */
    for (i = 1; i < info->num_instrs; i++) {
        if ((mask & BITMAP_MASK(4 - i)) >> (4 - i) == (info->firstcond & 0x1))
            info->preds |= BITMAP_MASK(i);
    }
}

void
it_block_info_init(it_block_info_t *info, decode_info_t *di)
{
    it_block_info_init_immeds(info, (byte)decode_immed(di, 0, OPSZ_4b, false),
                              (byte)decode_immed(di, 4, OPSZ_4b, false));
}

uint
instr_it_block_get_count(instr_t *it_instr)
{
    if (instr_get_opcode(it_instr) != OP_it ||
        !opnd_is_immed_int(instr_get_src(it_instr, 1)))
        return 0;
    return decode_it_block_num_instrs(opnd_get_immed_int(instr_get_src(it_instr, 1)));
}

dr_pred_type_t
instr_it_block_get_pred(instr_t *it_instr, uint index)
{
    it_block_info_t info;
    if (instr_get_opcode(it_instr) != OP_it ||
        !opnd_is_immed_int(instr_get_src(it_instr, 0)) ||
        !opnd_is_immed_int(instr_get_src(it_instr, 1)))
        return DR_PRED_NONE;
    it_block_info_init_immeds(&info, opnd_get_immed_int(instr_get_src(it_instr, 1)),
                              opnd_get_immed_int(instr_get_src(it_instr, 0)));
    if (index >= info.num_instrs)
        return DR_PRED_NONE;
    return it_block_instr_predicate(info, index);
}

static byte
set_bit(byte mask, int pos, int val)
{
    if (val == 1)
        mask |= 1 << pos;
    else
        mask &= ~(1 << pos);
    return mask;
}

bool
instr_it_block_compute_immediates(dr_pred_type_t pred0, dr_pred_type_t pred1,
                                  dr_pred_type_t pred2, dr_pred_type_t pred3,
                                  byte *firstcond_out, byte *mask_out)
{
    byte mask = 0;
    byte firstcond = pred0 - DR_PRED_EQ;
    byte first_bit0 = firstcond & 0x1;
    byte first_not0 = (~first_bit0) & 0x1;
    dr_pred_type_t invert0 = instr_invert_predicate(pred0);
    uint num_instrs = IT_BLOCK_MAX_INSTRS;
    LOG(THREAD_GET, LOG_EMIT, 5, "%s: %s, %s, %s, %s; bit0=%d\n", __FUNCTION__,
        instr_predicate_name(pred0), instr_predicate_name(pred1),
        instr_predicate_name(pred2), instr_predicate_name(pred3), first_bit0);
    /* We could take in an array, but that's harder to use for the caller,
     * so we end up w/ an unrolled loop here:
     */
    if (pred1 == DR_PRED_NONE)
        num_instrs = 1;
    else {
        if (pred1 != pred0 && pred1 != invert0)
            return false;
        mask = set_bit(mask, 3, (pred1 == pred0) ? first_bit0 : first_not0);
        if (pred2 == DR_PRED_NONE)
            num_instrs = 2;
        else {
            if (pred2 != pred0 && pred2 != invert0)
                return false;
            mask = set_bit(mask, 2, (pred2 == pred0) ? first_bit0 : first_not0);
            if (pred3 == DR_PRED_NONE)
                num_instrs = 3;
            else {
                if (pred3 != pred0 && pred3 != invert0)
                    return false;
                mask = set_bit(mask, 1, (pred3 == pred0) ? first_bit0 : first_not0);
            }
        }
    }
    mask |= BITMAP_MASK(IT_BLOCK_MAX_INSTRS - num_instrs);
    if (mask_out != NULL)
        *mask_out = mask;
    if (firstcond_out != NULL)
        *firstcond_out = firstcond;
    return true;
}

instr_t *
instr_it_block_create(void *drcontext, dr_pred_type_t pred0, dr_pred_type_t pred1,
                      dr_pred_type_t pred2, dr_pred_type_t pred3)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    byte firstcond = 0, mask = 0;
    if (!instr_it_block_compute_immediates(pred0, pred1, pred2, pred3, &firstcond,
                                           &mask)) {
        CLIENT_ASSERT(false, "invalid predicates");
    }
    /* I did not want to include the massive instr_create_api.h here */
    return INSTR_CREATE_it(dcontext, OPND_CREATE_INT(firstcond), OPND_CREATE_INT(mask));
}

const instr_info_t *
decode_instr_info_T32_32(decode_info_t *di)
{
    const instr_info_t *info;
    uint idx;
    /* We use instr_word for cases where we're dealing w/ coproc/SIMD instrs,
     * whose decoding is very similar to A32.
     */
    uint instr_word = di->instr_word;
    /* First, split by whether coprocessor or not */
    if (TESTALL(0xec00, di->halfwordA)) {
        /* coproc */
        if (TEST(0x1000, di->halfwordA)) {
            idx = (((instr_word >> 20) & 0x3) |
                   ((instr_word >> 21) & 0x1c)); /*bits 25:23,21:20*/
            info = &T32_coproc_f[idx];
        } else {
            idx = ((instr_word >> 20) & 0x3f); /*bits 25:20*/
            info = &T32_coproc_e[idx];
        }
    } else {
        /* non-coproc */
        if (TESTALL(0xf000, di->halfwordA)) {
            /* bits A11,B15:14,B12 */
            idx = (((di->halfwordA >> 8) & 0x8) | ((di->halfwordB >> 13) & 0x6) |
                   ((di->halfwordB >> 12) & 0x1));
            info = &T32_base_f[idx];
        } else {
            idx = (di->halfwordA >> 4) & 0x3f /*bits A9:4*/;
            info = &T32_base_e[idx];
        }
    }
    /* If an extension, discard the old info and get a new one.
     * The SIMD instruction tables are very similar to the A32 tables.
     * It may be possible to share the A32 tables and apply programmatic
     * transformations to the opcodes (something like:
     *   s/0xf2/0xef/;s/0xf3/0xff/;s/0xe2/0xef/;s/0xe3/0xef/;s/0xf4/0xf9/
     * and for opcodes that use i8x28_16_0, s/0xf/0xe/).
     */
    while (info->type > INVALID) {
        if (info->type == EXT_FOPC8) {
            idx = (di->halfwordA >> 4) & 0xff /*bits A11:4*/;
            ASSERT(idx < 0xfc);
            info = &T32_ext_fopc8[info->code][idx];
        } else if (info->type == EXT_RAPC) {
            idx = (((di->instr_word >> 16) & 0xf) /*bits 19:16*/ != 0xf) ? 0 : 1;
            info = &T32_ext_RAPC[info->code][idx];
        } else if (info->type == EXT_RBPC) {
            idx = (((di->instr_word >> 12) & 0xf) /*bits 15:12*/ != 0xf) ? 0 : 1;
            info = &T32_ext_RBPC[info->code][idx];
        } else if (info->type == EXT_RCPC) {
            idx = (((di->instr_word >> 8) & 0xf) /*bits 11:8*/ != 0xf) ? 0 : 1;
            info = &T32_ext_RCPC[info->code][idx];
        } else if (info->type == EXT_A10_6_4) {
            idx = (((di->halfwordA >> 7) & 0x8) |
                   ((di->halfwordA >> 4) & 0x7)) /*bits A10,6:4*/;
            info = &T32_ext_bits_A10_6_4[info->code][idx];
        } else if (info->type == EXT_A9_7_eq1) {
            idx = (((di->halfwordA >> 7) & 0x7) /*bits A9:7*/ == 0x7) ? 0 : 1;
            info = &T32_ext_A9_7_eq1[info->code][idx];
        } else if (info->type == EXT_B10_8) {
            idx = ((di->halfwordB >> 8) & 0x7) /*bits B10:8*/;
            info = &T32_ext_bits_B10_8[info->code][idx];
        } else if (info->type == EXT_B2_0) {
            idx = (di->halfwordB & 0x7) /*bits B2:0*/;
            info = &T32_ext_bits_B2_0[info->code][idx];
        } else if (info->type == EXT_B5_4) {
            idx = ((di->halfwordB >> 4) & 0x3) /*bits B5:4*/;
            info = &T32_ext_bits_B5_4[info->code][idx];
        } else if (info->type == EXT_B6_4) {
            idx = ((di->halfwordB >> 4) & 0x7) /*bits B6:4*/;
            info = &T32_ext_bits_B6_4[info->code][idx];
        } else if (info->type == EXT_B7_4) {
            idx = ((di->halfwordB >> 4) & 0xf) /*bits B7:4*/;
            info = &T32_ext_bits_B7_4[info->code][idx];
        } else if (info->type == EXT_B7_4_eq1) {
            idx = (((di->halfwordB >> 4) & 0xf) /*bits B7:4*/ == 0xf) ? 0 : 1;
            info = &T32_ext_B7_4_eq1[info->code][idx];
        } else if (info->type == EXT_B4) {
            idx = ((di->halfwordB >> 4) & 0x1) /*bit B4*/;
            info = &T32_ext_bit_B4[info->code][idx];
        } else if (info->type == EXT_B5) {
            idx = ((di->halfwordB >> 5) & 0x1) /*bit B5*/;
            info = &T32_ext_bit_B5[info->code][idx];
        } else if (info->type == EXT_B7) {
            idx = ((di->halfwordB >> 7) & 0x1) /*bit B7*/;
            info = &T32_ext_bit_B7[info->code][idx];
        } else if (info->type == EXT_B11) {
            idx = ((di->halfwordB >> 8) & 0x1) /*bit B11*/;
            info = &T32_ext_bit_B11[info->code][idx];
        } else if (info->type == EXT_B13) {
            idx = ((di->halfwordB >> 8) & 0x1) /*bit B13*/;
            info = &T32_ext_bit_B13[info->code][idx];
        } else if (info->type == EXT_IMM126) {
            idx = (((di->halfwordB >> 10) & 0x1c) |
                   ((di->halfwordB >> 6) & 0x3)) /*bits B14:12,7:6*/;
            idx = (idx == 0) ? 0 : 1;
            info = &T32_ext_imm126[info->code][idx];
        } else if (info->type == EXT_OPCBX) {
            if (!TEST(0x800, di->halfwordB))
                idx = 0;
            else
                idx = 1 + ((di->halfwordB >> 8) & 0x7) /*bits 10:8*/;
            info = &T32_ext_opcBX[info->code][idx];
        } else if (info->type == EXT_OPC4) {
            idx = decode_opc4(instr_word);
            info = &T32_ext_opc4[info->code][idx];
        } else if (info->type == EXT_FP) {
            info = &T32_ext_fp[info->code][decode_ext_fp_idx(instr_word)];
        } else if (info->type == EXT_FPA) {
            uint idx = decode_ext_fpa_idx(instr_word);
            if (idx == 3)
                info = &invalid_instr;
            else
                info = &T32_ext_opc4fpA[info->code][idx];
        } else if (info->type == EXT_FPB) {
            info = &T32_ext_opc4fpB[info->code][decode_ext_fpb_idx(instr_word)];
        } else if (info->type == EXT_IMM1916) {
            int imm = ((instr_word >> 16) & 0xf); /*bits 19:16*/
            idx = (imm == 0) ? 0 : (imm == 1 ? 1 : 2);
            info = &T32_ext_imm1916[info->code][idx];
        } else if (info->type == EXT_BIT6) {
            idx = ((instr_word >> 6) & 0x1) /*bit 6 */;
            info = &T32_ext_bit6[info->code][idx];
        } else if (info->type == EXT_BIT19) {
            idx = ((instr_word >> 19) & 0x1) /*bit 19 */;
            info = &T32_ext_bit19[info->code][idx];
        } else if (info->type == EXT_BITS16) {
            idx = ((instr_word >> 16) & 0xf) /*bits 19:16*/;
            info = &T32_ext_bits16[info->code][idx];
        } else if (info->type == EXT_BITS20) {
            idx = ((instr_word >> 20) & 0xf) /*bits 23:20 */;
            info = &T32_ext_bits20[info->code][idx];
        } else if (info->type == EXT_IMM1816) {
            idx = (((instr_word >> 16) & 0x7) /*bits 18:16*/ == 0) ? 0 : 1;
            info = &T32_ext_imm1816[info->code][idx];
        } else if (info->type == EXT_IMM2016) {
            idx = (((instr_word >> 16) & 0x1f) /*bits 20:16*/ == 0) ? 0 : 1;
            info = &T32_ext_imm2016[info->code][idx];
        } else if (info->type == EXT_SIMD6) {
            info = &T32_ext_simd6[info->code][decode_ext_simd6_idx(instr_word)];
        } else if (info->type == EXT_SIMD5) {
            info = &T32_ext_simd5[info->code][decode_ext_simd5_idx(instr_word)];
        } else if (info->type == EXT_SIMD5B) {
            info = &T32_ext_simd5b[info->code][decode_ext_simd5b_idx(instr_word)];
        } else if (info->type == EXT_SIMD8) {
            info = &T32_ext_simd8[info->code][decode_ext_simd8_idx(instr_word)];
        } else if (info->type == EXT_SIMD6B) {
            info = &T32_ext_simd6b[info->code][decode_ext_simd6b_idx(instr_word)];
        } else if (info->type == EXT_SIMD2) {
            info = &T32_ext_simd2[info->code][decode_ext_simd2_idx(instr_word)];
        } else if (info->type == EXT_IMM6L) {
            info = &T32_ext_imm6L[info->code][decode_ext_imm6l_idx(instr_word)];
        } else if (info->type == EXT_VLDA) {
            /* this table stops at 0xa in top bits, to save space */
            if (((instr_word >> 8) & 0xf) > 0xa)
                info = &invalid_instr;
            else
                info = &T32_ext_vldA[info->code][decode_ext_vlda_idx(instr_word)];
        } else if (info->type == EXT_VLDB) {
            info = &T32_ext_vldB[info->code][decode_ext_vldb_idx(instr_word)];
        } else if (info->type == EXT_VLDC) {
            info = &T32_ext_vldC[info->code][decode_ext_vldc_idx(instr_word)];
        } else if (info->type == EXT_VLDD) {
            info = &T32_ext_vldD[info->code][decode_ext_vldd_idx(instr_word)];
        } else if (info->type == EXT_VTB) {
            info = &T32_ext_vtb[info->code][decode_ext_vtb_idx(instr_word)];
        } else {
            ASSERT_NOT_REACHED();
            info = NULL;
            break;
        }
    }

    return info;
}

const instr_info_t *
decode_instr_info_T32_it(decode_info_t *di)
{
    const instr_info_t *info;
    uint idx;
    idx = (di->instr_word >> 12) & 0xf; /* bits 15:12 */
    info = &T32_16_it_opc4[idx];
    while (info->type > INVALID) {
        /* XXX: we compare info->type in the order listed in table_t32_16_it.c,
         * we may want to optimize the order by puting more common instrs
         * or larger tables earler.
         */
        if (info->type == EXT_11) {
            idx = (di->instr_word >> 11) & 0x1; /* bit 11 */
            info = &T32_16_it_ext_bit_11[info->code][idx];
        } else if (info->type == EXT_11_10) {
            idx = (di->instr_word >> 10) & 0x3; /* bits 11:10 */
            info = &T32_16_it_ext_bits_11_10[info->code][idx];
        } else if (info->type == EXT_11_9) {
            idx = (di->instr_word >> 9) & 0x7; /* bits 11:9 */
            info = &T32_16_it_ext_bits_11_9[info->code][idx];
        } else if (info->type == EXT_11_8) {
            idx = (di->instr_word >> 8) & 0xf; /* bits 11:8 */
            info = &T32_16_it_ext_bits_11_8[info->code][idx];
        } else if (info->type == EXT_9_6) {
            idx = (di->instr_word >> 6) & 0xf; /* bits 9:6 */
            info = &T32_16_it_ext_bits_9_6[info->code][idx];
        } else if (info->type == EXT_7) {
            idx = (di->instr_word >> 7) & 0x1; /* bit 7 */
            info = &T32_16_it_ext_bit_7[info->code][idx];
        } else if (info->type == EXT_10_9) {
            idx = (di->instr_word >> 9) & 0x3; /* bits 10:9 */
            info = &T32_16_it_ext_bits_10_9[info->code][idx];
        } else if (info->type == EXT_10_8) {
            idx = decode_T32_16_ext_bits_10_8_idx(di->instr_word);
            info = &T32_16_it_ext_bits_10_8[info->code][idx];
        } else if (info->type == EXT_7_6) {
            idx = (di->instr_word >> 6) & 0x3; /* bits 7:6 */
            info = &T32_16_it_ext_bits_7_6[info->code][idx];
        } else if (info->type == EXT_6_4) {
            idx = (di->instr_word >> 4) & 0x7; /* bits 6:4 */
            info = &T32_16_it_ext_bits_6_4[info->code][idx];
        } else if (info->type == EXT_10_6) {
            idx = di->instr_word & 0x7c0; /* bits 10:6 */
            if (idx != 0)
                idx = 1;
            info = &T32_16_it_ext_imm_10_6[info->code][idx];
        } else {
            ASSERT_NOT_REACHED();
            info = NULL;
            break;
        }
    }
    return info;
}

const instr_info_t *
decode_instr_info_T32_16(decode_info_t *di)
{
    const instr_info_t *info;
    uint idx;
    idx = (di->instr_word >> 12) & 0xf; /* bits 15:12 */
    info = &T32_16_opc4[idx];
    while (info->type > INVALID) {
        /* XXX: we compare info->type in the order listed in table_t32_16.c,
         * we may want to optimize the order by puting more common instrs
         * or larger tables earler.
         */
        if (info->type == EXT_11) {
            idx = (di->instr_word >> 11) & 0x1; /* bit 11 */
            info = &T32_16_ext_bit_11[info->code][idx];
        } else if (info->type == EXT_11_10) {
            idx = (di->instr_word >> 10) & 0x3; /* bits 11:10 */
            info = &T32_16_ext_bits_11_10[info->code][idx];
        } else if (info->type == EXT_11_9) {
            idx = (di->instr_word >> 9) & 0x7; /* bits 11:9 */
            info = &T32_16_ext_bits_11_9[info->code][idx];
        } else if (info->type == EXT_11_8) {
            idx = (di->instr_word >> 8) & 0xf; /* bits 11:8 */
            info = &T32_16_ext_bits_11_8[info->code][idx];
        } else if (info->type == EXT_9_6) {
            idx = (di->instr_word >> 6) & 0xf; /* bits 9:6 */
            info = &T32_16_ext_bits_9_6[info->code][idx];
        } else if (info->type == EXT_7) {
            idx = (di->instr_word >> 7) & 0x1; /* bit 7 */
            info = &T32_16_ext_bit_7[info->code][idx];
        } else if (info->type == EXT_5_4) {
            idx = (di->instr_word >> 4) & 0x3; /* bit 5:4 */
            info = &T32_16_ext_bits_5_4[info->code][idx];
        } else if (info->type == EXT_10_9) {
            idx = (di->instr_word >> 9) & 0x3; /* bits 10:9 */
            info = &T32_16_ext_bits_10_9[info->code][idx];
        } else if (info->type == EXT_10_8) {
            /* check whether Rn is also listed in reglist */
            idx = decode_T32_16_ext_bits_10_8_idx(di->instr_word);
            info = &T32_16_ext_bits_10_8[info->code][idx];
        } else if (info->type == EXT_7_6) {
            idx = (di->instr_word >> 6) & 0x3; /* bits 7:6 */
            info = &T32_16_ext_bits_7_6[info->code][idx];
        } else if (info->type == EXT_3_0) {
            idx = di->instr_word & 0xf; /* bits 3:0 */
            if (idx != 0)
                idx = 1;
            info = &T32_16_ext_imm_3_0[info->code][idx];
        } else if (info->type == EXT_10_6) {
            idx = di->instr_word & 0x7c0; /* bits 10:6 */
            if (idx != 0)
                idx = 1;
            info = &T32_16_ext_imm_10_6[info->code][idx];
        } else if (info->type == EXT_6_4) {
            idx = (di->instr_word >> 4) & 0x7; /* bits 6:4 */
            info = &T32_16_ext_bits_6_4[info->code][idx];
        } else {
            ASSERT_NOT_REACHED();
            info = NULL;
            break;
        }
    }
    return info;
}

const instr_info_t *
decode_instr_info_A32(decode_info_t *di)
{
    const instr_info_t *info;
    uint idx;
    uint instr_word = di->instr_word;

    /* We first split by whether it's predicated */
    di->predicate = decode_predicate(instr_word, 28) + DR_PRED_EQ;
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
            int imm = ((instr_word >> 16) & 0xf); /*bits 19:16*/
            idx = (imm == 0) ? 0 : (imm == 1 ? 1 : 2);
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
            info = &A32_ext_fp[info->code][decode_ext_fp_idx(instr_word)];
        } else if (info->type == EXT_FPA) {
            uint idx = decode_ext_fpa_idx(instr_word);
            if (idx == 3)
                info = &invalid_instr;
            else
                info = &A32_ext_opc4fpA[info->code][idx];
        } else if (info->type == EXT_FPB) {
            info = &A32_ext_opc4fpB[info->code][decode_ext_fpb_idx(instr_word)];
        } else if (info->type == EXT_BITS16) {
            idx = ((instr_word >> 16) & 0xf) /*bits 19:16*/;
            info = &A32_ext_bits16[info->code][idx];
        } else if (info->type == EXT_RAPC) {
            idx = (((instr_word >> 16) & 0xf) /*bits 19:16*/ != 0xf) ? 0 : 1;
            info = &A32_ext_RAPC[info->code][idx];
        } else if (info->type == EXT_RBPC) {
            idx = (((instr_word >> 12) & 0xf) /*bits 15:12*/ != 0xf) ? 0 : 1;
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
            info = &A32_ext_simd6[info->code][decode_ext_simd6_idx(instr_word)];
        } else if (info->type == EXT_SIMD5) {
            info = &A32_ext_simd5[info->code][decode_ext_simd5_idx(instr_word)];
        } else if (info->type == EXT_SIMD5B) {
            info = &A32_ext_simd5b[info->code][decode_ext_simd5b_idx(instr_word)];
        } else if (info->type == EXT_SIMD8) {
            info = &A32_ext_simd8[info->code][decode_ext_simd8_idx(instr_word)];
        } else if (info->type == EXT_SIMD6B) {
            info = &A32_ext_simd6b[info->code][decode_ext_simd6b_idx(instr_word)];
        } else if (info->type == EXT_SIMD2) {
            info = &A32_ext_simd2[info->code][decode_ext_simd2_idx(instr_word)];
        } else if (info->type == EXT_IMM6L) {
            info = &A32_ext_imm6L[info->code][decode_ext_imm6l_idx(instr_word)];
        } else if (info->type == EXT_VLDA) {
            /* this table stops at 0xa in top bits, to save space */
            if (((instr_word >> 8) & 0xf) > 0xa)
                info = &invalid_instr;
            else
                info = &A32_ext_vldA[info->code][decode_ext_vlda_idx(instr_word)];
        } else if (info->type == EXT_VLDB) {
            info = &A32_ext_vldB[info->code][decode_ext_vldb_idx(instr_word)];
        } else if (info->type == EXT_VLDC) {
            info = &A32_ext_vldC[info->code][decode_ext_vldc_idx(instr_word)];
        } else if (info->type == EXT_VLDD) {
            info = &A32_ext_vldD[info->code][decode_ext_vldd_idx(instr_word)];
        } else if (info->type == EXT_VTB) {
            info = &A32_ext_vtb[info->code][decode_ext_vtb_idx(instr_word)];
        }
    }
    return info;
}

/* Disassembles the instruction at pc into the data structures ret_info
 * and di.  Returns a pointer to the pc of the next instruction.
 * Returns NULL on an invalid instruction.
 * Caller should set di->isa_mode.
 */
static byte *
read_instruction(dcontext_t *dcontext, byte *pc, byte *orig_pc,
                 const instr_info_t **ret_info,
                 decode_info_t *di _IF_DEBUG(bool report_invalid))
{
    const instr_info_t *info;

    /* Initialize di */
    di->decorated_pc = pc;
    /* We support auto-decoding an LSB=1 address as Thumb (i#1688).  We don't
     * change the thread mode, just the local mode, and we return an LSB=1 next pc.
     * We allow either of the copy or orig to have the LSB set and do not require
     * them to match as some uses cases have a local buffer for pc.
     */
    if (TEST(0x1, (ptr_uint_t)pc) || TEST(0x1, (ptr_uint_t)orig_pc)) {
        di->isa_mode = DR_ISA_ARM_THUMB;
        pc = PC_AS_LOAD_TGT(DR_ISA_ARM_THUMB, pc);
        orig_pc = PC_AS_LOAD_TGT(DR_ISA_ARM_THUMB, orig_pc);
    } else
        di->isa_mode = dr_get_isa_mode(dcontext);
    di->start_pc = pc;
    di->orig_pc = orig_pc;
    di->mem_needs_reglist_sz = NULL;
    di->reglist_sz = -1;
    di->predicate = DR_PRED_NONE;
    di->T32_16 = false;
    di->shift_type_idx = UINT_MAX;
    if (di->isa_mode == DR_ISA_ARM_THUMB)
        di->decode_state = *get_decode_state(dcontext);

    /* Read instr bytes and find instr_info */
    if (di->isa_mode == DR_ISA_ARM_THUMB) {
        di->halfwordA = *(ushort *)pc;
        pc += sizeof(ushort);
        /* First, split by whether 16 or 32 bits */
        if (TESTALL(0xe800, di->halfwordA) || TESTALL(0xf000, di->halfwordA)) {
            /* 32 bits */
            di->halfwordB = *(ushort *)pc;
            pc += sizeof(ushort);
            /* We put A up high (so this does NOT match little-endianness) */
            di->instr_word = (di->halfwordA << 16) | di->halfwordB;
            /* We use the same table for T32.32 instructions both
             * inside and outside IT blocks.
             */
            info = decode_instr_info_T32_32(di);
        } else {
            /* 16 bits */
            di->T32_16 = true;
            di->instr_word = di->halfwordA;
            if (decode_in_it_block(&di->decode_state, orig_pc, di))
                info = decode_instr_info_T32_it(di);
            else
                info = decode_instr_info_T32_16(di);
        }
        if (decode_in_it_block(&di->decode_state, orig_pc, di)) {
            if (info != NULL && info != &invalid_instr) {
                di->predicate = decode_state_advance(&di->decode_state, di);
                /* bkpt is always executed */
                if (info->type == OP_bkpt)
                    di->predicate = DR_PRED_NONE;
            } else
                decode_state_reset(&di->decode_state);
        } else if (info != NULL && info->type == OP_it) {
            decode_state_init(&di->decode_state, di, orig_pc);
        }
    } else if (di->isa_mode == DR_ISA_ARM_A32) {
        di->instr_word = *(uint *)pc;
        pc += sizeof(uint);
        info = decode_instr_info_A32(di);
    } else {
        /* XXX i#1569: A64 NYI */
        ASSERT_NOT_IMPLEMENTED(false);
        di->instr_word = 0;
        *ret_info = &invalid_instr;
        pc = NULL;
        goto read_instruction_finished;
    }

    CLIENT_ASSERT(info != NULL && info->type <= INVALID, "decoding table error");

    /* All required bits should be set */
    if ((di->instr_word & info->opcode) != info->opcode && info->type != INVALID)
        info = &invalid_instr;

    if (TESTANY(DECODE_PREDICATE_22 | DECODE_PREDICATE_8, info->flags)) {
        di->predicate = DR_PRED_EQ +
            decode_predicate(di->instr_word,
                             TEST(DECODE_PREDICATE_22, info->flags) ? 22 : 8);
    }

    /* We should now have either a valid OP_ opcode or an invalid opcode */
    if (info == &invalid_instr || info->type < OP_FIRST || info->type > OP_LAST) {
        DODEBUG({
            /* PR 605161: don't report on DR addresses */
            if (report_invalid && !is_dynamo_address(di->start_pc)) {
                SYSLOG_INTERNAL_WARNING_ONCE("Invalid opcode encountered");
                LOG(THREAD_GET, LOG_ALL, 1, "Invalid %s opcode @" PFX ": 0x%08x\n",
                    di->isa_mode == DR_ISA_ARM_A32 ? "ARM" : "Thumb", di->start_pc,
                    di->instr_word);
            }
        });
        *ret_info = &invalid_instr;
        pc = NULL;
        goto read_instruction_finished;
    }

    /* Unlike x86, we have a fixed size, so we're done */
    *ret_info = info;

read_instruction_finished:
    if (di->isa_mode == DR_ISA_ARM_THUMB)
        set_decode_state(dcontext, &di->decode_state);
    if (pc != NULL) {
        /* i#1688: keep LSB=1 decoration */
        pc = di->decorated_pc + (pc - di->start_pc);
    }
    return pc;
}

/* We have 3 callers.  Only one plans to decode its instr's operands: and for
 * that caller, decode_common(), we'd have to remember the original instr_info_t
 * in an extra local for all decodes.  We decided that it's better to pay for an
 * extra operand decode for OP_msr (and have a simpler routine here) than affect
 * the common case.
 */
static inline uint
decode_eflags_to_instr_eflags(decode_info_t *di, const instr_info_t *info)
{
    uint res = info->eflags;
    if (info->type == OP_msr) {
        /* i#1817: msr writes a subset determined by 1st immed */
        uint sel;
        /* For decoding eflags w/o operands we need this one operand */
        opnd_t immed;
        uint num = 0;
        ASSERT(info->src1_type == TYPE_I_b16 || info->src1_type == TYPE_I_b8);
        if (!decode_operand(di, info->src1_type, info->src1_size, &immed, &num))
            return 0; /* Return empty set on bogus instr */
        sel = opnd_get_immed_int(immed);
        if (!TESTALL(EFLAGS_MSR_NZCVQ, sel))
            res &= ~(EFLAGS_WRITE_NZCV | EFLAGS_WRITE_Q);
        if (!TESTALL(EFLAGS_MSR_G, sel))
            res &= ~(EFLAGS_WRITE_GE);
    }
    if (di->predicate != DR_PRED_OP && di->predicate != DR_PRED_AL &&
        di->predicate != DR_PRED_NONE) {
        res |= EFLAGS_READ_ARITH;
    }
    return res;
}

byte *
decode_eflags_usage(void *drcontext, byte *pc, uint *usage, dr_opnd_query_flags_t flags)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    const instr_info_t *info;
    decode_info_t di;
    uint eflags;
    pc = read_instruction(dcontext, pc, pc, &info, &di _IF_DEBUG(true));
    eflags = decode_eflags_to_instr_eflags(&di, info);
    *usage = instr_eflags_conditionally(eflags, di.predicate, flags);
    /* we're fine returning NULL on failure */
    return pc;
}

byte *
decode_opcode(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    const instr_info_t *info;
    decode_info_t di;
    pc = read_instruction(dcontext, pc, pc, &info, &di _IF_DEBUG(true));
    instr_set_isa_mode(instr, di.isa_mode);
    instr_set_opcode(instr, info->type);
    if (!instr_valid(instr)) {
        CLIENT_ASSERT(!instr_valid(instr), "decode_opcode: invalid instr");
        return NULL;
    }
    instr->eflags = decode_eflags_to_instr_eflags(&di, info);
    instr_set_eflags_valid(instr, true);
    instr_set_operands_valid(instr, false);
    instr_set_raw_bits(instr, pc, pc - di.orig_pc);
    return pc;
}

/* XXX: some of this code could be shared with x86/decode.c */
static byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr)
{
    const instr_info_t *info = &invalid_instr;
    decode_info_t di;
    byte *next_pc;
    uint num_dsts = 0, num_srcs = 0;
    opnd_t dsts[MAX_DST_OPNDS];
    opnd_t srcs[MAX_SRC_OPNDS];

    CLIENT_ASSERT(instr->opcode == OP_INVALID || instr->opcode == OP_UNDECODED,
                  "decode: instr is already decoded, may need to call instr_reset()");

    next_pc = read_instruction(dcontext, pc, orig_pc, &info,
                               &di _IF_DEBUG(!TEST(INSTR_IGNORE_INVALID, instr->flags)));
    instr_set_isa_mode(instr, di.isa_mode);
    instr_set_opcode(instr, info->type);
    di.opcode = info->type; /* needed for decode_cur_pc */
    /* failure up to this point handled fine -- we set opcode to OP_INVALID */
    if (next_pc == NULL) {
        LOG(THREAD, LOG_INTERP, 3, "decode: invalid instr at " PFX "\n", pc);
        CLIENT_ASSERT(!instr_valid(instr), "decode: invalid instr");
        return NULL;
    }
    instr->eflags = decode_eflags_to_instr_eflags(&di, info);
    instr_set_eflags_valid(instr, true);
    /* since we don't use set_src/set_dst we must explicitly say they're valid */
    instr_set_operands_valid(instr, true);

    if (di.predicate != DR_PRED_OP) {
        /* XXX: not bothering to mark invalid for DECODE_PREDICATE_28_AL */
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

    CLIENT_ASSERT(num_srcs <= sizeof(srcs) / sizeof(srcs[0]), "internal decode error");
    CLIENT_ASSERT(num_dsts <= sizeof(dsts) / sizeof(dsts[0]), "internal decode error");

    /* now copy operands into their real slots */
    instr_set_num_opnds(dcontext, instr, num_dsts, num_srcs);
    if (num_dsts > 0) {
        memcpy(instr->dsts, dsts, num_dsts * sizeof(opnd_t));
    }
    if (num_srcs > 0) {
        instr->src0 = srcs[0];
        if (num_srcs > 1) {
            memcpy(instr->srcs, &(srcs[1]), (num_srcs - 1) * sizeof(opnd_t));
        }
    }

    if (orig_pc != pc) {
        /* We do not want to copy when encoding and condone an invalid
         * relative target
         * TODO i#4016: Add re-relativization support without having to re-encode.
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
decode(void *drcontext, byte *pc, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return decode_common(dcontext, pc, pc, instr);
}

byte *
decode_from_copy(void *drcontext, byte *copy_pc, byte *orig_pc, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return decode_common(dcontext, copy_pc, orig_pc, instr);
}

byte *
decode_cti(void *drcontext, byte *pc, instr_t *instr)
{
    /* XXX i#1551: build a fast decoder for branches -- though it may not make
     * sense for 32-bit where many instrs can write to the pc.
     */
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return decode(dcontext, pc, instr);
}

byte *
decode_next_pc(void *drcontext, byte *pc)
{
    /* XXX: check for invalid opcodes, though maybe it's fine to never do so
     * (xref i#1685).
     */
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    dr_isa_mode_t isa_mode;
    byte *read_pc = pc;
    if (TEST(0x1, (ptr_uint_t)pc)) {
        isa_mode = DR_ISA_ARM_THUMB; /* and keep LSB=1 (i#1688) */
        read_pc = PC_AS_LOAD_TGT(DR_ISA_ARM_THUMB, read_pc);
    } else {
        isa_mode = dr_get_isa_mode(dcontext);
    }
    if (isa_mode == DR_ISA_ARM_THUMB) {
        ushort halfword = *(ushort *)pc;
        if (TESTALL(0xe800, halfword) || TESTALL(0xf000, halfword))
            return pc + THUMB_LONG_INSTR_SIZE;
        else
            return pc + THUMB_SHORT_INSTR_SIZE;
    } else
        return pc + ARM_INSTR_SIZE;
}

int
decode_sizeof(void *drcontext, byte *pc, int *num_prefixes)
{
    /* XXX: check for invalid opcodes, though maybe it's fine to never do so
     * (xref i#1685).
     */
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    byte *next_pc = decode_next_pc(dcontext, pc);
    return next_pc - pc;
}

/* XXX: share this with x86 */
byte *
decode_raw(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    /* XXX i#1551: set isa_mode of instr once we add that feature */
    int sz = decode_sizeof(dcontext, pc, NULL);
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

bool
decode_raw_is_jmp(dcontext_t *dcontext, byte *pc)
{
    dr_isa_mode_t mode = dr_get_isa_mode(dcontext);
    if (mode == DR_ISA_ARM_A32) {
        uint word = *(uint *)pc;
        return ((word & 0x0f000000) == 0x0a000000 && (word & 0xf0000000) != 0xf0000000);
    } else {
        return (((*(pc + 1)) & 0xf0) == 0xf0 && ((*(pc + 3)) & 0xd0) == 0x90);
    }
    return false;
}

byte *
decode_raw_jmp_target(dcontext_t *dcontext, byte *pc)
{
    dr_isa_mode_t mode = dr_get_isa_mode(dcontext);
    if (mode == DR_ISA_ARM_A32) {
        uint word = *(uint *)pc;
        int disp = word & 0xffffff;
        if (TEST(0x800000, disp))
            disp |= 0xff000000; /* sign-extend */
        return decode_cur_pc(pc, mode, OP_b, NULL) + (disp << 2);
    } else {
        /* A10,B13,B11,A9:0,B10:0 x2, but B13 and B11 are flipped if A10 is 0 */
        /* XXX: share with decoder's TYPE_J_b26_b13_b11_b16_b0 */
        ushort valA = *(ushort *)pc;
        ushort valB = *(ushort *)(pc + 2);
        uint bitA10 = (valA & 0x0400) >> 10;
        uint bitB13 = (valB & 0x2000) >> 13;
        uint bitB11 = (valB & 0x0800) >> 11;
        int disp = valB & 0x7ff; /* B10:0 */
        disp |= (valA & 0x3ff) << 11;
        disp |= ((bitA10 == 0 ? (bitB11 == 0 ? 1 : 0) : bitB11) << 21);
        disp |= ((bitA10 == 0 ? (bitB13 == 0 ? 1 : 0) : bitB13) << 22);
        disp |= bitA10 << 23;
        if (bitA10 == 1)
            disp |= 0xff000000; /* sign-extend */
        return decode_cur_pc(pc, mode, OP_b, NULL) + (disp << 1);
    }
    return NULL;
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
get_next_instr_info(const instr_info_t *info)
{
    return (const instr_info_t *)(info->code);
}

byte
decode_first_opcode_byte(int opcode)
{
    CLIENT_ASSERT(false, "should not be used on ARM");
    return 0;
}

/* In addition to ISA mode, we use in_block to indicate if we are in an IT block
 * for Thumb mode and select correct op_instr entries.
 */
const instr_info_t *
opcode_to_encoding_info(uint opc, dr_isa_mode_t isa_mode, bool it_block)
{
    if (isa_mode == DR_ISA_ARM_A32)
        return op_instr[opc].A32;
    else if (isa_mode == DR_ISA_ARM_THUMB)
        return (it_block ? op_instr[opc].T32_it : op_instr[opc].T32);
    else {
        CLIENT_ASSERT(false, "NYI i#1551");
        return NULL;
    }
}

DR_API
const char *
decode_opcode_name(int opcode)
{
    const instr_info_t *info = opcode_to_encoding_info(opcode, DR_ISA_ARM_A32, false);
    if (info == NULL) {
        info = opcode_to_encoding_info(opcode, DR_ISA_ARM_THUMB,
                                       /* names do not change in IT block */
                                       false);
    }
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
    case TYPE_R_U:
    case TYPE_R_V:
    case TYPE_R_W:
    case TYPE_R_X:
    case TYPE_R_Y:
    case TYPE_R_Z:
    case TYPE_R_V_DUP:
    case TYPE_R_W_DUP:
    case TYPE_R_Z_DUP:
    case TYPE_R_A_TOP:
    case TYPE_R_B_TOP:
    case TYPE_R_C_TOP:
    case TYPE_R_D_TOP:
    case TYPE_R_D_NEGATED:
    case TYPE_R_B_EVEN:
    case TYPE_R_B_PLUS1:
    case TYPE_R_D_EVEN:
    case TYPE_R_D_PLUS1:
    case TYPE_R_A_EQ_D:
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
    case TYPE_SP: return true;
    }
    return false;
}

bool
optype_is_gpr(int optype)
{
    switch (optype) {
    case TYPE_R_A:
    case TYPE_R_B:
    case TYPE_R_C:
    case TYPE_R_D:
    case TYPE_R_U:
    case TYPE_R_V:
    case TYPE_R_W:
    case TYPE_R_X:
    case TYPE_R_Y:
    case TYPE_R_Z:
    case TYPE_R_V_DUP:
    case TYPE_R_W_DUP:
    case TYPE_R_Z_DUP:
    case TYPE_R_A_TOP:
    case TYPE_R_B_TOP:
    case TYPE_R_C_TOP:
    case TYPE_R_D_TOP:
    case TYPE_R_D_NEGATED:
    case TYPE_R_B_EVEN:
    case TYPE_R_B_PLUS1:
    case TYPE_R_D_EVEN:
    case TYPE_R_D_PLUS1:
    case TYPE_R_A_EQ_D:
    case TYPE_LR:
    case TYPE_SP: return true;
    }
    return false;
}

#ifdef DEBUG
#    ifndef STANDALONE_DECODER

/* Until we have more thorough tests, we perform some sanity consistency checks
 * on app instrs that we process.
 * Running this code inside the decode loop is too hard wrt IT
 * tracking, so we require a full walk over an instrlist.  Cloning one
 * instr in isolation and encoding also does not work wrt encoding so
 * we tweak the raw bits and then restore.
 */
void
check_encode_decode_consistency(dcontext_t *dcontext, instrlist_t *ilist)
{
    instr_t *check;
    /* Avoid incorrect IT state from a bb like "subs.n;it;bx.eq" where decoding
     * from the subs will match the "prior instr" case (b/c 2 short instrs looks
     * like 1 long instr).
     */
    decode_state_reset(get_decode_state(dcontext));
    encode_reset_it_block(dcontext);
    for (check = instrlist_first(ilist); check != NULL; check = instr_get_next(check)) {
        byte buf[THUMB_LONG_INSTR_SIZE];
        instr_t tmp;
        byte *pc, *npc;
        app_pc addr = instr_get_raw_bits(check);
        int check_len = instr_length(dcontext, check);
        instr_set_raw_bits_valid(check, false);
        pc = instr_encode_to_copy(dcontext, check, buf, addr);
        instr_init(dcontext, &tmp);
        npc = decode_from_copy(dcontext, buf, addr, &tmp);
        if (npc != pc || !instr_same(check, &tmp)) {
            LOG(THREAD, LOG_EMIT, 1, "ERROR: from app:  %04x %04x  ", *(ushort *)addr,
                *(ushort *)(addr + 2));
            instr_disassemble(dcontext, check, THREAD);
            LOG(THREAD, LOG_EMIT, 1, "\nvs from encoding: %04x %04x  ", *(ushort *)buf,
                *(ushort *)(buf + 2));
            instr_disassemble(dcontext, &tmp, THREAD);
            LOG(THREAD, LOG_EMIT, 1, "\n ");
        }
        ASSERT(instr_same(check, &tmp));
        if (pc - buf != check_len) {
            /* The fragile IT block tracking will get off if our encoding doesn't
             * match the app's in length, b/c we're advancing according to app length
             * while IT tracking will advance at our length.  We try to adjust for
             * that here, unfortunately by violating abstraction.
             * XXX: can we do better?  Can we make an interface for this that
             * a client could use?  Should IT advancing compute orig_pc length
             * instead of using di->t32_16 which is based on copy pc?
             */
            decode_state_t *ds = get_decode_state(dcontext);
            if (ds->itb_info.num_instrs != 0) {
                ds->pc += check_len - (pc - buf);
            }
        }
        instr_set_raw_bits_valid(check, true);
        instr_free(dcontext, &tmp);
    }
}

static bool
optype_is_reglist(int optype)
{
    switch (optype) {
    case TYPE_L_8b:
    case TYPE_L_9b_LR:
    case TYPE_L_9b_PC:
    case TYPE_L_16b:
    case TYPE_L_16b_NO_SP:
    case TYPE_L_16b_NO_SP_PC:
    case TYPE_L_CONSEC:
    case TYPE_L_VBx2:
    case TYPE_L_VBx3:
    case TYPE_L_VBx4:
    case TYPE_L_VBx2D:
    case TYPE_L_VBx3D:
    case TYPE_L_VBx4D:
    case TYPE_L_VAx2:
    case TYPE_L_VAx3:
    case TYPE_L_VAx4: return true;
    }
    return false;
}

static void
decode_check_reglist(int optype[], uint num_types)
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

static void
decode_check_reg_dup(int src_type[], uint num_srcs, int dst_type[], uint num_dsts)
{
    uint i;
    /* TYPE_R_*_DUP are always srcs and the 1st dst is the corresponding non-dup type */
    for (i = 0; i < num_srcs; i++) {
        switch (src_type[i]) {
        case TYPE_R_V_DUP: ASSERT(dst_type[0] == TYPE_R_V); break;
        case TYPE_R_W_DUP: ASSERT(dst_type[0] == TYPE_R_W); break;
        case TYPE_R_Z_DUP: ASSERT(dst_type[0] == TYPE_R_Z); break;
        }
    }
    for (i = 0; i < num_dsts; i++) {
        switch (dst_type[i]) {
        case TYPE_R_V_DUP:
        case TYPE_R_W_DUP:
        case TYPE_R_Z_DUP: ASSERT(false);
        }
    }
}

static void
decode_check_writeback(int src_type[], uint num_srcs, int dst_type[], uint num_dsts)
{
    uint i;
    for (i = 0; i < num_srcs; i++) {
        switch (src_type[i]) {
        case TYPE_M_POS_I5x4:
        case TYPE_M_SP_POS_I8x4:
        case TYPE_M_PCREL_POS_I8x4:
            /* no writeback */
            ASSERT(dst_type[1] == TYPE_NONE);
            break;
        }
    }
    for (i = 0; i < num_dsts; i++) {
        switch (dst_type[i]) {
        case TYPE_M_POS_I5x4:
        case TYPE_M_SP_POS_I8x4:
        case TYPE_M_PCREL_POS_I8x4:
            /* no writeback */
            ASSERT(dst_type[1] == TYPE_NONE);
            break;
        }
    }
}

static void
decode_check_opnds(int src_type[], uint num_srcs, int dst_type[], uint num_dsts)
{
    decode_check_reglist(src_type, num_srcs);
    decode_check_reglist(dst_type, num_dsts);
    decode_check_reg_dup(src_type, num_srcs, dst_type, num_dsts);
    decode_check_writeback(src_type, num_srcs, dst_type, num_dsts);
}
#    endif /* STANDALONE_DECODER */

static void
check_ISA(dr_isa_mode_t isa_mode)
{
#    define MAX_TYPES 8
    DOCHECK(2, {
        uint opc;
        uint i;
        for (opc = OP_FIRST; opc < OP_AFTER_LAST; opc++) {
            const instr_info_t *info;
            for (i = 0; i < 2; i++) {
                info = opcode_to_encoding_info(opc, isa_mode, i == 0 ? true : false);
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

                    decode_check_opnds(src_type, num_srcs, dst_type, num_dsts);

                    info = get_next_instr_info(info);
                }
            }
        }
    });
}

void
decode_debug_checks_arch(void)
{
    check_ISA(DR_ISA_ARM_A32);
    check_ISA(DR_ISA_ARM_THUMB);
}
#endif /* DEBUG */

#ifdef DECODE_UNIT_TEST
/* FIXME i#1551: add unit tests here.  How divide vs suite/tests/api/ tests? */
#    include "instr_create_shared.h"

int
main()
{
    bool res = true;
    standalone_init();
    standalone_exit();
    return res;
}

#endif /* DECODE_UNIT_TEST */
