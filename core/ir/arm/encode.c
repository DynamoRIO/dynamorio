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

/* ARM encoder */
/* FIXME i#1569: add A64 support: for now just A32 */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "disassemble.h"
#include "decode_private.h"

/* Extra logging for encoding */
#define ENC_LEVEL 6

/* Order corresponds to DR_REG_ enum. */
const char *const reg_names[] = {
    "<NULL>",
    "<invalid>",
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "sp",
    "lr",
    "pc",
    "q0",
    "q1",
    "q2",
    "q3",
    "q4",
    "q5",
    "q6",
    "q7",
    "q8",
    "q9",
    "q10",
    "q11",
    "q12",
    "q13",
    "q14",
    "q15",
    "q16",
    "q17",
    "q18",
    "q19",
    "q20",
    "q21",
    "q22",
    "q23",
    "q24",
    "q25",
    "q26",
    "q27",
    "q28",
    "q29",
    "q30",
    "q31",
    "d0",
    "d1",
    "d2",
    "d3",
    "d4",
    "d5",
    "d6",
    "d7",
    "d8",
    "d9",
    "d10",
    "d11",
    "d12",
    "d13",
    "d14",
    "d15",
    "d16",
    "d17",
    "d18",
    "d19",
    "d20",
    "d21",
    "d22",
    "d23",
    "d24",
    "d25",
    "d26",
    "d27",
    "d28",
    "d29",
    "d30",
    "d31",
    "s0",
    "s1",
    "s2",
    "s3",
    "s4",
    "s5",
    "s6",
    "s7",
    "s8",
    "s9",
    "s10",
    "s11",
    "s12",
    "s13",
    "s14",
    "s15",
    "s16",
    "s17",
    "s18",
    "s19",
    "s20",
    "s21",
    "s22",
    "s23",
    "s24",
    "s25",
    "s26",
    "s27",
    "s28",
    "s29",
    "s30",
    "s31",
    "h0",
    "h1",
    "h2",
    "h3",
    "h4",
    "h5",
    "h6",
    "h7",
    "h8",
    "h9",
    "h10",
    "h11",
    "h12",
    "h13",
    "h14",
    "h15",
    "h16",
    "h17",
    "h18",
    "h19",
    "h20",
    "h21",
    "h22",
    "h23",
    "h24",
    "h25",
    "h26",
    "h27",
    "h28",
    "h29",
    "h30",
    "h31",
    "b0",
    "b1",
    "b2",
    "b3",
    "b4",
    "b5",
    "b6",
    "b7",
    "b8",
    "b9",
    "b10",
    "b11",
    "b12",
    "b13",
    "b14",
    "b15",
    "b16",
    "b17",
    "b18",
    "b19",
    "b20",
    "b21",
    "b22",
    "b23",
    "b24",
    "b25",
    "b26",
    "b27",
    "b28",
    "b29",
    "b30",
    "b31",
    "c0",
    "c1",
    "c2",
    "c3",
    "c4",
    "c5",
    "c6",
    "c7",
    "c8",
    "c9",
    "c10",
    "c11",
    "c12",
    "c13",
    "c14",
    "c15",
    "cpsr",
    "spsr",
    "fpscr",
    IF_X64_ELSE("tpidr_el0", "tpidrurw"),
    IF_X64_ELSE("tpidrro_el0", "tpidruro"),
};

/* Maps sub-registers to their containing register. */
/* Order corresponds to DR_REG_ enum. */
const reg_id_t dr_reg_fixer[] = {
    REG_NULL,
    REG_NULL,
    DR_REG_R0,
    DR_REG_R1,
    DR_REG_R2,
    DR_REG_R3,
    DR_REG_R4,
    DR_REG_R5,
    DR_REG_R6,
    DR_REG_R7,
    DR_REG_R8,
    DR_REG_R9,
    DR_REG_R10,
    DR_REG_R11,
    DR_REG_R12,
    DR_REG_R13,
    DR_REG_R14,
    DR_REG_R15,
    /* q0-q31 */
    DR_REG_Q0,
    DR_REG_Q1,
    DR_REG_Q2,
    DR_REG_Q3,
    DR_REG_Q4,
    DR_REG_Q5,
    DR_REG_Q6,
    DR_REG_Q7,
    DR_REG_Q8,
    DR_REG_Q9,
    DR_REG_Q10,
    DR_REG_Q11,
    DR_REG_Q12,
    DR_REG_Q13,
    DR_REG_Q14,
    DR_REG_Q15,
    /* x64-only but simpler code to not ifdef it */
    DR_REG_Q16,
    DR_REG_Q17,
    DR_REG_Q18,
    DR_REG_Q19,
    DR_REG_Q20,
    DR_REG_Q21,
    DR_REG_Q22,
    DR_REG_Q23,
    DR_REG_Q24,
    DR_REG_Q25,
    DR_REG_Q26,
    DR_REG_Q27,
    DR_REG_Q28,
    DR_REG_Q29,
    DR_REG_Q30,
    DR_REG_Q31,
    /* d0-d31 */
    /* For AArch64, the smaller SIMD names refer to the lower
     * bits of the corresponding same-number larger SIMD register.
     * But for AArch32, the smaller ones are compressed such that
     * they refer to the top and bottom.  B and H are AArch64-only.
     */
    DR_REG_Q0,
    DR_REG_Q0,
    DR_REG_Q1,
    DR_REG_Q1,
    DR_REG_Q2,
    DR_REG_Q2,
    DR_REG_Q3,
    DR_REG_Q3,
    DR_REG_Q4,
    DR_REG_Q4,
    DR_REG_Q5,
    DR_REG_Q5,
    DR_REG_Q6,
    DR_REG_Q6,
    DR_REG_Q7,
    DR_REG_Q7,
    DR_REG_Q8,
    DR_REG_Q8,
    DR_REG_Q9,
    DR_REG_Q9,
    DR_REG_Q10,
    DR_REG_Q10,
    DR_REG_Q11,
    DR_REG_Q11,
    DR_REG_Q12,
    DR_REG_Q12,
    DR_REG_Q13,
    DR_REG_Q13,
    DR_REG_Q14,
    DR_REG_Q14,
    DR_REG_Q15,
    DR_REG_Q15,
    /* s0-s31 */
    DR_REG_Q0,
    DR_REG_Q0,
    DR_REG_Q0,
    DR_REG_Q0,
    DR_REG_Q1,
    DR_REG_Q1,
    DR_REG_Q1,
    DR_REG_Q1,
    DR_REG_Q2,
    DR_REG_Q2,
    DR_REG_Q2,
    DR_REG_Q2,
    DR_REG_Q3,
    DR_REG_Q3,
    DR_REG_Q3,
    DR_REG_Q3,
    DR_REG_Q4,
    DR_REG_Q4,
    DR_REG_Q4,
    DR_REG_Q4,
    DR_REG_Q5,
    DR_REG_Q5,
    DR_REG_Q5,
    DR_REG_Q5,
    DR_REG_Q6,
    DR_REG_Q6,
    DR_REG_Q6,
    DR_REG_Q6,
    DR_REG_Q7,
    DR_REG_Q7,
    DR_REG_Q7,
    DR_REG_Q7,
    /* h0-h31: AArch64-only */
    DR_REG_Q0,
    DR_REG_Q1,
    DR_REG_Q2,
    DR_REG_Q3,
    DR_REG_Q4,
    DR_REG_Q5,
    DR_REG_Q6,
    DR_REG_Q7,
    DR_REG_Q8,
    DR_REG_Q9,
    DR_REG_Q10,
    DR_REG_Q11,
    DR_REG_Q12,
    DR_REG_Q13,
    DR_REG_Q14,
    DR_REG_Q15,
    DR_REG_D16,
    DR_REG_D17,
    DR_REG_D18,
    DR_REG_D19,
    DR_REG_D20,
    DR_REG_D21,
    DR_REG_D22,
    DR_REG_D23,
    DR_REG_D24,
    DR_REG_D25,
    DR_REG_D26,
    DR_REG_D27,
    DR_REG_D28,
    DR_REG_D29,
    DR_REG_D30,
    DR_REG_D31,
    /* b0-b31: AArch64-only */
    DR_REG_Q0,
    DR_REG_Q1,
    DR_REG_Q2,
    DR_REG_Q3,
    DR_REG_Q4,
    DR_REG_Q5,
    DR_REG_Q6,
    DR_REG_Q7,
    DR_REG_Q8,
    DR_REG_Q9,
    DR_REG_Q10,
    DR_REG_Q11,
    DR_REG_Q12,
    DR_REG_Q13,
    DR_REG_Q14,
    DR_REG_Q15,
    DR_REG_D16,
    DR_REG_D17,
    DR_REG_D18,
    DR_REG_D19,
    DR_REG_D20,
    DR_REG_D21,
    DR_REG_D22,
    DR_REG_D23,
    DR_REG_D24,
    DR_REG_D25,
    DR_REG_D26,
    DR_REG_D27,
    DR_REG_D28,
    DR_REG_D29,
    DR_REG_D30,
    DR_REG_D31,
    DR_REG_CR0,
    DR_REG_CR1,
    DR_REG_CR2,
    DR_REG_CR3,
    DR_REG_CR4,
    DR_REG_CR5,
    DR_REG_CR6,
    DR_REG_CR7,
    DR_REG_CR8,
    DR_REG_CR9,
    DR_REG_CR10,
    DR_REG_CR11,
    DR_REG_CR12,
    DR_REG_CR13,
    DR_REG_CR14,
    DR_REG_CR15,
    DR_REG_CPSR,
    DR_REG_SPSR,
    DR_REG_FPSCR,
    DR_REG_TPIDRURW,
    DR_REG_TPIDRURO,
};

const char *const type_names[] = {
    "TYPE_NONE",
    "TYPE_R_A",
    "TYPE_R_B",
    "TYPE_R_C",
    "TYPE_R_D",
    "TYPE_R_U",
    "TYPE_R_V",
    "TYPE_R_W",
    "TYPE_R_X",
    "TYPE_R_Y",
    "TYPE_R_Z",
    "TYPE_R_V_DUP",
    "TYPE_R_W_DUP",
    "TYPE_R_Z_DUP",
    "TYPE_R_A_TOP",
    "TYPE_R_B_TOP",
    "TYPE_R_C_TOP",
    "TYPE_R_D_TOP",
    "TYPE_R_D_NEGATED",
    "TYPE_R_B_EVEN",
    "TYPE_R_B_PLUS1",
    "TYPE_R_D_EVEN",
    "TYPE_R_D_PLUS1",
    "TYPE_R_A_EQ_D",
    "TYPE_CR_A",
    "TYPE_CR_B",
    "TYPE_CR_C",
    "TYPE_CR_D",
    "TYPE_V_A",
    "TYPE_V_B",
    "TYPE_V_C",
    "TYPE_V_C_3b",
    "TYPE_V_C_4b",
    "TYPE_W_A",
    "TYPE_W_B",
    "TYPE_W_C",
    "TYPE_W_C_PLUS1",
    "TYPE_SPSR",
    "TYPE_CPSR",
    "TYPE_FPSCR",
    "TYPE_LR",
    "TYPE_SP",
    "TYPE_PC",
    "TYPE_I_b0",
    "TYPE_I_x4_b0",
    "TYPE_I_SHIFTED_b0",
    "TYPE_NI_b0",
    "TYPE_NI_x4_b0",
    "TYPE_I_b3",
    "TYPE_I_b4",
    "TYPE_I_b5",
    "TYPE_I_b6",
    "TYPE_I_b7",
    "TYPE_I_b8",
    "TYPE_I_b9",
    "TYPE_I_b10",
    "TYPE_I_b16",
    "TYPE_I_b17",
    "TYPE_I_b18",
    "TYPE_I_b19",
    "TYPE_I_b20",
    "TYPE_I_b21",
    "TYPE_I_b0_b5",
    "TYPE_I_b4_b8",
    "TYPE_I_b4_b16",
    "TYPE_I_b5_b3",
    "TYPE_I_b8_b0",
    "TYPE_NI_b8_b0",
    "TYPE_I_b8_b16",
    "TYPE_I_b8_b24_b16_b0",
    "TYPE_I_b8_b28_b16_b0",
    "TYPE_I_b12_b6",
    "TYPE_I_b16_b0",
    "TYPE_I_b16_b26_b12_b0",
    "TYPE_I_b21_b5",
    "TYPE_I_b21_b6",
    "TYPE_I_b26_b12_b0",
    "TYPE_I_b26_b12_b0_z",
    "TYPE_J_b0",
    "TYPE_J_x4_b0",
    "TYPE_J_b0_b24",
    "TYPE_J_b9_b3",
    "TYPE_J_b26_b11_b13_b16_b0",
    "TYPE_J_b26_b13_b11_b16_b0",
    "TYPE_SHIFT_b4",
    "TYPE_SHIFT_b5",
    "TYPE_SHIFT_b6",
    "TYPE_SHIFT_b21",
    "TYPE_SHIFT_LSL",
    "TYPE_SHIFT_ASR",
    "TYPE_L_8b",
    "TYPE_L_9b_LR",
    "TYPE_L_9b_PC",
    "TYPE_L_16b",
    "TYPE_L_16b_NO_SP",
    "TYPE_L_16b_NO_SP_PC",
    "TYPE_L_CONSEC",
    "TYPE_L_VBx2",
    "TYPE_L_VBx3",
    "TYPE_L_VBx4",
    "TYPE_L_VBx2D",
    "TYPE_L_VBx3D",
    "TYPE_L_VBx4D",
    "TYPE_L_VAx2",
    "TYPE_L_VAx3",
    "TYPE_L_VAx4",
    "TYPE_M",
    "TYPE_M_SP",
    "TYPE_M_POS_REG",
    "TYPE_M_NEG_REG",
    "TYPE_M_POS_SHREG",
    "TYPE_M_NEG_SHREG",
    "TYPE_M_POS_LSHREG",
    "TYPE_M_POS_LSH1REG",
    "TYPE_M_POS_I12",
    "TYPE_M_NEG_I12",
    "TYPE_M_SI9",
    "TYPE_M_POS_I8",
    "TYPE_M_NEG_I8",
    "TYPE_M_POS_I8x4",
    "TYPE_M_NEG_I8x4",
    "TYPE_M_SP_POS_I8x4",
    "TYPE_M_POS_I4_4",
    "TYPE_M_NEG_I4_4",
    "TYPE_M_SI7",
    "TYPE_M_POS_I5",
    "TYPE_M_POS_I5x2",
    "TYPE_M_POS_I5x4",
    "TYPE_M_PCREL_POS_I8x4",
    "TYPE_M_PCREL_POS_I12",
    "TYPE_M_PCREL_NEG_I12",
    "TYPE_M_PCREL_S9",
    "TYPE_M_PCREL_U9",
    "TYPE_M_UP_OFFS",
    "TYPE_M_DOWN",
    "TYPE_M_DOWN_OFFS",
    "TYPE_M_SP_DOWN_OFFS",
    "TYPE_K",
};

/* Global data structure to track the decode state,
 * it should be only used for drdecodelib or early init/late exit.
 * FIXME i#1595: add multi-dcontext support to drdecodelib.
 */
static encode_state_t global_encode_state;

