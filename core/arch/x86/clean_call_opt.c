/* ******************************************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
 * ******************************************************************************/

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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* file "clean_call_opt.c" */

#include "../globals.h"
#include "arch.h"
#include "instrument.h"
#include "../hashtable.h"
#include "disassemble.h"
#include "instr_create_shared.h"
#include "../clean_call_opt.h"

/* make code more readable by shortening long lines
 * we mark everything we add as a meta-instr to avoid hitting
 * client asserts on setting translation fields
 */
#define POST instrlist_meta_postinsert
#define PRE instrlist_meta_preinsert

void
analyze_callee_regs_usage(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    int i, num_regparm;

    ci->num_simd_used = 0;
    ci->num_opmask_used = 0;
    /* Part of the array might be left uninitialized if
     * proc_num_simd_registers() < MCXT_NUM_SIMD_SLOTS.
     */
    memset(ci->simd_used, 0, sizeof(bool) * proc_num_simd_registers());
    memset(ci->opmask_used, 0, sizeof(bool) * MCXT_NUM_OPMASK_SLOTS);
    memset(ci->reg_used, 0, sizeof(bool) * DR_NUM_GPR_REGS);
    ci->write_flags = false;
    for (instr = instrlist_first(ilist); instr != NULL; instr = instr_get_next(instr)) {
        /* XXX: this is not efficient as instr_uses_reg will iterate over
         * every operands, and the total would be (NUM_REGS * NUM_OPNDS)
         * for each instruction. However, since this will be only called
         * once for each clean call callee, it will have little performance
         * impact unless there are a lot of different clean call callees.
         */
        /* XMM registers usage */
        for (i = 0; i < proc_num_simd_registers(); i++) {
            if (!ci->simd_used[i] &&
                (instr_uses_reg(instr, (DR_REG_START_XMM + (reg_id_t)i)) ||
                 instr_uses_reg(instr, (DR_REG_START_YMM + (reg_id_t)i)) ||
                 instr_uses_reg(instr, (DR_REG_START_ZMM + (reg_id_t)i)))) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee " PFX " uses XMM%d at " PFX "\n", ci->start, i,
                    instr_get_app_pc(instr));
                ci->simd_used[i] = true;
                ci->num_simd_used++;
            }
        }
        for (i = 0; i < proc_num_opmask_registers(); i++) {
            if (!ci->opmask_used[i] &&
                instr_uses_reg(instr, (DR_REG_START_OPMASK + (reg_id_t)i))) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee " PFX " uses k%d at " PFX "\n", ci->start, i,
                    instr_get_app_pc(instr));
                ci->opmask_used[i] = true;
                ci->num_opmask_used++;
            }
        }
        /* General purpose registers */
        for (i = 0; i < DR_NUM_GPR_REGS; i++) {
            reg_id_t reg = DR_REG_XAX + (reg_id_t)i;
            if (!ci->reg_used[i] &&
                /* Later we'll rewrite stack accesses to not use XSP or XBP. */
                reg != DR_REG_XSP && (reg != DR_REG_XBP || !ci->standard_fp) &&
                instr_uses_reg(instr, reg)) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee " PFX " uses REG %s at " PFX "\n", ci->start,
                    reg_names[reg], instr_get_app_pc(instr));
                ci->reg_used[i] = true;
                callee_info_reserve_slot(ci, SLOT_REG, reg);
            }
        }
        /* callee update aflags */
        if (!ci->write_flags) {
            if (TESTANY(EFLAGS_WRITE_6,
                        instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL))) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee " PFX " updates aflags\n", ci->start);
                ci->write_flags = true;
            }
        }
    }

    /* check if callee read aflags from caller */
    /* set it false for the case of empty callee. */
    if (ZMM_ENABLED()) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: AVX-512 enabled, forced to save aflags\n", ci->start);
    } else {
        ci->read_flags = false;
        for (instr = instrlist_first(ilist); instr != NULL;
             instr = instr_get_next(instr)) {
            uint flags = instr_get_arith_flags(instr, DR_QUERY_DEFAULT);
            if (TESTANY(EFLAGS_READ_6, flags)) {
                ci->read_flags = true;
                break;
            }
            if (TESTALL(EFLAGS_WRITE_6, flags))
                break;
            if (instr_is_return(instr))
                break;
            if (instr_is_cti(instr)) {
                ci->read_flags = true;
                break;
            }
        }
        if (ci->read_flags) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee " PFX " reads aflags from caller\n", ci->start);
        }
    }

    /* If we read or write aflags, we need to reserve a slot to save them.
     * We may or may not use the slot at the call site, but it needs to be
     * reserved just in case.
     */
    if (ci->read_flags || ci->write_flags || ZMM_ENABLED()) {
        /* XXX: We can optimize away the flags spill to memory if the callee
         * does not use xax.
         */
        callee_info_reserve_slot(ci, SLOT_FLAGS, 0);
        /* Spilling flags clobbers xax, so we need to spill the app xax first.
         * If the callee used xax, then the slot will already be reserved.
         */
        if (!ci->reg_used[DR_REG_XAX - DR_REG_XAX]) {
            callee_info_reserve_slot(ci, SLOT_REG, DR_REG_XAX);
        }
    }

    /* i#987, i#988: reg might be used for arg passing but not used in callee */
    num_regparm = MIN(ci->num_args, NUM_REGPARM);
    for (i = 0; i < num_regparm; i++) {
        reg_id_t reg = d_r_regparms[i];
        if (!ci->reg_used[reg - DR_REG_XAX]) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee " PFX " uses REG %s for arg passing\n", ci->start,
                reg_names[reg]);
            ci->reg_used[reg - DR_REG_XAX] = true;
            callee_info_reserve_slot(ci, SLOT_REG, reg);
        }
    }
}

