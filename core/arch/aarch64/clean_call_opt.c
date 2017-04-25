/* **********************************************************
 * Copyright (c) 2016-2017 ARM Limited. All rights reserved.
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

/* file "clean_call_opt.c" */

#include "../globals.h"
#include "arch.h"
#include "../clean_call_opt.h"

#ifdef CLIENT_INTERFACE

/* For fast recognition we do not check the instructions operand by operand.
 * Instead we test the encoding directly.
 */

/* remove variable bits in the encoding */
#define STP_LDP_ENC_MASK 0x7fc07fff
#define STR_LDR_ENC_MASK 0xbfc003ff
#define MOV_STK_ENC_MASK 0x7f0003ff
#define STP_LDP_REG_MASK 0xffff83e0
#define STR_LDR_REG_MASK 0xffffffe0

/* stp x29, x30, [sp, #frame_size]! */
#define PUSH_FP_LR_ENC 0x29807bfd
/* ldp x29, x30, [sp], #frame_size */
#define POP_FP_LR_ENC 0x28c07bfd
/* add sp, sp, #frame_size */
#define ADD_SP_ENC 0x110003ff
/* sub sp, sp, #frame_size */
#define SUB_SP_ENC 0x510003ff
/* mov x29, sp */
#define MOV_X29_SP_ENC 0x910003fd
/* stp xx, xx, [sp, #offset] */
#define STP_SP_ENC 0x290003e0
/* ldp xx, xx, [sp, #offset] */
#define LDP_SP_ENC 0x294003e0
/* str xx, [sp, #offset] */
#define STR_SP_ENC 0xb90003e0
/* ldr xx, [sp, #offset] */
#define LDR_SP_ENC 0xb94003e0

static inline bool
instr_is_push_fp_and_lr(instr_t *instr)
{
    uint enc = *(uint *)instr->bytes;
    return (enc & STP_LDP_ENC_MASK) == PUSH_FP_LR_ENC;
}

static inline bool
instr_is_pop_fp_and_lr(instr_t *instr)
{
    uint enc = *(uint *)instr->bytes;
    return (enc & STP_LDP_ENC_MASK) == POP_FP_LR_ENC;
}

static inline bool
instr_is_move_frame_ptr(instr_t *instr)
{
    uint enc = *(uint *)instr->bytes;
    return enc == MOV_X29_SP_ENC;
}

static inline bool
instr_is_add_stk_ptr(instr_t *instr)
{
    uint enc = *(uint *)instr->bytes;
    return (enc & MOV_STK_ENC_MASK) == ADD_SP_ENC;
}

static inline bool
instr_is_sub_stk_ptr(instr_t *instr)
{
    uint enc = *(uint *)instr->bytes;
    return (enc & MOV_STK_ENC_MASK) == SUB_SP_ENC;
}

static inline bool
instr_is_push_reg_pair(instr_t *instr, reg_id_t *reg1, reg_id_t *reg2)
{
    uint enc = *(uint *)instr->bytes;
    enc = enc & STP_LDP_ENC_MASK;
    if ((enc & STP_LDP_REG_MASK) != STP_SP_ENC)
        return false;
    *reg1 = (reg_id_t)(enc & 31) + DR_REG_START_GPR;
    *reg2 = (reg_id_t)(enc >> 10 & 31) + DR_REG_START_GPR;
    return true;
}

static inline bool
instr_is_pop_reg_pair(instr_t *instr, reg_id_t *reg1, reg_id_t *reg2)
{
    uint enc = *(uint *)instr->bytes;
    enc = enc & STP_LDP_ENC_MASK;
    if ((enc & STP_LDP_REG_MASK) != LDP_SP_ENC)
        return false;
    *reg1 = (reg_id_t)(enc & 31) + DR_REG_START_GPR;
    *reg2 = (reg_id_t)(enc >> 10 & 31) + DR_REG_START_GPR;
    return true;
}

static inline bool
instr_is_push_reg(instr_t *instr, reg_id_t *reg)
{
    uint enc = *(uint *)instr->bytes;
    enc = enc & STR_LDR_ENC_MASK;
    if ((enc & STR_LDR_REG_MASK) != STR_SP_ENC)
        return false;
    *reg = (reg_id_t)(enc & 31) + DR_REG_START_GPR;
    return true;
}

static inline bool
instr_is_pop_reg(instr_t *instr, reg_id_t *reg)
{
    uint enc = *(uint *)instr->bytes;
    enc = enc & STR_LDR_ENC_MASK;
    if ((enc & STR_LDR_REG_MASK) != LDR_SP_ENC)
        return false;
    *reg = (reg_id_t)(enc & 31) + DR_REG_START_GPR;
    return true;
}

