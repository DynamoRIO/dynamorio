/* **********************************************************
 * Copyright (c) 2013-2016 Google, Inc.   All rights reserved.
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
#include "../ext_utils.h"
#include <string.h>
#include <limits.h>
#include <stddef.h> /* offsetof */

#ifdef DEBUG
# define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
# define LOG(dc, mask, level, ...) dr_log(dc, mask, level, __VA_ARGS__)
#else
# define ASSERT(x, msg) /* nothing */
# define LOG(dc, mask, level, ...) /* nothing */
#endif

#ifdef WINDOWS
# define DISPLAY_ERROR(msg) dr_messagebox(msg)
#else
# define DISPLAY_ERROR(msg) dr_fprintf(STDERR, "%s\n", msg);
#endif

#define PRE instrlist_meta_preinsert

/* This should be pretty hard to exceed as there aren't this many GPRs */
#define MAX_SPILLS (SPILL_SLOT_MAX + 8)

#define AFLAGS_SLOT 0 /* always */

/* We support using GPR registers only: [DR_REG_START_GPR..DR_REG_STOP_GPR] */

#define REG_DEAD ((void*)(ptr_uint_t)0)
#define REG_LIVE ((void*)(ptr_uint_t)1)
#define REG_UNKNOWN ((void*)(ptr_uint_t)2) /* only used outside drmgr insert phase */

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
    /* With lazy restore, and b/c we must set native to false, we need to record
     * whether we spilled or not (we could instead record live_idx at time of
     * reservation).
     */
    bool ever_spilled;

    /* Where is the app value for this reg? */
    bool native;   /* app value is in original app reg */
    reg_id_t xchg; /* if !native && != REG_NULL, value was exchanged w/ this dead reg */
    int slot;      /* if !native && xchg==REG_NULL, value is in this TLS slot # */
} reg_info_t;

/* We use this in per_thread_t.slot_use[] and other places */
#define DR_REG_EFLAGS DR_REG_INVALID

#define GPR_IDX(reg) ((reg) - DR_REG_START_GPR)

typedef struct _per_thread_t {
    instr_t *cur_instr;
    int live_idx;
    reg_info_t reg[DR_NUM_GPR_REGS];
    reg_info_t aflags;
    reg_id_t slot_use[MAX_SPILLS]; /* holds the reg_id_t of which reg is inside */
    int pending_unreserved; /* count of to-be-lazily-restored unreserved regs */
    /* We store the linear address of our TLS for access from another thread: */
    byte *tls_seg_base;
} per_thread_t;

static drreg_options_t ops;

static int tls_idx = -1;
static uint tls_slot_offs;
static reg_id_t tls_seg;

#ifdef DEBUG
static uint stats_max_slot;
#endif

static drreg_status_t
drreg_restore_reg_now(void *drcontext, instrlist_t *ilist, instr_t *inst,
                      per_thread_t *pt, reg_id_t reg);

static drreg_status_t
drreg_restore_aflags(void *drcontext, instrlist_t *ilist, instr_t *where,
                     per_thread_t *pt, bool release);

static drreg_status_t
drreg_spill_aflags(void *drcontext, instrlist_t *ilist, instr_t *where,
                   per_thread_t *pt);

static void
drreg_report_error(drreg_status_t res, const char *msg)
{
    if (ops.error_callback != NULL) {
        if ((*ops.error_callback)(res))
            return;
    }
    ASSERT(false, msg);
    DISPLAY_ERROR(msg);
    dr_abort();
}

/***************************************************************************
 * SPILLING AND RESTORING
 */

static uint
find_free_slot(per_thread_t *pt)
{
    uint i;
    /* 0 is always reserved for AFLAGS_SLOT */
    ASSERT(AFLAGS_SLOT == 0, "AFLAGS_SLOT is not 0");
    for (i = AFLAGS_SLOT+1; i < MAX_SPILLS; i++) {
        if (pt->slot_use[i] == DR_REG_NULL)
            return i;
    }
    return MAX_SPILLS;
}

/* Up to caller to update pt->reg, including .ever_spilled.
 * This routine updates pt->slot_use.
 */
static void
spill_reg(void *drcontext, per_thread_t *pt, reg_id_t reg, uint slot,
          instrlist_t *ilist, instr_t *where)
{
    ASSERT(pt->slot_use[slot] == DR_REG_NULL ||
           pt->slot_use[slot] == reg, "internal tracking error");
    pt->slot_use[slot] = reg;
    if (slot < ops.num_spill_slots) {
        dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                                tls_slot_offs + slot*sizeof(reg_t), reg);
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
    ASSERT(pt->slot_use[slot] == reg ||
           /* aflags can be saved and restored using different regs */
           (slot == AFLAGS_SLOT && pt->slot_use[slot] != DR_REG_NULL),
           "internal tracking error");
    if (release)
        pt->slot_use[slot] = DR_REG_NULL;
    if (slot < ops.num_spill_slots) {
        dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                               tls_slot_offs + slot*sizeof(reg_t), reg);
    } else {
        dr_spill_slot_t DR_slot = (dr_spill_slot_t)(slot - ops.num_spill_slots);
        dr_restore_reg(drcontext, ilist, where, reg, DR_slot);
    }
}

static reg_t
get_spilled_value(void *drcontext, uint slot)
{
    if (slot < ops.num_spill_slots) {
        per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
        return *(reg_t *)
            (pt->tls_seg_base + tls_slot_offs + slot*sizeof(reg_t));
    } else {
        dr_spill_slot_t DR_slot = (dr_spill_slot_t)(slot - ops.num_spill_slots);
        return dr_read_saved_reg(drcontext, DR_slot);
    }
}

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
            pt->reg[GPR_IDX(reg)].app_uses++;
            /* Tools that instrument memory uses (memtrace, Dr. Memory, etc.)
             * want to double-count memory opnd uses, as they need to restore
             * the app value to get the memory address into a register there.
             * We go ahead and do that for all tools.
             */
            if (opnd_is_memory_reference(opnd))
                pt->reg[GPR_IDX(reg)].app_uses++;
        }
    }
}

