/* **********************************************************
 * Copyright (c) 2013-2015 Google, Inc.   All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* DynamoRIO Register Management Extension: a mediator for
 * selecting, preserving, and using registers among multiple
 * instrumentation components.
 */

/* XXX i#511: currently the whole interface is tied to drmgr.
 * Should we also provide an interface that works on standalone instrlists?
 * Distinguish by name, "drregi_*" or sthg.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drvector.h"
#include "drreg.h"
#include <string.h>
#include <limits.h>

#ifdef DEBUG
# define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#else
# define ASSERT(x, msg) /* nothing */
#endif

#define TESTALL(mask, var) (((mask) & (var)) == (mask))
#define TESTANY(mask, var) (((mask) & (var)) != 0)
#define TEST TESTANY

#define PRE instrlist_meta_preinsert

/* This should be pretty hard to exceed as there aren't this many GPRs */
#define MAX_SPILLS (SPILL_SLOT_MAX + 8)

#define AFLAGS_SLOT 0 /* always */

/* We support using GPR registers only: [DR_REG_START_GPR..DR_REG_STOP_GPR] */

#define REG_DEAD ((void*)(ptr_uint_t)0)
#define REG_LIVE ((void*)(ptr_uint_t)1)

typedef struct _reg_info_t {
    /* XXX: better to flip around and store bitvector of registers per instr
     * in a single drvector_t?
     */
    /* The live vector holds one entry per app instr in the bb.
     * For registers, each vector entry holds REG_{LIVE,DEAD}.
     * For aflags, each vector entry holds a ptr_uint_t with the EFLAGS_READ_ARITH bits
     * telling which arithmetic flags are live at that point.
     */
    drvector_t live;
    bool in_use;
    uint app_uses; /* # of uses in this bb by app */

    /* Where is the app value for this reg? */
    bool native;   /* app value is in original app reg */
    reg_id_t xchg; /* if !native && != REG_NULL, value was exchanged w/ this dead reg */
    int slot;      /* if !native && xchg==REG_NULL, value is in this TLS slot # */
} reg_info_t;

/* We use this in per_thread_t.slot_use[] and other places */
#define DR_REG_EFLAGS DR_REG_INVALID

typedef struct _per_thread_t {
    instr_t *cur_instr;
    int live_idx;
    reg_info_t reg[DR_NUM_GPR_REGS];
    reg_info_t aflags;
    uint app_uses_min, app_uses_max;
    reg_id_t slot_use[MAX_SPILLS]; /* holds the reg_id_t of which reg is inside */
} per_thread_t;

static drreg_options_t ops;

static int tls_idx = -1;
static uint tls_slot_offs;
static reg_id_t tls_seg;

#ifdef DEBUG
static uint stats_max_slot;
#endif

/***************************************************************************
 * SPILLING AND RESTORING
 */

static uint
find_free_slot(per_thread_t *pt)
{
    uint i;
    for (i = 0; i < MAX_SPILLS; i++) {
        if (pt->slot_use[i] == DR_REG_NULL)
            return i;
    }
    return MAX_SPILLS;
}

/* Up to caller to update pt->reg.  This routine updates pt->slot_use. */
static void
spill_reg(void *drcontext, per_thread_t *pt, reg_id_t reg, uint slot,
          instrlist_t *ilist, instr_t *where)
{
    ASSERT(pt->slot_use[slot] == DR_REG_NULL, "internal tracking error");
    pt->slot_use[slot] = reg;
    if (slot < ops.num_spill_slots) {
        dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                                tls_slot_offs + slot*sizeof(ptr_uint_t), reg);
    } else {
        dr_spill_slot_t DR_slot = (dr_spill_slot_t)(slot - ops.num_spill_slots);
        dr_save_reg(drcontext, ilist, where, reg, DR_slot);
    }
#ifdef DEBUG
    if (slot > stats_max_slot)
        stats_max_slot = slot; /* racy but that's ok */
#endif
}