/* We use push/pop pattern to detect callee saved registers,
 * and assume that the code later won't change those saved value
 * on the stack.
 */
void
analyze_callee_save_reg(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *top, *bot, *push_xbp, *pop_xbp, *instr, *enter, *leave;

    ASSERT(ilist != NULL);
    ci->num_callee_save_regs = 0;
    top = instrlist_first(ilist);
    bot = instrlist_last(ilist);
    if (top == bot) {
        /* zero or one instruction only, no callee save */
        return;
    }
    /* 1. frame pointer usage analysis. */
    /* i#392-c#4: frame pointer code might be in the middle
     * 0xf771f390 <compiler_inscount>:      call   0xf7723a19 <get_pc_thunk>
     * 0xf771f395 <compiler_inscount+5>:    add    $0x6c5f,%ecx
     * 0xf771f39b <compiler_inscount+11>:   push   %ebp
     * 0xf771f39c <compiler_inscount+12>:   mov    %esp,%ebp
     * 0xf771f39e <compiler_inscount+14>:   mov    0x8(%ebp),%eax
     * 0xf771f3a1 <compiler_inscount+17>:   pop    %ebp
     * 0xf771f3a2 <compiler_inscount+18>:   add    %eax,0x494(%ecx)
     * 0xf771f3a8 <compiler_inscount+24>:   ret
     */
    /* for easy of comparison, create push xbp, pop xbp */
    push_xbp = INSTR_CREATE_push(dcontext, opnd_create_reg(DR_REG_XBP));
    pop_xbp = INSTR_CREATE_pop(dcontext, opnd_create_reg(DR_REG_XBP));
    /* i#392-c#4: search for frame enter/leave pair */
    enter = NULL;
    leave = NULL;
    for (instr = top; instr != bot; instr = instr_get_next(instr)) {
        if (instr_get_opcode(instr) == OP_enter || instr_same(push_xbp, instr)) {
            enter = instr;
            break;
        }
    }
    if (enter != NULL) {
        for (instr = bot; instr != enter; instr = instr_get_prev(instr)) {
            if (instr_get_opcode(instr) == OP_leave || instr_same(pop_xbp, instr)) {
                leave = instr;
                break;
            }
        }
    }
    /* Check enter/leave pair  */
    if (enter != NULL && leave != NULL &&
        (ci->bwd_tgt == NULL || instr_get_app_pc(enter) < ci->bwd_tgt) &&
        (ci->fwd_tgt == NULL || instr_get_app_pc(leave) >= ci->fwd_tgt)) {
        /* check if xbp is fp */
        if (instr_get_opcode(enter) == OP_enter) {
            ci->standard_fp = true;
        } else {
            /* i#392-c#2: mov xsp => xbp might not be right after push_xbp */
            for (instr = instr_get_next(enter); instr != leave;
                 instr = instr_get_next(instr)) {
                if (instr != NULL &&
                    /* we want to use instr_same to find "mov xsp => xbp",
                     * but it could be OP_mov_ld or OP_mov_st, so use opnds
                     * for comparison instead.
                     */
                    instr_num_srcs(instr) == 1 && instr_num_dsts(instr) == 1 &&
                    opnd_is_reg(instr_get_src(instr, 0)) &&
                    opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_XSP &&
                    opnd_is_reg(instr_get_dst(instr, 0)) &&
                    opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_XBP) {
                    /* found mov xsp => xbp */
                    ci->standard_fp = true;
                    /* remove it */
                    instrlist_remove(ilist, instr);
                    instr_destroy(GLOBAL_DCONTEXT, instr);
                    break;
                }
            }
        }
        if (ci->standard_fp) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee " PFX " use XBP as frame pointer\n", ci->start);
        } else {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee " PFX " callee-saves reg xbp at " PFX " and " PFX "\n",
                ci->start, instr_get_app_pc(enter), instr_get_app_pc(leave));
            ci->callee_save_regs[DR_REG_XBP - DR_REG_XAX] = true;
            ci->num_callee_save_regs++;
        }
        /* remove enter/leave or push/pop xbp pair */
        instrlist_remove(ilist, enter);
        instrlist_remove(ilist, leave);
        instr_destroy(GLOBAL_DCONTEXT, enter);
        instr_destroy(GLOBAL_DCONTEXT, leave);
        top = instrlist_first(ilist);
        bot = instrlist_last(ilist);
    }
    instr_destroy(dcontext, push_xbp);
    instr_destroy(dcontext, pop_xbp);

    /* get the rest callee save regs */
    /* XXX: the callee save may be corrupted by memory update on the stack. */
    /* XXX: the callee save may use mov instead of push/pop */
    while (top != NULL && bot != NULL) {
        /* if not in the first/last bb, break */
        if ((ci->bwd_tgt != NULL && instr_get_app_pc(top) >= ci->bwd_tgt) ||
            (ci->fwd_tgt != NULL && instr_get_app_pc(bot) < ci->fwd_tgt) ||
            instr_is_cti(top) || instr_is_cti(bot))
            break;
        /* XXX: I saw some compiler inserts nop, need to handle. */
        /* push/pop pair check */
        if (instr_get_opcode(top) != OP_push || instr_get_opcode(bot) != OP_pop ||
            !opnd_same(instr_get_src(top, 0), instr_get_dst(bot, 0)) ||
            !opnd_is_reg(instr_get_src(top, 0)) ||
            opnd_get_reg(instr_get_src(top, 0)) == REG_XSP)
            break;
        /* It is a callee saved reg, we will do our own save for it. */
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee " PFX " callee-saves reg %s at " PFX " and " PFX "\n",
            ci->start, reg_names[opnd_get_reg(instr_get_src(top, 0))],
            instr_get_app_pc(top), instr_get_app_pc(bot));
        ci->callee_save_regs[opnd_get_reg(instr_get_src(top, 0)) - DR_REG_XAX] = true;
        ci->num_callee_save_regs++;
        /* remove & destroy the push/pop pairs */
        instrlist_remove(ilist, top);
        instr_destroy(GLOBAL_DCONTEXT, top);
        instrlist_remove(ilist, bot);
        instr_destroy(GLOBAL_DCONTEXT, bot);
        /* get next pair */
        top = instrlist_first(ilist);
        bot = instrlist_last(ilist);
    }
}