/* This event has to go last, to handle labels inserted by other components:
 * else our indices get off, and we can't simply skip labels in the
 * per-instr event b/c we need the liveness to advance at the label
 * but not after the label.
 */
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
        pt->reg[GPR_IDX(reg)].app_uses = 0;

    /* Reverse scan is more efficient.  This means our indices are also reversed. */
    for (inst = instrlist_last(bb); inst != NULL; inst = instr_get_prev(inst)) {
        /* We consider both meta and app instrs, to handle rare cases of meta instrs
         * being inserted during app2app for corner cases.
         */

        bool xfer = (instr_is_cti(inst) || instr_is_interrupt(inst) ||
                     instr_is_syscall(inst));

        /* GPR liveness */
        LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX":", __FUNCTION__,
            index, instr_get_app_pc(inst));
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            void *value = REG_LIVE;
            /* DRi#1849: COND_SRCS here includes addressing regs in dsts */
            if (instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS))
                value = REG_LIVE;
            /* make sure we don't consider writes to sub-regs */
            else if (instr_writes_to_exact_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS)
                     /* a write to a 32-bit reg for amd64 zeroes the top 32 bits */
                     IF_X86_X64(|| instr_writes_to_exact_reg(inst, reg_64_to_32(reg),
                                                             DR_QUERY_INCLUDE_COND_SRCS)))
                value = REG_DEAD;
            else if (xfer)
                value = REG_LIVE;
            else if (index > 0)
                value = drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, index-1);
            LOG(drcontext, LOG_ALL, 3, " %s=%d", get_register_name(reg),
                (int)(ptr_uint_t)value);
            drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, index, value);
        }

        /* aflags liveness */
        aflags_new = instr_get_arith_flags(inst, DR_QUERY_INCLUDE_COND_SRCS);
        if (xfer)
            aflags_cur = EFLAGS_READ_ARITH; /* assume flags are read before written */
        else {
            uint aflags_read, aflags_w2r;
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
        LOG(drcontext, LOG_ALL, 3, " flags=%d\n", aflags_cur);
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

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drreg_event_bb_insert_early(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                          bool for_trace, bool translating, void *user_data)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    pt->cur_instr = inst;
    pt->live_idx--; /* counts backward */
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drreg_event_bb_insert_late(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                           bool for_trace, bool translating, void *user_data)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    reg_id_t reg;
    instr_t *next = instr_get_next(inst);
    bool restored_for_read[DR_NUM_GPR_REGS];
    drreg_status_t res;

    /* For unreserved regs still spilled, we lazily do the restore here.  We also
     * update reserved regs wrt app uses.
     */

    /* Before each app read, or at end of bb, restore aflags to app value */
    uint aflags = (uint)(ptr_uint_t) drvector_get_entry(&pt->aflags.live, pt->live_idx);
    if (!pt->aflags.native &&
        (drmgr_is_last_instr(drcontext, inst) ||
         TESTANY(EFLAGS_READ_ARITH, instr_get_eflags(inst, DR_QUERY_DEFAULT)) ||
         /* Writing just a subset needs to combine with the original unwritten */
         (TESTANY(EFLAGS_WRITE_ARITH, instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)) &&
          aflags != 0 /*0 means everything is dead*/))) {
        /* Restore aflags to app value */
        LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX" aflags=0x%x: lazily restoring aflags\n",
            __FUNCTION__, pt->live_idx, instr_get_app_pc(inst), aflags);
        res = drreg_restore_aflags(drcontext, bb, inst, pt, false/*keep slot*/);
        if (res != DRREG_SUCCESS)
            drreg_report_error(res, "failed to restore flags before app read");
        if (!pt->aflags.in_use) {
            pt->aflags.native = true;
            pt->slot_use[AFLAGS_SLOT] = DR_REG_NULL;
        }
    }

    /* Before each app read, or at end of bb, restore spilled registers to app values: */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        restored_for_read[GPR_IDX(reg)] = false;
        if (!pt->reg[GPR_IDX(reg)].native) {
            if (drmgr_is_last_instr(drcontext, inst) ||
                instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_ALL) ||
                /* Treat a partial write as a read, to restore rest of reg */
                (instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL) &&
                 !instr_writes_to_exact_reg(inst, reg, DR_QUERY_INCLUDE_ALL)) ||
                /* Treat a conditional write as a read and a write to handle the
                 * condition failing and our write handling saving the wrong value.
                 */
                (instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL) &&
                 !instr_writes_to_reg(inst, reg, DR_QUERY_DEFAULT))) {
                if (!pt->reg[GPR_IDX(reg)].in_use) {
                    LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": lazily restoring %s\n",
                        __FUNCTION__, pt->live_idx, instr_get_app_pc(inst),
                        get_register_name(reg));
                    res = drreg_restore_reg_now(drcontext, bb, inst, pt, reg);
                    if (res != DRREG_SUCCESS)
                        drreg_report_error(res, "lazy restore failed");
                    ASSERT(pt->pending_unreserved > 0, "should not go negative");
                    pt->pending_unreserved--;
                } else {
                    /* We need to move the tool's value somewhere else.
                     * We use a separate slot for that (and we document that
                     * tools should request an extra slot for each cross-app-instr
                     * register).
                     * XXX: optimize via xchg w/ a dead reg.
                     */
                    uint tmp_slot = find_free_slot(pt);
                    if (tmp_slot == MAX_SPILLS) {
                        drreg_report_error(DRREG_ERROR_OUT_OF_SLOTS,
                                           "failed to preserve tool val around app read");
                    }
                    /* The approach:
                     *   + spill reg (tool val) to new slot
                     *   + restore to reg (app val) from app slot
                     *   + <app instr>
                     *   + restore to reg (tool val) from new slot
                     * XXX: if we change this, we need to update
                     * drreg_event_restore_state().
                     */
                    LOG(drcontext, LOG_ALL, 3,
                        "%s @%d."PFX": restoring %s for app read\n", __FUNCTION__,
                        pt->live_idx, instr_get_app_pc(inst), get_register_name(reg));
                    spill_reg(drcontext, pt, reg, tmp_slot, bb, inst);
                    restore_reg(drcontext, pt, reg,
                                pt->reg[GPR_IDX(reg)].slot,
                                bb, inst, false/*keep slot*/);
                    restore_reg(drcontext, pt, reg, tmp_slot, bb, next, true);
                    /* Share the tool val spill if this inst writes too */
                    restored_for_read[GPR_IDX(reg)] = true;
                    /* We keep .native==false */
                }
            }
       }
    }

    /* After aflags write by app, update spilled app value */
    if (TESTANY(EFLAGS_WRITE_ARITH, instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)) &&
        /* Is everything written later? */
        (pt->live_idx == 0 ||
         (ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx-1) != 0)) {
        if (pt->aflags.in_use) {
            LOG(drcontext, LOG_ALL, 3,
                "%s @%d."PFX": re-spilling aflags after app write\n",
                __FUNCTION__, pt->live_idx, instr_get_app_pc(inst));
            res = drreg_spill_aflags(drcontext, bb, next/*after*/, pt);
            if (res != DRREG_SUCCESS) {
                drreg_report_error(res, "failed to spill aflags after app write");
            }
            pt->aflags.native = false;
        } else if (!pt->aflags.native) {
            /* give up slot */
            pt->slot_use[AFLAGS_SLOT] = DR_REG_NULL;
            pt->aflags.native = true;
        }
    }

    /* After each app write, update spilled app values: */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (pt->reg[GPR_IDX(reg)].in_use) {
            if (instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL) &&
                /* Don't bother if reg is dead beyond this write */
                (ops.conservative ||
                 pt->live_idx == 0 ||
                 drvector_get_entry(&pt->reg[GPR_IDX(reg)].live,
                                    pt->live_idx-1) == REG_LIVE)) {
                uint tmp_slot = MAX_SPILLS;
                if (pt->reg[GPR_IDX(reg)].xchg != DR_REG_NULL) {
                    /* XXX i#511: NYI */
                    drreg_report_error(DRREG_ERROR_FEATURE_NOT_AVAILABLE, "xchg NYI");
                }
                /* Approach (we share 1st and last w/ read, if reads and writes):
                 *   + spill reg (tool val) to new slot
                 *   + <app instr>
                 *   + spill reg (app val) to app slot
                 *   + restore to reg from new slot (tool val)
                 * XXX: if we change this, we need to update
                 * drreg_event_restore_state().
                 */
                LOG(drcontext, LOG_ALL, 3,
                    "%s @%d."PFX": re-spilling %s after app write\n", __FUNCTION__,
                    pt->live_idx, instr_get_app_pc(inst), get_register_name(reg));
                if (!restored_for_read[GPR_IDX(reg)]) {
                    tmp_slot = find_free_slot(pt);
                    if (tmp_slot == MAX_SPILLS) {
                        drreg_report_error(DRREG_ERROR_OUT_OF_SLOTS,
                                           "failed to preserve tool val wrt app write");
                    }
                    spill_reg(drcontext, pt, reg, tmp_slot, bb, inst);
                }
                spill_reg(drcontext, pt, reg,
                          pt->reg[GPR_IDX(reg)].slot, bb, next/*after*/);
                pt->reg[GPR_IDX(reg)].ever_spilled = true;
                if (!restored_for_read[GPR_IDX(reg)])
                    restore_reg(drcontext, pt, reg, tmp_slot, bb, next/*after*/, true);
            }
        } else if (!pt->reg[GPR_IDX(reg)].native &&
                   instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL)) {
            /* For an unreserved reg that's written, just drop the slot */
            ASSERT(!pt->reg[GPR_IDX(reg)].ever_spilled, "reg was dead");
            res = drreg_restore_reg_now(drcontext, bb, inst, pt, reg);
            if (res != DRREG_SUCCESS)
                drreg_report_error(res, "slot release on app write failed");
            pt->pending_unreserved--;
        }
    }