/* Up to caller to update pt->reg.  This routine updates pt->slot_use if release==true. */
static void
restore_reg(void *drcontext, per_thread_t *pt, reg_id_t reg, uint slot,
            instrlist_t *ilist, instr_t *where, bool release)
{
    ASSERT(pt->slot_use[slot] == reg, "internal tracking error");
    if (release)
        pt->slot_use[slot] = DR_REG_NULL;
    if (slot < ops.num_spill_slots) {
        dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                               tls_slot_offs + slot*sizeof(ptr_uint_t), reg);
    } else {
        dr_spill_slot_t DR_slot = (dr_spill_slot_t)(slot - ops.num_spill_slots);
        dr_restore_reg(drcontext, ilist, where, reg, DR_slot);
    }
}

DR_EXPORT
drreg_status_t
drreg_max_slots_used(OUT uint *max)
{
#ifdef DEBUG
    if (max == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    *max = stats_max_slot;
    return DRREG_SUCCESS;
#else
    return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
#endif
}

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

static void
count_app_uses(per_thread_t *pt, opnd_t opnd)
{
    int i;
    for (i = 0; i < opnd_num_regs_used(opnd); i++) {
        reg_id_t reg = opnd_get_reg_used(opnd, i);
        if (reg_is_gpr(reg)) {
            reg = reg_to_pointer_sized(reg);
            pt->reg[reg-DR_REG_START_GPR].app_uses++;
            /* Tools that instrument memory uses (memtrace, Dr. Memory, etc.)
             * want to double-count memory opnd uses, as they need to restore
             * the app value to get the memory address into a register there.
             * We go ahead and do that for all tools.
             */
            if (opnd_is_memory_reference(opnd))
                pt->reg[reg-DR_REG_START_GPR].app_uses++;
        }
    }
}

static dr_emit_flags_t
drreg_event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                        bool for_trace, bool translating, OUT void **user_data)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    instr_t *inst;
    ptr_uint_t aflags_new, aflags_cur = 0;
    uint index = 0;
    reg_id_t reg;

    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++)
        pt->reg[reg-DR_REG_START_GPR].app_uses = 0;

    /* Reverse scan is more efficient.  This means our indices are also reversed. */
    for (inst = instrlist_last(bb); inst != NULL; inst = instr_get_prev(inst)) {
        /* We consider both meta and app instrs, to handle rare cases of meta instrs
         * being inserted during app2app for corner cases.
         */

        bool xfer = (instr_is_cti(inst) || instr_is_interrupt(inst) ||
                     instr_is_syscall(inst));

        /* GPR liveness */
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            void *value = REG_LIVE;
            if (xfer || instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS))
                value = REG_LIVE;
            /* make sure we don't consider writes to sub-regs */
            else if (instr_writes_to_exact_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS)
                     /* a write to a 32-bit reg for amd64 zeroes the top 32 bits */
                     IF_X86_X64(|| instr_writes_to_exact_reg(inst, reg_64_to_32(reg),
                                                             DR_QUERY_INCLUDE_COND_SRCS)))
                value = REG_DEAD;
            else if (index > 0)
                value = drvector_get_entry(&pt->reg[reg-DR_REG_START_GPR].live, index-1);
            drvector_set_entry(&pt->reg[reg-DR_REG_START_GPR].live, index, value);
        }

        /* aflags liveness */
        aflags_new = instr_get_arith_flags(inst, DR_QUERY_INCLUDE_COND_SRCS);
        if (xfer)
            aflags_cur = EFLAGS_READ_ARITH; /* assume flags are read before written */
        else {
            uint aflags_cur, aflags_read, aflags_w2r;
            if (index == 0)
                aflags_cur = EFLAGS_READ_ARITH; /* assume flags are read before written */
            else {
                aflags_cur = (uint)(ptr_uint_t)
                    drvector_get_entry(&pt->aflags.live, index-1);
            }
            aflags_read = (aflags_new & EFLAGS_READ_ARITH);
            /* if a flag is read by inst, set the read bit */
            aflags_cur |= (aflags_new & EFLAGS_READ_ARITH);
            /* if a flag is written and not read by inst, clear the read bit */
            aflags_w2r = EFLAGS_WRITE_TO_READ(aflags_new & EFLAGS_WRITE_ARITH);
            aflags_cur &= ~(aflags_w2r & ~aflags_read);
        }
        drvector_set_entry(&pt->aflags.live, index, (void *)(ptr_uint_t)aflags_cur);

        if (instr_is_app(inst)) {
            int i;
            for (i = 0; i < instr_num_dsts(inst); i++)
                count_app_uses(pt, instr_get_dst(inst, i));
            for (i = 0; i < instr_num_srcs(inst); i++)
                count_app_uses(pt, instr_get_src(inst, i));
        }

        index++;
    }

    pt->live_idx = index;

    pt->app_uses_min = UINT_MAX;
    pt->app_uses_max = 0;
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (pt->reg[reg-DR_REG_START_GPR].app_uses > pt->app_uses_max)
            pt->app_uses_max = pt->reg[reg-DR_REG_START_GPR].app_uses;
        if (pt->reg[reg-DR_REG_START_GPR].app_uses < pt->app_uses_min)
            pt->app_uses_min = pt->reg[reg-DR_REG_START_GPR].app_uses;
    }

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drreg_event_bb_insert_pre(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                          bool for_trace, bool translating, void *user_data)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    pt->cur_instr = inst;
    pt->live_idx--; /* counts backward */
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drreg_event_bb_insert_post(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                           bool for_trace, bool translating, void *user_data)
{
    /* FIXME i#511 NYI: we need to:
     * + for unreserved regs still spilled, lazily do the restore here.
     * + for still-reserved regs, restore or re-spill app regs that are read/written
     *   and put the tool's value somewhere else during the app instr.
     */
    return DR_EMIT_DEFAULT;
}