void
analyze_callee_tls(dcontext_t *dcontext, callee_info_t *ci)
{
    /* access to TLS means we do need to swap/preserve TEB/PEB fields
     * for library isolation (errno, etc.)
     */
    instr_t *instr;
    int i;
    ci->tls_used = false;
    for (instr = instrlist_first(ci->ilist); instr != NULL;
         instr = instr_get_next(instr)) {
        /* we assume any access via app's tls is to app errno. */
        for (i = 0; i < instr_num_srcs(instr); i++) {
            opnd_t opnd = instr_get_src(instr, i);
            if (opnd_is_far_base_disp(opnd) && opnd_get_segment(opnd) == LIB_SEG_TLS)
                ci->tls_used = true;
        }
        for (i = 0; i < instr_num_dsts(instr); i++) {
            opnd_t opnd = instr_get_dst(instr, i);
            if (opnd_is_far_base_disp(opnd) && opnd_get_segment(opnd) == LIB_SEG_TLS)
                ci->tls_used = true;
        }
    }
    if (ci->tls_used) {
        LOG(THREAD, LOG_CLEANCALL, 2, "CLEANCALL: callee " PFX " accesses far memory\n",
            ci->start);
    }
}

app_pc
check_callee_instr_level2(dcontext_t *dcontext, callee_info_t *ci, app_pc next_pc,
                          app_pc cur_pc, app_pc tgt_pc)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr = NULL;
    instr_t ins;
    app_pc tmp_pc;
    opnd_t src = OPND_CREATE_INTPTR(next_pc);
    instr_init(dcontext, &ins);
    TRY_EXCEPT(
        dcontext, { tmp_pc = decode(dcontext, tgt_pc, &ins); },
        {
            ASSERT_CURIOSITY(false && "crashed while decoding clean call");
            instr_free(dcontext, &ins);
            return NULL;
        });
    DOLOG(3, LOG_CLEANCALL, { disassemble_with_bytes(dcontext, tgt_pc, THREAD); });
    /* "pop %r1" or "mov [%rsp] %r1" */
    if (!(((instr_get_opcode(&ins) == OP_pop) ||
           (instr_get_opcode(&ins) == OP_mov_ld &&
            opnd_same(instr_get_src(&ins, 0), OPND_CREATE_MEMPTR(REG_XSP, 0)))) &&
          opnd_is_reg(instr_get_dst(&ins, 0)))) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee calls out is not PIC code, "
            "bailout\n");
        instr_free(dcontext, &ins);
        return NULL;
    }
    /* replace with "mov next_pc r1" */
    /* XXX: the memory on top of stack will not be next_pc. */
    instr = INSTR_CREATE_mov_imm(GLOBAL_DCONTEXT, instr_get_dst(&ins, 0), src);
    instr_set_translation(instr, cur_pc);
    instrlist_append(ilist, instr);
    ci->num_instrs++;
    instr_reset(dcontext, &ins);
    if (tgt_pc != next_pc) { /* a callout */
        TRY_EXCEPT(
            dcontext, { tmp_pc = decode(dcontext, tmp_pc, &ins); },
            {
                ASSERT_CURIOSITY(false && "crashed while decoding clean call");
                instr_free(dcontext, &ins);
                return NULL;
            });
        if (!instr_is_return(&ins)) {
            instr_free(dcontext, &ins);
            return NULL;
        }
        instr_free(dcontext, &ins);
    }
    LOG(THREAD, LOG_CLEANCALL, 2, "CLEANCALL: special PIC code at: " PFX "\n", cur_pc);
    ci->bailout = false;
    instr_free(dcontext, &ins);
    if (tgt_pc == next_pc)
        return tmp_pc;
    else
        return next_pc;
}

