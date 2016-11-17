/* ******************************************************************************
 * Copyright (c) 2010-2014 Google, Inc.  All rights reserved.
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

/* file "cleancallopt.c" */

#include "../globals.h"
#include "arch.h"
#include "instrument.h"
#include "../hashtable.h"
#include "disassemble.h"
#include "instr_create.h"

/* make code more readable by shortening long lines
 * we mark everything we add as a meta-instr to avoid hitting
 * client asserts on setting translation fields
 */
#define POST instrlist_meta_postinsert
#define PRE  instrlist_meta_preinsert


/****************************************************************************
 * clean call callee info table for i#42 and i#43
 */

#ifdef CLIENT_INTERFACE
/* hashtable for storing analyzed callee info */
static generic_table_t  *callee_info_table;
/* we only free callee info at exit, when callee_info_table_exit is true. */
static bool callee_info_table_exit = false;
#define INIT_HTABLE_SIZE_CALLEE 6 /* should remain small */

static void
callee_info_init(callee_info_t *ci)
{
    uint i;
    memset(ci, 0, sizeof(*ci));
    ci->bailout = true;
    /* to be conservative */
    ci->has_locals   = true;
    ci->write_aflags = true;
    ci->read_aflags  = true;
    ci->tls_used   = true;
    /* We use loop here and memset in analyze_callee_regs_usage later.
     * We could reverse the logic and use memset to set the value below,
     * but then later in analyze_callee_regs_usage, we have to use the loop.
     */
    /* assuming all xmm registers are used */
    ci->num_xmms_used = NUM_SIMD_REGS;
    for (i = 0; i < NUM_SIMD_REGS; i++)
        ci->xmm_used[i] = true;
    for (i = 0; i < NUM_GP_REGS; i++)
        ci->reg_used[i] = true;
    ci->spill_reg = DR_REG_INVALID;
}

static void
callee_info_free(dcontext_t *dcontext, callee_info_t *ci)
{
    if (ci->ilist != NULL) {
        ASSERT(ci->opt_inline);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ci->ilist);
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ci, callee_info_t,
                   ACCT_CLEANCALL, PROTECTED);
}

static callee_info_t *
callee_info_create(app_pc start, uint num_args)
{
    callee_info_t *info;

    info = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, callee_info_t,
                           ACCT_CLEANCALL, PROTECTED);
    callee_info_init(info);
    info->start = start;
    info->num_args = num_args;
    return info;
}

static void
callee_info_reserve_slot(callee_info_t *ci, slot_kind_t kind, reg_id_t value)
{
    if (ci->slots_used < BUFFER_SIZE_ELEMENTS(ci->scratch_slots)) {
        if (kind == SLOT_REG)
            value = dr_reg_fixer[value];
        ci->scratch_slots[ci->slots_used].kind = kind;
        ci->scratch_slots[ci->slots_used].value = value;
    } else {
        LOG(THREAD_GET, LOG_CLEANCALL, 2,
            "CLEANCALL: unable to fulfill callee_info_reserve_slot for "
            "kind %d value %d\n", kind, value);
    }
    /* We check if slots_used > CLEANCALL_NUM_INLINE_SLOTS to detect failure. */
    ci->slots_used++;
}

static opnd_t
callee_info_slot_opnd(callee_info_t *ci, slot_kind_t kind, reg_id_t value)
{
    uint i;
    if (kind == SLOT_REG)
        value = dr_reg_fixer[value];
    for (i = 0; i < BUFFER_SIZE_ELEMENTS(ci->scratch_slots); i++) {
        if (ci->scratch_slots[i].kind  == kind &&
            ci->scratch_slots[i].value == value) {
            int disp = (int)offsetof(unprotected_context_t,
                                     inline_spill_slots[i]);
            return opnd_create_base_disp(ci->spill_reg, DR_REG_NULL, 0, disp,
                                         OPSZ_PTR);
        }
    }
    ASSERT_MESSAGE(CHKLVL_ASSERTS, "Tried to find scratch slot for value "
                   "without calling callee_info_reserve_slot for it", false);
    return opnd_create_null();
}

static void
callee_info_table_init(void)
{
    callee_info_table =
        generic_hash_create(GLOBAL_DCONTEXT,
                            INIT_HTABLE_SIZE_CALLEE,
                            80 /* load factor: not perf-critical */,
                            HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
                            (void(*)(dcontext_t*, void*)) callee_info_free
                            _IF_DEBUG("callee-info table"));
}

static void
callee_info_table_destroy(void)
{
    callee_info_table_exit = true;
    generic_hash_destroy(GLOBAL_DCONTEXT, callee_info_table);
}

static callee_info_t *
callee_info_table_lookup(void *callee)
{
    callee_info_t *ci;
    TABLE_RWLOCK(callee_info_table, read, lock);
    ci = generic_hash_lookup(GLOBAL_DCONTEXT, callee_info_table,
                             (ptr_uint_t)callee);
    TABLE_RWLOCK(callee_info_table, read, unlock);
    /* We only delete the callee_info from the callee_info_table
     * when destroy the table on exit, so we can keep the ci
     * without holding the lock.
     */
    return ci;
}

static callee_info_t *
callee_info_table_add(callee_info_t *ci)
{
    callee_info_t *info;
    TABLE_RWLOCK(callee_info_table, write, lock);
    info = generic_hash_lookup(GLOBAL_DCONTEXT, callee_info_table,
                               (ptr_uint_t)ci->start);
    if (info == NULL) {
        info = ci;
        generic_hash_add(GLOBAL_DCONTEXT, callee_info_table,
                         (ptr_uint_t)ci->start, (void *)ci);
    } else {
        /* Have one in the table, free the new one and use existing one.
         * We cannot free the existing one in the table as it might be used by
         * other thread without holding the lock.
         * Since we assume callee should never be changed, they should have
         * the same content of ci.
         */
        callee_info_free(GLOBAL_DCONTEXT, ci);
    }
    TABLE_RWLOCK(callee_info_table, write, unlock);
    return info;
}

/****************************************************************************/
/* clean call optimization code */