/***************************************************************************
 * REGISTER RESERVATION
 */

drreg_status_t
drreg_reserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                        drvector_t *reg_allowed, OUT reg_id_t *reg_out)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    uint slot = find_free_slot(pt);
    uint min_uses = UINT_MAX;
    reg_id_t reg, best_reg = DR_REG_NULL;
    ASSERT(drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_INSERTION,
           "must be called from drmgr insertion phase");
    if (reg_out == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (slot == MAX_SPILLS)
        return DRREG_ERROR_OUT_OF_SLOTS;

    /* Pick a register */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        uint idx = reg - DR_REG_START_GPR;
        if (pt->reg[idx].in_use)
            continue;
        if (reg == dr_get_stolen_reg() IF_ARM(|| reg == DR_REG_PC))
            continue;
        if (reg_allowed != NULL && drvector_get_entry(reg_allowed, idx) == NULL)
            continue;
        /* If we had a hint as to local vs whole-bb we could downgrade being
         * dead right now as a priority
         */
        if (drvector_get_entry(&pt->reg[idx].live, pt->live_idx) == REG_DEAD)
            break;
        if (pt->reg[idx].app_uses == pt->app_uses_min)
            break;
        if (pt->reg[idx].app_uses < min_uses) {
            best_reg = reg;
            min_uses = pt->reg[idx].app_uses;
        }
    }
    if (reg > DR_REG_STOP_GPR) {
        if (best_reg != DR_REG_NULL)
            reg = best_reg;
        else
            return DRREG_ERROR_REG_CONFLICT;
    }

    pt->reg[reg-DR_REG_START_GPR].in_use = true;
    if (drvector_get_entry(&pt->reg[reg-DR_REG_START_GPR].live, pt->live_idx) == REG_LIVE)
        spill_reg(drcontext, pt, reg, slot, ilist, where);
    /* Even if dead now, we need to own a slot for it in case reserved past dead point */
    pt->reg[reg-DR_REG_START_GPR].native = false;
    pt->reg[reg-DR_REG_START_GPR].xchg = DR_REG_NULL;
    pt->reg[reg-DR_REG_START_GPR].slot = slot;
    *reg_out = reg;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_get_app_value(void *drcontext, instrlist_t *ilist, instr_t *where,
                    reg_id_t app_reg, reg_id_t dst_reg)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    if (!reg_is_pointer_sized(app_reg) || !reg_is_pointer_sized(dst_reg))
        return DRREG_ERROR_INVALID_PARAMETER;

    /* check if app_reg is stolen reg */
    if (app_reg == dr_get_stolen_reg()) {
        if (dr_insert_get_stolen_reg_value(drcontext, ilist, where, dst_reg))
            return DRREG_SUCCESS;
        ASSERT(false, "internal error on getting stolen reg app value");
        return DRREG_ERROR;
    }
    /* check if app_reg is an unreserved reg */
    if (!pt->reg[app_reg-DR_REG_START_GPR].in_use) {
        PRE(ilist, where, XINST_CREATE_move(drcontext,
                                            opnd_create_reg(dst_reg),
                                            opnd_create_reg(app_reg)));
        return DRREG_SUCCESS;
    }

    /* we may lost the app value for a dead reg */
    if (drvector_get_entry(&pt->reg[app_reg-DR_REG_START_GPR].live,
                           pt->live_idx) != REG_LIVE)
        return DRREG_ERROR_NO_APP_VALUE;
    /* restore app value back to app_reg */
    if (pt->reg[app_reg-DR_REG_START_GPR].xchg != DR_REG_NULL) {
        /* FIXME i#511: NYI */
        return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
    }
    restore_reg(drcontext, pt, app_reg,
                pt->reg[app_reg-DR_REG_START_GPR].slot,
                ilist, where, false);
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_unreserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg)
{
    /* FIXME i#511: lazily restore: wait in case someone else wants a local scratch.
     * Wait for next app instr or even until next app read/write (==
     * drreg_event_bb_insert_post).
     */

    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    if (!pt->reg[reg-DR_REG_START_GPR].in_use)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (drvector_get_entry(&pt->reg[reg-DR_REG_START_GPR].live, pt->live_idx) ==
        REG_LIVE) {
        if (pt->reg[reg-DR_REG_START_GPR].xchg != DR_REG_NULL) {
            /* FIXME i#511: NYI */
            return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
        }
        restore_reg(drcontext, pt, reg, pt->reg[reg-DR_REG_START_GPR].slot,
                    ilist, where, true);
    }
    pt->reg[reg-DR_REG_START_GPR].in_use = false;
    pt->reg[reg-DR_REG_START_GPR].native = true;
    return DRREG_SUCCESS;
}