void
analyze_callee_regs_usage(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    uint i, num_regparm;

    /* XXX implement bitset for optimisation */
    memset(ci->reg_used, 0, sizeof(bool) * NUM_GP_REGS);
    ci->num_simd_used = 0;
    memset(ci->simd_used, 0, sizeof(bool) * NUM_SIMD_REGS);
    /* Scratch registers used for setting up the jump to the clean callee. */
    ci->reg_used[SCRATCH_REG0] = true;
    ci->reg_used[SCRATCH_REG1] = true;
    ci->reg_used[DR_REG_X11 - DR_REG_START_GPR] = true;

    for (instr  = instrlist_first(ilist);
         instr != NULL;
         instr  = instr_get_next(instr)) {

        /* General purpose registers */
        for (i = 0; i < NUM_GP_REGS; i++) {
            reg_id_t reg = DR_REG_START_GPR + (reg_id_t)i;
            if (!ci->reg_used[i] &&
                instr_uses_reg(instr, reg)) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee "PFX" uses REG %s at "PFX"\n",
                    ci->start, reg_names[reg],
                    instr_get_app_pc(instr));
                ci->reg_used[i] = true;
                callee_info_reserve_slot(ci, SLOT_REG, reg);
            }
        }

        /* SIMD register usage */
        for (i=0; i<NUM_SIMD_REGS; i++) {
            if (!ci->simd_used[i] &&
                instr_uses_reg(instr, (DR_REG_Q0 + (reg_id_t)i))) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee "PFX" uses VREG%d at "PFX"\n",
                    ci->start, i, instr_get_app_pc(instr));
                ci->simd_used[i] = true;
                ci->num_simd_used++;
            }
        }
    }

    num_regparm = MIN(ci->num_args, NUM_REGPARM);
    for (i = 0; i < num_regparm; i++) {
        reg_id_t reg = regparms[i];
        if (!ci->reg_used[reg - DR_REG_START_GPR]) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee "PFX" uses REG %s for arg passing\n",
                ci->start, reg_names[reg]);
            ci->reg_used[reg - DR_REG_START_GPR] = true;
            callee_info_reserve_slot(ci, SLOT_REG, reg);
        }
    }
    /* FIXME i#1621: the following checks are still missing:
     *    - analysis of eflags (depends on i#2263)
     */
}

/* We use stp/ldp/str/ldr [sp, #imm] pattern to detect callee saved registers,
 * and assume that the code later won't change those saved value
 * on the stack.
 */