/* The max number of instructions try to decode from a function. */
#define MAX_NUM_FUNC_INSTRS 4096
/* the max number of instructions the callee can have for inline. */
#define MAX_NUM_INLINE_INSTRS 20

/* Decode instruction from callee and return the next_pc to be decoded. */
static app_pc
decode_callee_instr(dcontext_t *dcontext, callee_info_t *ci, app_pc instr_pc)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    app_pc   next_pc = NULL;

    instr = instr_create(GLOBAL_DCONTEXT);
    instrlist_append(ilist, instr);
    ci->num_instrs++;
    TRY_EXCEPT(dcontext, {
        next_pc = decode(GLOBAL_DCONTEXT, instr_pc, instr);
    }, { /* EXCEPT */
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: crash on decoding callee instruction at: "PFX"\n",
            instr_pc);
        ASSERT_CURIOSITY(false && "crashed while decoding clean call");
        ci->bailout = true;
        return NULL;
    });
    if (!instr_valid(instr)) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: decoding invalid instruction at: "PFX"\n", instr_pc);
        ci->bailout = true;
        return NULL;
    }
    instr_set_translation(instr, instr_pc);
    DOLOG(3, LOG_CLEANCALL, {
        disassemble_with_bytes(dcontext, instr_pc, THREAD);
    });
    return next_pc;
}

/* check newly decoded instruction from callee */
static app_pc
check_callee_instr(dcontext_t *dcontext, callee_info_t *ci, app_pc next_pc)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    app_pc   cur_pc, tgt_pc;

    if (next_pc == NULL)
        return NULL;
    instr  = instrlist_last(ilist);
    cur_pc = instr_get_app_pc(instr);
    ASSERT(next_pc == cur_pc + instr_length(dcontext, instr));
    if (!instr_is_cti(instr)) {
        /* special instructions, bail out. */
        if (instr_is_syscall(instr) || instr_is_interrupt(instr)) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: bail out on syscall or interrupt at: "PFX"\n",
                cur_pc);
            ci->bailout = true;
            return NULL;
        }
        return next_pc;
    } else { /* cti instruc */
        if (instr_is_mbr(instr)) {
            /* check if instr is return, and if return is the last instr. */
            if (!instr_is_return(instr) || ci->fwd_tgt > cur_pc) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: bail out on indirect branch at: "PFX"\n",
                    cur_pc);
                ci->bailout = true;
            }
            return NULL;
        } else if (instr_is_call(instr)) {
            tgt_pc = opnd_get_pc(instr_get_target(instr));
            /* remove and destroy the call instruction */
            ci->bailout = true;
            instrlist_remove(ilist, instr);
            instr_destroy(GLOBAL_DCONTEXT, instr);
            instr = NULL;
            ci->num_instrs--;
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee calls out at: "PFX" to "PFX"\n",
                cur_pc, tgt_pc);
            /* check special PIC code:
             * 1. call next_pc; pop r1;
             * or
             * 2. call pic_func;
             *    and in pic_func: mov [%xsp] %r1; ret;
             */
            if (INTERNAL_OPTION(opt_cleancall) >= 1) {
                instr_t ins;
                app_pc  tmp_pc;
                opnd_t src = OPND_CREATE_INTPTR(next_pc);
                instr_init(dcontext, &ins);
                TRY_EXCEPT(dcontext, {
                    tmp_pc = decode(dcontext, tgt_pc, &ins);
                }, {
                    ASSERT_CURIOSITY(false &&
                                     "crashed while decoding clean call");
                    instr_free(dcontext, &ins);
                    return NULL;
                });
                DOLOG(3, LOG_CLEANCALL, {
                    disassemble_with_bytes(dcontext, tgt_pc, THREAD);
                });
                /* "pop %r1" or "mov [%rsp] %r1" */
                if (!(((instr_get_opcode(&ins) == OP_pop) ||
                       (instr_get_opcode(&ins) == OP_mov_ld &&
                        opnd_same(instr_get_src(&ins, 0),
                                  OPND_CREATE_MEMPTR(REG_XSP, 0)))) &&
                      opnd_is_reg(instr_get_dst(&ins, 0)))) {
                    LOG(THREAD, LOG_CLEANCALL, 2,
                        "CLEANCALL: callee calls out is not PIC code, "
                        "bailout\n");
                    instr_free(dcontext, &ins);
                    return NULL;
                }
                /* replace with "mov next_pc r1" */
                /* XXX: the memory on top of stack will not be next_pc. */
                instr = INSTR_CREATE_mov_imm
                    (GLOBAL_DCONTEXT, instr_get_dst(&ins, 0), src);
                instr_set_translation(instr, cur_pc);
                instrlist_append(ilist, instr);
                ci->num_instrs++;
                instr_reset(dcontext, &ins);
                if (tgt_pc != next_pc) { /* a callout */
                    TRY_EXCEPT(dcontext, {
                        tmp_pc = decode(dcontext, tmp_pc, &ins);
                    }, {
                        ASSERT_CURIOSITY(false &&
                                         "crashed while decoding clean call");
                        instr_free(dcontext, &ins);
                        return NULL;
                    });
                    if (!instr_is_return(&ins)) {
                        instr_free(dcontext, &ins);
                        return NULL;
                    }
                    instr_free(dcontext, &ins);
                }
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: special PIC code at: "PFX"\n",
                    cur_pc);
                ci->bailout = false;
                instr_free(dcontext, &ins);
                if (tgt_pc == next_pc)
                    return tmp_pc;
                else
                    return next_pc;
            }
        } else { /* ubr or cbr */
            tgt_pc = opnd_get_pc(instr_get_target(instr));
            if (tgt_pc < cur_pc) { /* backward branch */
                if (tgt_pc < ci->start) {
                    LOG(THREAD, LOG_CLEANCALL, 2,
                        "CLEANCALL: bail out on out-of-range branch at: "PFX
                        "to "PFX"\n", cur_pc, tgt_pc);
                    ci->bailout = true;
                    return NULL;
                } else if (ci->bwd_tgt == NULL || tgt_pc < ci->bwd_tgt) {
                    ci->bwd_tgt = tgt_pc;
                }
            } else { /* forward branch */
                if (ci->fwd_tgt == NULL || tgt_pc > ci->fwd_tgt) {
                    ci->fwd_tgt = tgt_pc;
                }
            }
        }
    }
    return next_pc;
}