/***************************************************************************
 * ARITHMETIC FLAGS
 */

DR_EXPORT
drreg_status_t
drreg_reserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    uint aflags = (uint)(ptr_uint_t) drvector_get_entry(&pt->aflags.live, pt->live_idx);
#ifdef X86
    uint temp_slot = find_free_slot(pt);
#elif defined(ARM)
    drreg_status_t res = DRREG_SUCCESS;
    reg_id_t scratch;
#endif
    ASSERT(drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_INSERTION,
           "must be called from drmgr insertion phase");
    /* Just like scratch regs, flags are exclusively owned */
    if (pt->aflags.in_use)
        return DRREG_ERROR_IN_USE;
    if (!TESTANY(EFLAGS_READ_ARITH, aflags)) {
        pt->aflags.in_use = true;
        pt->aflags.native = true;
        return DRREG_SUCCESS;
    }

#ifdef X86
    if (temp_slot == MAX_SPILLS)
        return DRREG_ERROR_OUT_OF_SLOTS;
    if (pt->reg[DR_REG_XAX-DR_REG_START_GPR].in_use) {
        /* No way to tell whoever is using xax that we need it */
        /* FIXME i#511: pick an unreserved reg, spill it, and put xax there
         * temporarily.  Store aflags in our dedicated aflags tls slot.
         */
        return DRREG_ERROR_REG_CONFLICT;
    }
    if (drvector_get_entry(&pt->reg[DR_REG_XAX-DR_REG_START_GPR].live, pt->live_idx) ==
        REG_LIVE)
        spill_reg(drcontext, pt, DR_REG_XAX, temp_slot, ilist, where);
    PRE(ilist, where, INSTR_CREATE_lahf(drcontext));
    if (TEST(EFLAGS_READ_OF, aflags)) {
        PRE(ilist, where,
            INSTR_CREATE_setcc(drcontext, OP_seto, opnd_create_reg(DR_REG_AL)));
    }
    /* XXX i#511: as an optimization we could keep the flags in xax itself where
     * possible.  For now, for simplicity, we always put into aflags TLS slot.
     */
    spill_reg(drcontext, pt, DR_REG_XAX, AFLAGS_SLOT, ilist, where);
    if (drvector_get_entry(&pt->reg[DR_REG_XAX-DR_REG_START_GPR].live, pt->live_idx) ==
        REG_LIVE)
        restore_reg(drcontext, pt, DR_REG_XAX, temp_slot, ilist, where, true);