#ifdef DEBUG
    if (drmgr_is_last_instr(drcontext, inst)) {
        uint i;
        reg_id_t reg;
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            ASSERT(!pt->aflags.in_use, "user failed to unreserve aflags");
            ASSERT(pt->aflags.native, "user failed to unreserve aflags");
            ASSERT(!pt->reg[GPR_IDX(reg)].in_use, "user failed to unreserve a register");
            ASSERT(pt->reg[GPR_IDX(reg)].native, "user failed to unreserve a register");
        }
        for (i = 0; i < MAX_SPILLS; i++) {
            ASSERT(pt->slot_use[i] == DR_REG_NULL, "user failed to unreserve a register");
        }
    }
#endif

    return DR_EMIT_DEFAULT;
}

/***************************************************************************
 * USE OUTSIDE INSERT PHASE
 */

/* For use outside drmgr's insert phase where we don't know the bounds of the
 * app instrs, we fall back to a more expensive liveness analysis on each
 * insertion.
 *
 * XXX: we'd want to add a new API for instru2instru that takes in
 * both the save and restore points at once to allow keeping aflags in
 * eax and other optimizations.
 */
static drreg_status_t
drreg_forward_analysis(void *drcontext, instr_t *start)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    instr_t *inst;
    ptr_uint_t aflags_new, aflags_cur = 0;
    reg_id_t reg;

    /* We just use index 0 of the live vectors */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        pt->reg[GPR_IDX(reg)].app_uses = 0;
        drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, 0, REG_UNKNOWN);
        pt->reg[GPR_IDX(reg)].ever_spilled = false;
    }

    /* We have to consider meta instrs as well */
    for (inst = start; inst != NULL; inst = instr_get_next(inst)) {
        if (instr_is_cti(inst) || instr_is_interrupt(inst) || instr_is_syscall(inst))
            break;

        /* GPR liveness */
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            void *value = REG_UNKNOWN;
            if (drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, 0) != REG_UNKNOWN)
                continue;
            /* DRi#1849: COND_SRCS here includes addressing regs in dsts */
            if (instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS))
                value = REG_LIVE;
            /* make sure we don't consider writes to sub-regs */
            else if (instr_writes_to_exact_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS)
                     /* a write to a 32-bit reg for amd64 zeroes the top 32 bits */
                     IF_X86_X64(|| instr_writes_to_exact_reg(inst, reg_64_to_32(reg),
                                                             DR_QUERY_INCLUDE_COND_SRCS)))
                value = REG_DEAD;
            if (value != REG_UNKNOWN)
                drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, 0, value);
        }

        /* aflags liveness */
        aflags_new = instr_get_arith_flags(inst, DR_QUERY_INCLUDE_COND_SRCS);
        /* reading and writing counts only as reading */
        aflags_new &= (~(EFLAGS_READ_TO_WRITE(aflags_new)));
        /* reading doesn't count if already written */
        aflags_new &= (~(EFLAGS_WRITE_TO_READ(aflags_cur)));
        aflags_cur |= aflags_new;

        if (instr_is_app(inst)) {
            int i;
            for (i = 0; i < instr_num_dsts(inst); i++)
                count_app_uses(pt, instr_get_dst(inst, i));
            for (i = 0; i < instr_num_srcs(inst); i++)
                count_app_uses(pt, instr_get_src(inst, i));
        }
    }

    pt->live_idx = 0;

    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, 0) == REG_UNKNOWN)
            drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, 0, REG_LIVE);
    }
    drvector_set_entry(&pt->aflags.live, 0, (void *)(ptr_uint_t)
                       /* set read bit if not written */
                       (EFLAGS_READ_ARITH & (~(EFLAGS_WRITE_TO_READ(aflags_cur)))));
    return DRREG_SUCCESS;
}