static void
check_callee_ilist(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *cti, *tgt, *ret;
    app_pc   tgt_pc;
    if (!ci->bailout) {
        /* no target pc of any branch is in a middle of an instruction,
         * replace target pc with target instr
         */
        ret = instrlist_last(ilist);
        /* must be RETURN, otherwise, bugs in decode_callee_ilist */
        ASSERT(instr_is_return(ret));
        for (cti  = instrlist_first(ilist);
             cti != ret;
             cti  = instr_get_next(cti)) {
            if (!instr_is_cti(cti))
                continue;
            ASSERT(!instr_is_mbr(cti));
            tgt_pc = opnd_get_pc(instr_get_target(cti));
            for (tgt  = instrlist_first(ilist);
                 tgt != NULL;
                 tgt  = instr_get_next(tgt)) {
                if (tgt_pc == instr_get_app_pc(tgt))
                    break;
            }
            if (tgt == NULL) {
                /* cannot find a target instruction, bail out */
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: bail out on strange internal branch at: "PFX
                    "to "PFX"\n", instr_get_app_pc(cti), tgt_pc);
                ci->bailout = true;
                break;
            }
        }
        /* remove RETURN as we do not need it any more */
        instrlist_remove(ilist, ret);
        instr_destroy(GLOBAL_DCONTEXT, ret);
    }
    if (ci->bailout) {
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        ci->ilist = NULL;
    }
}

static void
decode_callee_ilist(dcontext_t *dcontext, callee_info_t *ci)
{
    app_pc cur_pc;

    ci->ilist = instrlist_create(GLOBAL_DCONTEXT);
    cur_pc = ci->start;

    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: decoding callee starting at: "PFX"\n", ci->start);
    ci->bailout = false;
    while (cur_pc != NULL) {
        cur_pc = decode_callee_instr(dcontext, ci, cur_pc);
        cur_pc = check_callee_instr(dcontext, ci, cur_pc);
    }
    check_callee_ilist(dcontext, ci);
}

static void
analyze_callee_regs_usage(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    uint i, num_regparm;

    ci->num_xmms_used = 0;
    memset(ci->xmm_used, 0, sizeof(bool) * NUM_SIMD_REGS);
    memset(ci->reg_used, 0, sizeof(bool) * NUM_GP_REGS);
    ci->write_aflags = false;
    for (instr  = instrlist_first(ilist);
         instr != NULL;
         instr  = instr_get_next(instr)) {
        /* XXX: this is not efficient as instr_uses_reg will iterate over
         * every operands, and the total would be (NUM_REGS * NUM_OPNDS)
         * for each instruction. However, since this will be only called
         * once for each clean call callee, it will have little performance
         * impact unless there are a lot of different clean call callees.
         */
        /* XMM registers usage */
        for (i = 0; i < NUM_SIMD_REGS; i++) {
            if (!ci->xmm_used[i] &&
                instr_uses_reg(instr, (DR_REG_XMM0 + (reg_id_t)i))) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee "PFX" uses XMM%d at "PFX"\n",
                    ci->start, i, instr_get_app_pc(instr));
                ci->xmm_used[i] = true;
                ci->num_xmms_used++;
            }
        }
        /* General purpose registers */
        for (i = 0; i < NUM_GP_REGS; i++) {
            reg_id_t reg = DR_REG_XAX + (reg_id_t)i;
            if (!ci->reg_used[i] &&
                /* Later we'll rewrite stack accesses to not use XSP or XBP. */
                reg != DR_REG_XSP &&
                (reg != DR_REG_XBP || !ci->xbp_is_fp) &&
                instr_uses_reg(instr, reg)) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee "PFX" uses REG %s at "PFX"\n",
                    ci->start, reg_names[reg],
                    instr_get_app_pc(instr));
                ci->reg_used[i] = true;
                callee_info_reserve_slot(ci, SLOT_REG, reg);
            }
        }
        /* callee update aflags */
        if (!ci->write_aflags) {
            if (TESTANY(EFLAGS_WRITE_6,
                        instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL))) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee "PFX" updates aflags\n", ci->start);
                ci->write_aflags = true;
            }
        }
    }

    /* check if callee read aflags from caller */
    /* set it false for the case of empty callee. */
    ci->read_aflags = false;
    for (instr  = instrlist_first(ilist);
         instr != NULL;
         instr  = instr_get_next(instr)) {
        uint flags = instr_get_arith_flags(instr, DR_QUERY_DEFAULT);
        if (TESTANY(EFLAGS_READ_6, flags)) {
            ci->read_aflags = true;
            break;
        }
        if (TESTALL(EFLAGS_WRITE_6, flags))
            break;
        if (instr_is_return(instr))
            break;
        if (instr_is_cti(instr)) {
            ci->read_aflags = true;
            break;
        }
    }
    if (ci->read_aflags) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee "PFX" reads aflags from caller\n", ci->start);
    }

    /* If we read or write aflags, we need to reserve a slot to save them.
     * We may or may not use the slot at the call site, but it needs to be
     * reserved just in case.
     */
    if (ci->read_aflags || ci->write_aflags) {
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
        reg_id_t reg = regparms[i];
        if (!ci->reg_used[reg - DR_REG_XAX]) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee "PFX" uses REG %s for arg passing\n",
                ci->start, reg_names[reg]);
            ci->reg_used[reg - DR_REG_XAX] = true;
            callee_info_reserve_slot(ci, SLOT_REG, reg);
        }
    }
}

/* We use push/pop pattern to detect callee saved registers,
 * and assume that the code later won't change those saved value
 * on the stack.
 */