#elif defined(ARM)
    res = drreg_reserve_register(drcontext, ilist, where, NULL, &scratch);
    if (res != DRREG_SUCCESS)
        return res;
    dr_save_arith_flags_to_reg(drcontext, ilist, where, scratch);
    spill_reg(drcontext, pt, scratch, AFLAGS_SLOT, ilist, where);
    res = drreg_unreserve_register(drcontext, ilist, where, scratch);
    if (res != DRREG_SUCCESS)
        return res; /* XXX: undo already-inserted instrs? */
#endif

    pt->aflags.in_use = true;
    pt->aflags.native = false;
    pt->aflags.xchg = DR_REG_NULL;
    pt->aflags.slot = AFLAGS_SLOT;
    return DRREG_SUCCESS;
}

DR_EXPORT
drreg_status_t
drreg_unreserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    /* XXX i#1551:: restore lazily: wait in case someone else wants a local scratch.
     * wait for next app instr or even until next app read/write (in
     * drreg_event_bb_insert_post()).
     */
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    uint aflags = (uint)(ptr_uint_t) drvector_get_entry(&pt->aflags.live, pt->live_idx);
#ifdef X86
    uint temp_slot = find_free_slot(pt);
#elif defined(ARM)
    drreg_status_t res = DRREG_SUCCESS;
    reg_id_t scratch;
#endif
    ASSERT(drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_INSERTION,
           "must be called from drmgr insertion phase");
    pt->aflags.in_use = false;
    if (!TESTANY(EFLAGS_READ_ARITH, aflags))
        return true;
#ifdef X86
    if (pt->reg[DR_REG_XAX-DR_REG_START_GPR].in_use) {
        /* FIXME i#511: pick an unreserved reg, spill it, and put xax there
         * temporarily.
         */
        return DRREG_ERROR_REG_CONFLICT;
    }
    if (temp_slot == MAX_SPILLS)
        return DRREG_ERROR_OUT_OF_SLOTS;
    if (drvector_get_entry(&pt->reg[DR_REG_XAX-DR_REG_START_GPR].live, pt->live_idx) ==
        REG_LIVE)
        spill_reg(drcontext, pt, DR_REG_XAX, temp_slot, ilist, where);
    restore_reg(drcontext, pt, DR_REG_XAX, AFLAGS_SLOT, ilist, where, true);
    if (TEST(EFLAGS_READ_OF, aflags)) {
        PRE(ilist, where, INSTR_CREATE_add
            (drcontext, opnd_create_reg(DR_REG_AL), OPND_CREATE_INT8(0x7f)));
    }
    PRE(ilist, where, INSTR_CREATE_sahf(drcontext));
    if (drvector_get_entry(&pt->reg[DR_REG_XAX-DR_REG_START_GPR].live, pt->live_idx) ==
        REG_LIVE)
        restore_reg(drcontext, pt, DR_REG_XAX, temp_slot, ilist, where, true);
#elif defined(ARM)
    res = drreg_reserve_register(drcontext, ilist, where, NULL, &scratch);
    if (res != DRREG_SUCCESS)
        return res;
    restore_reg(drcontext, pt, scratch, AFLAGS_SLOT, ilist, where, true);
    dr_restore_arith_flags_from_reg(drcontext, ilist, where, scratch);
    res = drreg_unreserve_register(drcontext, ilist, where, scratch);
    if (res != DRREG_SUCCESS)
        return res; /* XXX: undo already-inserted instrs? */
#endif
    return DRREG_SUCCESS;
}

DR_EXPORT
drreg_status_t
drreg_aflags_liveness(void *drcontext, OUT uint *value)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    ASSERT(drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_INSERTION,
           "must be called from drmgr insertion phase");
    if (value == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    *value = (uint)(ptr_uint_t) drvector_get_entry(&pt->aflags.live, pt->live_idx);
    return DRREG_SUCCESS;
}