bool
check_callee_ilist_inline(dcontext_t *dcontext, callee_info_t *ci)
{
    instr_t *instr, *next_instr;
    opnd_t opnd, mem_ref, slot;
    bool opt_inline = true;
    int i;
    /* Now we need scan instructions in the list,
     * check if possible for inline, and convert memory reference
     */
    mem_ref = opnd_create_null();
    ci->has_locals = false;
    for (instr = instrlist_first(ci->ilist); instr != NULL; instr = next_instr) {
        uint opc = instr_get_opcode(instr);
        next_instr = instr_get_next(instr);
        /* sanity checks on stack usage */
        if (instr_writes_to_reg(instr, DR_REG_XBP, DR_QUERY_INCLUDE_ALL) &&
            ci->standard_fp) {
            /* xbp must not be changed if xbp is used for frame pointer */
            LOG(THREAD, LOG_CLEANCALL, 1,
                "CLEANCALL: callee " PFX " cannot be inlined: XBP is updated.\n",
                ci->start);
            opt_inline = false;
            break;
        } else if (instr_writes_to_reg(instr, DR_REG_XSP, DR_QUERY_INCLUDE_ALL)) {
            /* stack pointer update, we only allow:
             * lea [xsp, disp] => xsp
             * xsp + imm_int => xsp
             * xsp - imm_int => xsp
             */
            if (ci->has_locals) {
                /* we do not allow stack adjustment after accessing the stack */
                opt_inline = false;
            }
            if (opc == OP_lea) {
                /* lea [xsp, disp] => xsp */
                opnd = instr_get_src(instr, 0);
                if (!opnd_is_base_disp(opnd) || opnd_get_base(opnd) != DR_REG_XSP ||
                    opnd_get_index(opnd) != DR_REG_NULL)
                    opt_inline = false;
            } else if (opc == OP_sub || opc == OP_add) {
                /* xsp +/- int => xsp */
                if (!opnd_is_immed_int(instr_get_src(instr, 0)))
                    opt_inline = false;
            } else {
                /* other cases like push/pop are not allowed */
                opt_inline = false;
            }
            if (opt_inline) {
                LOG(THREAD, LOG_CLEANCALL, 3,
                    "CLEANCALL: removing frame adjustment at " PFX ".\n",
                    instr_get_app_pc(instr));
                instrlist_remove(ci->ilist, instr);
                instr_destroy(GLOBAL_DCONTEXT, instr);
                continue;
            } else {
                LOG(THREAD, LOG_CLEANCALL, 1,
                    "CLEANCALL: callee " PFX " cannot be inlined: "
                    "complicated stack pointer update at " PFX ".\n",
                    ci->start, instr_get_app_pc(instr));
                break;
            }
        } else if (instr_reg_in_src(instr, DR_REG_XSP) ||
                   (instr_reg_in_src(instr, DR_REG_XBP) && ci->standard_fp)) {
            /* Detect stack address leakage */
            /* lea [xsp/xbp] */
            if (opc == OP_lea)
                opt_inline = false;
            /* any direct use reg xsp or xbp */
            for (i = 0; i < instr_num_srcs(instr); i++) {
                opnd_t src = instr_get_src(instr, i);
                if (opnd_is_reg(src) &&
                    (reg_overlap(REG_XSP, opnd_get_reg(src)) ||
                     (reg_overlap(REG_XBP, opnd_get_reg(src)) && ci->standard_fp)))
                    break;
            }
            if (i != instr_num_srcs(instr))
                opt_inline = false;
            if (!opt_inline) {
                LOG(THREAD, LOG_CLEANCALL, 1,
                    "CLEANCALL: callee " PFX " cannot be inlined: "
                    "stack pointer leaked " PFX ".\n",
                    ci->start, instr_get_app_pc(instr));
                break;
            }
        }
        /* Check how many stack variables the callee has.
         * We will not inline the callee if it has more than one stack variable.
         */
        if (instr_reads_memory(instr)) {
            for (i = 0; i < instr_num_srcs(instr); i++) {
                opnd = instr_get_src(instr, i);
                if (!opnd_is_base_disp(opnd))
                    continue;
                if (opnd_get_base(opnd) != DR_REG_XSP &&
                    (opnd_get_base(opnd) != DR_REG_XBP || !ci->standard_fp))
                    continue;
                if (!ci->has_locals) {
                    /* We see the first one, remember it. */
                    mem_ref = opnd;
                    callee_info_reserve_slot(ci, SLOT_LOCAL, 0);
                    if (ci->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
                        LOG(THREAD, LOG_CLEANCALL, 1,
                            "CLEANCALL: callee " PFX " cannot be inlined: "
                            "not enough slots for local.\n",
                            ci->start);
                        break;
                    }
                    ci->has_locals = true;
                } else if (!opnd_same(opnd, mem_ref)) {
                    /* Check if it is the same stack var as the one we saw.
                     * If different, no inline.
                     */
                    LOG(THREAD, LOG_CLEANCALL, 1,
                        "CLEANCALL: callee " PFX " cannot be inlined: "
                        "more than one stack location is accessed " PFX ".\n",
                        ci->start, instr_get_app_pc(instr));
                    break;
                }
                /* replace the stack location with the scratch slot. */
                slot = callee_info_slot_opnd(ci, SLOT_LOCAL, 0);
                opnd_set_size(&slot, opnd_get_size(mem_ref));
                instr_set_src(instr, i, slot);
            }
            if (i != instr_num_srcs(instr)) {
                opt_inline = false;
                break;
            }
        }
        if (instr_writes_memory(instr)) {
            for (i = 0; i < instr_num_dsts(instr); i++) {
                opnd = instr_get_dst(instr, i);
                if (!opnd_is_base_disp(opnd))
                    continue;
                if (opnd_get_base(opnd) != DR_REG_XSP &&
                    (opnd_get_base(opnd) != DR_REG_XBP || !ci->standard_fp))
                    continue;
                if (!ci->has_locals) {
                    mem_ref = opnd;
                    callee_info_reserve_slot(ci, SLOT_LOCAL, 0);
                    if (ci->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
                        LOG(THREAD, LOG_CLEANCALL, 1,
                            "CLEANCALL: callee " PFX " cannot be inlined: "
                            "not enough slots for local.\n",
                            ci->start);
                        break;
                    }
                    ci->has_locals = true;
                } else if (!opnd_same(opnd, mem_ref)) {
                    /* currently we only allows one stack refs */
                    LOG(THREAD, LOG_CLEANCALL, 1,
                        "CLEANCALL: callee " PFX " cannot be inlined: "
                        "more than one stack location is accessed " PFX ".\n",
                        ci->start, instr_get_app_pc(instr));
                    break;
                }
                /* replace the stack location with the scratch slot. */
                slot = callee_info_slot_opnd(ci, SLOT_LOCAL, 0);
                opnd_set_size(&slot, opnd_get_size(mem_ref));
                instr_set_dst(instr, i, slot);
            }
            if (i != instr_num_dsts(instr)) {
                opt_inline = false;
                break;
            }
        }
    }

    if (instr != NULL)
        opt_inline = false;
    return opt_inline;
}