static void
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
    pop_xbp  = INSTR_CREATE_pop(dcontext, opnd_create_reg(DR_REG_XBP));
    /* i#392-c#4: search for frame enter/leave pair */
    enter = NULL;
    leave = NULL;
    for (instr = top; instr != bot; instr = instr_get_next(instr)) {
        if (instr_get_opcode(instr) == OP_enter ||
            instr_same(push_xbp, instr)) {
            enter = instr;
            break;
        }
    }
    if (enter != NULL) {
        for (instr = bot; instr != enter; instr = instr_get_prev(instr)) {
            if (instr_get_opcode(instr) == OP_leave ||
                instr_same(pop_xbp, instr)) {
                leave = instr;
                break;
            }
        }
    }
    /* Check enter/leave pair  */
    if (enter != NULL && leave != NULL &&
        (ci->bwd_tgt == NULL || instr_get_app_pc(enter) <  ci->bwd_tgt) &&
        (ci->fwd_tgt == NULL || instr_get_app_pc(leave) >= ci->fwd_tgt)) {
        /* check if xbp is fp */
        if (instr_get_opcode(enter) == OP_enter) {
            ci->xbp_is_fp = true;
        } else {
            /* i#392-c#2: mov xsp => xbp might not be right after push_xbp */
            for (instr  = instr_get_next(enter);
                 instr != leave;
                 instr  = instr_get_next(instr)) {
                if (instr != NULL &&
                    /* we want to use instr_same to find "mov xsp => xbp",
                     * but it could be OP_mov_ld or OP_mov_st, so use opnds
                     * for comparison instead.
                     */
                    instr_num_srcs(instr) == 1 &&
                    instr_num_dsts(instr) == 1 &&
                    opnd_is_reg(instr_get_src(instr, 0)) &&
                    opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_XSP &&
                    opnd_is_reg(instr_get_dst(instr, 0)) &&
                    opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_XBP) {
                    /* found mov xsp => xbp */
                    ci->xbp_is_fp = true;
                    /* remove it */
                    instrlist_remove(ilist, instr);
                    instr_destroy(GLOBAL_DCONTEXT, instr);
                    break;
                }
            }
        }
        if (ci->xbp_is_fp) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee "PFX" use XBP as frame pointer\n", ci->start);
        } else {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee "PFX" callee-saves reg xbp at "PFX" and "PFX"\n",
                ci->start, instr_get_app_pc(enter), instr_get_app_pc(leave));
            ci->callee_save_regs
                [DR_REG_XBP - DR_REG_XAX] = true;
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
            (ci->fwd_tgt != NULL && instr_get_app_pc(bot) <  ci->fwd_tgt) ||
            instr_is_cti(top) || instr_is_cti(bot))
            break;
        /* XXX: I saw some compiler inserts nop, need to handle. */
        /* push/pop pair check */
        if (instr_get_opcode(top) != OP_push ||
            instr_get_opcode(bot) != OP_pop  ||
            !opnd_same(instr_get_src(top, 0), instr_get_dst(bot, 0)) ||
            !opnd_is_reg(instr_get_src(top, 0)) ||
            opnd_get_reg(instr_get_src(top, 0)) == REG_XSP)
            break;
        /* It is a callee saved reg, we will do our own save for it. */
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee "PFX" callee-saves reg %s at "PFX" and "PFX"\n",
            ci->start, reg_names[opnd_get_reg(instr_get_src(top, 0))],
            instr_get_app_pc(top), instr_get_app_pc(bot));
        ci->callee_save_regs
            [opnd_get_reg(instr_get_src(top, 0)) - DR_REG_XAX] = true;
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

static void
analyze_callee_tls(dcontext_t *dcontext, callee_info_t *ci)
{
    /* access to TLS means we do need to swap/preserve TEB/PEB fields
     * for library isolation (errno, etc.)
     */
    instr_t *instr;
    int i;
    ci->tls_used = false;
    for (instr  = instrlist_first(ci->ilist);
         instr != NULL;
         instr  = instr_get_next(instr)) {
        /* we assume any access via app's tls is to app errno. */
        for (i = 0; i < instr_num_srcs(instr); i++) {
            opnd_t opnd = instr_get_src(instr, i);
            if (opnd_is_far_base_disp(opnd) &&
                opnd_get_segment(opnd) == LIB_SEG_TLS)
                ci->tls_used = true;
        }
        for (i = 0; i < instr_num_dsts(instr); i++) {
            opnd_t opnd = instr_get_dst(instr, i);
            if (opnd_is_far_base_disp(opnd) &&
                opnd_get_segment(opnd) == LIB_SEG_TLS)
                ci->tls_used = true;
        }
    }
    if (ci->tls_used) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee "PFX" accesses far memory\n", ci->start);
    }
}

/* Pick a register to use as a base register pointing to our spill slots.
 * We can't use a register that is:
 * - DR_XSP (need a valid stack in case of fault)
 * - DR_XAX (could be used for args or aflags)
 * - REGPARM_0 on X64 (RDI on Lin and RCX on Win; for N>1 args, avoid REGPARM_<=N)
 * - used by the callee
 */
static void
analyze_callee_pick_spill_reg(dcontext_t *dcontext, callee_info_t *ci)
{
    uint i;
    for (i = 0; i < NUM_GP_REGS; i++) {
        reg_id_t reg = DR_REG_XAX + (reg_id_t)i;
        if (reg == DR_REG_XSP ||
            reg == DR_REG_XAX IF_X64(|| reg == REGPARM_0))
            continue;
        if (!ci->reg_used[i]) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: picking spill reg %s for callee "PFX"\n",
                reg_names[reg], ci->start);
            ci->spill_reg = reg;
            return;
        }
    }

    /* This won't happen unless someone increases CLEANCALL_NUM_INLINE_SLOTS or
     * handles calls with more arguments.  There are at least 8 GPRs, 4 spills,
     * and 3 other regs we can't touch, so one will be available.
     */
    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: failed to pick spill reg for callee "PFX"\n", ci->start);
    /* Fail to inline by setting ci->spill_reg == DR_REG_INVALID. */
    ci->spill_reg = DR_REG_INVALID;
}