/***************************************************************************
 * REGISTER RESERVATION
 */

drreg_status_t
drreg_init_and_fill_vector(drvector_t *vec, bool allowed)
{
    reg_id_t reg;
    if (vec == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    drvector_init(vec, DR_NUM_GPR_REGS, false/*!synch*/, NULL);
    for (reg = 0; reg < DR_NUM_GPR_REGS; reg++)
        drvector_set_entry(vec, reg, allowed ? (void *)(ptr_uint_t)1 : NULL);
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_set_vector_entry(drvector_t *vec, reg_id_t reg, bool allowed)
{
    if (vec == NULL || reg < DR_REG_START_GPR || reg > DR_REG_STOP_GPR)
        return DRREG_ERROR_INVALID_PARAMETER;
    drvector_set_entry(vec, reg - DR_REG_START_GPR,
                       allowed ? (void *)(ptr_uint_t)1 : NULL);
    return DRREG_SUCCESS;
}

/* Assumes liveness info is already set up in per_thread_t */
static drreg_status_t
drreg_reserve_reg_internal(void *drcontext, instrlist_t *ilist, instr_t *where,
                           drvector_t *reg_allowed, bool only_if_no_spill,
                           OUT reg_id_t *reg_out)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    uint slot = MAX_SPILLS;
    uint min_uses = UINT_MAX;
    reg_id_t reg = DR_REG_STOP_GPR + 1, best_reg = DR_REG_NULL;
    bool already_spilled = false;
    if (reg_out == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;

    /* First, try to use a previously unreserved but not yet lazily restored reg.
     * This must be first to avoid accumulating slots beyond the requested max.
     * Because we drop an unreserved reg when the app writes to it, we should
     * never pick an unreserved and unspilled yet not currently dead reg over
     * some other dead reg.
     */
    if (pt->pending_unreserved > 0) {
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            uint idx = GPR_IDX(reg);
            if (!pt->reg[idx].native && !pt->reg[idx].in_use &&
                (reg_allowed == NULL || drvector_get_entry(reg_allowed, idx) != NULL) &&
                (!only_if_no_spill || pt->reg[idx].ever_spilled ||
                 drvector_get_entry(&pt->reg[idx].live, pt->live_idx) == REG_DEAD)) {
                slot = pt->reg[idx].slot;
                pt->pending_unreserved--;
                already_spilled = pt->reg[idx].ever_spilled;
                LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": using un-restored %s slot %d\n",
                    __FUNCTION__, pt->live_idx, instr_get_app_pc(where),
                    get_register_name(reg), slot);
                break;
            }
        }
    }

    if (reg > DR_REG_STOP_GPR) {
        /* Look for a dead register, or the least-used register */
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            uint idx = GPR_IDX(reg);
            if (pt->reg[idx].in_use)
                continue;
            if (reg == dr_get_stolen_reg()
                IF_ARM(|| reg == DR_REG_PC)
                /* Avoid xsp, even if it appears dead in things like OP_sysenter */
                IF_X86(|| reg == DR_REG_XSP))
                continue;
            if (reg_allowed != NULL && drvector_get_entry(reg_allowed, idx) == NULL)
                continue;
            /* If we had a hint as to local vs whole-bb we could downgrade being
             * dead right now as a priority
             */
            if (drvector_get_entry(&pt->reg[idx].live, pt->live_idx) == REG_DEAD)
                break;
            if (only_if_no_spill)
                continue;
            if (pt->reg[idx].app_uses < min_uses) {
                best_reg = reg;
                min_uses = pt->reg[idx].app_uses;
            }
        }
    }
    if (reg > DR_REG_STOP_GPR) {
        if (best_reg != DR_REG_NULL)
            reg = best_reg;
        else
            return DRREG_ERROR_REG_CONFLICT;
    }
    if (slot == MAX_SPILLS) {
        slot = find_free_slot(pt);
        if (slot == MAX_SPILLS)
            return DRREG_ERROR_OUT_OF_SLOTS;
    }

    ASSERT(!pt->reg[GPR_IDX(reg)].in_use, "overlapping uses");
    pt->reg[GPR_IDX(reg)].in_use = true;
    if (!already_spilled) {
        /* Even if dead now, we need to own a slot in case reserved past dead point */
        if (ops.conservative ||
            drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, pt->live_idx) ==
            REG_LIVE) {
            LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": spilling %s to slot %d\n",
                __FUNCTION__, pt->live_idx, instr_get_app_pc(where),
                get_register_name(reg), slot);
            spill_reg(drcontext, pt, reg, slot, ilist, where);
            pt->reg[GPR_IDX(reg)].ever_spilled = true;
        } else {
            LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": no need to spill %s to slot %d\n",
                __FUNCTION__, pt->live_idx, instr_get_app_pc(where),
                get_register_name(reg), slot);
            pt->slot_use[slot] = reg;
            pt->reg[GPR_IDX(reg)].ever_spilled = false;
        }
    } else {
        LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": %s already spilled to slot %d\n",
            __FUNCTION__, pt->live_idx, instr_get_app_pc(where), get_register_name(reg),
            slot);
    }
    pt->reg[GPR_IDX(reg)].native = false;
    pt->reg[GPR_IDX(reg)].xchg = DR_REG_NULL;
    pt->reg[GPR_IDX(reg)].slot = slot;
    *reg_out = reg;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_reserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                       drvector_t *reg_allowed, OUT reg_id_t *reg_out)
{
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        drreg_status_t res = drreg_forward_analysis(drcontext, where);
        if (res != DRREG_SUCCESS)
            return res;
    }
    return drreg_reserve_reg_internal(drcontext, ilist, where, reg_allowed,
                                      false, reg_out);
}