void
analyze_clean_call_aflags(dcontext_t *dcontext, clean_call_info_t *cci, instr_t *where)
{
    callee_info_t *ci = cci->callee_info;
    instr_t *instr;

    /* If there's a flags read, we clear the flags.  If there's a write or read,
     * we save them, because a read creates a clear which is a write. */
    cci->skip_clear_flags = !ci->read_flags;
    cci->skip_save_flags = !(ci->write_flags || ci->read_flags || ZMM_ENABLED());
    /* XXX: this is a more aggressive optimization by analyzing the ilist
     * to be instrumented. The client may change the ilist, which violate
     * the analysis result. For example,
     * I do not need save the aflags now if an instruction
     * after "where" updating all aflags, but later the client can
     * insert an instruction reads the aflags before that instruction.
     */
    if (INTERNAL_OPTION(opt_cleancall) > 1 && !cci->skip_save_flags) {
        for (instr = where; instr != NULL; instr = instr_get_next(instr)) {
            uint flags = instr_get_arith_flags(instr, DR_QUERY_DEFAULT);
            if (TESTANY(EFLAGS_READ_6, flags) || instr_is_cti(instr))
                break;
            if (TESTALL(EFLAGS_WRITE_6, flags)) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: inserting clean call " PFX ", skip saving aflags.\n",
                    ci->start);
                cci->skip_save_flags = true;
                break;
            }
        }
    }
}