static void
analyze_callee_inline(dcontext_t *dcontext, callee_info_t *ci)
{
    instr_t *instr, *next_instr;
    opnd_t opnd, mem_ref, slot;
    bool opt_inline = true;
    int i;

    mem_ref = opnd_create_null();
    /* a set of condition checks */
    if (INTERNAL_OPTION(opt_cleancall) < 2) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: opt_cleancall: %d.\n",
            ci->start, INTERNAL_OPTION(opt_cleancall));
        opt_inline = false;
    }
    if (ci->num_instrs > MAX_NUM_INLINE_INSTRS) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: num of instrs: %d.\n",
            ci->start, ci->num_instrs);
        opt_inline = false;
    }
    if (ci->bwd_tgt != NULL || ci->fwd_tgt != NULL) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: has control flow.\n",
            ci->start);
        opt_inline = false;
    }
    if (ci->num_xmms_used != 0) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: uses XMM.\n",
            ci->start);
        opt_inline = false;
    }
    if (ci->tls_used) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: accesses TLS.\n",
            ci->start);
        opt_inline = false;
    }
    if (ci->spill_reg == DR_REG_INVALID) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined:"
            " unable to pick spill reg.\n", ci->start);
        opt_inline = false;
    }
    if (!SCRATCH_ALWAYS_TLS() || ci->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined:"
            " not enough scratch slots.\n", ci->start);
        opt_inline = false;
    }
    if (!opt_inline) {
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ci->ilist);
        ci->ilist = NULL;
        return;
    }

    /* Now we need scan instructions in the list,
     * check if possible for inline, and convert memory reference
     */
    ci->has_locals = false;
    for (instr  = instrlist_first(ci->ilist);
         instr != NULL;
         instr  = next_instr) {
        uint opc = instr_get_opcode(instr);
        next_instr = instr_get_next(instr);
        /* sanity checks on stack usage */
        if (instr_writes_to_reg(instr, DR_REG_XBP, DR_QUERY_INCLUDE_ALL) &&
            ci->xbp_is_fp) {
            /* xbp must not be changed if xbp is used for frame pointer */
            LOG(THREAD, LOG_CLEANCALL, 1,
                "CLEANCALL: callee "PFX" cannot be inlined: XBP is updated.\n",
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
                if (!opnd_is_base_disp(opnd)           ||
                    opnd_get_base(opnd)  != DR_REG_XSP ||
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
                    "CLEANCALL: removing frame adjustment at "PFX".\n",
                    instr_get_app_pc(instr));
                instrlist_remove(ci->ilist, instr);
                instr_destroy(GLOBAL_DCONTEXT, instr);
                continue;
            } else {
                LOG(THREAD, LOG_CLEANCALL, 1,
                    "CLEANCALL: callee "PFX" cannot be inlined: "
                    "complicated stack pointer update at "PFX".\n",
                    ci->start, instr_get_app_pc(instr));
                break;
            }
        } else if (instr_reg_in_src(instr, DR_REG_XSP) ||
                   (instr_reg_in_src(instr, DR_REG_XBP) && ci->xbp_is_fp)) {
            /* Detect stack address leakage */
            /* lea [xsp/xbp] */
            if (opc == OP_lea)
                opt_inline = false;
            /* any direct use reg xsp or xbp */
            for (i = 0; i < instr_num_srcs(instr); i++) {
                opnd_t src = instr_get_src(instr, i);
                if (opnd_is_reg(src) &&
                    (reg_overlap(REG_XSP, opnd_get_reg(src))  ||
                     (reg_overlap(REG_XBP, opnd_get_reg(src)) && ci->xbp_is_fp)))
                    break;
            }
            if (i != instr_num_srcs(instr))
                opt_inline = false;
            if (!opt_inline) {
                LOG(THREAD, LOG_CLEANCALL, 1,
                    "CLEANCALL: callee "PFX" cannot be inlined: "
                    "stack pointer leaked "PFX".\n",
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
                    (opnd_get_base(opnd) != DR_REG_XBP || !ci->xbp_is_fp))
                    continue;
                if (!ci->has_locals) {
                    /* We see the first one, remember it. */
                    mem_ref = opnd;
                    callee_info_reserve_slot(ci, SLOT_LOCAL, 0);
                    if (ci->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
                        LOG(THREAD, LOG_CLEANCALL, 1,
                            "CLEANCALL: callee "PFX" cannot be inlined: "
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
                        "CLEANCALL: callee "PFX" cannot be inlined: "
                        "more than one stack location is accessed "PFX".\n",
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
                    (opnd_get_base(opnd) != DR_REG_XBP || !ci->xbp_is_fp))
                    continue;
                if (!ci->has_locals) {
                    mem_ref = opnd;
                    callee_info_reserve_slot(ci, SLOT_LOCAL, 0);
                    if (ci->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
                        LOG(THREAD, LOG_CLEANCALL, 1,
                            "CLEANCALL: callee "PFX" cannot be inlined: "
                            "not enough slots for local.\n",
                            ci->start);
                        break;
                    }
                    ci->has_locals = true;
                } else if (!opnd_same(opnd, mem_ref)) {
                    /* currently we only allows one stack refs */
                    LOG(THREAD, LOG_CLEANCALL, 1,
                        "CLEANCALL: callee "PFX" cannot be inlined: "
                        "more than one stack location is accessed "PFX".\n",
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
    if (instr == NULL && opt_inline) {
        ci->opt_inline = true;
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" can be inlined.\n", ci->start);
    } else {
        /* not inline callee, so ilist is not needed. */
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined.\n", ci->start);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ci->ilist);
        ci->ilist = NULL;
    }
}

static void
analyze_callee_ilist(dcontext_t *dcontext, callee_info_t *ci)
{
    ASSERT(!ci->bailout && ci->ilist != NULL);
    /* Remove frame setup and reg pushes before analyzing reg usage. */
    if (INTERNAL_OPTION(opt_cleancall) >= 1) {
        analyze_callee_save_reg(dcontext, ci);
    }
    analyze_callee_regs_usage(dcontext, ci);
    if (INTERNAL_OPTION(opt_cleancall) < 1) {
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ci->ilist);
        ci->ilist = NULL;
    } else {
        analyze_callee_tls(dcontext, ci);
        analyze_callee_pick_spill_reg(dcontext, ci);
        analyze_callee_inline(dcontext, ci);
    }
}

static void
analyze_clean_call_aflags(dcontext_t *dcontext,
                          clean_call_info_t *cci, instr_t *where)
{
    callee_info_t *ci = cci->callee_info;
    instr_t *instr;

    /* If there's a flags read, we clear the flags.  If there's a write or read,
     * we save them, because a read creates a clear which is a write. */
    cci->skip_clear_eflags = !ci->read_aflags;
    cci->skip_save_aflags  = !(ci->write_aflags || ci->read_aflags);
    /* XXX: this is a more aggressive optimization by analyzing the ilist
     * to be instrumented. The client may change the ilist, which violate
     * the analysis result. For example,
     * I do not need save the aflags now if an instruction
     * after "where" updating all aflags, but later the client can
     * insert an instruction reads the aflags before that instruction.
     */
    if (INTERNAL_OPTION(opt_cleancall) > 1 && !cci->skip_save_aflags) {
        for (instr = where; instr != NULL; instr = instr_get_next(instr)) {
            uint flags = instr_get_arith_flags(instr, DR_QUERY_DEFAULT);
            if (TESTANY(EFLAGS_READ_6, flags) || instr_is_cti(instr))
                break;
            if (TESTALL(EFLAGS_WRITE_6, flags)) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: inserting clean call "PFX
                    ", skip saving aflags.\n", ci->start);
                cci->skip_save_aflags = true;
                break;
            }
        }
    }
}

static void
analyze_clean_call_regs(dcontext_t *dcontext, clean_call_info_t *cci)
{
    uint i, num_regparm;
    callee_info_t *info = cci->callee_info;

    /* 1. xmm registers */
    for (i = 0; i < NUM_SIMD_REGS; i++) {
        if (info->xmm_used[i]) {
            cci->xmm_skip[i] = false;
        } else {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call "PFX
                ", skip saving XMM%d.\n", info->start, i);
            cci->xmm_skip[i] = true;
            cci->num_xmms_skip++;
        }
    }
    if (INTERNAL_OPTION(opt_cleancall) > 2 && cci->num_xmms_skip != NUM_SIMD_REGS)
        cci->should_align = false;
    /* 2. general purpose registers */
    /* set regs not to be saved for clean call */
    for (i = 0; i < NUM_GP_REGS; i++) {
        if (info->reg_used[i]) {
            cci->reg_skip[i] = false;
        } else {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call "PFX
                ", skip saving reg %s.\n",
                info->start, reg_names[DR_REG_XAX + (reg_id_t)i]);
            cci->reg_skip[i] = true;
            cci->num_regs_skip++;
        }
    }
    /* we need save/restore rax if save aflags because rax is used */
    if (!cci->skip_save_aflags && cci->reg_skip[0]) {
        LOG(THREAD, LOG_CLEANCALL, 3,
            "CLEANCALL: if inserting clean call "PFX
            ", cannot skip saving reg xax.\n", info->start);
        cci->reg_skip[0] = false;
        cci->num_regs_skip++;
    }
    /* i#987: args are passed via regs in 64-bit, which will clober those regs,
     * so we should not skip any regs that are used for arg passing.
     * XXX: we do not support args passing via XMMs,
     * see docs for dr_insert_clean_call
     * XXX: we can elminate the arg passing instead since it is not used
     * if marked for skip. However, we have to handle cases like some args
     * are used and some are not.
     */
    num_regparm = cci->num_args < NUM_REGPARM ? cci->num_args : NUM_REGPARM;
    for (i = 0; i < num_regparm; i++) {
        if (cci->reg_skip[regparms[i] - DR_REG_XAX]) {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call "PFX
                ", cannot skip saving reg %s due to param passing.\n",
                info->start, reg_names[regparms[i]]);
            cci->reg_skip[regparms[i] - DR_REG_XAX] = false;
            cci->num_regs_skip--;
            /* We cannot call callee_info_reserve_slot for reserving slot
             * on inlining the callee here, because we are in clean call
             * analysis not callee anaysis.
             * Also the reg for arg passing should be first handled in
             * analyze_callee_regs_usage on callee_info creation.
             * If we still reach here, it means the number args changes
             * for the same clean call, so we will not inline it and do not
             * need call callee_info_reserve_slot either.
             */
        }
    }
}