drreg_status_t
drreg_reserve_dead_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                            drvector_t *reg_allowed, OUT reg_id_t *reg_out)
{
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        drreg_status_t res = drreg_forward_analysis(drcontext, where);
        if (res != DRREG_SUCCESS)
            return res;
    }
    return drreg_reserve_reg_internal(drcontext, ilist, where, reg_allowed,
                                      true, reg_out);
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
        /* DR will refuse to load into the same reg (the caller must use
         * opnd_replace_reg() with a scratch reg in that case).
         */
        if (dst_reg == app_reg)
            return DRREG_ERROR_INVALID_PARAMETER;
        if (dr_insert_get_stolen_reg_value(drcontext, ilist, where, dst_reg))
            return DRREG_SUCCESS;
        ASSERT(false, "internal error on getting stolen reg app value");
        return DRREG_ERROR;
    }

    /* check if app_reg is an unspilled reg */
    if (pt->reg[GPR_IDX(app_reg)].native) {
        LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": reg %s already native\n", __FUNCTION__,
            pt->live_idx, instr_get_app_pc(where), get_register_name(app_reg));
        if (dst_reg != app_reg) {
            PRE(ilist, where, XINST_CREATE_move(drcontext,
                                                opnd_create_reg(dst_reg),
                                                opnd_create_reg(app_reg)));
        }
        return DRREG_SUCCESS;
    }

    /* we may have lost the app value for a dead reg */
    if (!pt->reg[GPR_IDX(app_reg)].ever_spilled) {
        LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": reg %s never spilled\n", __FUNCTION__,
            pt->live_idx, instr_get_app_pc(where), get_register_name(app_reg));
        return DRREG_ERROR_NO_APP_VALUE;
    }
    /* restore app value back to app_reg */
    if (pt->reg[GPR_IDX(app_reg)].xchg != DR_REG_NULL) {
        /* XXX i#511: NYI */
        return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
    }
    LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": getting app value for %s\n",
        __FUNCTION__, pt->live_idx, instr_get_app_pc(where), get_register_name(app_reg));
    restore_reg(drcontext, pt, app_reg,
                pt->reg[GPR_IDX(app_reg)].slot,
                ilist, where, false);
    return DRREG_SUCCESS;
}

