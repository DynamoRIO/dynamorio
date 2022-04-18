/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
 * Copyright (c) 2003-2007 Determina Corp.
 * Copyright (c) 2001-2003 Massachusetts Institute of Technology
 * Copyright (c) 2000-2001 Hewlett-Packard Company
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

/* file "clean_call_opt_shared.c" */

#include "../globals.h"
#include "arch.h"
#include "instrument.h"
#include "../hashtable.h"
#include "disassemble.h"
#include "instr_create_shared.h"
#include "clean_call_opt.h"

/****************************************************************************
 * clean call callee info table for i#42 and i#43
 */

/* hashtable for storing analyzed callee info */
static generic_table_t *callee_info_table;
/* we only free callee info at exit, when callee_info_table_exit is true. */
static bool callee_info_table_exit = false;
#define INIT_HTABLE_SIZE_CALLEE 6 /* should remain small */

static void
callee_info_init(callee_info_t *ci)
{
    int i;
    memset(ci, 0, sizeof(*ci));
    ci->bailout = true;
    /* to be conservative */
    ci->has_locals = true;
    ci->write_flags = true;
    ci->read_flags = true;
    ci->tls_used = true;
    /* We use loop here and memset in analyze_callee_regs_usage later.
     * We could reverse the logic and use memset to set the value below,
     * but then later in analyze_callee_regs_usage, we have to use the loop.
     */
    /* assuming all [xyz]mm registers are used */
    ci->num_simd_used = proc_num_simd_registers();
    for (i = 0; i < proc_num_simd_registers(); i++)
        ci->simd_used[i] = true;
#ifdef X86
    ci->num_opmask_used = proc_num_opmask_registers();
    for (i = 0; i < proc_num_opmask_registers(); i++)
        ci->opmask_used[i] = true;
#endif
    for (i = 0; i < DR_NUM_GPR_REGS; i++)
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
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ci, callee_info_t, ACCT_CLEANCALL, PROTECTED);
}

static callee_info_t *
callee_info_create(app_pc start, uint num_args)
{
    callee_info_t *info;

    info = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, callee_info_t, ACCT_CLEANCALL, PROTECTED);
    callee_info_init(info);
    info->start = start;
    info->num_args = num_args;
    return info;
}

void
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
            "kind %d value %d\n",
            kind, value);
    }
    /* We check if slots_used > CLEANCALL_NUM_INLINE_SLOTS to detect failure. */
    ci->slots_used++;
}

opnd_t
callee_info_slot_opnd(callee_info_t *ci, slot_kind_t kind, reg_id_t value)
{
    uint i;
    if (kind == SLOT_REG)
        value = dr_reg_fixer[value];
    for (i = 0; i < BUFFER_SIZE_ELEMENTS(ci->scratch_slots); i++) {
        if (ci->scratch_slots[i].kind == kind && ci->scratch_slots[i].value == value) {
            int disp = (int)offsetof(unprotected_context_t, inline_spill_slots[i]);
            return opnd_create_base_disp(ci->spill_reg, DR_REG_NULL, 0, disp, OPSZ_PTR);
        }
    }
    ASSERT_MESSAGE(CHKLVL_ASSERTS,
                   "Tried to find scratch slot for value "
                   "without calling callee_info_reserve_slot for it",
                   false);
    return opnd_create_null();
}

static void
callee_info_table_init(void)
{
    callee_info_table = generic_hash_create(
        GLOBAL_DCONTEXT, INIT_HTABLE_SIZE_CALLEE, 80 /* load factor: not perf-critical */,
        HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
        (void (*)(dcontext_t *, void *))callee_info_free _IF_DEBUG("callee-info table"));
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
    ci = generic_hash_lookup(GLOBAL_DCONTEXT, callee_info_table, (ptr_uint_t)callee);
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
    info = generic_hash_lookup(GLOBAL_DCONTEXT, callee_info_table, (ptr_uint_t)ci->start);
    if (info == NULL) {
        info = ci;
        generic_hash_add(GLOBAL_DCONTEXT, callee_info_table, (ptr_uint_t)ci->start,
                         (void *)ci);
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

/* The max number of instructions the callee can have for inline. */
#define MAX_NUM_INLINE_INSTRS 20

/* Decode instruction from callee and return the next_pc to be decoded. */
static app_pc
decode_callee_instr(dcontext_t *dcontext, callee_info_t *ci, app_pc instr_pc)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    app_pc next_pc = NULL;

    instr = instr_create(GLOBAL_DCONTEXT);
    instrlist_append(ilist, instr);
    ci->num_instrs++;
    TRY_EXCEPT(
        dcontext, { next_pc = decode(GLOBAL_DCONTEXT, instr_pc, instr); },
        { /* EXCEPT */
          LOG(THREAD, LOG_CLEANCALL, 2,
              "CLEANCALL: crash on decoding callee instruction at: " PFX "\n", instr_pc);
          ASSERT_CURIOSITY(false && "crashed while decoding clean call");
          ci->bailout = true;
          return NULL;
        });
    if (!instr_valid(instr)) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: decoding invalid instruction at: " PFX "\n", instr_pc);
        ci->bailout = true;
        return NULL;
    }
    instr_set_translation(instr, instr_pc);
    DOLOG(3, LOG_CLEANCALL, { disassemble_with_bytes(dcontext, instr_pc, THREAD); });
    return next_pc;
}