static void
analyze_clean_call_args(dcontext_t *dcontext,
                        clean_call_info_t *cci,
                        opnd_t *args)
{
    uint i, j, num_regparm;

    num_regparm = cci->num_args < NUM_REGPARM ? cci->num_args : NUM_REGPARM;
    /* If a param uses a reg, DR need restore register value, which assumes
     * the full context switch with priv_mcontext_t layout,
     * in which case we need keep priv_mcontext_t layout.
     */
    cci->save_all_regs = false;
    for (i = 0; i < cci->num_args; i++) {
        if (opnd_is_reg(args[i]))
            cci->save_all_regs = true;
        for (j = 0; j < num_regparm; j++) {
            if (opnd_uses_reg(args[i], regparms[j]))
                cci->save_all_regs = true;
        }
    }
    /* We only set cci->reg_skip all to false later if we fail to inline.  We
     * only need to preserve the layout if we're not inlining.
     */
}

static bool
analyze_clean_call_inline(dcontext_t *dcontext, clean_call_info_t *cci)
{
    callee_info_t *info = cci->callee_info;
    bool opt_inline = true;

    if (INTERNAL_OPTION(opt_cleancall) <= 1) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", opt_cleancall %d.\n",
            info->start, INTERNAL_OPTION(opt_cleancall));
        opt_inline = false;
    }
    if (cci->num_args > 1) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", number of args %d > 1.\n",
            info->start, cci->num_args);
        opt_inline = false;
    }
    if (cci->num_args > info->num_args) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX
            ", number of args increases.\n",
            info->start);
        opt_inline = false;
    }
    if (cci->save_fpstate) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", saving fpstate.\n",
            info->start);
        opt_inline = false;
    }
    if (!info->opt_inline) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", complex callee.\n",
            info->start);
        opt_inline = false;
    }
    if (info->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", used %d slots, "
            "> %d available slots.\n",
            info->start, info->slots_used, CLEANCALL_NUM_INLINE_SLOTS);
        opt_inline = false;
    }
    if (!opt_inline) {
        if (cci->save_all_regs) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: inserting clean call "PFX
                ", save all regs in priv_mcontext_t layout.\n",
                info->start);
            cci->num_regs_skip = 0;
            memset(cci->reg_skip, 0, sizeof(bool) * NUM_GP_REGS);
            cci->should_align = true;
        } else {
            uint i;
            for (i = 0; i < NUM_GP_REGS; i++) {
                if (!cci->reg_skip[i] && info->callee_save_regs[i]) {
                    cci->reg_skip[i] = true;
                    cci->num_regs_skip++;
                }
            }
        }
        if (cci->num_xmms_skip == NUM_SIMD_REGS) {
            STATS_INC(cleancall_xmm_skipped);
        }
        if (cci->skip_save_aflags) {
            STATS_INC(cleancall_aflags_save_skipped);
        }
        if (cci->skip_clear_eflags) {
            STATS_INC(cleancall_aflags_clear_skipped);
        }
    } else {
        cci->ilist = instrlist_clone(dcontext, info->ilist);
    }
    return opt_inline;
}