static drreg_status_t
drreg_restore_reg_now(void *drcontext, instrlist_t *ilist, instr_t *inst,
                      per_thread_t *pt, reg_id_t reg)
{
    if (pt->reg[GPR_IDX(reg)].ever_spilled) {
        if (pt->reg[GPR_IDX(reg)].xchg != DR_REG_NULL) {
            /* XXX i#511: NYI */
            return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
        }
        LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": restoring %s\n",
            __FUNCTION__, pt->live_idx, instr_get_app_pc(inst), get_register_name(reg));
        restore_reg(drcontext, pt, reg,
                    pt->reg[GPR_IDX(reg)].slot, ilist, inst, true);
    } else /* still need to release slot */
        pt->slot_use[pt->reg[GPR_IDX(reg)].slot] = DR_REG_NULL;
    pt->reg[GPR_IDX(reg)].native = true;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_unreserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    if (!pt->reg[GPR_IDX(reg)].in_use)
        return DRREG_ERROR_INVALID_PARAMETER;
    LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX" %s\n", __FUNCTION__,
        pt->live_idx, instr_get_app_pc(where), get_register_name(reg));
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        /* We have no way to lazily restore.  We do not bother at this point
         * to try and eliminate back-to-back spill/restore pairs.
         */
        drreg_status_t res = drreg_restore_reg_now(drcontext, ilist, where, pt, reg);
        if (res != DRREG_SUCCESS)
            return res;
    } else {
        /* We lazily restore in drreg_event_bb_insert_late(), in case
         * someone else wants a local scratch.
         */
        pt->pending_unreserved++;
    }
    pt->reg[GPR_IDX(reg)].in_use = false;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_reservation_info(void *drcontext, reg_id_t reg, opnd_t *opnd OUT,
                       bool *is_dr_slot OUT, uint *tls_offs OUT)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    uint slot;
    if (!pt->reg[GPR_IDX(reg)].in_use)
        return DRREG_ERROR_INVALID_PARAMETER;
    slot = pt->reg[GPR_IDX(reg)].slot;
    ASSERT(pt->slot_use[slot] == reg, "internal tracking error");

    if (slot < ops.num_spill_slots) {
        if (opnd != NULL)
            *opnd = dr_raw_tls_opnd(drcontext, tls_seg, tls_slot_offs);
        if (is_dr_slot != NULL)
            *is_dr_slot = false;
        if (tls_offs != NULL)
            *tls_offs = tls_slot_offs + slot*sizeof(reg_t);
    } else {
        dr_spill_slot_t DR_slot = (dr_spill_slot_t)(slot - ops.num_spill_slots);
        if (opnd != NULL) {
            if (DR_slot < dr_max_opnd_accessible_spill_slot())
                *opnd = dr_reg_spill_slot_opnd(drcontext, DR_slot);
            else {
                /* Multi-step so no single opnd */
                *opnd = opnd_create_null();
            }
        }
        if (is_dr_slot != NULL)
            *is_dr_slot = true;
        if (tls_offs != NULL)
            *tls_offs = DR_slot;
    }
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_is_register_dead(void *drcontext, reg_id_t reg, instr_t *inst, bool *dead)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    if (dead == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        drreg_status_t res = drreg_forward_analysis(drcontext, inst);
        if (res != DRREG_SUCCESS)
            return res;
        ASSERT(pt->live_idx == 0, "non-drmgr-insert always uses 0 index");
    }
    *dead = drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, pt->live_idx) == REG_DEAD;
    return DRREG_SUCCESS;
}

/***************************************************************************
 * ARITHMETIC FLAGS
 */