/* check newly decoded instruction from callee */
static app_pc
check_callee_instr(dcontext_t *dcontext, callee_info_t *ci, app_pc next_pc)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    app_pc cur_pc, tgt_pc;

    if (next_pc == NULL)
        return NULL;
    instr = instrlist_last(ilist);
    cur_pc = instr_get_app_pc(instr);
    ASSERT(next_pc == cur_pc + instr_length(dcontext, instr));
    if (!instr_is_cti(instr)) {
        /* special instructions, bail out. */
        if (instr_is_syscall(instr) || instr_is_interrupt(instr)) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: bail out on syscall or interrupt at: " PFX "\n", cur_pc);
            ci->bailout = true;
            return NULL;
        }
        return next_pc;
    } else { /* cti instruc */
        if (instr_is_mbr(instr)) {
            /* check if instr is return, and if return is the last instr. */
            if (!instr_is_return(instr) || ci->fwd_tgt > cur_pc) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: bail out on indirect branch at: " PFX "\n", cur_pc);
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
                "CLEANCALL: callee calls out at: " PFX " to " PFX "\n", cur_pc, tgt_pc);
            /* check special PIC code:
             * 1. call next_pc; pop r1;
             * or
             * 2. call pic_func;
             *    and in pic_func: mov [%xsp] %r1; ret;
             */
            if (INTERNAL_OPTION(opt_cleancall) >= 1)
                return check_callee_instr_level2(dcontext, ci, next_pc, cur_pc, tgt_pc);
        } else { /* ubr or cbr */
            tgt_pc = opnd_get_pc(instr_get_target(instr));
            if (tgt_pc < cur_pc) { /* backward branch */
                if (tgt_pc < ci->start) {
                    LOG(THREAD, LOG_CLEANCALL, 2,
                        "CLEANCALL: bail out on out-of-range branch at: " PFX "to " PFX
                        "\n",
                        cur_pc, tgt_pc);
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
    app_pc tgt_pc;
    if (!ci->bailout) {
        /* no target pc of any branch is in a middle of an instruction,
         * replace target pc with target instr
         */
        ret = instrlist_last(ilist);
        /* must be RETURN, otherwise, bugs in decode_callee_ilist */
        ASSERT(instr_is_return(ret));
        for (cti = instrlist_first(ilist); cti != ret; cti = instr_get_next(cti)) {
            if (!instr_is_cti(cti))
                continue;
            ASSERT(!instr_is_mbr(cti));
            tgt_pc = opnd_get_pc(instr_get_target(cti));
            for (tgt = instrlist_first(ilist); tgt != NULL; tgt = instr_get_next(tgt)) {
                if (tgt_pc == instr_get_app_pc(tgt))
                    break;
            }
            if (tgt == NULL) {
                /* cannot find a target instruction, bail out */
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: bail out on strange internal branch at: " PFX "to " PFX
                    "\n",
                    instr_get_app_pc(cti), tgt_pc);
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

    LOG(THREAD, LOG_CLEANCALL, 2, "CLEANCALL: decoding callee starting at: " PFX "\n",
        ci->start);
    ci->bailout = false;
    while (cur_pc != NULL) {
        cur_pc = decode_callee_instr(dcontext, ci, cur_pc);
        cur_pc = check_callee_instr(dcontext, ci, cur_pc);
    }
    check_callee_ilist(dcontext, ci);
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
    for (i = 0; i < DR_NUM_GPR_REGS; i++) {
        reg_id_t reg = DR_REG_START_GPR + (reg_id_t)i;
        if (reg == DR_REG_XSP IF_X86(|| reg == DR_REG_XAX) IF_X86_64(|| reg == REGPARM_0))
            continue;
        if (!ci->reg_used[i]) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: picking spill reg %s for callee " PFX "\n", reg_names[reg],
                ci->start);
            ci->spill_reg = reg;
            return;
        }
    }

    /* This won't happen unless someone increases CLEANCALL_NUM_INLINE_SLOTS or
     * handles calls with more arguments.  There are at least 8 GPRs, 4 spills,
     * and 3 other regs we can't touch, so one will be available.
     */
    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: failed to pick spill reg for callee " PFX "\n", ci->start);
    /* Fail to inline by setting ci->spill_reg == DR_REG_INVALID. */
    ci->spill_reg = DR_REG_INVALID;
}

static void
analyze_callee_inline(dcontext_t *dcontext, callee_info_t *ci)
{
    bool opt_inline = true;

    /* a set of condition checks */
    if (INTERNAL_OPTION(opt_cleancall) < 2) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee " PFX " cannot be inlined: opt_cleancall: %d.\n",
            ci->start, INTERNAL_OPTION(opt_cleancall));
        opt_inline = false;
    }
    if (ci->num_instrs > MAX_NUM_INLINE_INSTRS) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee " PFX " cannot be inlined: num of instrs: %d.\n",
            ci->start, ci->num_instrs);
        opt_inline = false;
    }
    if (ci->bwd_tgt != NULL || ci->fwd_tgt != NULL) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee " PFX " cannot be inlined: has control flow.\n",
            ci->start);
        opt_inline = false;
    }
    if (ci->num_simd_used != 0) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee " PFX " cannot be inlined: uses SIMD.\n", ci->start);
        opt_inline = false;
    }