DR_EXPORT
drreg_status_t
drreg_are_aflags_dead(void *drcontext, instr_t *inst, bool *dead)
{
    uint flags;
    drreg_status_t res = drreg_aflags_liveness(drcontext, &flags);
    if (res != DRREG_SUCCESS)
        return res;
    if (dead == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    *dead = !TESTANY(EFLAGS_READ_ARITH, flags);
    return DRREG_SUCCESS;
}

/***************************************************************************
 * RESTORE STATE
 */

static bool
drreg_event_restore_state(void *drcontext, bool restore_memory,
                          dr_restore_state_info_t *info)
{
    /* FIXME i#511: To achieve a clean and simple reserve-and-unreserve interface w/o
     * specifying up front how many whole-bb regs are needed, we have to pay with a
     * complex state xl8 scheme.  We need to decode the in-cache fragment and walk
     * it, recognizing our own spills and restores.
     */
    return true;
}

/***************************************************************************
 * INIT AND EXIT
 */

static int drreg_init_count;

static void
drreg_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *) dr_thread_alloc(drcontext, sizeof(*pt));
    reg_id_t reg;
    drmgr_set_tls_field(drcontext, tls_idx, (void *) pt);

    memset(pt, 0, sizeof(*pt));
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        drvector_init(&pt->reg[reg-DR_REG_START_GPR].live, 20,
                      false/*!synch*/, NULL);
        pt->reg[reg-DR_REG_START_GPR].native = true;
    }
    drvector_init(&pt->aflags.live, 20, false/*!synch*/, NULL);
}

static void
drreg_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    reg_id_t reg;
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        drvector_delete(&pt->reg[reg-DR_REG_START_GPR].live);
    }
    drvector_delete(&pt->aflags.live);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

DR_EXPORT
drreg_status_t
drreg_init(drreg_options_t *ops_in)
{
    drmgr_priority_t high_priority = {
        sizeof(high_priority), DRMGR_PRIORITY_NAME_DRREG_HIGH, NULL, NULL,
        DRMGR_PRIORITY_INSERT_DRREG_HIGH
    };
    drmgr_priority_t low_priority = {
        sizeof(low_priority), DRMGR_PRIORITY_NAME_DRREG_LOW, NULL, NULL,
        DRMGR_PRIORITY_INSERT_DRREG_LOW
    };
    drmgr_priority_t fault_priority = {
        sizeof(fault_priority), DRMGR_PRIORITY_NAME_DRREG_FAULT, NULL, NULL,
        DRMGR_PRIORITY_FAULT_DRREG
    };

    int count = dr_atomic_add32_return_sum(&drreg_init_count, 1);
    /* XXX i#511: instead of allowing only one drreg_init() and all other
     * components to be passed in scratch regs by a master,
     * we could instead shift true init to
     * drreg_thread_init() and thus consider all callers' requests?  For
     * the num_ fields we can easily sum them: but what about if we
     * later add bool or other fields that are harder to combine?
     */
    if (count > 1)
        return DRREG_SUCCESS;

    if (ops_in->struct_size < sizeof(ops))
        return DRREG_ERROR_INVALID_PARAMETER;
    ops = *ops_in;

    drmgr_init();

    if (!drmgr_register_thread_init_event(drreg_thread_init) ||
        !drmgr_register_thread_exit_event(drreg_thread_exit))
        return DRREG_ERROR;
    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return DRREG_ERROR;

    if (!drmgr_register_bb_instrumentation_event
        (drreg_event_bb_analysis, drreg_event_bb_insert_pre, &high_priority) ||
        !drmgr_register_bb_instrumentation_event
        (NULL, drreg_event_bb_insert_post, &low_priority) ||
        !drmgr_register_restore_state_ex_event_ex
        (drreg_event_restore_state, &fault_priority))
        return DRREG_ERROR;

    if (!dr_raw_tls_calloc(&tls_seg, &tls_slot_offs, ops.num_spill_slots, 0))
        return DRREG_ERROR;

    return DRREG_SUCCESS;
}

DR_EXPORT
drreg_status_t
drreg_exit(void)
{
    int count = dr_atomic_add32_return_sum(&drreg_init_count, -1);
    if (count != 0)
        return DRREG_SUCCESS;

    if (!drmgr_unregister_thread_init_event(drreg_thread_init) ||
        !drmgr_unregister_thread_exit_event(drreg_thread_exit))
        return DRREG_ERROR;

    drmgr_unregister_tls_field(tls_idx);
    if (!drmgr_unregister_bb_instrumentation_event(drreg_event_bb_analysis) ||
        !drmgr_unregister_bb_insertion_event(drreg_event_bb_insert_post) ||
        !drmgr_unregister_restore_state_ex_event(drreg_event_restore_state))
        return DRREG_ERROR;

    drmgr_exit();

    if (!dr_raw_tls_cfree(tls_slot_offs, ops.num_spill_slots))
        return DRREG_ERROR;

    return DRREG_SUCCESS;
}