bool
analyze_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instr_t *where,
                   void *callee, bool save_fpstate, bool always_out_of_line,
                   uint num_args, opnd_t *args)
{
    callee_info_t *ci;
    /* by default, no inline optimization */
    bool should_inline = false;

    CLIENT_ASSERT(callee != NULL, "Clean call target is NULL");
    /* 1. init clean_call_info */
    clean_call_info_init(cci, callee, save_fpstate, num_args);
    /* 2. check runtime optimization options */
    if (INTERNAL_OPTION(opt_cleancall) > 0) {
        /* 3. search if callee was analyzed before */
        ci = callee_info_table_lookup(callee);
        /* 4. this callee is not seen before */
        if (ci == NULL) {
            STATS_INC(cleancall_analyzed);
            LOG(THREAD, LOG_CLEANCALL, 2, "CLEANCALL: analyze callee "PFX"\n", callee);
            /* 4.1. create func_info */
            ci = callee_info_create((app_pc)callee, num_args);
            /* 4.2. decode the callee */
            decode_callee_ilist(dcontext, ci);
            /* 4.3. analyze the instrlist */
            if (ci->bailout) {
                callee_info_init(ci);
                ci->start = (app_pc)callee;
            } else
                analyze_callee_ilist(dcontext, ci);
            /* 4.4. add info into callee list */
            ci = callee_info_table_add(ci);
        }
        cci->callee_info = ci;
        if (!ci->bailout) {
            /* 5. aflags optimization analysis */
            analyze_clean_call_aflags(dcontext, cci, where);
            /* 6. register optimization analysis */
            analyze_clean_call_regs(dcontext, cci);
            /* 7. check arguments */
            analyze_clean_call_args(dcontext, cci, args);
            /* 8. inline optimization analysis */
            should_inline = analyze_clean_call_inline(dcontext, cci);
        }
    }
    /* 9. derived fields */
    /* XXX: for x64, skipping a single reg or flags still results in a huge
     * code sequence to put in place: we may want to still use out-of-line
     * unless multiple regs are able to be skipped.
     */
    if ((cci->num_xmms_skip == 0 /* save all xmms */ &&
         cci->num_regs_skip == 0 /* save all regs */ &&
         !cci->skip_save_aflags) ||
        always_out_of_line)
        cci->out_of_line_swap = true;

    return should_inline;
}

static void
insert_inline_reg_save(dcontext_t *dcontext, clean_call_info_t *cci,
                       instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    callee_info_t *ci = cci->callee_info;
    int i;

    /* Don't spill anything if we don't have to. */
    if (cci->num_regs_skip == NUM_GP_REGS && cci->skip_save_aflags &&
        !ci->has_locals) {
        return;
    }

    /* Spill a register to TLS and point it at our unprotected_context_t. */
    PRE(ilist, where, instr_create_save_to_tls
        (dcontext, ci->spill_reg, TLS_XAX_SLOT));
    insert_get_mcontext_base(dcontext, ilist, where, ci->spill_reg);

    /* Save used registers. */
    ASSERT(cci->num_xmms_skip == NUM_SIMD_REGS);
    for (i = 0; i < NUM_GP_REGS; i++) {
        if (!cci->reg_skip[i]) {
            reg_id_t reg_id = DR_REG_XAX + (reg_id_t)i;
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: inlining clean call "PFX", saving reg %s.\n",
                ci->start, reg_names[reg_id]);
            PRE(ilist, where, INSTR_CREATE_mov_st
                (dcontext, callee_info_slot_opnd(ci, SLOT_REG, reg_id),
                 opnd_create_reg(reg_id)));
        }
    }

    /* Save aflags if necessary via XAX, which was just saved if needed. */
    if (!cci->skip_save_aflags) {
        ASSERT(!cci->reg_skip[DR_REG_XAX - DR_REG_XAX]);
        dr_save_arith_flags_to_xax(dcontext, ilist, where);
        PRE(ilist, where, INSTR_CREATE_mov_st
            (dcontext, callee_info_slot_opnd(ci, SLOT_FLAGS, 0),
             opnd_create_reg(DR_REG_XAX)));
        /* Restore app XAX here if it's needed to materialize the argument. */
        if (cci->num_args > 0 && opnd_uses_reg(args[0], DR_REG_XAX)) {
            PRE(ilist, where, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(DR_REG_XAX),
                 callee_info_slot_opnd(ci, SLOT_REG, DR_REG_XAX)));
        }
    }
}

static void
insert_inline_reg_restore(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *where)
{
    int i;
    callee_info_t *ci = cci->callee_info;

    /* Don't restore regs if we don't have to. */
    if (cci->num_regs_skip == NUM_GP_REGS && cci->skip_save_aflags &&
        !ci->has_locals) {
        return;
    }

    /* Restore aflags before regs because it uses xax. */
    if (!cci->skip_save_aflags) {
        PRE(ilist, where, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(DR_REG_XAX),
             callee_info_slot_opnd(ci, SLOT_FLAGS, 0)));
        dr_restore_arith_flags_from_xax(dcontext, ilist, where);
    }

    /* Now restore all registers. */
    for (i = NUM_GP_REGS - 1; i >= 0; i--) {
        if (!cci->reg_skip[i]) {
            reg_id_t reg_id = DR_REG_XAX + (reg_id_t)i;
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: inlining clean call "PFX", restoring reg %s.\n",
                ci->start, reg_names[reg_id]);
            PRE(ilist, where, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(reg_id),
                 callee_info_slot_opnd(ci, SLOT_REG, reg_id)));
        }
    }

    /* Restore reg used for unprotected_context_t pointer. */
    PRE(ilist, where, instr_create_restore_from_tls
        (dcontext, ci->spill_reg, TLS_XAX_SLOT));
}