static drreg_status_t
drreg_spill_aflags(void *drcontext, instrlist_t *ilist, instr_t *where, per_thread_t *pt)
{
#ifdef X86
    uint aflags = (uint)(ptr_uint_t) drvector_get_entry(&pt->aflags.live, pt->live_idx);
    uint temp_slot = find_free_slot(pt);
    if (temp_slot == MAX_SPILLS)
        return DRREG_ERROR_OUT_OF_SLOTS;
    if (pt->reg[DR_REG_XAX-DR_REG_START_GPR].in_use) {
        /* No way to tell whoever is using xax that we need it */
        /* XXX i#511: pick an unreserved reg, spill it, and put xax there
         * temporarily.  Store aflags in our dedicated aflags tls slot.
         */
        return DRREG_ERROR_REG_CONFLICT;
    }
    if (ops.conservative ||
        drvector_get_entry(&pt->reg[DR_REG_XAX-DR_REG_START_GPR].live, pt->live_idx) ==
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
    if (ops.conservative ||
        drvector_get_entry(&pt->reg[DR_REG_XAX-DR_REG_START_GPR].live, pt->live_idx) ==
        REG_LIVE)
        restore_reg(drcontext, pt, DR_REG_XAX, temp_slot, ilist, where, true);
#elif defined(ARM)
    drreg_status_t res = DRREG_SUCCESS;
    reg_id_t scratch;
    res = drreg_reserve_reg_internal(drcontext, ilist, where, NULL, false, &scratch);
    if (res != DRREG_SUCCESS)
        return res;
    dr_save_arith_flags_to_reg(drcontext, ilist, where, scratch);
    spill_reg(drcontext, pt, scratch, AFLAGS_SLOT, ilist, where);
    res = drreg_unreserve_register(drcontext, ilist, where, scratch);
    if (res != DRREG_SUCCESS)
        return res; /* XXX: undo already-inserted instrs? */
#endif
    return DRREG_SUCCESS;
}

static drreg_status_t
drreg_restore_aflags(void *drcontext, instrlist_t *ilist, instr_t *where,
                     per_thread_t *pt, bool release)
{
#ifdef X86
    uint aflags = (uint)(ptr_uint_t) drvector_get_entry(&pt->aflags.live, pt->live_idx);
    uint temp_slot = find_free_slot(pt);
    if (pt->reg[DR_REG_XAX-DR_REG_START_GPR].in_use) {
        /* XXX i#511: pick an unreserved reg, spill it, and put xax there
         * temporarily.
         */
        return DRREG_ERROR_REG_CONFLICT;
    }
    if (temp_slot == MAX_SPILLS)
        return DRREG_ERROR_OUT_OF_SLOTS;
    if (ops.conservative ||
        drvector_get_entry(&pt->reg[DR_REG_XAX-DR_REG_START_GPR].live, pt->live_idx) ==
        REG_LIVE)
        spill_reg(drcontext, pt, DR_REG_XAX, temp_slot, ilist, where);
    restore_reg(drcontext, pt, DR_REG_XAX, AFLAGS_SLOT, ilist, where, release);
    if (TEST(EFLAGS_READ_OF, aflags)) {
        PRE(ilist, where, INSTR_CREATE_add
            (drcontext, opnd_create_reg(DR_REG_AL), OPND_CREATE_INT8(0x7f)));
    }
    PRE(ilist, where, INSTR_CREATE_sahf(drcontext));
    if (ops.conservative ||
        drvector_get_entry(&pt->reg[DR_REG_XAX-DR_REG_START_GPR].live, pt->live_idx) ==
        REG_LIVE)
        restore_reg(drcontext, pt, DR_REG_XAX, temp_slot, ilist, where, true);
#elif defined(ARM)
    drreg_status_t res = DRREG_SUCCESS;
    reg_id_t scratch;
    res = drreg_reserve_reg_internal(drcontext, ilist, where, NULL, false, &scratch);
    if (res != DRREG_SUCCESS)
        return res;
    restore_reg(drcontext, pt, scratch, AFLAGS_SLOT, ilist, where, release);
    dr_restore_arith_flags_from_reg(drcontext, ilist, where, scratch);
    res = drreg_unreserve_register(drcontext, ilist, where, scratch);
    if (res != DRREG_SUCCESS)
        return res; /* XXX: undo already-inserted instrs? */
#endif
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_reserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    drreg_status_t res;
    uint aflags;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        res = drreg_forward_analysis(drcontext, where);
        if (res != DRREG_SUCCESS)
            return res;
        ASSERT(pt->live_idx == 0, "non-drmgr-insert always uses 0 index");
    }
    aflags = (uint)(ptr_uint_t) drvector_get_entry(&pt->aflags.live, pt->live_idx);
    /* Just like scratch regs, flags are exclusively owned */
    if (pt->aflags.in_use)
        return DRREG_ERROR_IN_USE;
    if (!TESTANY(EFLAGS_READ_ARITH, aflags)) {
        pt->aflags.in_use = true;
        pt->aflags.native = true;
        LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": aflags are dead\n",
            __FUNCTION__, pt->live_idx, instr_get_app_pc(where));
        return DRREG_SUCCESS;
    }
    /* Check for a prior reservation not yet lazily restored */
    if (!pt->aflags.native) {
        LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": using un-restored aflags\n",
            __FUNCTION__, pt->live_idx, instr_get_app_pc(where));
        ASSERT(pt->slot_use[AFLAGS_SLOT] != DR_REG_NULL, "lost slot reservation");
        pt->aflags.in_use = true;
        return DRREG_SUCCESS;
    }

    LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX": spilling aflags\n",
        __FUNCTION__, pt->live_idx, instr_get_app_pc(where));
    res = drreg_spill_aflags(drcontext, ilist, where, pt);
    if (res != DRREG_SUCCESS)
        return res;
    pt->aflags.in_use = true;
    pt->aflags.native = false;
    pt->aflags.xchg = DR_REG_NULL;
    pt->aflags.slot = AFLAGS_SLOT;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_unreserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    if (!pt->aflags.in_use)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        /* We have no way to lazily restore.  We do not bother at this point
         * to try and eliminate back-to-back spill/restore pairs.
         */
        if (!pt->aflags.native) {
            drreg_restore_aflags(drcontext, ilist, where, pt, true/*release*/);
            pt->aflags.native = true;
        }
        pt->slot_use[AFLAGS_SLOT] = DR_REG_NULL;
    }
    LOG(drcontext, LOG_ALL, 3, "%s @%d."PFX"\n", __FUNCTION__,
        pt->live_idx, instr_get_app_pc(where));
    /* We lazily restore in drreg_event_bb_insert_late(), in case
     * someone else wants the aflags locally.
     */
    pt->aflags.in_use = false;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_aflags_liveness(void *drcontext, instr_t *inst, OUT uint *value)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    if (value == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        drreg_status_t res = drreg_forward_analysis(drcontext, inst);
        if (res != DRREG_SUCCESS)
            return res;
        ASSERT(pt->live_idx == 0, "non-drmgr-insert always uses 0 index");
    }
    *value = (uint)(ptr_uint_t) drvector_get_entry(&pt->aflags.live, pt->live_idx);
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_are_aflags_dead(void *drcontext, instr_t *inst, bool *dead)
{
    uint flags;
    drreg_status_t res = drreg_aflags_liveness(drcontext, inst, &flags);
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
    /* To achieve a clean and simple reserve-and-unreserve interface w/o specifying
     * up front how many cross-app-instr scratch regs (and then limited to whole-bb
     * regs with stored per-bb info, like Dr. Memory does), we have to pay with a
     * complex state xl8 scheme.  We need to decode the in-cache fragment and walk
     * it, recognizing our own spills and restores.  We distinguish a tool value
     * spill to a temp slot (from drreg_event_bb_insert_late()) by watching for
     * a spill of an already-spilled reg to a different slot.
     */
    uint spilled_to[DR_NUM_GPR_REGS];
    uint spilled_to_aflags = MAX_SPILLS;
    reg_id_t reg;
    instr_t inst;
    byte *prev_pc, *pc = info->fragment_info.cache_start_pc;
    uint offs;
    bool spill, tls;
    if (pc == NULL)
        return true; /* fault not in cache */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++)
        spilled_to[GPR_IDX(reg)] = MAX_SPILLS;
    LOG(drcontext, LOG_ALL, 3, "%s: processing fault @"PFX": decoding from "PFX"\n",
        __FUNCTION__, info->raw_mcontext->pc, pc);
    instr_init(drcontext, &inst);
    while (pc < info->raw_mcontext->pc) {
        instr_reset(drcontext, &inst);
        prev_pc = pc;
        pc = decode(drcontext, pc, &inst);

        /* XXX i#511: if we add xchg to our arsenal we'll have to detect it here */
        if (instr_is_reg_spill_or_restore(drcontext, &inst, &tls, &spill, &reg, &offs)) {
            uint slot;
            /* Is this from our raw TLS? */
            if (tls && offs >= tls_slot_offs &&
                offs < (tls_slot_offs + ops.num_spill_slots*sizeof(reg_t))) {
                slot = (offs - tls_slot_offs) / sizeof(reg_t);
            } else {
                /* We assume a DR spill slot, in TLS or thread-private mcontext */
                if (tls) {
                    uint DR_min_offs =
                        opnd_get_disp(dr_reg_spill_slot_opnd(drcontext, SPILL_SLOT_1));
                    slot = (offs - DR_min_offs) / sizeof(reg_t);
                } else {
                    /* We assume mcontext spill offs is 0 */
                    slot = offs / sizeof(reg_t);
                }
                slot += ops.num_spill_slots;
            }
            LOG(drcontext, LOG_ALL, 3, "%s @"PFX" found %s to %s offs=0x%x => slot %d\n",
                __FUNCTION__, prev_pc, spill ? "spill" : "restore",
                get_register_name(reg), offs, slot);
            if (spill) {
                if (slot == AFLAGS_SLOT) {
                    spilled_to_aflags = slot;
                } else if (spilled_to[GPR_IDX(reg)] < MAX_SPILLS &&
                           /* allow redundant spill */
                           spilled_to[GPR_IDX(reg)] != slot) {
                    /* This reg is already spilled: we assume that this new spill
                     * is to a tmp slot for preserving the tool's value.
                     */
                    LOG(drcontext, LOG_ALL, 3, "%s @"PFX": ignoring tool spill\n",
                        __FUNCTION__, pc);
                } else {
                    spilled_to[GPR_IDX(reg)] = slot;
                }
            } else {
                if (slot == AFLAGS_SLOT && spilled_to_aflags == slot)
                    spilled_to_aflags = MAX_SPILLS;
                else if (spilled_to[GPR_IDX(reg)] == slot)
                    spilled_to[GPR_IDX(reg)] = MAX_SPILLS;
                else {
                    LOG(drcontext, LOG_ALL, 3, "%s @"PFX": ignoring restore\n",
                        __FUNCTION__, pc);
                }
            }
        }
    }
    instr_free(drcontext, &inst);

    if (spilled_to_aflags < MAX_SPILLS) {
        reg_t val = get_spilled_value(drcontext, spilled_to_aflags);
        reg_t newval = info->mcontext->xflags;
#ifdef ARM
        newval &= ~(EFLAGS_ARITH);
        newval |= val;
#elif defined(X86)
        uint sahf = (val & 0xff00) >> 8;
        newval &= ~(EFLAGS_ARITH);
        newval |= sahf;
        if (TEST(1, val)) /* seto */
            newval |= EFLAGS_OF;
#endif
        LOG(drcontext, LOG_ALL, 3, "%s: restoring aflags from "PFX" to "PFX"\n",
            __FUNCTION__, info->mcontext->xflags, newval);
        info->mcontext->xflags = newval;
    }
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (spilled_to[GPR_IDX(reg)] < MAX_SPILLS) {
            reg_t val = get_spilled_value(drcontext, spilled_to[GPR_IDX(reg)]);
            LOG(drcontext, LOG_ALL, 3, "%s: restoring %s from "PFX" to "PFX"\n",
                __FUNCTION__, get_register_name(reg), reg_get_value(reg, info->mcontext),
                val);
            reg_set_value(reg, info->mcontext, val);
        }
    }

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
        drvector_init(&pt->reg[GPR_IDX(reg)].live, 20,
                      false/*!synch*/, NULL);
        pt->aflags.native = true;
        pt->reg[GPR_IDX(reg)].native = true;
    }
    drvector_init(&pt->aflags.live, 20, false/*!synch*/, NULL);
    pt->tls_seg_base = dr_get_dr_segment_base(tls_seg);
}