#ifdef X86
    if (ci->num_opmask_used != 0) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee " PFX " cannot be inlined: uses mask register.\n",
            ci->start);
        opt_inline = false;
    }
#endif
    if (ci->tls_used) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee " PFX " cannot be inlined: accesses TLS.\n", ci->start);
        opt_inline = false;
    }
    if (ci->spill_reg == DR_REG_INVALID) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee " PFX " cannot be inlined:"
            " unable to pick spill reg.\n",
            ci->start);
        opt_inline = false;
    }
    if (!SCRATCH_ALWAYS_TLS() || ci->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee " PFX " cannot be inlined:"
            " not enough scratch slots.\n",
            ci->start);
        opt_inline = false;
    }
    if (!opt_inline) {
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ci->ilist);
        ci->ilist = NULL;
        return;
    }

    /* Check if possible for inline, and convert memory references */
    if (!check_callee_ilist_inline(dcontext, ci))
        opt_inline = false;

    if (opt_inline) {
        ci->opt_inline = true;
        LOG(THREAD, LOG_CLEANCALL, 1, "CLEANCALL: callee " PFX " can be inlined.\n",
            ci->start);
    } else {
        /* not inline callee, so ilist is not needed. */
        LOG(THREAD, LOG_CLEANCALL, 1, "CLEANCALL: callee " PFX " cannot be inlined.\n",
            ci->start);
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
analyze_clean_call_regs(dcontext_t *dcontext, clean_call_info_t *cci)
{
    int i, num_regparm;
    callee_info_t *info = cci->callee_info;

    /* 1. xmm registers */
    for (i = 0; i < proc_num_simd_registers(); i++) {
        if (info->simd_used[i]) {
            cci->simd_skip[i] = false;
        } else {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call " PFX ", skip saving [XYZ]MM%d.\n",
                info->start, i);
            cci->simd_skip[i] = true;
            cci->num_simd_skip++;
        }
    }
#ifdef X86
    for (i = 0; i < proc_num_opmask_registers(); i++) {
        if (info->opmask_used[i]) {
            cci->opmask_skip[i] = false;
        } else {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call " PFX ", skip saving k%d.\n",
                info->start, i);
            cci->opmask_skip[i] = true;
            cci->num_opmask_skip++;
        }
    }
#endif
    if (INTERNAL_OPTION(opt_cleancall) > 2 &&
        cci->num_simd_skip != proc_num_simd_registers())
        cci->should_align = false;
    /* 2. general purpose registers */
    /* set regs not to be saved for clean call */
    for (i = 0; i < DR_NUM_GPR_REGS; i++) {
        if (info->reg_used[i]) {
            cci->reg_skip[i] = false;
        } else {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call " PFX ", skip saving reg %s.\n",
                info->start, reg_names[DR_REG_START_GPR + (reg_id_t)i]);
            cci->reg_skip[i] = true;
            cci->num_regs_skip++;
        }
    }

#ifdef X86
    /* we need save/restore rax if save aflags because rax is used */
    if (!cci->skip_save_flags && cci->reg_skip[0]) {
        LOG(THREAD, LOG_CLEANCALL, 3,
            "CLEANCALL: if inserting clean call " PFX ", cannot skip saving reg xax.\n",
            info->start);
        cci->reg_skip[0] = false;
        cci->num_regs_skip--;
    }