static encode_state_t *
get_encode_state(dcontext_t *dcontext)
{
    ASSERT(sizeof(encode_state_t) <= sizeof(dcontext->encode_state));
    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT)
        return &global_encode_state;
    else
        return (encode_state_t *)&dcontext->encode_state;
}

static void
set_encode_state(dcontext_t *dcontext, encode_state_t *state)
{
    ASSERT(sizeof(*state) <= sizeof(dcontext->encode_state));
    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT)
        global_encode_state = *state;
    else {
        *(encode_state_t *)&dcontext->encode_state = *state;
    }
}

static void
encode_state_init(encode_state_t *state, decode_info_t *di, instr_t *instr)
{
    /* We need to set di->instr_word for it_block_info_init */
    if (instr_raw_bits_valid(instr))
        di->instr_word = *(ushort *)instr->bytes;
    else {
        ASSERT(instr_operands_valid(instr));
        di->instr_word = (opnd_get_immed_int(instr_get_src(instr, 0)) << 4) |
            opnd_get_immed_int(instr_get_src(instr, 1));
    }
    it_block_info_init(&state->itb_info, di);
    /* forward to the next non-label instr */
    for (state->instr = instr_get_next(instr);
         state->instr != NULL && instr_is_label(state->instr);
         state->instr = instr_get_next(state->instr))
        ; /* do nothing to skip the label instr */
    if (state->instr == NULL) {
        CLIENT_ASSERT(instr_get_prev(instr) == NULL, /* ok if not in ilist */
                      "invalid IT block sequence");
        it_block_info_reset(&state->itb_info);
    }
}

static void
encode_state_reset(encode_state_t *state)
{
    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "exited IT block\n");
    it_block_info_reset(&state->itb_info);
    state->instr = NULL;
}

static dr_pred_type_t
encode_state_advance(encode_state_t *state, instr_t *instr)
{
    dr_pred_type_t pred;
    pred = it_block_instr_predicate(state->itb_info, state->itb_info.cur_instr);
    /* We don't want to point state->instr beyond end IT block, to avoid our
     * prior-instr matching logic matching too far.  We also don't want to
     * reset yet, so we can handle a prior-instr on the last instr.
     */
    if (it_block_info_advance(&state->itb_info)) {
        /* forward to the next non-label instr */
        for (state->instr = instr_get_next(instr);
             state->instr != NULL && instr_is_label(state->instr);
             state->instr = instr_get_next(state->instr))
            ; /* do nothing to skip the label instr */
    }
    return pred;
}

static inline bool
encode_in_it_block(encode_state_t *state, instr_t *instr)
{
    if (state->itb_info.num_instrs != 0) {
        instr_t *prev;
        LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL,
            "in IT: cur=%d, in=" PFX " %d vs " PFX " %d\n", state->itb_info.cur_instr,
            state->instr, state->instr->opcode, instr, instr->opcode);
        ASSERT(state->instr != NULL);
        if (instr == state->instr) {
            /* Look for a duplicate call to the final instr in the block, where
             * we left state->instr where it was.
             */
            if (state->itb_info.cur_instr == state->itb_info.num_instrs) {
                /* Undo the advance */
                state->itb_info.cur_instr--;
            }
            return true;
        }
        for (prev = instr_get_prev(state->instr); prev != NULL && instr_is_label(prev);
             prev = instr_get_prev(prev))
            ; /* do nothing to skip the label instr */
        if (instr == prev) {
            if (state->itb_info.cur_instr == 0)
                return false; /* still on OP_it */
            else {
                /* Undo the advance */
                state->instr = instr;
                state->itb_info.cur_instr--;
                return true;
            }
        }
        /* no match, reset the state */
        encode_state_reset(state);
    }
    return false;
}

static void
encode_track_it_block_di(dcontext_t *dcontext, decode_info_t *di, instr_t *instr)
{
    if (instr_opcode_valid(instr) && instr_get_opcode(instr) == OP_it) {
        LOG(THREAD, LOG_EMIT, ENC_LEVEL, "start IT block\n");
        encode_state_init(&di->encode_state, di, instr);
        set_encode_state(dcontext, &di->encode_state);
    } else if (di->encode_state.itb_info.num_instrs != 0) {
        if (encode_in_it_block(&di->encode_state, instr)) {
            LOG(THREAD, LOG_EMIT, ENC_LEVEL, "inside IT block\n");
            /* encode_state is reset if reach the end of IT block */
            encode_state_advance(&di->encode_state, instr);
        }
        set_encode_state(dcontext, &di->encode_state);
    } else if (instr_get_isa_mode(instr) == DR_ISA_ARM_THUMB &&
               instr_get_predicate(instr) != DR_PRED_NONE) {
        /* Our state might have been reset due to an instr or insrlist free. */
        instr_t *prev = instr_get_prev(instr);
        int count = 0;
        while (prev != NULL && count < 4) {
            if (instr_opcode_valid(prev) && instr_get_opcode(prev) == OP_it) {
                encode_state_init(&di->encode_state, di, prev);
                prev = instr_get_next(prev);
                while (prev != instr) {
                    if (encode_in_it_block(&di->encode_state, prev))
                        encode_state_advance(&di->encode_state, prev);
                    prev = instr_get_next(prev);
                }
                set_encode_state(dcontext, &di->encode_state);
                break;
            }
            ++count;
            prev = instr_get_prev(prev);
        }
    }
}

void
encode_track_it_block(dcontext_t *dcontext, instr_t *instr)
{
    decode_info_t di;
    di.encode_state = *get_encode_state(dcontext);
    encode_track_it_block_di(dcontext, &di, instr);
}

void
encode_reset_it_block(dcontext_t *dcontext)
{
    encode_state_t state;
    encode_state_reset(&state);
    set_encode_state(dcontext, &state);
}

void
encode_instr_freed_event(dcontext_t *dcontext, instr_t *instr)
{
    encode_state_t *state = get_encode_state(dcontext);
    if (state->instr == instr)
        encode_reset_it_block(dcontext);
}

#ifdef DEBUG
void
encode_debug_checks(void)
{
    CLIENT_ASSERT(sizeof(dr_reg_fixer) / sizeof(dr_reg_fixer[0]) == DR_REG_LAST_ENUM + 1,
                  "internal register enum error");
    CLIENT_ASSERT(sizeof(reg_names) / sizeof(reg_names[0]) == DR_REG_LAST_ENUM + 1,
                  "reg_names missing an entry");
    CLIENT_ASSERT(sizeof(type_names) / sizeof(type_names[0]) == TYPE_BEYOND_LAST_ENUM,
                  "type_names missing an entry");
}
#endif

opnd_size_t
resolve_size_upward(opnd_size_t size)
{
    switch (size) {
    case OPSZ_1_of_4:
    case OPSZ_2_of_4: return OPSZ_4;

    case OPSZ_1_of_8:
    case OPSZ_2_of_8:
    case OPSZ_4_of_8: return OPSZ_8;

    case OPSZ_1_of_16:
    case OPSZ_2_of_16:
    case OPSZ_4_of_16:
    case OPSZ_8_of_16:
    case OPSZ_12_of_16:
    case OPSZ_14_of_16:
    case OPSZ_15_of_16: return OPSZ_16;

    case OPSZ_16_of_32: return OPSZ_32;
    default: return size;
    }
}

opnd_size_t
resolve_size_downward(opnd_size_t size)
{
    switch (size) {
    case OPSZ_1_of_4:
    case OPSZ_1_of_8:
    case OPSZ_1_of_16: return OPSZ_1;
    case OPSZ_2_of_4:
    case OPSZ_2_of_8:
    case OPSZ_2_of_16: return OPSZ_2;
    case OPSZ_4_of_16:
    case OPSZ_4_of_8: return OPSZ_4;
    case OPSZ_8_of_16: return OPSZ_8;
    case OPSZ_12_of_16: return OPSZ_12;
    case OPSZ_14_of_16: return OPSZ_14;
    case OPSZ_15_of_16: return OPSZ_15;
    case OPSZ_16_of_32: return OPSZ_16;
    default: return size;
    }
}

static bool
reg_is_cpreg(reg_id_t reg)
{
    return (reg >= DR_REG_CR0 && reg <= DR_REG_CR15);
}

static reg_id_t
reg_simd_start(reg_id_t reg)
{
    if (reg >= DR_REG_B0 && reg <= DR_REG_B31)
        return DR_REG_B0;
    if (reg >= DR_REG_H0 && reg <= DR_REG_H31)
        return DR_REG_H0;
    if (reg >= DR_REG_S0 && reg <= DR_REG_S31)
        return DR_REG_S0;
    if (reg >= DR_REG_D0 && reg <= DR_REG_D31)
        return DR_REG_D0;
    if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31)
        return DR_REG_Q0;
    CLIENT_ASSERT(false, "internal encoder error: not a simd reg");
    return DR_REG_NULL;
}

static bool
encode_shift_values(dr_shift_type_t shift, uint amount, ptr_int_t *sh2 OUT,
                    ptr_int_t *val OUT)
{
    if (shift == DR_SHIFT_NONE) {
        *sh2 = 0;
        *val = 0;
        return (amount == 0);
    } else if (shift == DR_SHIFT_LSL) {
        *sh2 = SHIFT_ENCODING_LSL;
        *val = amount;
        return (amount >= 1 && amount <= 31);
    } else if (shift == DR_SHIFT_LSR) {
        *sh2 = SHIFT_ENCODING_LSR;
        *val = (amount == 32) ? 0 : amount;
        return (amount >= 1 && amount <= 32);
    } else if (shift == DR_SHIFT_ASR) {
        *sh2 = SHIFT_ENCODING_ASR;
        *val = (amount == 32) ? 0 : amount;
        return (amount >= 1 && amount <= 32);
    } else if (shift == DR_SHIFT_RRX) {
        *sh2 = SHIFT_ENCODING_RRX;
        *val = 0;
        return (amount == 1);
    } else if (shift == DR_SHIFT_ROR) {
        *sh2 = SHIFT_ENCODING_RRX;
        *val = amount;
        return (amount >= 1 && amount <= 31);
    }
    return false;
}

/* 0 stride means no stride */
static bool
encode_reglist_ok(decode_info_t *di, opnd_size_t size_temp, instr_t *in, bool is_dst,
                  uint *counter INOUT, uint min_num, uint max_num, bool is_simd,
                  uint stride, uint prior, reg_id_t excludeA, reg_id_t excludeB,
                  reg_id_t base_reg)
{
    opnd_size_t size_temp_up = resolve_size_upward(size_temp);
    opnd_size_t size_temp_down = resolve_size_downward(size_temp);
    uint i, base_reg_cnt = 0;
    opnd_t opnd;
    opnd_size_t size_op;
    reg_id_t reg, last_reg = DR_REG_NULL;

    if (di->T32_16 && is_simd)
        return false;

    /* Undo what encode_opnd_ok already did */
    (*counter)--;
    /* We rule out more than one reglist per template in decode_debug_checks_arch() */
    di->reglist_start = *counter;
    for (i = 0; i < max_num; i++) {
        uint opnum = *counter;
        if (is_dst) {
            if (opnum >= instr_num_dsts(in))
                break;
            opnd = instr_get_dst(in, opnum);
        } else {
            if (opnum >= instr_num_srcs(in))
                break;
            opnd = instr_get_src(in, opnum);
        }
        size_op = opnd_get_size(opnd);
        if (!opnd_is_reg(opnd))
            break;
        reg = opnd_get_reg(opnd);
        LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  reglist %d: considering %s\n", i,
            reg_names[reg]);
        if (reg == base_reg)
            base_reg_cnt++;
        if (i > 0 && stride > 0 && reg != last_reg + stride)
            break;
        if (is_simd ? !reg_is_simd(reg) : !reg_is_gpr(reg))
            break;
        if (reg == excludeA || reg == excludeB)
            break;
        LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  reglist %d: size %s vs %s %s\n", i,
            size_names[size_op], size_names[size_temp], size_names[size_temp_up]);
        if (!(size_op == size_temp || size_op == size_temp_up ||
              size_op == size_temp_down))
            break;
        if (di->T32_16 && reg > DR_REG_R7 &&
            /* only R0-R7 and PC/LR could be used in the T32.16 reglist */
            !(max_num == 9 &&
              ((reg == DR_REG_PC && is_dst) /* pop in T32.16 */ ||
               (reg == DR_REG_LR && !is_dst) /* push in T32.16 */)))
            break;
        last_reg = reg;
        (*counter)++;
    }
    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  reglist_start: %d, reglist_stop: %d\n",
        di->reglist_start, *counter);
    di->reglist_stop = *counter;
    if (di->reglist_stop - di->reglist_start < min_num)
        return false;
    /* Due to possible rollback of greedy reglists we can't compare to the
     * memory size here so we check later.
     */
    di->reglist_sz = (prior + di->reglist_stop - di->reglist_start) *
        /* Be sure to use the sub-reg size from the template */
        opnd_size_in_bytes(size_temp);
    di->reglist_itemsz = size_temp; /* in case of rollback */
    di->reglist_simd = is_simd;     /* in case of rollback */
    di->reglist_min_num = min_num;  /* in case of rollback */
    /* For T32.16, the base reg should appear either in the reglist or as
     * a writeback reg once and only once.
     */
    if (di->T32_16 && max_num == 8 && base_reg != REG_NULL && base_reg_cnt != 1)
        return false;
    return true;
}

/* Called when beyond the operand count of the instr.  Due to the first entry
 * of a SIMD reglist being its own separate template entry, we have to specially
 * handle a single-entry list here.
 */
static bool
encode_simd_reglist_single_entry(decode_info_t *di, byte optype, opnd_size_t size_temp)
{
    if (optype == TYPE_L_CONSEC) {
        /* XXX: an "unpredictable" instr with a count of 0 will end up being encoded
         * with a valid 1 or 2 count and thus won't match the decode: but that
         * seems ok for such a corner case.
         */
        di->reglist_stop = di->reglist_start = 0;
        /* Be sure to use the sub-reg size from the template */
        di->reglist_sz = opnd_size_in_bytes(size_temp);
        /* There should be no rollback, but just to be complete: */
        di->reglist_itemsz = size_temp;
        di->reglist_simd = true;
        di->reglist_min_num = 0;
        return true;
    }
    return false;
}

static bool
check_reglist_size(decode_info_t *di)
{
    /* Rollback of greedy reglists means we can't check reglist sizes until the end */
    if (di->memop_sz == OPSZ_VAR_REGLIST && di->reglist_sz == 0) {
        di->errmsg = "No register list found to match memory operand size";
        return false;
    } else if (di->reglist_sz > 0 && di->memop_sz != OPSZ_NA &&
               di->reglist_sz != opnd_size_in_bytes(di->memop_sz) &&
               di->memop_sz != OPSZ_VAR_REGLIST) {
        LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  check reglist=%d memop=%s(%d)\n",
            di->reglist_sz, size_names[di->memop_sz], opnd_size_in_bytes(di->memop_sz));
        di->errmsg = "Register list size %d bytes does not match memory operand size";
        di->errmsg_param = di->reglist_sz;
        return false;
    }
    return true;
}