static void
insert_inline_arg_setup(dcontext_t *dcontext, clean_call_info_t *cci,
                        instrlist_t *ilist, instr_t *where, opnd_t *args)
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
    if (IF_X64_ELSE(!ci->reg_used[regparms[0] - DR_REG_XAX],
                    !ci->has_locals)) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee "PFX" doesn't read arg, skipping arg setup.\n",
            ci->start);
        return;
    }

    ASSERT(cci->num_args == 1);
    arg = args[0];
    regparm = shrink_reg_for_param(IF_X64_ELSE(regparms[0], DR_REG_XAX), arg);

    if (opnd_uses_reg(arg, ci->spill_reg)) {
        if (opnd_is_reg(arg)) {
            /* Trying to pass the spill reg (or a subreg) as the arg. */
            reg_id_t arg_reg = opnd_get_reg(arg);
            arg = opnd_create_tls_slot(os_tls_offset(TLS_XAX_SLOT));
            opnd_set_size(&arg, reg_get_size(arg_reg));
            if (arg_reg == DR_REG_AH ||  /* Don't rely on ordering. */
                arg_reg == DR_REG_BH ||
                arg_reg == DR_REG_CH ||
                arg_reg == DR_REG_DH) {
                /* If it's one of the high sub-registers, add 1 to offset. */
                opnd_set_disp(&arg, opnd_get_disp(arg) + 1);
            }
        } else {
            /* Too complicated to rewrite if it's embedded in the operand.  Just
             * restore spill_reg during the arg materialization.  Hopefully this
             * doesn't happen very often.
             */
            PRE(ilist, where, instr_create_restore_from_tls
                (dcontext, ci->spill_reg, TLS_XAX_SLOT));
            DOLOG(2, LOG_CLEANCALL, {
                char disas_arg[MAX_OPND_DIS_SZ];
                opnd_disassemble_to_buffer(dcontext, arg, disas_arg,
                                           BUFFER_SIZE_ELEMENTS(disas_arg));
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: passing arg %s using spill reg %s to callee "PFX" "
                    "requires extra spills, consider using a different register.\n",
                    disas_arg, reg_names[ci->spill_reg], ci->start);
            });
            restored_spill_reg = true;
        }
    }

    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: inlining clean call "PFX", passing arg via reg %s.\n",
        ci->start, reg_names[regparm]);
    if (opnd_is_immed_int(arg)) {
        PRE(ilist, where, INSTR_CREATE_mov_imm
            (dcontext, opnd_create_reg(regparm), arg));
    } else {
        PRE(ilist, where, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(regparm), arg));
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
        "CLEANCALL: inlining clean call "PFX", passing arg via slot.\n",
        ci->start);
    PRE(ilist, where, INSTR_CREATE_mov_st
        (dcontext, callee_info_slot_opnd(ci, SLOT_LOCAL, 0),
         opnd_create_reg(DR_REG_XAX)));
#endif
}

void
insert_inline_clean_call(dcontext_t *dcontext, clean_call_info_t *cci,
                         instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    instrlist_t *callee = cci->ilist;
    instr_t *instr;

    ASSERT(cci->ilist != NULL);
    ASSERT(SCRATCH_ALWAYS_TLS());
    /* 0. update stats */
    STATS_INC(cleancall_inlined);
    /* 1. save registers */
    insert_inline_reg_save(dcontext, cci, ilist, where, args);
    /* 2. setup parameters */
    insert_inline_arg_setup(dcontext, cci, ilist, where, args);
    /* 3. inline clean call ilist */
    instr = instrlist_first(callee);
    while (instr != NULL) {
        instrlist_remove(callee, instr);
        /* XXX: if client has a xl8 handler we assume it will handle any faults
         * in the callee (which should already have a translation set to the
         * callee): and if not we assume there will be no such faults.
         * We can't have a translation with no handler.
         */
        if (IF_CLIENT_INTERFACE_ELSE(!dr_xl8_hook_exists(), true))
            instr_set_translation(instr, NULL);
        instrlist_meta_preinsert(ilist, where, instr);
        instr = instrlist_first(callee);
    }
    instrlist_destroy(dcontext, callee);
    cci->ilist = NULL;
    /* 4. restore registers */
    insert_inline_reg_restore(dcontext, cci, ilist, where);
    /* XXX: the inlined code looks like this
     *   mov    %rax -> %gs:0x00
     *   mov    %rdi -> %gs:0x01
     *   mov    $0x00000003 -> %edi
     *   mov    <rel> 0x0000000072200c00 -> %rax
     *   movsxd %edi -> %rdi
     *   add    %rdi (%rax) -> (%rax)
     *   mov    %gs:0x00 -> %rax
     *   mov    %gs:0x01 -> %rdi
     *   ...
     * we can do some constant propagation optimization here,
     * leave it for higher optimization level.
     */
}

void
clean_call_opt_init(void)
{
    callee_info_init(&default_callee_info);
    callee_info_table_init();
}

void
clean_call_opt_exit(void)
{
    callee_info_table_destroy();
}

#else /* CLIENT_INTERFACE */

/* Stub implementation ifndef CLIENT_INTERFACE.  Initializes cci and returns
 * false for no inlining.  We use dr_insert_clean_call internally, but we don't
 * need it to do inlining.
 */
bool
analyze_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instr_t *where,
                   void *callee, bool save_fpstate, bool always_out_of_line,
                   uint num_args, opnd_t *args)
{
    CLIENT_ASSERT(callee != NULL, "Clean call target is NULL");
    /* 1. init clean_call_info */
    clean_call_info_init(cci, callee, save_fpstate, num_args);
    return false;
}

#endif /* CLIENT_INTERFACE */

/***************************************************************************/