#endif

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
        if (cci->reg_skip[d_r_regparms[i] - DR_REG_START_GPR]) {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call " PFX
                ", cannot skip saving reg %s due to param passing.\n",
                info->start, reg_names[d_r_regparms[i]]);
            cci->reg_skip[d_r_regparms[i] - DR_REG_START_GPR] = false;
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
analyze_clean_call_args(dcontext_t *dcontext, clean_call_info_t *cci, opnd_t *args)
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
            if (opnd_uses_reg(args[i], d_r_regparms[j]))
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
            "CLEANCALL: fail inlining clean call " PFX ", opt_cleancall %d.\n",
            info->start, INTERNAL_OPTION(opt_cleancall));
        opt_inline = false;
    }
    if (cci->num_args > 1) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call " PFX ", number of args %d > 1.\n",
            info->start, cci->num_args);
        opt_inline = false;
    }
    if (cci->num_args > info->num_args) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call " PFX ", number of args increases.\n",
            info->start);
        opt_inline = false;
    }
    if (cci->save_fpstate) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call " PFX ", saving fpstate.\n",
            info->start);
        opt_inline = false;
    }
    if (!info->opt_inline) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call " PFX ", complex callee.\n",
            info->start);
        opt_inline = false;
    }
    if (info->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call " PFX ", used %d slots, "
            "> %d available slots.\n",
            info->start, info->slots_used, CLEANCALL_NUM_INLINE_SLOTS);
        opt_inline = false;
    }
    if (!opt_inline) {
        if (cci->save_all_regs) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: inserting clean call " PFX
                ", save all regs in priv_mcontext_t layout.\n",
                info->start);
            cci->num_regs_skip = 0;
            memset(cci->reg_skip, 0, sizeof(bool) * DR_NUM_GPR_REGS);
            cci->should_align = true;
        } else {
            uint i;
            for (i = 0; i < DR_NUM_GPR_REGS; i++) {
                if (!cci->reg_skip[i] && info->callee_save_regs[i]) {
                    cci->reg_skip[i] = true;
                    cci->num_regs_skip++;
                }
            }
        }
        if (cci->num_simd_skip == proc_num_simd_registers()) {
            STATS_INC(cleancall_simd_skipped);
        }
#ifdef X86
        if (proc_num_opmask_registers() > 0 &&
            cci->num_opmask_skip == proc_num_opmask_registers()) {
            STATS_INC(cleancall_opmask_skipped);
        }
#endif
        if (cci->skip_save_flags) {
            STATS_INC(cleancall_aflags_save_skipped);
        }
        if (cci->skip_clear_flags) {
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
            LOG(THREAD, LOG_CLEANCALL, 2, "CLEANCALL: analyze callee " PFX "\n", callee);
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

    /* Thresholds for out-of-line calls. The values are based on a guess. The bar
     * for generating out-of-line calls is quite low, so the code size is kept low.
     */
#ifdef X86
    /* Use out-of-line calls if more than 3 SIMD registers or 3 mask registers need to be
     * saved.
     */
#    define SIMD_SAVE_THRESHOLD 3
#    define OPMASK_SAVE_THRESHOLD 3
#    ifdef X64
    /* Use out-of-line calls if more than 3 GP registers need to be saved. */
#        define GPR_SAVE_THRESHOLD 3
#    else
    /* On X86, a single pusha instruction is used to save the GPRs, so we do not take
     * the number of GPRs that need saving into account.
     */
#        define GPR_SAVE_THRESHOLD DR_NUM_GPR_REGS
#    endif
#elif defined(AARCH64)
    /* Use out-of-line calls if more than 6 SIMD registers need to be saved. */
#    define SIMD_SAVE_THRESHOLD 6
    /* Use out-of-line calls if more than 6 GP registers need to be saved. */
#    define GPR_SAVE_THRESHOLD 6
#endif

#if defined(X86) || defined(AARCH64)
    /* 9. derived fields */
    /* Use out-of-line calls if more than SIMD_SAVE_THRESHOLD SIMD registers have
     * to be saved or if more than GPR_SAVE_THRESHOLD GP registers have to be saved.
     * For AVX-512, a threshold of 3 mask registers has been added.
     * XXX: This should probably be in arch-specific clean_call_opt.c.
     */
    if ((proc_num_simd_registers() - cci->num_simd_skip) > SIMD_SAVE_THRESHOLD ||
        IF_X86((proc_num_opmask_registers() - cci->num_opmask_skip) >
                   OPMASK_SAVE_THRESHOLD ||)(DR_NUM_GPR_REGS - cci->num_regs_skip) >
            GPR_SAVE_THRESHOLD ||
        always_out_of_line)
        cci->out_of_line_swap = true;
#endif

    return should_inline;
}

void
insert_inline_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *where, opnd_t *args)
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
        if (!dr_xl8_hook_exists())
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