static ptr_int_t
get_immed_val_shared(decode_info_t *di, opnd_t opnd, bool relative, bool selected)
{
    if (opnd_is_immed_int(opnd))
        return opnd_get_immed_int(opnd);
    else if (opnd_is_near_instr(opnd)) {
        if (selected)
            di->has_instr_opnds = true;
        if (relative) {
            /* We ignore instr's shift for relative: shouldn't happen */
            CLIENT_ASSERT(opnd_get_shift(opnd) == 0,
                          "relative shifted instr not supported");
            /* For A32, "cur PC" is "PC + 8"; "PC + 4" for Thumb, sometimes aligned */
            return (ptr_int_t)opnd_get_instr(opnd)->offset -
                (di->cur_offs +
                 decode_cur_pc(di->final_pc, di->isa_mode, di->opcode, NULL) -
                 di->final_pc);
        } else {
            ptr_int_t val = (ptr_int_t)opnd_get_instr(opnd)->offset - (di->cur_offs) +
                (ptr_int_t)di->final_pc;
            /* Support insert_mov_instr_addr() by truncating to opnd size */
            uint bits = opnd_size_in_bits(opnd_get_size(opnd));
            val >>= opnd_get_shift(opnd);
            val &= ((1 << bits) - 1);
            if (opnd_get_shift(opnd) == 0)
                return (ptr_int_t)PC_AS_JMP_TGT(di->isa_mode, (byte *)val);
            else
                return val; /* don't add 1 to the top part! */
        }
    } else if (opnd_is_near_pc(opnd)) {
        if (relative) {
            /* For A32, "cur PC" is "PC + 8"; "PC + 4" for Thumb, sometimes aligned */
            return (
                ptr_int_t)(opnd_get_pc(opnd) -
                           decode_cur_pc(di->final_pc, di->isa_mode, di->opcode, NULL));
        } else {
            return (ptr_int_t)opnd_get_pc(opnd);
        }
    }
    CLIENT_ASSERT(false, "invalid immed opnd type");
    return 0;
}

static ptr_int_t
get_immed_val_rel(decode_info_t *di, opnd_t opnd)
{
    return get_immed_val_shared(di, opnd, true /*relative*/, true /*selected*/);
}

static ptr_int_t
get_immed_val_abs(decode_info_t *di, opnd_t opnd)
{
    return get_immed_val_shared(di, opnd, false /*relative*/, true /*selected*/);
}

static bool
encode_immed_ok(decode_info_t *di, opnd_size_t size_temp, ptr_int_t val,
                int scale /* 0 means no scale */, bool is_signed, bool negated)
{
    uint bits = opnd_size_in_bits(size_temp);
    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL,
        "  immed ok: val/scale %d/%d %s vs bits %d (=> %d), wb %s %d neg=%d\n", val,
        scale, size_names[size_temp], bits, 1 << bits, size_names[di->check_wb_disp_sz],
        di->check_wb_disp_sz, negated);
    /* Ensure writeback disp matches memop disp */
    ASSERT(scale >= 0);
    if (scale > 1 && (val % scale) != 0)
        return false;
    if (di->check_wb_disp_sz != OPSZ_NA && di->check_wb_disp_sz == size_temp &&
        di->check_wb_disp != (negated ? -val : val))
        return false;
    /* convert val to the actual val to be encoded */
    if (scale != 0)
        val = val / scale;
    if (is_signed) {
        if (val < 0)
            return -val <= (1 << (bits - 1));
        else
            return val < (1 << (bits - 1));
    } else {
        ptr_uint_t uval = (ptr_uint_t)val;
        return uval < (1 << bits);
    }
}

static bool
encode_immed_int_or_instr_ok(decode_info_t *di, opnd_size_t size_temp, int scale,
                             opnd_t opnd, bool is_signed, bool negated, bool relative,
                             bool check_range)
{
    /* We'll take a pc for any immediate */
    if (opnd_is_immed_int(opnd) || opnd_is_near_instr(opnd) || opnd_is_near_pc(opnd)) {
        ptr_int_t val = get_immed_val_shared(di, opnd, relative, false /*just checking*/);
        return (!check_range ||
                encode_immed_ok(di, size_temp, val, scale, is_signed, negated));
    }
    return false;
}

static bool
encode_A32_modified_immed_ok(decode_info_t *di, opnd_size_t size_temp, opnd_t opnd)
{
    ptr_uint_t val; /* uint for bit manip w/o >> adding 1's */
    int i;
    uint unval;
    if (di->isa_mode != DR_ISA_ARM_A32) {
        CLIENT_ASSERT(false, "encoding chains are mixed up: thumb pointing at arm");
        return false;
    }
    if (size_temp != OPSZ_12b)
        return false;
    if (!opnd_is_immed_int(opnd) && !opnd_is_near_instr(opnd) && !opnd_is_near_pc(opnd))
        return false;
    val = (ptr_uint_t)get_immed_val_shared(di, opnd, false /*abs*/,
                                           false /*just checking*/);
    /* Check for each possible rotated pattern, and store the encoding
     * now to avoid re-doing this work at real encode time.
     * The rotation can produce two separate non-zero sequences which are a
     * pain to analyze directly, so instead we just try each possible rotation
     * and "undo" the encoded rotation to see if we get a single-byte value.
     * We're supposed to pick the one with the smallest rotation, so we walk backward.
     */
    for (i = 16; i > 0; i--) {
        /* ROR val, i*2 */
        unval = (val >> (i * 2)) | (val << (32 - (i * 2)));
        if (unval < 0x100) {
            int rot = 16 - i;
            di->mod_imm_enc = (rot << 8) | unval;
            return true;
        }
    }
    return false;
}

static bool
encode_T32_modified_immed_ok(decode_info_t *di, opnd_size_t size_temp, opnd_t opnd)
{
    ptr_uint_t val; /* uint for bit manip w/o >> adding 1's */
    int i, first_one;
    if (di->isa_mode != DR_ISA_ARM_THUMB) {
        CLIENT_ASSERT(false, "encoding chains are mixed up: arm pointing at thumb");
        return false;
    }
    if (size_temp != OPSZ_12b)
        return false;
    if (!opnd_is_immed_int(opnd) && !opnd_is_near_instr(opnd) && !opnd_is_near_pc(opnd))
        return false;
    val = (ptr_uint_t)get_immed_val_shared(di, opnd, false /*abs*/,
                                           false /*just checking*/);
    /* Check for each pattern, and store the encoding now to avoid re-doing
     * this work at real encode time.
     */
    /* 0) 00000000 00000000 00000000 abcdefgh */
    if ((val & 0x000000ff) == val) {
        di->mod_imm_enc = /*code 0*/ val;
        return true;
    }
    /* 1) 00000000 abcdefgh 00000000 abcdefgh */
    if ((val & 0x00ff00ff) == val && (val >> 16 == (val & 0xff))) {
        di->mod_imm_enc = (1 << 8) /*code 1*/ | (val & 0xff);
        return true;
    }
    /* 2) abcdefgh 00000000 abcdefgh 00000000 */
    if ((val & 0xff00ff00) == val && (val >> 16 == (val & 0xff00))) {
        di->mod_imm_enc = (2 << 8) /*code 2*/ | (val >> 24);
        return true;
    }
    /* 3) abcdefgh abcdefgh abcdefgh abcdefgh */
    if ((((val >> 24) & 0xff) == (val & 0xff)) &&
        (((val >> 16) & 0xff) == (val & 0xff)) && (((val >> 8) & 0xff) == (val & 0xff))) {
        di->mod_imm_enc = (3 << 8) /*code 3*/ | (val & 0xff);
        return true;
    }
    /* 4) ROR of 1bcdefgh */
    first_one = -1;
    for (i = 31; i >= 0; i--) {
        if (TEST(1 << i, val)) {
            if (first_one == -1)
                first_one = i;
            else if (first_one - i >= 8)
                return false;
        }
    }
    if (first_one < 8) /* ROR must be from 8 through 31 */
        return false;
    /* ROR amount runs from 8 through 31: 8 has 1bcdefgh starting at bit 31. */
    di->mod_imm_enc = ((8 + (31 - first_one)) << 7) | ((val >> (first_one - 7)) & 0x7f);
    return true;
}

static bool
encode_SIMD_modified_immed_ok(decode_info_t *di, opnd_size_t size_temp, opnd_t opnd)
{
    ptr_uint_t val; /* uint for bit manip w/o >> adding 1's */
    uint cmode = 0, size = 0;
    if (size_temp != OPSZ_12b)
        return false;
    if (!opnd_is_immed_int(opnd) && !opnd_is_near_instr(opnd) && !opnd_is_near_pc(opnd))
        return false;
    val = (ptr_uint_t)get_immed_val_shared(di, opnd, false /*abs*/,
                                           false /*just checking*/);
    /* We've encoded the data type into the opcode, and to avoid confusing some
     * of these constants with others for the wrong type we have to dispatch on
     * all possible opcodes that come here.
     */
    switch (di->opcode) {
    case OP_vmov_i8: size = 8; break;
    case OP_vbic_i16:
    case OP_vmov_i16:
    case OP_vmvn_i16:
    case OP_vorr_i16: size = 16; break;
    case OP_vbic_i32:
    case OP_vmov_i32:
    case OP_vmvn_i32:
    case OP_vorr_i32: size = 32; break;
    case OP_vmov_i64: size = 64; break;
    case OP_vmov_f32:
        size = 33; /* code for "f32" */
        break;
    default:
        CLIENT_ASSERT(false, "encoding table error: SIMD immed on unexpected opcode");
        return false;
    }
    /* Xref AdvSIMDExpandImm in the manual.
     * Check for each pattern, and store the encoding now to avoid re-doing
     * this work at real encode time.
     * There is some overlap between cmode and size specifier bits used
     * to distinguish our opcodes, but we bitwise-or everything together,
     * and the templates already include required cmode bits.
     */
    if ((size == 8 || size == 16 || size == 32) && (val & 0x000000ff) == val) {
        /* cmode = 000x => 00000000 00000000 00000000 abcdefgh */
        /* cmode = 100x => 00000000 abcdefgh */
        /* cmode = 1110 => abcdefgh */
        /* template should already contain the required cmode bits */
    } else if ((size == 16 || size == 32) && (val & 0x0000ff00) == val) {
        /* cmode = 001x => 00000000 00000000 abcdefgh 00000000 */
        /* cmode = 101x => abcdefgh 00000000 */
        cmode = 2; /* for _i16, template should already have top cmode bit set */
        val = val >> 8;
    } else if (size == 32 && (val & 0x00ff0000) == val) {
        /* cmode = 010x => 00000000 abcdefgh 00000000 00000000 */
        cmode = 4;
        val = val >> 16;
    } else if (size == 32 && (val & 0xff000000) == val) {
        /* cmode = 011x => abcdefgh 00000000 00000000 00000000 */
        cmode = 6;
        val = val >> 24;
    } else if (size == 32 && (val & 0x0000ffff) == val && (val & 0x000000ff) == 0xff) {
        /* cmode = 1100 => 00000000 00000000 abcdefgh 11111111 */
        cmode = 0xc;
        val = val >> 8;
    } else if (size == 32 && (val & 0x00ffffff) == val && (val & 0x0000ffff) == 0xffff) {
        /* cmode = 1101 => 00000000 abcdefgh 11111111 11111111 */
        cmode = 0xd;
        val = val >> 16;
    } else if (size == 33 /*f32*/ && (val & 0xfff80000) == val &&
               ((val & 0x7e000000) == 0x3e000000 || (val & 0x7e000000) == 0x40000000)) {
        /* cmode = 1111 => aBbbbbbc defgh000 00000000 00000000 */
        cmode = 0xf;
        val = ((val >> 24) & 0x80) | ((val >> 19) & 0x7f);
    } else if (size == 64 && opnd_is_immed_int64(opnd)) {
        int64 val64 = opnd_get_immed_int64(opnd);
        int low = (int)val64;
        int high = (int)(val64 >> 32);
        if (((low & 0xff000000) == 0xff000000 || (low & 0xff000000) == 0) &&
            ((low & 0x00ff0000) == 0x00ff0000 || (low & 0x00ff0000) == 0) &&
            ((low & 0x0000ff00) == 0x0000ff00 || (low & 0x0000ff00) == 0) &&
            ((low & 0x000000ff) == 0x000000ff || (low & 0x000000ff) == 0) &&
            ((high & 0xff000000) == 0xff000000 || (high & 0xff000000) == 0) &&
            ((high & 0x00ff0000) == 0x00ff0000 || (high & 0x00ff0000) == 0) &&
            ((high & 0x0000ff00) == 0x0000ff00 || (high & 0x0000ff00) == 0) &&
            ((high & 0x000000ff) == 0x000000ff || (high & 0x000000ff) == 0)) {
            /* cmode = 1110 =>
             *   aaaaaaaa bbbbbbbb cccccccc dddddddd eeeeeeee ffffffff gggggggg hhhhhhhh
             */
            val = 0;
            val |= TESTALL(0xff000000, high) ? 0x80 : 0;
            val |= TESTALL(0x00ff0000, high) ? 0x40 : 0;
            val |= TESTALL(0x0000ff00, high) ? 0x20 : 0;
            val |= TESTALL(0x000000ff, high) ? 0x10 : 0;
            val |= TESTALL(0xff000000, low) ? 0x08 : 0;
            val |= TESTALL(0x00ff0000, low) ? 0x04 : 0;
            val |= TESTALL(0x0000ff00, low) ? 0x02 : 0;
            val |= TESTALL(0x000000ff, low) ? 0x01 : 0;
        } else
            return false;
    } else
        return false;
    di->mod_imm_enc = (cmode << 8) | val;
    return true;
}

static bool
encode_VFP_modified_immed_ok(decode_info_t *di, opnd_size_t size_temp, opnd_t opnd)
{
    ptr_uint_t val; /* uint for bit manip w/o >> adding 1's */
    if (size_temp != OPSZ_1)
        return false;
    if (!opnd_is_immed_int(opnd) && !opnd_is_near_instr(opnd) && !opnd_is_near_pc(opnd))
        return false;
    val = (ptr_uint_t)get_immed_val_shared(di, opnd, false /*abs*/,
                                           false /*just checking*/);
    /* Xref VFPIMDExpandImm in the manual.
     * Check for each pattern, and store the encoding now to avoid re-doing
     * this work at real encode time.
     */
    if ((val & 0xfff80000) == val &&
        ((val & 0x7e000000) == 0x3e000000 || (val & 0x7e000000) == 0x40000000)) {
        /* aBbbbbbc defgh000 00000000 00000000 */
        val = ((val >> 24) & 0x80) | ((val >> 19) & 0x7f);
    } else if (opnd_is_immed_int64(opnd)) {
        /* aBbbbbbb bbcdefgh 00000000 00000000 00000000 00000000 00000000 00000000 */
        int64 val64 = opnd_get_immed_int64(opnd);
        int low = (int)val64;
        int high = (int)(val64 >> 32);
        if (low == 0 && (high & 0xffff0000) == high &&
            ((high & 0x7fc00000) == 0x3fc00000 || (high & 0x7fc00000) == 0x40000000)) {
            val = ((high >> 24) & 0x80) | ((val >> 16) & 0x7f);
        } else
            return false;
    } else
        return false;
    di->mod_imm_enc = val;
    return true;
}