void
insert_inline_reg_save(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                       instr_t *where, opnd_t *args)
{
    callee_info_t *ci = cci->callee_info;
    int i;

    /* Don't spill anything if we don't have to. */
    if (cci->num_regs_skip == DR_NUM_GPR_REGS && cci->skip_save_flags &&
        !ci->has_locals) {
        return;
    }

    /* Spill a register to TLS and point it at our unprotected_context_t. */
    PRE(ilist, where, instr_create_save_to_tls(dcontext, ci->spill_reg, TLS_XAX_SLOT));
    insert_get_mcontext_base(dcontext, ilist, where, ci->spill_reg);

    /* Save used registers. */
    ASSERT(cci->num_simd_skip == proc_num_simd_registers());
    ASSERT(cci->num_opmask_skip == proc_num_opmask_registers());
    for (i = 0; i < DR_NUM_GPR_REGS; i++) {
        if (!cci->reg_skip[i]) {
            reg_id_t reg_id = DR_REG_XAX + (reg_id_t)i;
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: inlining clean call " PFX ", saving reg %s.\n", ci->start,
                reg_names[reg_id]);
            PRE(ilist, where,
                INSTR_CREATE_mov_st(dcontext, callee_info_slot_opnd(ci, SLOT_REG, reg_id),
                                    opnd_create_reg(reg_id)));
        }
    }

    /* Save aflags if necessary via XAX, which was just saved if needed. */
    if (!cci->skip_save_flags) {
        ASSERT(!cci->reg_skip[DR_REG_XAX - DR_REG_XAX]);
        dr_save_arith_flags_to_xax(dcontext, ilist, where);
        PRE(ilist, where,
            INSTR_CREATE_mov_st(dcontext, callee_info_slot_opnd(ci, SLOT_FLAGS, 0),
                                opnd_create_reg(DR_REG_XAX)));
        /* Restore app XAX here if it's needed to materialize the argument. */
        if (cci->num_args > 0 && opnd_uses_reg(args[0], DR_REG_XAX)) {
            PRE(ilist, where,
                INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(DR_REG_XAX),
                                    callee_info_slot_opnd(ci, SLOT_REG, DR_REG_XAX)));
        }
    }
}