void
analyze_callee_save_reg(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *top, *bot, *instr;
    reg_id_t reg1, reg2;
    bool not_found;
    /* pointers to instructions of interest */
    instr_t *enter = NULL, *leave = NULL;

    ci->num_callee_save_regs = 0;
    top = instrlist_first(ilist);
    bot = instrlist_last(ilist);

    /* zero or one instruction only, no callee save */
    if (top == bot)
        return;

    /* Stack frame analysis
     * A typical function (fewer than 8 arguments) has the following form:
     * (a) stp x29, x30, [sp, #-frame_size]!
     * (b) mov x29, sp
     * (c) stp x19, x20, [sp, #callee_save_offset]
     * (c) str x21, [sp, #callee_save_offset+8]
     * ...
     * (c) ldp x19, x20, [sp, #callee_save_offset]
     * (c) ldr x21, [sp, #callee_save_offset+8]
     * (a) ldp x29, x30, [sp], #frame_size
     *     ret
     * Pair (a) appears when the callee calls another function.
     * If the callee is a leaf function, pair (a) typically has the following form:
     * (a) sub, sp, sp, #frame_size
     * (a) add, sp, sp, #frame_size
     * If (b) is found, x29 is used as the frame pointer.
     * Pair (c) may have two forms, using stp/ldp for register pairs
     * or str/ldr for a single callee-saved register.
     */
     /* Check for pair (a) */
    for (instr = top; instr != bot; instr = instr_get_next(instr)) {
        if (instr->bytes == NULL)
            continue;
        if (instr_is_push_fp_and_lr(instr) ||
            instr_is_sub_stk_ptr(instr)) {
            enter = instr;
            break;
        }
    }
    if (enter != NULL) {
        for (instr = bot; instr != enter; instr = instr_get_prev(instr)) {
            if (!instr->bytes)
                continue;
            if (instr_is_pop_fp_and_lr(instr) ||
                instr_is_add_stk_ptr(instr)) {
                leave = instr;
                break;
            }
        }
    }
    /* Check for (b) */
    ci->standard_fp = false;
    if (enter != NULL && leave != NULL &&
        (ci->bwd_tgt == NULL || instr_get_app_pc(enter) <  ci->bwd_tgt) &&
        (ci->fwd_tgt == NULL || instr_get_app_pc(leave) >= ci->fwd_tgt)) {
        for (instr = instr_get_next(enter);
             instr != leave;
             instr = instr_get_next(instr)) {
            if (instr_is_move_frame_ptr(instr)) {
                ci->standard_fp = true;
                /* Remove this instruction. */
                instrlist_remove(ilist, instr);
                instr_destroy(GLOBAL_DCONTEXT, instr);
                break;
            }
        }
        if (ci->standard_fp) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee "PFX" use X29 as frame pointer\n", ci->start);
        }
        /* remove pair (a) */
        instrlist_remove(ilist, enter);
        instrlist_remove(ilist, leave);
        instr_destroy(GLOBAL_DCONTEXT, enter);
        instr_destroy(GLOBAL_DCONTEXT, leave);
        top = instrlist_first(ilist);
        bot = instrlist_last(ilist);
    }
    /* Check for (c): callee-saved registers */
    while (top != NULL && bot != NULL) {
        /* if not in the first/last bb, break */
        if ((ci->bwd_tgt != NULL && instr_get_app_pc(top) >= ci->bwd_tgt) ||
            (ci->fwd_tgt != NULL && instr_get_app_pc(bot) <  ci->fwd_tgt) ||
            instr_is_cti(top) || instr_is_cti(bot))
            break;
        if (instr_is_push_reg_pair(top, &reg1, &reg2)) {
            not_found = true;
            /* If a save reg pair is found and the register,
             * search from the bottom for restore.
             */
            for (instr = bot; !instr_is_cti(instr); instr = instr_get_prev(instr)) {
                reg_id_t reg1_c, reg2_c;
                if (instr_is_pop_reg_pair(instr, &reg1_c, &reg2_c) &&
                    reg1 == reg1_c &&
                    reg2 == reg2_c) {
                    /* found a save/restore pair */
                    ci->callee_save_regs[reg1] = true;
                    ci->callee_save_regs[reg2] = true;
                    ci->num_callee_save_regs += 2;
                    /* remove & destroy the pairs */
                    instrlist_remove(ilist, top);
                    instr_destroy(GLOBAL_DCONTEXT, top);
                    instrlist_remove(ilist, instr);
                    instr_destroy(GLOBAL_DCONTEXT, instr);
                    /* get next pair */
                    top = instrlist_first(ilist);
                    bot = instrlist_last(ilist);
                    not_found = false;
                }
            }
            if (not_found)
                break;
        } else if (instr_is_push_reg(top, &reg1)) {
            not_found = true;
            /* If a save reg is found, search from the bottom for restore. */
            for (instr = bot; instr != top; instr = instr_get_prev(instr)) {
                reg_id_t reg1_c;
                if (instr_is_pop_reg(instr, &reg1_c) &&
                    reg1 == reg1_c) {
                    /* found a save/restore pair */
                    ci->callee_save_regs[reg1] = true;
                    ci->num_callee_save_regs += 1;
                    /* remove & destroy the pairs */
                    instrlist_remove(ilist, top);
                    instr_destroy(GLOBAL_DCONTEXT, top);
                    instrlist_remove(ilist, instr);
                    instr_destroy(GLOBAL_DCONTEXT, instr);
                    /* get next pair */
                    top = instrlist_first(ilist);
                    bot = instrlist_last(ilist);
                    break;
                }
            }
            if (not_found)
                break;
        } else
            break;
    }
}

void
analyze_callee_tls(dcontext_t *dcontext, callee_info_t *ci)
{
    /* FIXME i#1621: NYI on AArch64
     * Non-essential for cleancall_opt=1 optimizations.
     */
}

app_pc
check_callee_instr_level2(dcontext_t *dcontext, callee_info_t *ci, app_pc next_pc,
                          app_pc cur_pc, app_pc tgt_pc)
{
    /* FIXME i#1569: For opt level greater than 1, we abort. */
    return NULL;
}

bool
check_callee_ilist_inline(dcontext_t *dcontext, callee_info_t *ci)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569: NYI on AArch64 */
    return false;
}

void
analyze_clean_call_aflags(dcontext_t *dcontext,
                          clean_call_info_t *cci, instr_t *where)
{
    /* FIXME i#1621: NYI on AArch64
     * Non-essential for cleancall_opt=1 optimizations.
     */
}

void
insert_inline_reg_save(dcontext_t *dcontext, clean_call_info_t *cci,
                       instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569: NYI on AArch64 */
}

void
insert_inline_reg_restore(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *where)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569: NYI on AArch64 */
}

void
insert_inline_arg_setup(dcontext_t *dcontext, clean_call_info_t *cci,
                        instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569: NYI on AArch64 */
}

#endif /* CLIENT_INTERFACE */