static ptr_int_t
get_abspc_delta(decode_info_t *di, opnd_t opnd)
{
    /* For A32, "cur PC" is really "PC + 8"; "PC + 4" for Thumb, sometimes aligned */
    if (opnd_is_mem_instr(opnd)) {
        return (ptr_int_t)opnd_get_instr(opnd)->offset -
            (di->cur_offs + decode_cur_pc(di->final_pc, di->isa_mode, di->opcode, NULL) -
             di->final_pc) +
            opnd_get_mem_instr_disp(opnd);
    } else {
        CLIENT_ASSERT(opnd_is_rel_addr(opnd), "not an abspc type");
        return (ptr_int_t)opnd_get_addr(opnd) -
            (ptr_int_t)decode_cur_pc(di->final_pc, di->isa_mode, di->opcode, NULL);
    }
}

static bool
encode_abspc_ok(decode_info_t *di, opnd_size_t size_immed, opnd_t opnd, bool is_signed,
                bool negated, int scale)
{
    if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
        ptr_int_t delta = get_abspc_delta(di, opnd);
        bool res = false;
        if (negated) {
            res = (delta < 0 &&
                   (!di->check_reachable ||
                    encode_immed_ok(di, size_immed, -delta, scale, false /*unsigned*/,
                                    negated)));
        } else {
            res = (delta >= 0 &&
                   (!di->check_reachable ||
                    encode_immed_ok(di, size_immed, delta, scale, false /*unsigned*/,
                                    negated)));
        }
        if (res) {
            di->check_wb_base = DR_REG_PC;
            di->check_wb_disp_sz = size_immed;
            di->check_wb_disp = delta;
        }
        return res;
    }
    return false;
}

static int
opnd_get_signed_disp(opnd_t opnd)
{
    int disp = opnd_get_disp(opnd);
    if (TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)))
        return -disp;
    else
        return disp;
}