void
insert_inline_reg_restore(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *where)
{
    int i;
    callee_info_t *ci = cci->callee_info;

    /* Don't restore regs if we don't have to. */
    if (cci->num_regs_skip == DR_NUM_GPR_REGS && cci->skip_save_flags &&
        !ci->has_locals) {
        return;
    }

    /* Restore aflags before regs because it uses xax. */
    if (!cci->skip_save_flags) {
        PRE(ilist, where,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(DR_REG_XAX),
                                callee_info_slot_opnd(ci, SLOT_FLAGS, 0)));
        dr_restore_arith_flags_from_xax(dcontext, ilist, where);
    }

    /* Now restore all registers. */
    for (i = DR_NUM_GPR_REGS - 1; i >= 0; i--) {
        if (!cci->reg_skip[i]) {
            reg_id_t reg_id = DR_REG_XAX + (reg_id_t)i;
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: inlining clean call " PFX ", restoring reg %s.\n", ci->start,
                reg_names[reg_id]);
            PRE(ilist, where,
                INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(reg_id),
                                    callee_info_slot_opnd(ci, SLOT_REG, reg_id)));
        }
    }

    /* Restore reg used for unprotected_context_t pointer. */
    PRE(ilist, where,
        instr_create_restore_from_tls(dcontext, ci->spill_reg, TLS_XAX_SLOT));
}