static void
drreg_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    reg_id_t reg;
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        drvector_delete(&pt->reg[GPR_IDX(reg)].live);
    }
    drvector_delete(&pt->aflags.live);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

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

    if (ops_in->struct_size < offsetof(drreg_options_t, error_callback))
        return DRREG_ERROR_INVALID_PARAMETER;
    else if (ops_in->struct_size < sizeof(ops))
        ops.error_callback = NULL;
    ops = *ops_in;

    drmgr_init();

    if (!drmgr_register_thread_init_event(drreg_thread_init) ||
        !drmgr_register_thread_exit_event(drreg_thread_exit))
        return DRREG_ERROR;
    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return DRREG_ERROR;

    if (!drmgr_register_bb_instrumentation_event
        (NULL, drreg_event_bb_insert_early, &high_priority) ||
        !drmgr_register_bb_instrumentation_event
        (drreg_event_bb_analysis, drreg_event_bb_insert_late, &low_priority) ||
        !drmgr_register_restore_state_ex_event_ex
        (drreg_event_restore_state, &fault_priority))
        return DRREG_ERROR;

    if (!dr_raw_tls_calloc(&tls_seg, &tls_slot_offs, ops.num_spill_slots, 0))
        return DRREG_ERROR_OUT_OF_SLOTS;

    return DRREG_SUCCESS;
}

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
    if (!drmgr_unregister_bb_insertion_event(drreg_event_bb_insert_early) ||
        !drmgr_unregister_bb_instrumentation_event(drreg_event_bb_analysis) ||
        !drmgr_unregister_restore_state_ex_event(drreg_event_restore_state))
        return DRREG_ERROR;

    drmgr_exit();

    if (!dr_raw_tls_cfree(tls_slot_offs, ops.num_spill_slots))
        return DRREG_ERROR;

    return DRREG_SUCCESS;
}