static bool
encode_opnd_ok(decode_info_t *di, byte optype, opnd_size_t size_temp, instr_t *in,
               bool is_dst, uint *counter INOUT)
{
    uint opnum = (*counter)++;
    opnd_t opnd;
    opnd_size_t size_temp_up = resolve_size_upward(size_temp);
    opnd_size_t size_temp_down = resolve_size_downward(size_temp);
    opnd_size_t size_op;

    /* Roll back greedy reglist if necessary */
    if (di->reglist_stop > 0 && optype_is_reg(optype) &&
        (!di->reglist_simd || !optype_is_gpr(optype)) &&
        di->reglist_stop - 1 > di->reglist_start &&
        di->reglist_stop - di->reglist_start > di->reglist_min_num &&
        di->reglist_stop == opnum) {
        if ((is_dst &&
             (opnum >= instr_num_dsts(in) || !opnd_is_reg(instr_get_dst(in, opnum)))) ||
            (!is_dst &&
             (opnum >= instr_num_srcs(in) || !opnd_is_reg(instr_get_src(in, opnum))))) {
            LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  reglist rollback from %d-%d\n",
                di->reglist_start, di->reglist_stop);
            CLIENT_ASSERT(*counter > 1, "non-empty reglist plus inc here -> >= 2");
            di->reglist_stop--;
            (*counter)--;
            opnum--;
            di->reglist_sz -= di->reglist_itemsz;
        }
    }

    if (optype == TYPE_R_A_EQ_D) {
        /* Does not correspond to an actual opnd in the instr_t */
        if (opnum == 0)
            return false;
        (*counter)--;
        opnum--;
    }

    if (optype == TYPE_NONE) {
        return (is_dst ? (instr_num_dsts(in) < opnum) : (instr_num_srcs(in) < opnum));
    } else if (is_dst) {
        if (opnum >= instr_num_dsts(in))
            return encode_simd_reglist_single_entry(di, optype, size_temp);
        opnd = instr_get_dst(in, opnum);
    } else {
        if (opnum >= instr_num_srcs(in))
            return encode_simd_reglist_single_entry(di, optype, size_temp);
        opnd = instr_get_src(in, opnum);
    }

    DOLOG(ENC_LEVEL, LOG_EMIT, {
        LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  %s %s %d %-15s ", __FUNCTION__,
            is_dst ? "dst" : "src", *counter - 1, type_names[optype]);
        opnd_disassemble(GLOBAL_DCONTEXT, opnd, THREAD_GET);
        LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "\n");
    });

    size_op = opnd_get_size(opnd);

    switch (optype) {

    /* Register types */
    /* For registers, we support requesting whole reg when only part is in template */
    case TYPE_R_B:
    case TYPE_R_C:
    case TYPE_R_A_TOP:
    case TYPE_R_B_TOP:
    case TYPE_R_C_TOP:
    case TYPE_R_D_TOP:
    case TYPE_R_U:
    case TYPE_R_V:
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down));
    case TYPE_R_W:
    case TYPE_R_X:
    case TYPE_R_Y:
    case TYPE_R_Z:
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                opnd_get_reg(opnd) <= DR_REG_R7 /* first 8-bit */ &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down));
    case TYPE_R_V_DUP:
    case TYPE_R_W_DUP:
    case TYPE_R_Z_DUP:
        /* Assume TYPE_R_*_DUP are always srcs and the 1st dst is the corresponding
         * non-dup type, checked in decode_check_reg_dup().
         */
        return opnd_same(opnd, instr_get_dst(in, 0));
    case TYPE_R_A:
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down) &&
                /* Ensure writeback matches memop base */
                (di->check_wb_base == DR_REG_NULL ||
                 di->check_wb_base == opnd_get_reg(opnd)));
    case TYPE_R_D:
    case TYPE_R_D_NEGATED:
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down) &&
                /* Ensure writeback index matches memop index */
                (di->check_wb_index == DR_REG_NULL ||
                 di->check_wb_index == opnd_get_reg(opnd)));
    case TYPE_R_B_EVEN:
    case TYPE_R_D_EVEN:
        CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down) &&
                ((dr_reg_fixer[opnd_get_reg(opnd)] - DR_REG_START_GPR) % 2 == 0));
    case TYPE_R_B_PLUS1:
    case TYPE_R_D_PLUS1: {
        opnd_t prior;
        if (opnum == 0)
            return false;
        if (is_dst)
            prior = instr_get_dst(in, opnum - 1);
        else
            prior = instr_get_src(in, opnum - 1);
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down) &&
                opnd_is_reg(prior) && opnd_get_reg(prior) + 1 == opnd_get_reg(opnd));
    }
    case TYPE_R_A_EQ_D:
        /* We already adjusted opnd to point at prior up above */
        return (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down));
    case TYPE_CR_A:
    case TYPE_CR_B:
    case TYPE_CR_C:
    case TYPE_CR_D:
        return (opnd_is_reg(opnd) && reg_is_cpreg(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down));
    case TYPE_V_A:
    case TYPE_V_B:
    case TYPE_V_C:
    case TYPE_W_A:
    case TYPE_W_B:
    case TYPE_W_C:
        return (opnd_is_reg(opnd) && reg_is_simd(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down));
    case TYPE_V_C_3b:
        return (opnd_is_reg(opnd) && reg_is_simd(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down) &&
                (opnd_get_reg(opnd) - reg_simd_start(opnd_get_reg(opnd)) < 8));
    case TYPE_V_C_4b:
        return (opnd_is_reg(opnd) && reg_is_simd(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down) &&
                (opnd_get_reg(opnd) - reg_simd_start(opnd_get_reg(opnd)) < 16));
    case TYPE_W_C_PLUS1: {
        opnd_t prior;
        if (opnum == 0)
            return false;
        if (is_dst)
            prior = instr_get_dst(in, opnum - 1);
        else
            prior = instr_get_src(in, opnum - 1);
        return (opnd_is_reg(opnd) && reg_is_simd(opnd_get_reg(opnd)) &&
                (size_op == size_temp || size_op == size_temp_up ||
                 size_op == size_temp_down) &&
                opnd_is_reg(prior) && opnd_get_reg(prior) + 1 == opnd_get_reg(opnd));
    }
    case TYPE_SPSR: return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_SPSR);
    case TYPE_CPSR: return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_CPSR);
    case TYPE_FPSCR: return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_FPSCR);
    case TYPE_LR: return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_LR);
    case TYPE_SP: return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_SP);
    case TYPE_PC: return (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_PC);

    /* Register lists */
    case TYPE_L_8b:
    case TYPE_L_9b_LR:
    case TYPE_L_9b_PC:
    case TYPE_L_16b_NO_SP:
    case TYPE_L_16b_NO_SP_PC:
    case TYPE_L_16b: {
        /* Strategy: first, we disallow any template with a reglist followed by more
         * than one plain register type (checked in decode_debug_checks_arch()).
         * Then, we greedily eat all regs here.  On a subsequent reg type, we remove
         * one entry from the list if necessary.  This is simpler than trying to look
         * ahead, or to disallow any reg after a reglist (that would lead to
         * wrong-order-vs-asm for OP_vtbl and others).
         */
        uint max_num = gpr_list_num_bits(optype);
        reg_id_t base_reg = REG_NULL;
        if (optype == TYPE_L_8b) {
            /* For T32.16 the base reg should appear either in the reglist or as
             * a writeback reg once and only once.
             */
            opnd_t memop = is_dst ? instr_get_src(in, 0) : instr_get_dst(in, 0);
            if (!opnd_is_base_disp(memop))
                return false;
            base_reg = opnd_get_base(memop);
        }
        if (!encode_reglist_ok(
                di, size_temp, in, is_dst, counter, 0, max_num, false /*gpr*/,
                0 /*no restrictions*/, 0,
                (optype == TYPE_L_16b_NO_SP || optype == TYPE_L_16b_NO_SP_PC)
                    ? DR_REG_SP
                    : DR_REG_NULL,
                (optype == TYPE_L_16b_NO_SP_PC) ? DR_REG_PC : DR_REG_NULL, base_reg))
            return false;
        /* We refuse to encode as an empty list ("unpredictable", and harder to ensure
         * encoding templates are distinguishable)
         */
        return (di->reglist_stop > di->reglist_start);
    }
    case TYPE_L_CONSEC: {
        uint max_num;
        opnd_t prior;
        if (opnum == 0)
            return false;
        if (size_temp_up == OPSZ_8)
            max_num = 16; /* max for 64-bit regs */
        else {
            CLIENT_ASSERT(size_temp_up == OPSZ_4, "invalid LC size");
            max_num = 32;
        }
        if (is_dst)
            prior = instr_get_dst(in, opnum - 1);
        else
            prior = instr_get_src(in, opnum - 1);
        if (!opnd_is_reg(prior) || !reg_is_simd(opnd_get_reg(prior)))
            return false;
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 0, max_num,
                               true /*simd*/, 1 /*consec*/, 1 /*prior entry*/,
                               DR_REG_NULL, DR_REG_NULL, DR_REG_NULL))
            return false;
        /* We have to allow an empty list b/c the template has the 1st entry */
        return true;
    }
    case TYPE_L_VAx2:
    case TYPE_L_VBx2:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 2, 2, true /*simd*/,
                               1 /*consec*/, 0, DR_REG_NULL, DR_REG_NULL, DR_REG_NULL))
            return false;
        return (di->reglist_stop > di->reglist_start);
    case TYPE_L_VAx3:
    case TYPE_L_VBx3:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 3, 3, true /*simd*/,
                               1 /*consec*/, 0, DR_REG_NULL, DR_REG_NULL, DR_REG_NULL))
            return false;
        return (di->reglist_stop > di->reglist_start);
    case TYPE_L_VAx4:
    case TYPE_L_VBx4:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 4, 4, true /*simd*/,
                               1 /*consec*/, 0, DR_REG_NULL, DR_REG_NULL, DR_REG_NULL))
            return false;
        return (di->reglist_stop > di->reglist_start);

    case TYPE_L_VBx2D:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 2, 2, true /*simd*/,
                               2 /*doubly-spaced*/, 0, DR_REG_NULL, DR_REG_NULL,
                               DR_REG_NULL))
            return false;
        return (di->reglist_stop > di->reglist_start);
    case TYPE_L_VBx3D:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 3, 3, true /*simd*/,
                               2 /*doubly-spaced*/, 0, DR_REG_NULL, DR_REG_NULL,
                               DR_REG_NULL))
            return false;
        return (di->reglist_stop > di->reglist_start);
    case TYPE_L_VBx4D:
        if (!encode_reglist_ok(di, size_temp, in, is_dst, counter, 4, 4, true /*simd*/,
                               2 /*doubly-spaced*/, 0, DR_REG_NULL, DR_REG_NULL,
                               DR_REG_NULL))
            return false;
        return (di->reglist_stop > di->reglist_start);

    /* Immeds */
    case TYPE_I_b0:
    case TYPE_I_b3:
    case TYPE_I_b4:
    case TYPE_I_b6:
    case TYPE_I_b5:
    case TYPE_I_b8:
    case TYPE_I_b9:
    case TYPE_I_b10:
    case TYPE_I_b16:
    case TYPE_I_b17:
    case TYPE_I_b18:
    case TYPE_I_b19:
    case TYPE_I_b20:
    case TYPE_I_b21:
    case TYPE_I_b0_b5:
    case TYPE_I_b4_b8:
    case TYPE_I_b4_b16:
    case TYPE_I_b5_b3:
    case TYPE_I_b8_b0:
    case TYPE_I_b8_b16:
    case TYPE_I_b16_b26_b12_b0:
    case TYPE_I_b21_b5:
    case TYPE_I_b21_b6:
    case TYPE_I_b26_b12_b0_z:
        return encode_immed_int_or_instr_ok(di, size_temp, 1, opnd, false /*unsigned*/,
                                            false /*pos*/, false /*abs*/, true /*range*/);
    case TYPE_NI_b0:
    case TYPE_NI_b8_b0:
        return (opnd_is_immed_int(opnd) &&
                encode_immed_ok(di, size_temp, -opnd_get_immed_int(opnd), 1,
                                false /*unsigned*/, true /*negated*/));
    case TYPE_I_x4_b0:
        return encode_immed_int_or_instr_ok(di, size_temp, 4, opnd, false /*unsigned*/,
                                            false /*pos*/, false /*abs*/, true /*range*/);
    case TYPE_NI_x4_b0:
        return (opnd_is_immed_int(opnd) &&
                encode_immed_ok(di, size_temp, -opnd_get_immed_int(opnd), 4,
                                false /*unsigned*/, true /*negated*/));
    case TYPE_I_b12_b6:
    case TYPE_I_b7:
        if (size_temp == OPSZ_5b && di->shift_has_type && di->shift_type_idx == opnum - 1)
            di->shift_uses_immed = true;
        /* Allow one bit larger for shifts of 32, and check actual values in
         * encode_shift_values()
         */
        if (encode_immed_int_or_instr_ok(di, di->shift_uses_immed ? OPSZ_6b : size_temp,
                                         1, opnd, false /*unsigned*/, false /*pos*/,
                                         false /*abs*/, true /*range*/)) {
            /* Ensure abstracted shift values, and writeback, are ok */
            if (di->shift_uses_immed) {
                /* Best to compare raw values in case one side is not abstracted */
                ptr_int_t sh2, val;
                if (opnd_is_instr(opnd))
                    return false; /* not supported */
                LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  checking shift: %d %d\n",
                    di->shift_type, opnd_get_immed_int(opnd));
                if (!encode_shift_values(di->shift_type, opnd_get_immed_int(opnd), &sh2,
                                         &val))
                    return false;
                if (di->check_wb_shift) {
                    /* Ensure writeback shift matches memop shift */
                    return (sh2 == di->check_wb_shift_type &&
                            val == di->check_wb_shift_amount);
                }
            }
            return true;
        }
        return false;
    case TYPE_I_SHIFTED_b0: return encode_A32_modified_immed_ok(di, size_temp, opnd);
    case TYPE_I_b16_b0:
        if (size_temp == OPSZ_1)
            return encode_VFP_modified_immed_ok(di, size_temp, opnd);
        else {
            return encode_immed_int_or_instr_ok(di, size_temp, 1, opnd,
                                                false /*unsigned*/, false /*pos*/,
                                                false /*abs*/, true /*range*/);
        }
    case TYPE_I_b26_b12_b0: return encode_T32_modified_immed_ok(di, size_temp, opnd);
    case TYPE_I_b8_b24_b16_b0:
    case TYPE_I_b8_b28_b16_b0: return encode_SIMD_modified_immed_ok(di, size_temp, opnd);
    case TYPE_SHIFT_b4:
    case TYPE_SHIFT_b5:
    case TYPE_SHIFT_b6:
    case TYPE_SHIFT_b21:
        if (opnd_is_immed_int(opnd) &&
            /* For OPSZ_1b, allow full DR_SHIFT_* values.  Allow the extras we've
             * added: simpler to just require OPSZ_3b here and check further below.
             */
            encode_immed_ok(di, OPSZ_3b, opnd_get_immed_int(opnd), 1, false /*unsigned*/,
                            false /*pos*/)) {
            ptr_int_t val = opnd_get_immed_int(opnd);
            if (val > DR_SHIFT_NONE)
                return false;
            if (optype == TYPE_SHIFT_b6 || optype == TYPE_SHIFT_b21) {
                di->shift_1bit = true;
                if (val % 2 != 0 && val != DR_SHIFT_NONE)
                    return false;
            }
            di->shift_has_type = true;
            di->shift_type_idx = opnum;
            /* Store the shift type for TYPE_I_b7/TYPE_I_b12_b6, here + in real encode */
            di->shift_type = opnd_get_immed_int(opnd);
            return true;
        }
        return false;
    case TYPE_J_b0:
        return encode_immed_int_or_instr_ok(di, size_temp, 2, opnd, true /*signed*/,
                                            false /*pos*/, true /*rel*/,
                                            di->check_reachable);
    case TYPE_J_x4_b0: /* OP_b, OP_bl */
        return encode_immed_int_or_instr_ok(di, size_temp, 4, opnd, true /*signed*/,
                                            false /*pos*/, true /*rel*/,
                                            di->check_reachable);
    case TYPE_J_b0_b24: /* OP_blx imm24:H:0 */
        return encode_immed_int_or_instr_ok(di, size_temp, 2, opnd, true /*signed*/,
                                            false /*pos*/, true /*rel*/,
                                            di->check_reachable);
    case TYPE_J_b9_b3:
        return encode_immed_int_or_instr_ok(di, size_temp, 2, opnd, false /*unsigned*/,
                                            false /*pos*/, true /*rel*/,
                                            di->check_reachable);
    case TYPE_J_b26_b11_b13_b16_b0: /* T32 OP_b w/ cond */
        return encode_immed_int_or_instr_ok(di, size_temp, 2, opnd, true /*signed*/,
                                            false /*pos*/, true /*rel*/,
                                            di->check_reachable);
    case TYPE_J_b26_b13_b11_b16_b0: /* T32 OP_b uncond */
        return encode_immed_int_or_instr_ok(di, size_temp, 2, opnd, true /*signed*/,
                                            false /*pos*/, true /*rel*/,
                                            di->check_reachable);
    case TYPE_SHIFT_LSL:
        return (opnd_is_immed_int(opnd) &&
                opnd_get_immed_int(opnd) == SHIFT_ENCODING_LSL);
    case TYPE_SHIFT_ASR:
        return (opnd_is_immed_int(opnd) &&
                opnd_get_immed_int(opnd) == SHIFT_ENCODING_ASR);
    case TYPE_K: return (opnd_is_immed_int(opnd) && size_op == OPSZ_0);

    /* Memory */
    case TYPE_M:
    case TYPE_M_SP:
        return (opnd_is_base_disp(opnd) && opnd_get_base(opnd) >= DR_REG_R0 &&
                !(optype == TYPE_M_SP && opnd_get_base(opnd) != DR_REG_SP) &&
                !(di->T32_16 && optype != TYPE_M_SP && opnd_get_base(opnd) > DR_REG_R7) &&
                opnd_get_index(opnd) == REG_NULL &&
                opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
                opnd_get_disp(opnd) == 0 &&
                /* We check for OPSZ_VAR_REGLIST but no reglist in check_reglist_size() */
                (size_op == size_temp || size_temp == OPSZ_VAR_REGLIST ||
                 size_op == OPSZ_VAR_REGLIST));
    case TYPE_M_POS_I12:
    case TYPE_M_NEG_I12:
        if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            (BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                         optype == TYPE_M_NEG_I12) ||
             opnd_get_disp(opnd) == 0) &&
            encode_immed_ok(di, OPSZ_12b, opnd_get_disp(opnd), 1, false /*unsigned*/,
                            TEST(DR_OPND_NEGATED, opnd_get_flags(opnd))) &&
            size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_disp_sz = OPSZ_12b;
            di->check_wb_disp = opnd_get_signed_disp(opnd);
            return true;
        } else {
            return encode_abspc_ok(di, OPSZ_12b, opnd, false /*unsigned*/,
                                   optype == TYPE_M_NEG_I12, 0 /*no scale*/);
        }
    case TYPE_M_POS_REG:
    case TYPE_M_NEG_REG:
        if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
            !(di->T32_16 && opnd_get_base(opnd) > DR_REG_R7) &&
            opnd_get_index(opnd) != REG_NULL &&
            !(di->T32_16 && opnd_get_index(opnd) > DR_REG_R7) &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                        optype == TYPE_M_NEG_REG) &&
            opnd_get_disp(opnd) == 0 && size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_index = opnd_get_index(opnd);
            return true;
        } else
            return false;
    case TYPE_M_POS_SHREG:
    case TYPE_M_NEG_SHREG:
        if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) != REG_NULL &&
            BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                        optype == TYPE_M_NEG_SHREG) &&
            opnd_get_disp(opnd) == 0 && size_op == size_temp) {
            ptr_int_t sh2, val;
            uint amount;
            dr_shift_type_t shift = opnd_get_index_shift(opnd, &amount);
            if (!encode_shift_values(shift, amount, &sh2, &val))
                return false;
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_index = opnd_get_index(opnd);
            di->check_wb_shift = true;
            di->check_wb_shift_type = sh2;
            di->check_wb_shift_amount = val;
            return true;
        } else
            return false;
    case TYPE_M_POS_LSHREG:
    case TYPE_M_POS_LSH1REG:
        if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) != REG_NULL &&
            !TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)) && opnd_get_disp(opnd) == 0 &&
            size_op == size_temp) {
            ptr_int_t sh2, val;
            uint amount;
            dr_shift_type_t shift = opnd_get_index_shift(opnd, &amount);
            if (optype == TYPE_M_POS_LSHREG) {
                if (shift != DR_SHIFT_LSL && shift != DR_SHIFT_NONE)
                    return false;
            } else if (shift != DR_SHIFT_LSL || amount != 1)
                return false;
            if (!encode_shift_values(shift, amount, &sh2, &val))
                return false;
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_index = opnd_get_index(opnd);
            di->check_wb_shift = true;
            di->check_wb_shift_type = sh2;
            di->check_wb_shift_amount = val;
            return true;
        } else
            return false;
    case TYPE_M_SI9:
        return (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
                opnd_get_index(opnd) == REG_NULL &&
                opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
                encode_immed_ok(di, OPSZ_9b, opnd_get_signed_disp(opnd), 1,
                                true /*signed*/, false /*pos*/) &&
                size_op == size_temp);
    case TYPE_M_SI7:
        return (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
                opnd_get_index(opnd) == REG_NULL &&
                opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
                encode_immed_ok(di, OPSZ_7b, opnd_get_signed_disp(opnd), 1,
                                true /*signed*/, false /*pos*/) &&
                size_op == size_temp);
    case TYPE_M_SP_POS_I8x4:
        /* no possibility of writeback, checked in decode_check_writeback() */
        return (opnd_is_base_disp(opnd) && opnd_get_base(opnd) == DR_REG_SP &&
                opnd_get_index(opnd) == REG_NULL &&
                opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
                !TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)) &&
                encode_immed_ok(di, OPSZ_1, opnd_get_disp(opnd), 4, false /*unsigned*/,
                                false /*pos*/) &&
                size_op == size_temp);
    case TYPE_M_POS_I8:
    case TYPE_M_NEG_I8:
        if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            (BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                         optype == TYPE_M_NEG_I8) ||
             opnd_get_disp(opnd) == 0) &&
            encode_immed_ok(di, OPSZ_1, opnd_get_disp(opnd), 1, false /*unsigned*/,
                            TEST(DR_OPND_NEGATED, opnd_get_flags(opnd))) &&
            size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_disp_sz = OPSZ_1;
            di->check_wb_disp = opnd_get_signed_disp(opnd);
            return true;
        } else {
            return encode_abspc_ok(di, OPSZ_1, opnd, false /*unsigned*/,
                                   optype == TYPE_M_NEG_I8, 0 /*no scale*/);
        }
    case TYPE_M_POS_I8x4:
    case TYPE_M_NEG_I8x4:
        if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            (BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                         optype == TYPE_M_NEG_I8x4) ||
             opnd_get_disp(opnd) == 0) &&
            encode_immed_ok(di, OPSZ_1, opnd_get_disp(opnd), 4, false /*unsigned*/,
                            TEST(DR_OPND_NEGATED, opnd_get_flags(opnd))) &&
            size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_disp_sz = OPSZ_1;
            di->check_wb_disp = opnd_get_signed_disp(opnd);
            return true;
        } else {
            return encode_abspc_ok(di, OPSZ_1, opnd, false /*unsigned*/,
                                   optype == TYPE_M_NEG_I8x4, 4 /*scale*/);
        }
    case TYPE_M_POS_I4_4:
    case TYPE_M_NEG_I4_4:
        if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            (BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                         optype == TYPE_M_NEG_I4_4) ||
             opnd_get_disp(opnd) == 0) &&
            encode_immed_ok(di, OPSZ_1, opnd_get_disp(opnd), 1, false /*unsigned*/,
                            TEST(DR_OPND_NEGATED, opnd_get_flags(opnd))) &&
            size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_disp_sz = OPSZ_1;
            di->check_wb_disp = opnd_get_signed_disp(opnd);
            return true;
        } else {
            return encode_abspc_ok(di, OPSZ_1, opnd, false /*unsigned*/,
                                   optype == TYPE_M_NEG_I4_4, 0 /*no scale*/);
        }
    case TYPE_M_POS_I5:
        if (opnd_is_base_disp(opnd) &&
            opnd_get_base(opnd) <= DR_REG_R7 /* T32.16 only */ &&
            opnd_get_base(opnd) != REG_NULL && opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            !TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)) &&
            encode_immed_ok(di, OPSZ_5b, opnd_get_disp(opnd), 1, false /*unsigned*/,
                            false /*pos*/) &&
            size_op == size_temp) {
            /* no writeback */
            return true;
        } else {
            return encode_abspc_ok(di, OPSZ_5b, opnd, false /*unsigned*/,
                                   false /*negated*/, 0 /*no scale*/);
        }
    case TYPE_M_POS_I5x2:
        if (opnd_is_base_disp(opnd) &&
            opnd_get_base(opnd) <= DR_REG_R7 /* T32.16 only */ &&
            opnd_get_base(opnd) != REG_NULL && opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            !TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)) &&
            encode_immed_ok(di, OPSZ_5b, opnd_get_disp(opnd), 2, false /*unsigned*/,
                            false /*pos*/) &&
            size_op == size_temp) {
            /* no writeback */
            return true;
        } else {
            return encode_abspc_ok(di, OPSZ_5b, opnd, false /*unsigned*/,
                                   false /*negated*/, 2 /*scale*/);
        }
    case TYPE_M_POS_I5x4:
        if (opnd_is_base_disp(opnd) &&
            opnd_get_base(opnd) <= DR_REG_R7 /* T32.16 only */ &&
            opnd_get_base(opnd) != REG_NULL && opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            !TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)) &&
            encode_immed_ok(di, OPSZ_5b, opnd_get_disp(opnd), 4, false /*unsigned*/,
                            false /*pos*/) &&
            size_op == size_temp) {
            /* no writeback */
            return true;
        } else {
            return encode_abspc_ok(di, OPSZ_5b, opnd, false /*unsigned*/,
                                   false /*negated*/, 4 /*scale*/);
        }
    case TYPE_M_PCREL_POS_I8x4:
        if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) == DR_REG_PC &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            !TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)) &&
            encode_immed_ok(di, OPSZ_1, opnd_get_disp(opnd), 4, false /*unsigned*/,
                            false /*pos*/) &&
            size_op == size_temp) {
            /* no writeback */
            return true;
        } else {
            return encode_abspc_ok(di, OPSZ_5b, opnd, false /*unsigned*/,
                                   false /*negated*/, 4 /*scale*/);
        }
        return false;
    case TYPE_M_PCREL_POS_I12:
    case TYPE_M_PCREL_NEG_I12:
        if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) == DR_REG_PC &&
            opnd_get_index(opnd) == REG_NULL &&
            opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
            (BOOLS_MATCH(TEST(DR_OPND_NEGATED, opnd_get_flags(opnd)),
                         optype == TYPE_M_PCREL_NEG_I12) ||
             opnd_get_disp(opnd) == 0) &&
            encode_immed_ok(di, OPSZ_12b, opnd_get_disp(opnd), 1, false /*unsigned*/,
                            TEST(DR_OPND_NEGATED, opnd_get_flags(opnd))) &&
            size_op == size_temp) {
            di->check_wb_base = opnd_get_base(opnd);
            di->check_wb_disp_sz = OPSZ_12b;
            di->check_wb_disp = opnd_get_signed_disp(opnd);
            return true;
        } else {
            return encode_abspc_ok(di, OPSZ_1, opnd, false /*unsigned*/,
                                   optype == TYPE_M_PCREL_NEG_I12, 0 /*no scale*/);
        }
    case TYPE_M_UP_OFFS:
    case TYPE_M_DOWN_OFFS:
    case TYPE_M_SP_DOWN_OFFS:
    case TYPE_M_DOWN:
        di->memop_sz = size_op;
        return (opnd_is_base_disp(opnd) && opnd_get_base(opnd) != REG_NULL &&
                (opnd_get_base(opnd) == DR_REG_SP || optype != TYPE_M_SP_DOWN_OFFS) &&
                opnd_get_index(opnd) == REG_NULL &&
                opnd_get_index_shift(opnd, NULL) == DR_SHIFT_NONE &&
                /* We check for OPSZ_VAR_REGLIST but no reglist in check_reglist_size() */
                (size_temp == OPSZ_VAR_REGLIST || size_op == OPSZ_VAR_REGLIST ||
                 (size_op == size_temp &&
                  ((optype == TYPE_M_UP_OFFS && opnd_get_disp(opnd) == sizeof(void *)) ||
                   ((optype == TYPE_M_DOWN_OFFS || optype == TYPE_M_SP_DOWN_OFFS) &&
                    opnd_get_disp(opnd) ==
                        -(int)opnd_size_in_bytes(size_op) * sizeof(void *)) ||
                   (optype == TYPE_M_DOWN &&
                    opnd_get_disp(opnd) ==
                        -(int)(opnd_size_in_bytes(size_op) - 1) * sizeof(void *))))));
    default: CLIENT_ASSERT(false, "encode-ok error: unknown operand type");
    }

    return false;
}