void
insert_inline_arg_setup(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                        instr_t *where, opnd_t *args)
{
    reg_id_t regparm;
    callee_info_t *ci = cci->callee_info;
    opnd_t arg;
    bool restored_spill_reg = false;

    if (cci->num_args == 0)
        return;

    /* If the arg is un-referenced, don't set it up.  This is actually necessary
     * for correctness because we will not have spilled regparm[0] on x64 or
     * reserved SLOT_LOCAL for x86_32.
     */
    if (IF_X64_ELSE(!ci->reg_used[d_r_regparms[0] - DR_REG_XAX], !ci->has_locals)) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee " PFX " doesn't read arg, skipping arg setup.\n",
            ci->start);
        return;
    }

    ASSERT(cci->num_args == 1);
    arg = args[0];
    regparm = shrink_reg_for_param(IF_X64_ELSE(d_r_regparms[0], DR_REG_XAX), arg);

    if (opnd_uses_reg(arg, ci->spill_reg)) {
        if (opnd_is_reg(arg)) {
            /* Trying to pass the spill reg (or a subreg) as the arg. */
            reg_id_t arg_reg = opnd_get_reg(arg);
            arg = opnd_create_tls_slot(os_tls_offset(TLS_XAX_SLOT));
            opnd_set_size(&arg, reg_get_size(arg_reg));
            if (arg_reg == DR_REG_AH || /* Don't rely on ordering. */
                arg_reg == DR_REG_BH || arg_reg == DR_REG_CH || arg_reg == DR_REG_DH) {
                /* If it's one of the high sub-registers, add 1 to offset. */
                opnd_set_disp(&arg, opnd_get_disp(arg) + 1);
            }
        } else {
            /* Too complicated to rewrite if it's embedded in the operand.  Just
             * restore spill_reg during the arg materialization.  Hopefully this
             * doesn't happen very often.
             */
            PRE(ilist, where,
                instr_create_restore_from_tls(dcontext, ci->spill_reg, TLS_XAX_SLOT));
            DOLOG(2, LOG_CLEANCALL, {
                char disas_arg[MAX_OPND_DIS_SZ];
                opnd_disassemble_to_buffer(dcontext, arg, disas_arg,
                                           BUFFER_SIZE_ELEMENTS(disas_arg));
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: passing arg %s using spill reg %s to callee " PFX " "
                    "requires extra spills, consider using a different register.\n",
                    disas_arg, reg_names[ci->spill_reg], ci->start);
            });
            restored_spill_reg = true;
        }
    }

    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: inlining clean call " PFX ", passing arg via reg %s.\n", ci->start,
        reg_names[regparm]);
    if (opnd_is_immed_int(arg)) {
        PRE(ilist, where, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(regparm), arg));
    } else {
        PRE(ilist, where, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(regparm), arg));
    }

    /* Put the unprotected_context_t pointer back in spill_reg if we needed to
     * restore the app value.
     */
    if (restored_spill_reg) {
        insert_get_mcontext_base(dcontext, ilist, where, ci->spill_reg);
    }

#ifndef X64
    ASSERT(!cci->reg_skip[0]);
    /* Move xax to the scratch slot of the local.  We only allow at most one
     * local stack access, so the callee either does not use the argument, or
     * the local stack access is the arg.
     */
    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: inlining clean call " PFX ", passing arg via slot.\n", ci->start);
    PRE(ilist, where,
        INSTR_CREATE_mov_st(dcontext, callee_info_slot_opnd(ci, SLOT_LOCAL, 0),
                            opnd_create_reg(DR_REG_XAX)));
#endif
}

/***************************************************************************/