static void
decode_info_init_from_instr_info(decode_info_t *di, const instr_info_t *ii)
{
    di->T32_16 = (di->isa_mode == DR_ISA_ARM_THUMB && (ii->opcode & 0xffff0000) == 0);
    di->opcode = ii->type;
}

bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t *ii)
{
    uint num_dsts = 0, num_srcs = 0;
    dr_pred_type_t pred;

    if (ii == NULL || in == NULL)
        return false;
    pred = instr_get_predicate(in);

    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "%s 0x%08x\n", __FUNCTION__, ii->opcode);
    decode_info_init_from_instr_info(di, ii);

    if (encode_in_it_block(&di->encode_state, in) && di->check_reachable /*incl pred*/) {
        /* check if predicate match in IT block */
        if (pred !=
                it_block_instr_predicate(di->encode_state.itb_info,
                                         di->encode_state.itb_info.cur_instr) &&
            in->opcode != OP_bkpt /* bkpt is always executed */) {
            di->errmsg = "Predicate conflict with IT block";
            return false;
        }
    } else if (di->check_reachable /*incl pred*/) {
        /* Check predicate.  We're fine with DR_PRED_NONE == DR_PRED_AL. */
        if (pred == DR_PRED_OP) {
            di->errmsg = "DR_PRED_OP is an illegal predicate request";
            return false;
        } else if (TEST(DECODE_PREDICATE_28_AL, ii->flags)) {
            if (pred != DR_PRED_AL && pred != DR_PRED_NONE) {
                di->errmsg = "DR_PRED_AL is the only valid predicate";
                return false;
            }
        } else if (TESTANY(DECODE_PREDICATE_22 | DECODE_PREDICATE_8, ii->flags)) {
            if (pred == DR_PRED_AL || pred == DR_PRED_OP || pred == DR_PRED_NONE) {
                di->errmsg = "A predicate is required";
                return false;
            }
        } else if (!TESTANY(DECODE_PREDICATE_28 | DECODE_PREDICATE_22 |
                                DECODE_PREDICATE_8,
                            ii->flags)) {
            if (pred != DR_PRED_NONE) {
                di->errmsg = "No predicate is supported";
                return false;
            }
        } else if (pred != DR_PRED_NONE && in->opcode == OP_bkpt) {
            di->errmsg = "No predicate is allowed for bkpt instr";
            return false;
        }
    }

    /* Check each operand */
    do {
        if (ii->dst1_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->dst1_type, ii->dst1_size, in, true, &num_dsts)) {
                di->errmsg = "Destination operand #%d has wrong type/size";
                di->errmsg_param = num_dsts - 1;
                return false;
            }
        }
        if (ii->dst2_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->dst2_type, ii->dst2_size, in,
                                !TEST(DECODE_4_SRCS, ii->flags),
                                TEST(DECODE_4_SRCS, ii->flags) ? &num_srcs : &num_dsts)) {
                if (TEST(DECODE_4_SRCS, ii->flags)) {
                    di->errmsg = "Source operand #%d has wrong type/size";
                    di->errmsg_param = num_srcs - 1;
                } else {
                    di->errmsg = "Destination operand #%d has wrong type/size";
                    di->errmsg_param = num_dsts - 1;
                }
                return false;
            }
        }
        if (ii->src1_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->src1_type, ii->src1_size, in,
                                TEST(DECODE_3_DSTS, ii->flags),
                                TEST(DECODE_3_DSTS, ii->flags) ? &num_dsts : &num_srcs)) {
                if (TEST(DECODE_3_DSTS, ii->flags)) {
                    di->errmsg = "Destination operand #%d has wrong type/size";
                    di->errmsg_param = num_dsts - 1;
                } else {
                    di->errmsg = "Source operand #%d has wrong type/size";
                    di->errmsg_param = num_srcs - 1;
                }
                return false;
            }
        }
        if (ii->src2_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->src2_type, ii->src2_size, in, false, &num_srcs)) {
                di->errmsg = "Source operand #%d has wrong type/size";
                di->errmsg_param = num_srcs - 1;
                return false;
            }
        }
        if (ii->src3_type != TYPE_NONE) {
            if (!encode_opnd_ok(di, ii->src3_type, ii->src3_size, in, false, &num_srcs)) {
                di->errmsg = "Source operand #%d has wrong type/size";
                di->errmsg_param = num_srcs - 1;
                return false;
            }
        }
        ii = instr_info_extra_opnds(ii);
    } while (ii != NULL);

    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "  checking %d vs %d, %d vs %d\n", num_dsts,
        instr_num_dsts(in), num_srcs, instr_num_srcs(in));
    if (num_dsts < instr_num_dsts(in) || num_srcs < instr_num_srcs(in))
        return false;

    return check_reglist_size(di);
}

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr)
{
    memset(di, 0, sizeof(*di));
    di->isa_mode = instr_get_isa_mode(instr);
}

static void
encode_regA(decode_info_t *di, reg_id_t reg)
{
    /* A32 = 19:16 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 16;
}

static void
encode_regB(decode_info_t *di, reg_id_t reg)
{
    /* A32 = 15:12 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 12;
}

static void
encode_regC(decode_info_t *di, reg_id_t reg)
{
    /* A32 = 11:8 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 8;
}

static void
encode_regD(decode_info_t *di, reg_id_t reg)
{
    /* A32 = 3:0 */
    di->instr_word |= (reg - DR_REG_START_GPR);
}

static void
encode_regU(decode_info_t *di, reg_id_t reg)
{
    /* T32.16 = 6:3 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 3;
}

static void
encode_regV(decode_info_t *di, reg_id_t reg)
{
    /* T32.16 = 7, 2:0 */
    uint reg_bit = reg - DR_REG_START_GPR;
    if (reg > DR_REG_R7)
        reg_bit = (0x1 << 7) | (reg_bit & 0x7);
    di->instr_word |= reg_bit;
}

static void
encode_regW(decode_info_t *di, reg_id_t reg)
{
    /* T32.16 = 10:8 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 8;
}

static void
encode_regX(decode_info_t *di, reg_id_t reg)
{
    /* T32.16 = 8:6 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 6;
}

static void
encode_regY(decode_info_t *di, reg_id_t reg)
{
    /* T32.16 = 5:3 */
    di->instr_word |= (reg - DR_REG_START_GPR) << 3;
}

static void
encode_regZ(decode_info_t *di, reg_id_t reg)
{
    /* T32.16 = 2:0 */
    di->instr_word |= (reg - DR_REG_START_GPR);
}

static void
encode_immed(decode_info_t *di, uint start_bit, opnd_size_t size_temp, ptr_int_t val,
             bool is_signed)
{
    uint bits = opnd_size_in_bits(size_temp);
    di->instr_word |= (val & ((1 << bits) - 1)) << start_bit;
}

static void
encode_index_shift(decode_info_t *di, opnd_t opnd, bool encode_type)
{
    ptr_int_t sh2, val;
    uint amount;
    dr_shift_type_t shift = opnd_get_index_shift(opnd, &amount);
    if (!encode_shift_values(shift, amount, &sh2, &val)) {
        CLIENT_ASSERT(false, "internal encoding error");
        val = sh2 = 0;
    }
    if (di->isa_mode == DR_ISA_ARM_A32) {
        if (encode_type) {
            encode_immed(di, DECODE_INDEX_SHIFT_TYPE_BITPOS_A32,
                         DECODE_INDEX_SHIFT_TYPE_SIZE, sh2, false);
        }
        encode_immed(di, DECODE_INDEX_SHIFT_AMOUNT_BITPOS_A32,
                     DECODE_INDEX_SHIFT_AMOUNT_SIZE_A32, val, false);
    } else if (di->isa_mode == DR_ISA_ARM_THUMB) {
        ASSERT(!encode_type);
        encode_immed(di, DECODE_INDEX_SHIFT_AMOUNT_BITPOS_T32,
                     DECODE_INDEX_SHIFT_AMOUNT_SIZE_T32, val, false);
    } else
        CLIENT_ASSERT(false, "mode not supported");
}

static void
encode_operand(decode_info_t *di, byte optype, opnd_size_t size_temp, instr_t *in,
               bool is_dst, uint *counter INOUT)
{
    uint opnum = (*counter)++;
    opnd_size_t size_temp_up = resolve_size_upward(size_temp);
    opnd_t opnd;
    if (optype == TYPE_R_A_EQ_D) {
        /* Does not correspond to an actual opnd in the instr_t */
        (*counter)--;
        opnum--;
    }
    if (optype != TYPE_NONE) {
        if (is_dst) {
            if (opnum >= instr_num_dsts(in)) {
                CLIENT_ASSERT(optype == TYPE_L_CONSEC, "only SIMD list can exceed opnds");
                opnd = opnd_create_null();
            } else
                opnd = instr_get_dst(in, opnum);
        } else {
            if (opnum >= instr_num_srcs(in)) {
                CLIENT_ASSERT(optype == TYPE_L_CONSEC, "only SIMD list can exceed opnds");
                opnd = opnd_create_null();
            } else
                opnd = instr_get_src(in, opnum);
        }
    }

    switch (optype) {
    /* Registers */
    case TYPE_R_A:
    case TYPE_R_A_TOP:
    case TYPE_R_A_EQ_D: encode_regA(di, opnd_get_reg(opnd)); break;
    case TYPE_R_B:
    case TYPE_R_B_TOP:
    case TYPE_R_B_EVEN: encode_regB(di, opnd_get_reg(opnd)); break;
    case TYPE_R_C:
    case TYPE_R_C_TOP: encode_regC(di, opnd_get_reg(opnd)); break;
    case TYPE_R_D:
    case TYPE_R_D_TOP:
    case TYPE_R_D_NEGATED:
    case TYPE_R_D_EVEN: encode_regD(di, opnd_get_reg(opnd)); break;
    case TYPE_R_U: encode_regU(di, opnd_get_reg(opnd)); break;
    case TYPE_R_V: encode_regV(di, opnd_get_reg(opnd)); break;
    case TYPE_R_W: encode_regW(di, opnd_get_reg(opnd)); break;
    case TYPE_R_X: encode_regX(di, opnd_get_reg(opnd)); break;
    case TYPE_R_Y: encode_regY(di, opnd_get_reg(opnd)); break;
    case TYPE_R_Z: encode_regZ(di, opnd_get_reg(opnd)); break;
    case TYPE_R_V_DUP:
    case TYPE_R_W_DUP:
    case TYPE_R_Z_DUP:
        /* do nothing as the encoding is done by TYPE_R_V/W/Z */
        break;
    case TYPE_CR_A:
        encode_regA(di, opnd_get_reg(opnd) - DR_REG_CR0 + DR_REG_START_GPR);
        break;
    case TYPE_CR_B:
        encode_regB(di, opnd_get_reg(opnd) - DR_REG_CR0 + DR_REG_START_GPR);
        break;
    case TYPE_CR_C:
        encode_regC(di, opnd_get_reg(opnd) - DR_REG_CR0 + DR_REG_START_GPR);
        break;
    case TYPE_CR_D:
        encode_regD(di, opnd_get_reg(opnd) - DR_REG_CR0 + DR_REG_START_GPR);
        break;
    case TYPE_V_A:
    case TYPE_L_VAx2:
    case TYPE_L_VAx3:
    case TYPE_L_VAx4: {
        /* A32 = 7,19:16, but for Q regs 7,19:17 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31)
            di->instr_word |= ((val & 0x8) << 4) | ((val & 0x7) << 17);
        else
            di->instr_word |= ((val & 0x10) << 3) | ((val & 0xf) << 16);
        if (di->reglist_stop > 0)
            (*counter) += (di->reglist_stop - 1 - di->reglist_start);
        break;
    }
    case TYPE_V_B:
    case TYPE_L_VBx2:
    case TYPE_L_VBx3:
    case TYPE_L_VBx4:
    case TYPE_L_VBx2D:
    case TYPE_L_VBx3D:
    case TYPE_L_VBx4D: {
        /* A32 = 22,15:12, but for Q regs 22,15:13 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31)
            di->instr_word |= ((val & 0x8) << 19) | ((val & 0x7) << 13);
        else
            di->instr_word |= ((val & 0x10) << 18) | ((val & 0xf) << 12);
        if (optype != TYPE_V_B && di->reglist_stop > 0)
            (*counter) += (di->reglist_stop - 1 - di->reglist_start);
        break;
    }
    case TYPE_V_C: {
        /* A32 = 5,3:0, but for Q regs 5,3:1 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31)
            di->instr_word |= ((val & 0x8) << 2) | ((val & 0x7) << 1);
        else
            di->instr_word |= ((val & 0x10) << 1) | (val & 0xf);
        break;
    }
    case TYPE_W_A: {
        /* A32 = 19:16,7 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= ((val & 0x1e) << 15) | ((val & 0x1) << 7);
        break;
    }
    case TYPE_W_B: {
        /* A32 = 15:12,22 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= ((val & 0x1e) << 11) | ((val & 0x1) << 22);
        break;
    }
    case TYPE_W_C: {
        /* A32 = 3:0,5 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= ((val & 0x1e) >> 1) | ((val & 0x1) << 5);
        break;
    }
    case TYPE_V_C_3b: {
        /* A32 = 2:0 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= (val & 0x7);
        break;
    }
    case TYPE_V_C_4b: {
        /* A32 = 3:0 */
        reg_id_t reg = opnd_get_reg(opnd);
        uint val = reg - reg_simd_start(reg);
        di->instr_word |= (val & 0xf);
        break;
    }

    /* Register lists */
    case TYPE_L_8b:
    case TYPE_L_9b_LR:
    case TYPE_L_9b_PC:
    case TYPE_L_16b_NO_SP:
    case TYPE_L_16b_NO_SP_PC:
    case TYPE_L_16b: {
        uint i;
        CLIENT_ASSERT(di->reglist_start == *counter - 1, "internal reglist encode error");
        for (i = di->reglist_start; i < di->reglist_stop; i++) {
            opnd = is_dst ? instr_get_dst(in, i) : instr_get_src(in, i);
            if ((optype == TYPE_L_9b_LR && opnd_get_reg(opnd) == DR_REG_LR) ||
                (optype == TYPE_L_9b_PC && opnd_get_reg(opnd) == DR_REG_PC)) {
                di->instr_word |= 1 << 8;
            } else {
                di->instr_word |= 1 << (opnd_get_reg(opnd) - DR_REG_START_GPR);
            }
        }
        /* already incremented once */
        (*counter) += (di->reglist_stop - 1 - di->reglist_start);
        break;
    }
    case TYPE_L_CONSEC: {
        /* Consecutive multimedia regs: dword count in immed 7:0 */
        uint dwords = 1 /*in template*/ + di->reglist_stop - di->reglist_start;
        if (size_temp_up == OPSZ_8)
            dwords *= 2;
        else
            CLIENT_ASSERT(size_temp_up == OPSZ_4, "invalid LC size");
        di->instr_word |= dwords;
        if (di->reglist_stop > di->reglist_start)
            (*counter) += (di->reglist_stop - 1 - di->reglist_start);
        else if (di->reglist_stop == di->reglist_start)
            (*counter)--;
        break;
    }

    /* Immeds */
    case TYPE_I_b0:
        encode_immed(di, 0, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_x4_b0:
        encode_immed(di, 0, size_temp, get_immed_val_abs(di, opnd) >> 2,
                     false /*unsigned*/);
        break;
    case TYPE_I_SHIFTED_b0:
        if (size_temp == OPSZ_12b) {
            /* encode_A32_modified_immed_ok stored the encoded value for us */
            ptr_int_t val = di->mod_imm_enc;
            encode_immed(di, 0, OPSZ_12b, val, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported shifted-12 immed size");
        break;
    case TYPE_NI_b0:
        encode_immed(di, 0, size_temp, -get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_NI_x4_b0:
        encode_immed(di, 0, size_temp, -get_immed_val_abs(di, opnd) / 4,
                     false /*unsigned*/);
        break;
    case TYPE_I_b3:
        encode_immed(di, 3, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b4:
        encode_immed(di, 4, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b5:
        encode_immed(di, 5, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b6:
        encode_immed(di, 6, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b7:
        if (size_temp == OPSZ_5b && di->shift_has_type &&
            di->shift_type_idx == opnum - 1 && di->shift_uses_immed) {
            /* Convert to raw values */
            ptr_int_t sh2, val;
            if (!encode_shift_values(di->shift_type, opnd_get_immed_int(opnd), &sh2,
                                     &val)) {
                CLIENT_ASSERT(false, "internal encoding error");
                val = sh2 = 0;
            }
            if (di->shift_1bit)
                encode_immed(di, 6, OPSZ_1b, sh2 >> 1, false /*unsigned*/);
            else
                encode_immed(di, 5, OPSZ_2b, sh2, false /*unsigned*/);
            encode_immed(di, 7, size_temp, val, false /*unsigned*/);
        } else {
            encode_immed(di, 7, size_temp, get_immed_val_abs(di, opnd),
                         false /*unsigned*/);
        }
        break;
    case TYPE_I_b8:
        encode_immed(di, 8, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b9:
        encode_immed(di, 9, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b10:
        encode_immed(di, 10, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b16:
        encode_immed(di, 16, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b17:
        encode_immed(di, 17, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b18:
        encode_immed(di, 18, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b19:
        encode_immed(di, 19, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b20:
        encode_immed(di, 20, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b21:
        encode_immed(di, 21, size_temp, get_immed_val_abs(di, opnd), false /*unsigned*/);
        break;
    case TYPE_I_b0_b5: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_5b) {
            encode_immed(di, 5, OPSZ_1b, val, false /*unsigned*/);
            encode_immed(di, 0, OPSZ_4b, val >> 1, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 0-5 split immed size");
        break;
    }
    case TYPE_I_b4_b8: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_5b) {
            encode_immed(di, 8, OPSZ_4b, val, false /*unsigned*/);
            encode_immed(di, 4, OPSZ_1b, val >> 4, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 4-8 split immed size");
        break;
    }
    case TYPE_I_b4_b16: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_5b) {
            encode_immed(di, 16, OPSZ_4b, val, false /*unsigned*/);
            encode_immed(di, 4, OPSZ_1b, val >> 4, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 4-16 split immed size");
        break;
    }
    case TYPE_I_b5_b3: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_2b) {
            encode_immed(di, 3, OPSZ_1b, val, false /*unsigned*/);
            encode_immed(di, 5, OPSZ_1b, val >> 1, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 5-3 immed size");
        break;
    }
    case TYPE_NI_b8_b0:
    case TYPE_I_b8_b0: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (optype == TYPE_NI_b8_b0)
            val = -val;
        if (size_temp == OPSZ_2) {
            encode_immed(di, 0, OPSZ_4b, val, false /*unsigned*/);
            encode_immed(di, 8, OPSZ_12b, val >> 4, false /*unsigned*/);
        } else if (size_temp == OPSZ_1) {
            encode_immed(di, 0, OPSZ_4b, val, false /*unsigned*/);
            encode_immed(di, 8, OPSZ_4b, val >> 4, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 8-0 split immed size");
        break;
    }
    case TYPE_I_b8_b16: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_5b) {
            encode_immed(di, 16, OPSZ_4b, val, false /*unsigned*/);
            encode_immed(di, 8, OPSZ_1b, val >> 4, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 8-16 split immed size");
        break;
    }
    case TYPE_I_b12_b6: {
        ptr_int_t val = 0;
        if (size_temp == OPSZ_5b && di->shift_has_type &&
            di->shift_type_idx == opnum - 1 && di->shift_uses_immed) {
            /* Convert to raw values */
            ptr_int_t sh2;
            if (!encode_shift_values(di->shift_type, opnd_get_immed_int(opnd), &sh2,
                                     &val)) {
                CLIENT_ASSERT(false, "internal encoding error");
                val = sh2 = 0;
            }
            if (di->shift_1bit)
                encode_immed(di, 21, OPSZ_1b, sh2 >> 1, false /*unsigned*/);
            else
                encode_immed(di, 4, OPSZ_2b, sh2, false /*unsigned*/);
        } else
            val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_5b) {
            encode_immed(di, 6, OPSZ_2b, val, false /*unsigned*/);
            encode_immed(di, 12, OPSZ_3b, val >> 2, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 12-6 split immed size");
        break;
    }
    case TYPE_I_b16_b0: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_2) {
            encode_immed(di, 0, OPSZ_12b, val, false /*unsigned*/);
            encode_immed(di, 16, OPSZ_4b, val >> 12, false /*unsigned*/);
        } else if (size_temp == OPSZ_1) {
            /* encode_VFP_modified_immed_ok stored the encoded value for us */
            val = di->mod_imm_enc;
            encode_immed(di, 0, OPSZ_4b, val, false /*unsigned*/);
            encode_immed(di, 16, OPSZ_4b, val >> 4, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 16-0 split immed size");
        break;
    }
    case TYPE_I_b16_b26_b12_b0: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_2) {
            encode_immed(di, 0, OPSZ_1, val, false /*unsigned*/);
            encode_immed(di, 12, OPSZ_3b, val >> 8, false /*unsigned*/);
            encode_immed(di, 26, OPSZ_1b, val >> 11, false /*unsigned*/);
            encode_immed(di, 16, OPSZ_4b, val >> 12, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 16-26-12-0 split immed size");
        break;
    }
    case TYPE_I_b21_b5: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_3b) {
            encode_immed(di, 5, OPSZ_2b, val, false /*unsigned*/);
            encode_immed(di, 21, OPSZ_1b, val >> 2, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 21-5 split immed size");
        break;
    }
    case TYPE_I_b21_b6: {
        ptr_int_t val = get_immed_val_abs(di, opnd);
        if (size_temp == OPSZ_2b) {
            encode_immed(di, 6, OPSZ_1b, val, false /*unsigned*/);
            encode_immed(di, 21, OPSZ_1b, val >> 1, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 21-6 split immed size");
        break;
    }
    case TYPE_I_b8_b24_b16_b0:
    case TYPE_I_b8_b28_b16_b0: {
        if (size_temp == OPSZ_12b) {
            /* encode_SIMD_modified_immed_ok stored the encoded value for us */
            ptr_int_t val = di->mod_imm_enc;
            encode_immed(di, 0, OPSZ_4b, val, false /*unsigned*/);
            encode_immed(di, 16, OPSZ_3b, val >> 4, false /*unsigned*/);
            encode_immed(di, (optype == TYPE_I_b8_b28_b16_b0) ? 28 : 24, OPSZ_1b,
                         val >> 7, false /*unsigned*/);
            /* This is "cmode".  It overlaps with some opcode-defining bits (for _size
             * suffixes) but since we OR only and never clear it works out.
             */
            encode_immed(di, 8, OPSZ_4b, val >> 8, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 24-16-0 split immed size");
        break;
    }
    case TYPE_I_b26_b12_b0:
    case TYPE_I_b26_b12_b0_z:
        if (size_temp == OPSZ_12b) {
            /* encode_T32_modified_immed_ok stored the encoded value for us */
            ptr_int_t val;
            if (optype == TYPE_I_b26_b12_b0)
                val = di->mod_imm_enc;
            else
                val = get_immed_val_abs(di, opnd);
            encode_immed(di, 0, OPSZ_1, val, false /*unsigned*/);
            encode_immed(di, 12, OPSZ_3b, val >> 8, false /*unsigned*/);
            encode_immed(di, 26, OPSZ_1b, val >> 11, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 24-16-0 split immed size");
        break;
    case TYPE_J_b0:
        encode_immed(di, 0, size_temp, get_immed_val_rel(di, opnd) >> 1, true /*signed*/);
        break;
    case TYPE_J_x4_b0:
        encode_immed(di, 0, size_temp, get_immed_val_rel(di, opnd) >> 2, true /*signed*/);
        break;
    case TYPE_J_b0_b24: { /* OP_blx imm24:H:0 */
        ptr_int_t val = get_immed_val_rel(di, opnd);
        if (size_temp == OPSZ_25b) {
            encode_immed(di, 24, OPSZ_1b, val >> 1, false /*unsigned*/);
            encode_immed(di, 0, OPSZ_3, val >> 2, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 0-24 split immed size");
        break;
    }
    case TYPE_J_b9_b3: { /* OP_cb{n}z, ZeroExtend(i:imm5:0), [9,7:3]:0 */
        ptr_int_t val = get_immed_val_rel(di, opnd);
        encode_immed(di, 3, OPSZ_5b, val >> 1, false /*unsigned*/);
        encode_immed(di, 9, OPSZ_1b, val >> 6 /*5+1*/, false /*unsigned*/);
        break;
    }
    case TYPE_J_b26_b11_b13_b16_b0: { /* T32 OP_b w/ cond */
        ptr_int_t val = get_immed_val_rel(di, opnd);
        if (size_temp == OPSZ_20b) {
            encode_immed(di, 0, OPSZ_11b, val >> 1, false /*unsigned*/);
            encode_immed(di, 16, OPSZ_6b, val >> 12, false /*unsigned*/);
            encode_immed(di, 13, OPSZ_1b, val >> 18, false /*unsigned*/);
            encode_immed(di, 11, OPSZ_1b, val >> 19, false /*unsigned*/);
            encode_immed(di, 26, OPSZ_1b, val >> 20, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 0-24 split immed size");
        break;
    }
    case TYPE_J_b26_b13_b11_b16_b0: { /* T32 OP_b uncond */
        ptr_int_t val = get_immed_val_rel(di, opnd);
        if (size_temp == OPSZ_3) {
            /* 26,13,11,25:16,10:0 x2, but bits 13 and 11 are flipped if bit 26 is 0 */
            uint bit26 = (val >> 24) & 0x1; /* +1 for the x2 */
            uint bit13 = (val >> 23) & 0x1;
            uint bit11 = (val >> 22) & 0x1;
            if (bit26 == 0) {
                bit13 = (bit13 == 0 ? 1 : 0);
                bit11 = (bit11 == 0 ? 1 : 0);
            }
            encode_immed(di, 0, OPSZ_11b, val >> 1, false /*unsigned*/);
            encode_immed(di, 16, OPSZ_10b, val >> 12, false /*unsigned*/);
            encode_immed(di, 13, OPSZ_1b, bit13, false /*unsigned*/);
            encode_immed(di, 11, OPSZ_1b, bit11, false /*unsigned*/);
            encode_immed(di, 26, OPSZ_1b, bit26, false /*unsigned*/);
        } else
            CLIENT_ASSERT(false, "unsupported 0-24 split immed size");
        break;
    }
    case TYPE_SHIFT_b4:
        if (!di->shift_uses_immed) /* encoded in TYPE_I_b12_b6 */
            encode_immed(di, 4, size_temp, opnd_get_immed_int(opnd), false /*unsigned*/);
        break;
    case TYPE_SHIFT_b5:
        if (!di->shift_uses_immed) /* encoded in TYPE_I_b7 */
            encode_immed(di, 5, size_temp, opnd_get_immed_int(opnd), false /*unsigned*/);
        break;
    case TYPE_SHIFT_b6:
        if (!di->shift_uses_immed) { /* encoded in TYPE_I_b7 */
            encode_immed(di, 5, size_temp, opnd_get_immed_int(opnd) << 1,
                         false /*unsigned*/);
        }
        break;
    case TYPE_SHIFT_b21:
        if (!di->shift_uses_immed) { /* encoded in TYPE_I_b12_b6 */
            encode_immed(di, 21, size_temp, opnd_get_immed_int(opnd) << 1,
                         false /*unsigned*/);
        }
        break;

    /* Memory */
    case TYPE_M:
        if (di->T32_16)
            encode_regW(di, opnd_get_base(opnd));
        else
            encode_regA(di, opnd_get_base(opnd));
        break;
    case TYPE_M_UP_OFFS:
    case TYPE_M_DOWN:
    case TYPE_M_DOWN_OFFS: encode_regA(di, opnd_get_base(opnd)); break;
    case TYPE_M_SP:
    case TYPE_M_SP_DOWN_OFFS:
        /* do nothing */
        break;
    case TYPE_M_POS_I12:
    case TYPE_M_NEG_I12:
        if (opnd_is_base_disp(opnd)) {
            encode_regA(di, opnd_get_base(opnd));
            encode_immed(di, 0, OPSZ_12b, opnd_get_disp(opnd), false /*unsigned*/);
        } else if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
            ptr_int_t delta = get_abspc_delta(di, opnd);
            encode_regA(di, DR_REG_PC);
            encode_immed(di, 0, OPSZ_12b, delta < 0 ? -delta : delta, false /*unsigned*/);
        }
        break;
    case TYPE_M_POS_REG:
    case TYPE_M_NEG_REG:
        if (di->T32_16) {
            encode_regY(di, opnd_get_base(opnd));
            encode_regX(di, opnd_get_index(opnd));
        } else {
            encode_regA(di, opnd_get_base(opnd));
            encode_regD(di, opnd_get_index(opnd));
        }
        break;
    case TYPE_M_POS_SHREG:
    case TYPE_M_NEG_SHREG:
        encode_regA(di, opnd_get_base(opnd));
        encode_regD(di, opnd_get_index(opnd));
        encode_index_shift(di, opnd, true);
        break;
    case TYPE_M_POS_LSHREG:
        encode_regA(di, opnd_get_base(opnd));
        encode_regD(di, opnd_get_index(opnd));
        /* This shift is at 5:4, unlike the regular shifts */
        encode_index_shift(di, opnd, false /*type implicit*/);
        break;
    case TYPE_M_POS_LSH1REG:
        encode_regA(di, opnd_get_base(opnd));
        encode_regD(di, opnd_get_index(opnd));
        /* both shift type and amount are implicit */
        break;
    case TYPE_M_SI9:
        encode_regA(di, opnd_get_base(opnd));
        encode_immed(di, 12, OPSZ_9b, opnd_get_signed_disp(opnd), true /*signed*/);
        break;
    case TYPE_M_SI7:
        encode_regA(di, opnd_get_base(opnd));
        encode_immed(di, 0, OPSZ_7b, opnd_get_signed_disp(opnd), true /*signed*/);
        break;
    case TYPE_M_SP_POS_I8x4:
        encode_immed(di, 0, OPSZ_1, opnd_get_disp(opnd) / 4, false /*unsigned*/);
        break;
    case TYPE_M_POS_I8:
    case TYPE_M_NEG_I8:
        if (opnd_is_base_disp(opnd)) {
            encode_regA(di, opnd_get_base(opnd));
            encode_immed(di, 0, OPSZ_1, opnd_get_disp(opnd), false /*unsigned*/);
        } else if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
            ptr_int_t delta = get_abspc_delta(di, opnd);
            encode_regA(di, DR_REG_PC);
            encode_immed(di, 0, OPSZ_1, delta < 0 ? -delta : delta, false /*unsigned*/);
        }
        break;
    case TYPE_M_POS_I8x4:
    case TYPE_M_NEG_I8x4:
        if (opnd_is_base_disp(opnd)) {
            encode_regA(di, opnd_get_base(opnd));
            encode_immed(di, 0, OPSZ_1, opnd_get_disp(opnd) / 4, false /*unsigned*/);
        } else if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
            ptr_int_t delta = get_abspc_delta(di, opnd) / 4;
            encode_regA(di, DR_REG_PC);
            encode_immed(di, 0, OPSZ_1, delta < 0 ? -delta : delta, false /*unsigned*/);
        }
        break;
    case TYPE_M_POS_I4_4:
    case TYPE_M_NEG_I4_4:
        if (opnd_is_base_disp(opnd)) {
            encode_regA(di, opnd_get_base(opnd));
            encode_immed(di, 0, OPSZ_4b, opnd_get_disp(opnd), false /*unsigned*/);
            encode_immed(di, 8, OPSZ_4b, opnd_get_disp(opnd) >> 4, false /*unsigned*/);
        } else if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
            ptr_int_t delta = get_abspc_delta(di, opnd);
            encode_regA(di, DR_REG_PC);
            encode_immed(di, 0, OPSZ_4b, delta < 0 ? -delta : delta, false /*unsigned*/);
            encode_immed(di, 8, OPSZ_4b, (delta < 0 ? -delta : delta) >> 4,
                         false /*unsigned*/);
        }
        break;
    case TYPE_M_POS_I5:
        CLIENT_ASSERT(di->T32_16, "supported in T32.16 only");
        if (opnd_is_base_disp(opnd)) {
            encode_regY(di, opnd_get_base(opnd));
            encode_immed(di, 6, OPSZ_5b, opnd_get_disp(opnd), false /*unsigned*/);
        } else if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
            ptr_int_t delta = get_abspc_delta(di, opnd);
            encode_regY(di, DR_REG_PC);
            encode_immed(di, 6, OPSZ_5b, delta, false /*unsigned*/);
        }
        break;
    case TYPE_M_POS_I5x2:
        CLIENT_ASSERT(di->T32_16, "supported in T32.16 only");
        if (opnd_is_base_disp(opnd)) {
            encode_regY(di, opnd_get_base(opnd));
            encode_immed(di, 6, OPSZ_5b, opnd_get_disp(opnd) / 2, false /*unsigned*/);
        } else if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
            ptr_int_t delta = get_abspc_delta(di, opnd);
            encode_regY(di, DR_REG_PC);
            encode_immed(di, 6, OPSZ_5b, delta / 2, false /*unsigned*/);
        }
        break;
    case TYPE_M_POS_I5x4:
        CLIENT_ASSERT(di->T32_16, "supported in T32.16 only");
        if (opnd_is_base_disp(opnd)) {
            encode_regY(di, opnd_get_base(opnd));
            encode_immed(di, 6, OPSZ_5b, opnd_get_disp(opnd) / 4, false /*unsigned*/);
        } else if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
            ptr_int_t delta = get_abspc_delta(di, opnd);
            encode_regY(di, DR_REG_PC);
            encode_immed(di, 6, OPSZ_5b, delta / 4, false /*unsigned*/);
        }
        break;
    case TYPE_M_PCREL_POS_I8x4:
        if (opnd_is_base_disp(opnd)) {
            /* base is implied as PC */
            encode_immed(di, 0, OPSZ_1, opnd_get_disp(opnd) / 4, false /*unsigned*/);
        } else if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
            ptr_int_t delta = get_abspc_delta(di, opnd);
            encode_immed(di, 0, OPSZ_1, delta / 4, false /*unsigned*/);
        }
        break;
    case TYPE_M_PCREL_POS_I12:
    case TYPE_M_PCREL_NEG_I12:
        if (opnd_is_base_disp(opnd)) {
            /* base is implied as PC */
            encode_immed(di, 0, OPSZ_12b, opnd_get_disp(opnd), false /*unsigned*/);
        } else if (opnd_is_mem_instr(opnd) || opnd_is_rel_addr(opnd)) {
            ptr_int_t delta = get_abspc_delta(di, opnd);
            CLIENT_ASSERT(!di->T32_16, "unsupported in T32.16");
            encode_immed(di, 0, OPSZ_12b, delta < 0 ? -delta : delta, false /*unsigned*/);
        }
        break;

    case TYPE_NONE:
    case TYPE_R_D_PLUS1:
    case TYPE_R_B_PLUS1:
    case TYPE_W_C_PLUS1:
    case TYPE_SPSR:
    case TYPE_CPSR:
    case TYPE_FPSCR:
    case TYPE_LR:
    case TYPE_SP:
    case TYPE_PC:
    case TYPE_SHIFT_LSL:
    case TYPE_SHIFT_ASR:
    case TYPE_K: break; /* implicit or empty */

    default: CLIENT_ASSERT(false, "encode error: unknown operand type");
    }

    LOG(THREAD_GET, LOG_EMIT, ENC_LEVEL, "encode opnd %d => 0x%08x\n", *counter - 1,
        di->instr_word);
}

static void
encode_operands(decode_info_t *di, instr_t *in, const instr_info_t *ii)
{
    uint num_dsts = 0, num_srcs = 0;
    do {
        if (ii->dst1_type != TYPE_NONE)
            encode_operand(di, ii->dst1_type, ii->dst1_size, in, true, &num_dsts);
        if (ii->dst2_type != TYPE_NONE) {
            encode_operand(di, ii->dst2_type, ii->dst2_size, in,
                           !TEST(DECODE_4_SRCS, ii->flags),
                           TEST(DECODE_4_SRCS, ii->flags) ? &num_srcs : &num_dsts);
        }
        if (ii->src1_type != TYPE_NONE) {
            encode_operand(di, ii->src1_type, ii->src1_size, in,
                           TEST(DECODE_3_DSTS, ii->flags),
                           TEST(DECODE_3_DSTS, ii->flags) ? &num_dsts : &num_srcs);
        }
        if (ii->src2_type != TYPE_NONE)
            encode_operand(di, ii->src2_type, ii->src2_size, in, false, &num_srcs);
        if (ii->src3_type != TYPE_NONE)
            encode_operand(di, ii->src3_type, ii->src3_size, in, false, &num_srcs);
        ii = instr_info_extra_opnds(ii);
    } while (ii != NULL);
}

byte *
instr_encode_arch(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc,
                  bool check_reachable,
                  bool *has_instr_opnds /*OUT OPTIONAL*/
                      _IF_DEBUG(bool assert_reachable))
{
    const instr_info_t *info;
    decode_info_t di;

    if (instr_is_label(instr)) {
        if (has_instr_opnds != NULL)
            *has_instr_opnds = false;
        return copy_pc;
    }

    decode_info_init_for_instr(&di, instr);
    di.check_reachable = check_reachable;
    di.start_pc = copy_pc;
    di.final_pc = final_pc;
    di.cur_offs = (ptr_int_t)instr->offset;
    di.encode_state = *get_encode_state(dcontext);

    /* We need to track the IT block state even for raw-bits-valid instrs.
     * Unlike x86, we have no fast decoder that skips opcodes, so we should
     * always have the opcode, except for decode_fragment cases:
     * FIXME i#1551: investigate handling for decode_fragment for a branch inside
     * an IT block.  We should probably change decode_fragment() to fully
     * and separately decode all IT block instrs.
     */
    encode_track_it_block_di(dcontext, &di, instr);

    /* First, handle the already-encoded instructions */
    if (instr_raw_bits_valid(instr)) {
        CLIENT_ASSERT(check_reachable,
                      "internal encode error: cannot encode raw "
                      "bits and ignore reachability");
        /* Copy raw bits, possibly re-relativizing */
        if (has_instr_opnds != NULL)
            *has_instr_opnds = false;
        return copy_and_re_relativize_raw_instr(dcontext, instr, copy_pc, final_pc);
    }
    CLIENT_ASSERT(instr_operands_valid(instr), "instr_encode error: operands invalid");

    /* We delay this until after handling raw instrs to avoid trying to get the opcode
     * of a data-only instr.
     */
    di.opcode = instr_get_opcode(instr);

    info = instr_get_instr_info(instr);
    if (info == NULL) {
        if (has_instr_opnds != NULL)
            *has_instr_opnds = false;
        return NULL;
    }

    while (!encoding_possible(&di, instr, info)) {
        LOG(THREAD, LOG_EMIT, ENC_LEVEL, "\tencoding for 0x%x no good...\n",
            info->opcode);
        info = get_next_instr_info(info);
        if (info == NULL || info->type == OP_CONTD) {
            /* Use the errmsg to try and give a more helpful message */
            DODEBUG({
                if (di.errmsg != NULL && assert_reachable)
                    SYSLOG_INTERNAL_ERROR_ONCE(di.errmsg, di.errmsg_param);
            });
            DOLOG(1, LOG_EMIT, {
                LOG(THREAD, LOG_EMIT, 1, "ERROR: Could not find encoding for: ");
                instr_disassemble(dcontext, instr, THREAD);
                if (di.errmsg != NULL) {
                    LOG(THREAD, LOG_EMIT, 1, "\nReason: ");
                    LOG(THREAD, LOG_EMIT, 1, di.errmsg, di.errmsg_param);
                }
                LOG(THREAD, LOG_EMIT, 1, "\n");
            });
            return NULL;
        }
        /* We need to clear all the checking fields for each new template */
        /* We avoid using "&di.errmsg" to avoid a gcc 9 warning (i#4170). */
        memset(((char *)&di) + offsetof(decode_info_t, errmsg), 0,
               sizeof(di) - offsetof(decode_info_t, errmsg));
    }

    /* Encode into di.instr_word */
    di.instr_word = info->opcode;
    if (TEST(DECODE_PREDICATE_28, info->flags)) {
        dr_pred_type_t pred = instr_get_predicate(instr);
        if (pred == DR_PRED_NONE)
            pred = DR_PRED_AL;
        di.instr_word |= (pred - DR_PRED_EQ) << 28;
    } else if (TEST(DECODE_PREDICATE_22, info->flags)) {
        dr_pred_type_t pred = instr_get_predicate(instr);
        di.instr_word |= (pred - DR_PRED_EQ) << 22;
    } else if (TEST(DECODE_PREDICATE_8, info->flags)) {
        dr_pred_type_t pred = instr_get_predicate(instr);
        di.instr_word |= (pred - DR_PRED_EQ) << 8;
    }
    encode_operands(&di, instr, info);

    if (di.isa_mode == DR_ISA_ARM_THUMB) {
        if (di.instr_word >> 16 != 0) {
            *((ushort *)copy_pc) = (ushort)(di.instr_word >> 16);
            copy_pc += THUMB_SHORT_INSTR_SIZE;
        }
        *((ushort *)copy_pc) = (ushort)di.instr_word;
        copy_pc += THUMB_SHORT_INSTR_SIZE;
    } else {
        *((uint *)copy_pc) = di.instr_word;
        copy_pc += ARM_INSTR_SIZE;
    }
    if (has_instr_opnds != NULL)
        *has_instr_opnds = di.has_instr_opnds;
    return copy_pc;
}

byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr, byte *dst_pc,
                                 byte *final_pc)
{
    /* TODO i#4016: re-relativizing is NYI */
    ASSERT(instr_raw_bits_valid(instr));
    memcpy(dst_pc, instr->bytes, instr->length);
    return dst_pc + instr->length;
}

byte *
encode_raw_jmp(dr_isa_mode_t isa_mode, byte *target_pc, byte *dst_pc, byte *final_pc)
{
    if (isa_mode == DR_ISA_ARM_A32) {
        uint val = 0xea000000; /* unconditional OP_b */
        int disp = target_pc - decode_cur_pc(final_pc, isa_mode, OP_b, NULL);
        ASSERT(ALIGNED(disp, ARM_INSTR_SIZE));
        ASSERT(disp < 0x4000000 && disp >= -32 * 1024 * 1024); /* 26-bit max */
        val |= ((disp >> 2) & 0xffffff);
        *(uint *)dst_pc = val;
        return dst_pc + ARM_INSTR_SIZE;
    } else if (isa_mode == DR_ISA_ARM_THUMB) {
        ushort valA = 0xf000; /* OP_b */
        ushort valB = 0x9000; /* OP_b */
        int disp = target_pc - decode_cur_pc(final_pc, isa_mode, OP_b, NULL);
        ASSERT(ALIGNED(disp, THUMB_SHORT_INSTR_SIZE));
        /* A10,B13,B11,A9:0,B10:0 x2, but B13 and B11 are flipped if A10 is 0 */
        uint bitA10 = (disp >> 24) & 0x1; /* +1 for the x2 */
        uint bitB13 = (disp >> 23) & 0x1;
        uint bitB11 = (disp >> 22) & 0x1;
        ASSERT(disp < 0x2000000 && disp >= -16 * 1024 * 1024); /* 25-bit max */
        /* XXX: share with regular encode's TYPE_J_b26_b13_b11_b16_b0 */
        if (bitA10 == 0) {
            bitB13 = (bitB13 == 0 ? 1 : 0);
            bitB11 = (bitB11 == 0 ? 1 : 0);
        }
        valB |= (disp >> 1) & 0x7ff;  /* B10:0 */
        valA |= (disp >> 12) & 0x3ff; /* A9:0 */
        valB |= bitB13 << 13;
        valB |= bitB11 << 11;
        valA |= bitA10 << 10;
        *(ushort *)dst_pc = valA;
        *(ushort *)(dst_pc + 2) = valB;
        return dst_pc + THUMB_LONG_INSTR_SIZE;
    }
    /* FIXME i#1569: add AArch64 support */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}
