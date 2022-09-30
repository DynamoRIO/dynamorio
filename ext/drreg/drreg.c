/* **********************************************************
 * Copyright (c) 2013-2022 Google, Inc.   All rights reserved.
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
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define LOG(dc, mask, level, ...) dr_log(dc, mask, level, __VA_ARGS__)
#    define IF_DEBUG(x) x
#else
#    define ASSERT(x, msg)            /* nothing */
#    define LOG(dc, mask, level, ...) /* nothing */
#    define IF_DEBUG(x)               /* nothing */
#endif

#ifdef WINDOWS
#    define DISPLAY_ERROR(msg) dr_messagebox(msg)
#else
#    define DISPLAY_ERROR(msg) dr_fprintf(STDERR, "%s\n", msg);
#endif

#define PRE instrlist_meta_preinsert

/* This should be pretty hard to exceed as there aren't this many GPRs */
#define MAX_SPILLS (SPILL_SLOT_MAX + 8)

/* For simplicity, we assign aflags an alias that's one past the last gpr.
 * This is only for simpler handling of data structures like spill_slot.
 * Note that this is not the same as the aflags spill reg, which is the actual
 * gpr that contains the spilled native aflags.
 * XXX: Replace use of DR_REG_NULL as aflags alias in drreg routines like
 * drreg_statelessly_restore_app_value. Use AFLAGS_ALIAS_REG instead.
 */
#define AFLAGS_ALIAS_REG (DR_REG_STOP_GPR + 1)

/* We support using GPR registers only: [DR_REG_START_GPR..DR_REG_STOP_GPR] */

#define REG_DEAD ((void *)(ptr_uint_t)0)
#define REG_LIVE ((void *)(ptr_uint_t)1)
#define REG_UNKNOWN ((void *)(ptr_uint_t)2) /* only used outside drmgr insert phase */

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

#define GPR_IDX(reg) ((reg)-DR_REG_START_GPR)

typedef struct _per_thread_t {
    instr_t *cur_instr;
    int live_idx;
    reg_info_t reg[DR_NUM_GPR_REGS];
    reg_info_t aflags;
    reg_id_t slot_use[MAX_SPILLS]; /* holds the reg_id_t of which reg is inside */
    int pending_unreserved;        /* count of to-be-lazily-restored unreserved regs */
    /* We store the linear address of our TLS for access from another thread: */
    byte *tls_seg_base;
    /* bb-local values */
    drreg_bb_properties_t bb_props;
    bool bb_has_internal_flow;
} per_thread_t;

static drreg_options_t ops;

static int tls_idx = -1;
static uint tls_slot_offs;
static reg_id_t tls_seg;

#ifdef DEBUG
static uint stats_max_slot;
#endif

static per_thread_t *
get_tls_data(void *drcontext);

static drreg_status_t
drreg_restore_reg_now(void *drcontext, instrlist_t *ilist, instr_t *inst,
                      per_thread_t *pt, reg_id_t reg);

static void
drreg_move_aflags_from_reg(void *drcontext, instrlist_t *ilist, instr_t *where,
                           per_thread_t *pt, bool stateful);

static drreg_status_t
drreg_restore_aflags(void *drcontext, instrlist_t *ilist, instr_t *where,
                     per_thread_t *pt, bool release);

static drreg_status_t
drreg_spill_aflags(void *drcontext, instrlist_t *ilist, instr_t *where, per_thread_t *pt);

static bool
is_our_spill_or_restore(void *drcontext, instr_t *instr, bool *spill,
                        reg_id_t *reg_spilled, uint *slot_out, uint *offs_out);

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

#ifdef DEBUG
static inline app_pc
get_where_app_pc(instr_t *where)
{
    if (where == NULL)
        return NULL;
    return instr_get_app_pc(where);
}
#endif

/***************************************************************************
 * SPILLING AND RESTORING
 */

/* Returns whether the given ilist has an existing usage of the given slot
 * on or after `where`. Such a usage, if any, must have been added by a
 * previous instrumentation pass, and tells us that it is not safe to reuse
 * the slot in the current pass.
 */
static bool
has_pending_slot_usage_by_prior_pass(void *drcontext, per_thread_t *pt,
                                     instrlist_t *ilist, instr_t *where, uint slot)
{
    if (!TEST(DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS, pt->bb_props))
        return false;
    for (instr_t *in = where; in != NULL; in = instr_get_next(in)) {
        uint used_slot;
        if (is_our_spill_or_restore(drcontext, in, NULL, NULL, &used_slot, NULL)) {
            if (used_slot == slot) {
                return true;
            }
        }
    }
    return false;
}

static uint
find_free_slot(void *drcontext, per_thread_t *pt, instrlist_t *ilist, instr_t *where)
{
    uint i;
    for (i = 0; i < MAX_SPILLS; i++) {
        if (pt->slot_use[i] == DR_REG_NULL &&
            !has_pending_slot_usage_by_prior_pass(drcontext, pt, ilist, where, i)) {
            return i;
        }
    }
    return MAX_SPILLS;
}

static void
reset_aflags_spill_slot(per_thread_t *pt)
{
    /* May not need to do this if aflags were saved in some reg (like xax). */
    if (pt->aflags.slot != MAX_SPILLS) {
        pt->slot_use[pt->aflags.slot] = DR_REG_NULL;
        /* Unlike pt->reg[_].slot, we need to make sure to reset pt->aflags.slot
         * as we use this field to determine whether we are spilling/restoring
         * aflags.
         */
        pt->aflags.slot = MAX_SPILLS;
    }
}

/* Up to caller to update pt->reg, including .ever_spilled.
 * This routine updates pt->slot_use.
 */
static void
spill_reg(void *drcontext, per_thread_t *pt, reg_id_t reg, uint slot, instrlist_t *ilist,
          instr_t *where)
{
    ASSERT(slot >= 0, "invalid slot");
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s %d\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where), get_register_name(reg), slot);
    ASSERT(pt->slot_use[slot] == DR_REG_NULL || pt->slot_use[slot] == reg ||
               /* aflags can be saved and restored using different regs */
               slot == (uint)pt->aflags.slot,
           "internal tracking error");
    if (slot == (uint)pt->aflags.slot)
        pt->aflags.ever_spilled = true;
    pt->slot_use[slot] = reg;
    if (slot < ops.num_spill_slots) {
        dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                                tls_slot_offs + slot * sizeof(reg_t), reg);
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
    ASSERT(slot >= 0, "invalid slot");
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s slot=%d release=%d\n", __FUNCTION__,
        pt->live_idx, get_where_app_pc(where), get_register_name(reg), slot, release);
    ASSERT(pt->slot_use[slot] == reg ||
               /* aflags can be saved and restored using different regs */
               (slot == (uint)pt->aflags.slot && pt->slot_use[slot] != DR_REG_NULL),
           "internal tracking error");
    if (release) {
        pt->slot_use[slot] = DR_REG_NULL;
    }
    if (slot < ops.num_spill_slots) {
        dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                               tls_slot_offs + slot * sizeof(reg_t), reg);
    } else {
        dr_spill_slot_t DR_slot = (dr_spill_slot_t)(slot - ops.num_spill_slots);
        dr_restore_reg(drcontext, ilist, where, reg, DR_slot);
    }
}

static reg_t
get_spilled_value(void *drcontext, uint slot)
{
    if (slot < ops.num_spill_slots) {
        per_thread_t *pt = get_tls_data(drcontext);
        return *(reg_t *)(pt->tls_seg_base + tls_slot_offs + slot * sizeof(reg_t));
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
drreg_event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating, OUT void **user_data)
{
    per_thread_t *pt = get_tls_data(drcontext);
    instr_t *inst;
    ptr_uint_t aflags_new, aflags_cur = 0;
    uint index = 0;
    reg_id_t reg;

    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++)
        pt->reg[GPR_IDX(reg)].app_uses = 0;
    /* pt->bb_props is set to 0 at thread init and after each bb */
    pt->bb_has_internal_flow = false;

    /* Reverse scan is more efficient.  This means our indices are also reversed. */
    for (inst = instrlist_last(bb); inst != NULL; inst = instr_get_prev(inst)) {
        /* We consider both meta and app instrs, to handle rare cases of meta instrs
         * being inserted during app2app for corner cases. An example are app2app
         * emulation functions like drx_expand_scatter_gather().
         */

        bool xfer =
            (instr_is_cti(inst) || instr_is_interrupt(inst) || instr_is_syscall(inst));

        if (!pt->bb_has_internal_flow && (instr_is_ubr(inst) || instr_is_cbr(inst)) &&
            opnd_is_instr(instr_get_target(inst))) {
            /* i#1954: we disable some opts in the presence of control flow. */
            pt->bb_has_internal_flow = true;
            LOG(drcontext, DR_LOG_ALL, 2,
                "%s @%d." PFX ": disabling lazy restores due to intra-bb control flow\n",
                __FUNCTION__, index, get_where_app_pc(inst));
        }

        /* GPR liveness */
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ":", __FUNCTION__, index,
            get_where_app_pc(inst));
        for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            void *value = REG_LIVE;
            /* DRi#1849: COND_SRCS here includes addressing regs in dsts */
            if (instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS))
                value = REG_LIVE;
            /* make sure we don't consider writes to sub-regs */
            else if (instr_writes_to_exact_reg(inst, reg, DR_QUERY_INCLUDE_COND_SRCS)
                     /* A write to a 32-bit reg zeroes the top 32 bits for x86_64 and
                      * aarch64.
                      */
                     IF_X64(||
                            instr_writes_to_exact_reg(inst, reg_64_to_32(reg),
                                                      DR_QUERY_INCLUDE_COND_SRCS)))
                value = REG_DEAD;
            else if (xfer)
                value = REG_LIVE;
            else if (index > 0)
                value = drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, index - 1);
            LOG(drcontext, DR_LOG_ALL, 3, " %s=%d", get_register_name(reg),
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
                aflags_cur =
                    (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, index - 1);
            }
            aflags_read = (aflags_new & EFLAGS_READ_ARITH);
            /* if a flag is read by inst, set the read bit */
            aflags_cur |= (aflags_new & EFLAGS_READ_ARITH);
            /* if a flag is written and not read by inst, clear the read bit */
            aflags_w2r = EFLAGS_WRITE_TO_READ(aflags_new & EFLAGS_WRITE_ARITH);
            aflags_cur &= ~(aflags_w2r & ~aflags_read);
        }
        LOG(drcontext, DR_LOG_ALL, 3, " flags=%d\n", aflags_cur);
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
    per_thread_t *pt = get_tls_data(drcontext);
    pt->cur_instr = inst;
    pt->live_idx--; /* counts backward */
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drreg_event_bb_instru2instru_late(void *drcontext, void *tag, instrlist_t *bb,
                                  bool for_trace, bool translating)
{
    /* We preserve bb_props until late in instru2instru as there may be drreg usages
     * in other passes that need to look at it.
     */
    get_tls_data(drcontext)->bb_props = 0;
    return DR_EMIT_DEFAULT;
}

/* 'regs_restored' holds one slot per GPR using GPR_IDX() (no aflags slot). */
static drreg_status_t
drreg_insert_restore_all(void *drcontext, instrlist_t *bb, instr_t *inst,
                         bool force_restore, OUT bool *regs_restored)
{
    per_thread_t *pt = get_tls_data(drcontext);
    reg_id_t reg;
    instr_t *next = instr_get_next(inst);
    drreg_status_t res;

    /* Before each app read, or at end of bb, restore aflags to app value */
    uint aflags = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
    if (!pt->aflags.native &&
        (force_restore ||
         TESTANY(EFLAGS_READ_ARITH, instr_get_eflags(inst, DR_QUERY_DEFAULT)) ||
         /* Writing just a subset needs to combine with the original unwritten */
         (TESTANY(EFLAGS_WRITE_ARITH, instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL)) &&
          aflags != 0 /*0 means everything is dead*/) ||
         /* Annotation handlers and rseq mangling require app values.
          * (XXX i#5160: Refactor drreg vs core to remove special cases?)
          */
         (instr_is_label(inst) &&
          ((ptr_uint_t)instr_get_note(inst) == DR_NOTE_ANNOTATION ||
           (ptr_uint_t)instr_get_note(inst) == DR_NOTE_REG_BARRIER)) ||
         /* DR slots are not guaranteed across app instrs */
         (pt->aflags.slot != MAX_SPILLS &&
          pt->aflags.slot >= (int)ops.num_spill_slots))) {
        /* Restore aflags to app value */
        LOG(drcontext, DR_LOG_ALL, 3,
            "%s @%d." PFX " aflags=0x%x use=%d: lazily restoring aflags\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(inst), aflags, pt->aflags.in_use);
        res = drreg_restore_aflags(drcontext, bb, inst, pt, false /*keep slot*/);
        if (res != DRREG_SUCCESS) {
            LOG(drcontext, DR_LOG_ALL, 1,
                "%s @%d." PFX ": failed to restore flags before app read\n", __FUNCTION__,
                pt->live_idx, get_where_app_pc(inst));
            return res;
        }

        if (!pt->aflags.in_use) {
            pt->aflags.native = true;
            reset_aflags_spill_slot(pt);
        }
    }
    /* Before each app read, or at end of bb, restore spilled registers to app values.
     * XXX i#5121: Any app2app cross-app-instr use should manually restore app values
     * prior to app reads. That interacts well with the insertion phase since we treat
     * those app2app restores as app writes and end up with the app value here. If the
     * app2app phase has a regsiter in a spilled state at this instr and did not restore
     * the app value, the following insertion phase code will not restore the app value
     * but instead will restore the app2app meta value.
     *
     * For cases where instrumentation wants the app value at some point in the insertion
     * phase (e.g., for use with annotations, or for user-added calls to
     * drreg_get_app_value and drreg_statelessly_restore_app_value to work as expected),
     * the user needs to restore the app value at the same point in the app2app phase
     * also.
     */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (regs_restored != NULL)
            regs_restored[GPR_IDX(reg)] = false;
        if (!pt->reg[GPR_IDX(reg)].native) {
            if (force_restore || instr_reads_from_reg(inst, reg, DR_QUERY_INCLUDE_ALL) ||
                /* Annotation handlers and rseq mangling require app values.
                 * (XXX i#5160: Refactor drreg vs core to remove special cases?)
                 */
                (instr_is_label(inst) &&
                 ((ptr_uint_t)instr_get_note(inst) == DR_NOTE_ANNOTATION ||
                  (ptr_uint_t)instr_get_note(inst) == DR_NOTE_REG_BARRIER)) ||
                /* Treat a partial write as a read, to restore rest of reg */
                (instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL) &&
                 !instr_writes_to_exact_reg(inst, reg, DR_QUERY_INCLUDE_ALL)) ||
                /* Treat a conditional write as a read and a write to handle the
                 * condition failing and our write handling saving the wrong value.
                 */
                (instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL) &&
                 !instr_writes_to_reg(inst, reg, DR_QUERY_DEFAULT)) ||
                /* i#1954: for complex bbs we must restore before the next app instr */
                (!pt->reg[GPR_IDX(reg)].in_use &&
                 ((pt->bb_has_internal_flow &&
                   !TEST(DRREG_IGNORE_CONTROL_FLOW, pt->bb_props)) ||
                  TEST(DRREG_CONTAINS_SPANNING_CONTROL_FLOW, pt->bb_props))) ||
                /* If we're out of our own slots and are using a DR slot, we have to
                 * restore now b/c DR slots are not guaranteed across app instrs.
                 */
                pt->reg[GPR_IDX(reg)].slot >= (int)ops.num_spill_slots) {
                if (!pt->reg[GPR_IDX(reg)].in_use) {
                    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": lazily restoring %s\n",
                        __FUNCTION__, pt->live_idx, get_where_app_pc(inst),
                        get_register_name(reg));
                    res = drreg_restore_reg_now(drcontext, bb, inst, pt, reg);
                    if (res != DRREG_SUCCESS) {
                        LOG(drcontext, DR_LOG_ALL, 1,
                            "%s @%d." PFX ": lazy restore failed\n", __FUNCTION__,
                            pt->live_idx, get_where_app_pc(inst));
                        return res;
                    }
                    ASSERT(pt->pending_unreserved > 0, "should not go negative");
                    pt->pending_unreserved--;
                } else if (pt->aflags.xchg == reg) {
                    /* Bail on keeping the flags in the reg. */
                    drreg_move_aflags_from_reg(drcontext, bb, inst, pt, true);
                } else {
                    /* We need to move the tool's value somewhere else.
                     * We use a separate slot for that (and we document that
                     * tools should request an extra slot for each cross-app-instr
                     * register).
                     * XXX: optimize via xchg w/ a dead reg.
                     */
                    uint tmp_slot = find_free_slot(drcontext, pt, bb, inst);
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
                    LOG(drcontext, DR_LOG_ALL, 3,
                        "%s @%d." PFX ": restoring %s for app read\n", __FUNCTION__,
                        pt->live_idx, get_where_app_pc(inst), get_register_name(reg));
                    spill_reg(drcontext, pt, reg, tmp_slot, bb, inst);
                    restore_reg(drcontext, pt, reg, pt->reg[GPR_IDX(reg)].slot, bb, inst,
                                false /*keep slot*/);
                    restore_reg(drcontext, pt, reg, tmp_slot, bb, next, true);
                    /* Share the tool val spill if this inst writes too */
                    if (regs_restored != NULL)
                        regs_restored[GPR_IDX(reg)] = true;
                    /* We keep .native==false */
                }
            }
        }
    }

    return DRREG_SUCCESS;
}

/* 'restored_for_read' holds one slot per GPR using GPR_IDX() (no aflags slot). */
static drreg_status_t
drreg_insert_respill_all(void *drcontext, per_thread_t *pt, instrlist_t *bb,
                         instr_t *inst, instr_t *next, bool force_respill,
                         bool *restored_for_read)
{
    drreg_status_t res = DRREG_SUCCESS;
    reg_id_t reg;
    /* After aflags write by app, update spilled app value */
    if ((force_respill ||
         TESTANY(EFLAGS_WRITE_ARITH, instr_get_eflags(inst, DR_QUERY_INCLUDE_ALL))) &&
        /* Is everything written later? */
        (pt->live_idx == 0 ||
         (ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx - 1) != 0)) {
        if (pt->aflags.in_use) {
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @%d." PFX ": re-spilling aflags after app write\n", __FUNCTION__,
                pt->live_idx, get_where_app_pc(inst));
            /* Release current aflags slot if any. We'll reserve a new one if needed. */
            reset_aflags_spill_slot(pt);
            res = drreg_spill_aflags(drcontext, bb, next /*after*/, pt);
            if (res != DRREG_SUCCESS) {
                drreg_report_error(res, "failed to spill aflags after app write");
            }
            pt->aflags.native = false;
        } else if (!pt->aflags.native ||
                   (pt->aflags.slot != MAX_SPILLS &&
                    pt->slot_use[pt->aflags.slot] != DR_REG_NULL)
                       IF_X86(||
                              (pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use &&
                               pt->aflags.xchg == DR_REG_XAX))) {
            /* give up slot */
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @%d." PFX ": giving up aflags slot after app write\n", __FUNCTION__,
                pt->live_idx, get_where_app_pc(inst));
#ifdef X86
            if (pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use &&
                pt->aflags.xchg == DR_REG_XAX)
                drreg_move_aflags_from_reg(drcontext, bb, inst, pt, true);
#endif
            reset_aflags_spill_slot(pt);
            pt->aflags.native = true;
        }
    }

    /* After each app write, update spilled app values: */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (pt->reg[GPR_IDX(reg)].in_use) {
            if ((force_respill || instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL)) &&
                /* Don't bother if reg is dead beyond this write */
                (ops.conservative || pt->live_idx == 0 ||
                 drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, pt->live_idx - 1) ==
                     REG_LIVE ||
                 pt->aflags.xchg == reg)) {
                uint tmp_slot = MAX_SPILLS;
                if (pt->aflags.xchg == reg) {
                    /* Bail on keeping the flags in the reg. */
                    drreg_move_aflags_from_reg(drcontext, bb, inst, pt, true);
                    continue;
                }
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
                LOG(drcontext, DR_LOG_ALL, 3,
                    "%s @%d." PFX ": re-spilling %s after app write\n", __FUNCTION__,
                    pt->live_idx, get_where_app_pc(inst), get_register_name(reg));
                if (!restored_for_read[GPR_IDX(reg)]) {
                    tmp_slot = find_free_slot(drcontext, pt, bb, inst);
                    if (tmp_slot == MAX_SPILLS) {
                        drreg_report_error(DRREG_ERROR_OUT_OF_SLOTS,
                                           "failed to preserve tool val wrt app write");
                    }
                    spill_reg(drcontext, pt, reg, tmp_slot, bb, inst);
                }
                /* If the instr reads and writes both, make sure that the app-spill
                 * instr is added **before** the tool-restore instrs added by
                 * drreg_insert_restore_all called earlier in this routine.
                 */
                if (restored_for_read[GPR_IDX(reg)]) {
                    ASSERT(instr_get_prev(next) != NULL,
                           "missing tool value restore after app read");
                    spill_reg(drcontext, pt, reg, pt->reg[GPR_IDX(reg)].slot, bb,
                              instr_get_prev(next));
                } else {
                    spill_reg(drcontext, pt, reg, pt->reg[GPR_IDX(reg)].slot, bb,
                              next /*after*/);
                }
                pt->reg[GPR_IDX(reg)].ever_spilled = true;
                if (!restored_for_read[GPR_IDX(reg)])
                    restore_reg(drcontext, pt, reg, tmp_slot, bb, next /*after*/, true);
            }
        } else if (!pt->reg[GPR_IDX(reg)].native &&
                   instr_writes_to_reg(inst, reg, DR_QUERY_INCLUDE_ALL)) {
            /* For an unreserved reg that's written, just drop the slot, even
             * if it was spilled at an earlier reservation point.
             */
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @%d." PFX ": dropping slot for unreserved reg %s after app write\n",
                __FUNCTION__, pt->live_idx, get_where_app_pc(inst),
                get_register_name(reg));
            if (pt->reg[GPR_IDX(reg)].ever_spilled)
                pt->reg[GPR_IDX(reg)].ever_spilled = false; /* no need to restore */
            res = drreg_restore_reg_now(drcontext, bb, inst, pt, reg);
            if (res != DRREG_SUCCESS)
                drreg_report_error(res, "slot release on app write failed");
            pt->pending_unreserved--;
        }
    }
    return res;
}

static dr_emit_flags_t
drreg_event_bb_insert_late(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                           bool for_trace, bool translating, void *user_data)
{
    per_thread_t *pt = get_tls_data(drcontext);
    instr_t *next = instr_get_next(inst);
    bool restored_for_read[DR_NUM_GPR_REGS];
    drreg_status_t res;
    dr_pred_type_t pred = instrlist_get_auto_predicate(bb);

    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(bb, DR_PRED_NONE);
    /* For unreserved regs still spilled, we lazily do the restore here.  We also
     * update reserved regs wrt app uses.
     * The instruction list presented to us here are app instrs but may contain meta
     * instrs if any were inserted in app2app. Any such meta instr here will be treated
     * like an app instr.
     */
    bool do_last_spill = drmgr_is_last_instr(drcontext, inst) &&
        !TEST(DRREG_USER_RESTORES_AT_BB_END, pt->bb_props);
    res = drreg_insert_restore_all(drcontext, bb, inst, do_last_spill, restored_for_read);
    if (res != DRREG_SUCCESS)
        drreg_report_error(res, "failed to restore for reads");
    res =
        drreg_insert_respill_all(drcontext, pt, bb, inst, next, false, restored_for_read);
    if (res != DRREG_SUCCESS)
        drreg_report_error(res, "failed to update for writes");

#ifdef DEBUG
    if (drmgr_is_last_instr(drcontext, inst)) {
        uint i;
        for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
            ASSERT(!pt->aflags.in_use, "user failed to unreserve aflags");
            ASSERT(pt->aflags.native, "user failed to unreserve aflags");
            ASSERT(!pt->reg[GPR_IDX(reg)].in_use, "user failed to unreserve a register");
            ASSERT(pt->reg[GPR_IDX(reg)].native, "user failed to unreserve a register");
        }
        for (i = 0; i < MAX_SPILLS; i++) {
            if (pt->slot_use[i] != DR_REG_NULL) {
                ASSERT(pt->slot_use[i] == DR_REG_NULL,
                       "user failed to unreserve a register");
            }
        }
    }
#endif
    instrlist_set_auto_predicate(bb, pred);
    return DR_EMIT_DEFAULT;
}

drreg_status_t
drreg_restore_all(void *drcontext, instrlist_t *bb, instr_t *where)
{
    return drreg_insert_restore_all(drcontext, bb, where, true,
                                    NULL /* do not need to track reg restores */);
}

static void
drreg_event_clean_call_insertion(void *drcontext, instrlist_t *ilist, instr_t *where,
                                 dr_cleancall_save_t call_flags)
{
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        /* We only do automatic handling for app reads/writes in the insertion phase.
         * We do not support clean calls in app2app, and do not support these flags
         * for later phases.
         */
        if (TESTANY(DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT,
                    call_flags) &&
            /* Support clean calls inserted by DR during mangling. */
            drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_NONE) {
            drreg_report_error(
                DRREG_ERROR_FEATURE_NOT_AVAILABLE,
                "clean call app context flags not supported outside insertion phase");
        }
        return;
    }
    bool restored_for_read[DR_NUM_GPR_REGS];
    drreg_status_t res;
    if (TEST(DR_CLEANCALL_READS_APP_CONTEXT, call_flags)) {
        if (TEST(DR_CLEANCALL_MULTIPATH, call_flags)) {
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s: restoring statelessly for cleancall to read app regs\n",
                __FUNCTION__);
            res = drreg_statelessly_restore_all(drcontext, ilist, where,
                                                instr_get_next(where), NULL, NULL);
        } else {
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s: restoring for cleancall to read app regs\n", __FUNCTION__);
            res = drreg_insert_restore_all(drcontext, ilist, where, true,
                                           restored_for_read);
        }
        if (res != DRREG_SUCCESS)
            drreg_report_error(res, "failed to restore for clean call");
    }
    if (TEST(DR_CLEANCALL_WRITES_APP_CONTEXT, call_flags)) {
        if (TEST(DR_CLEANCALL_MULTIPATH, call_flags)) {
            /* We do not support this, partly b/c it is rare and complex.
             * We would need some kind of stateless re-spill.
             */
            drreg_report_error(DRREG_ERROR_FEATURE_NOT_AVAILABLE,
                               "combining DR_CLEANCALL_WRITES_APP_CONTEXT and "
                               "DR_CLEANCALL_MULTIPATH is not supported");
            return;
        }
        per_thread_t *pt = get_tls_data(drcontext);
        LOG(drcontext, DR_LOG_ALL, 3, "%s: updating after cleancall wrote app regs\n",
            __FUNCTION__);
        res = drreg_insert_respill_all(drcontext, pt, ilist, where, instr_get_next(where),
                                       true, restored_for_read);
        if (res != DRREG_SUCCESS)
            drreg_report_error(res, "failed to update for clean call");
    }
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
    per_thread_t *pt = get_tls_data(drcontext);
    instr_t *inst;
    ptr_uint_t aflags_new, aflags_cur = 0;
    reg_id_t reg;

    /* We just use index 0 of the live vectors */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        pt->reg[GPR_IDX(reg)].app_uses = 0;
        drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, 0, REG_UNKNOWN);
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
                     /* A write to a 32-bit reg zeroes the top 32 bits for x86_64 and
                      * aarch64.
                      */
                     IF_X64(||
                            instr_writes_to_exact_reg(inst, reg_64_to_32(reg),
                                                      DR_QUERY_INCLUDE_COND_SRCS)))
                value = REG_DEAD;
            if (value != REG_UNKNOWN)
                drvector_set_entry(&pt->reg[GPR_IDX(reg)].live, 0, value);
        }

        /* aflags liveness */
        aflags_new = instr_get_arith_flags(inst, DR_QUERY_INCLUDE_COND_SRCS);
        /* reading and writing counts only as reading */
        aflags_new &= (~(EFLAGS_READ_TO_WRITE(aflags_new)));
        /* writing doesn't count if already read */
        aflags_new &= (~(EFLAGS_READ_TO_WRITE(aflags_cur)));
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
    drvector_set_entry(&pt->aflags.live, 0,
                       (void *)(ptr_uint_t)
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
    drvector_init(vec, DR_NUM_GPR_REGS, false /*!synch*/, NULL);
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

/* Assumes liveness info is already set up in per_thread_t. Liveness should have either
 * been computed by a forward liveness scan upon every insertion if called outside of
 * insertion phase, see drreg_forward_analysis(). Or if called inside insertion
 * phase, at the end of drmgr's analysis phase once, see drreg_event_bb_analysis().
 * Please note that drreg is not yet able to properly handle multiple users if they use
 * drreg from in and outside of the insertion phase, xref i#3823.
 */
static drreg_status_t
drreg_reserve_reg_internal(void *drcontext, instrlist_t *ilist, instr_t *where,
                           drvector_t *reg_allowed, bool only_if_no_spill,
                           OUT reg_id_t *reg_out)
{
    per_thread_t *pt = get_tls_data(drcontext);
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
                LOG(drcontext, DR_LOG_ALL, 3,
                    "%s @%d." PFX ": using un-restored %s slot %d\n", __FUNCTION__,
                    pt->live_idx, get_where_app_pc(where), get_register_name(reg), slot);
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
            if (reg ==
                dr_get_stolen_reg() IF_ARM(|| reg == DR_REG_PC)
                /* Avoid xsp, even if it appears dead in things like OP_sysenter.
                 * On AArch64 use of SP is very restricted.
                 */
                IF_NOT_ARM(|| reg == DR_REG_XSP))
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
        else {
#ifdef X86
            /* If aflags was unreserved but is still in xax, give it up rather than
             * fail to reserve a new register.
             */
            if (!pt->aflags.in_use && pt->reg[GPR_IDX(DR_REG_XAX)].in_use &&
                pt->aflags.xchg == DR_REG_XAX &&
                (reg_allowed == NULL ||
                 drvector_get_entry(reg_allowed, GPR_IDX(DR_REG_XAX)) != NULL)) {
                LOG(drcontext, DR_LOG_ALL, 3,
                    "%s @%d." PFX ": taking xax from unreserved aflags\n", __FUNCTION__,
                    pt->live_idx, get_where_app_pc(where));
                drreg_move_aflags_from_reg(drcontext, ilist, where, pt, true);
                reg = DR_REG_XAX;
            } else
#endif
                return DRREG_ERROR_REG_CONFLICT;
        }
    }
    if (slot == MAX_SPILLS) {
        slot = find_free_slot(drcontext, pt, ilist, where);
        if (slot == MAX_SPILLS)
            return DRREG_ERROR_OUT_OF_SLOTS;
    }

    ASSERT(!pt->reg[GPR_IDX(reg)].in_use, "overlapping uses");
    pt->reg[GPR_IDX(reg)].in_use = true;
    if (!already_spilled) {
        /* Even if dead now, we need to own a slot in case reserved past dead point */
        if (ops.conservative ||
            drvector_get_entry(&pt->reg[GPR_IDX(reg)].live, pt->live_idx) == REG_LIVE) {
            LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": spilling %s to slot %d\n",
                __FUNCTION__, pt->live_idx, get_where_app_pc(where),
                get_register_name(reg), slot);
            spill_reg(drcontext, pt, reg, slot, ilist, where);
            pt->reg[GPR_IDX(reg)].ever_spilled = true;
        } else {
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @%d." PFX ": no need to spill %s to slot %d\n", __FUNCTION__,
                pt->live_idx, get_where_app_pc(where), get_register_name(reg), slot);
            pt->slot_use[slot] = reg;
            pt->reg[GPR_IDX(reg)].ever_spilled = false;
        }
    } else {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": %s already spilled to slot %d\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where), get_register_name(reg),
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
    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
    drreg_status_t res;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        res = drreg_forward_analysis(drcontext, where);
        if (res != DRREG_SUCCESS)
            return res;
    }
    /* FIXME i#3827: ever_spilled is not being reset. */
    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    res =
        drreg_reserve_reg_internal(drcontext, ilist, where, reg_allowed, false, reg_out);
    instrlist_set_auto_predicate(ilist, pred);
    return res;
}

drreg_status_t
drreg_reserve_dead_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                            drvector_t *reg_allowed, OUT reg_id_t *reg_out)
{
    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
    drreg_status_t res;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        res = drreg_forward_analysis(drcontext, where);
        if (res != DRREG_SUCCESS)
            return res;
    }
    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    res = drreg_reserve_reg_internal(drcontext, ilist, where, reg_allowed, true, reg_out);
    instrlist_set_auto_predicate(ilist, pred);
    return res;
}

drreg_status_t
drreg_restore_app_value(void *drcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t app_reg, reg_id_t dst_reg, bool stateful)
{
    per_thread_t *pt = get_tls_data(drcontext);
    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);

    if (!reg_is_pointer_sized(app_reg) || !reg_is_pointer_sized(dst_reg))
        return DRREG_ERROR_INVALID_PARAMETER;

    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);

    /* check if app_reg is stolen reg */
    if (app_reg == dr_get_stolen_reg()) {
        /* DR will refuse to load into the same reg (the caller must use
         * opnd_replace_reg() with a scratch reg in that case).
         */
        if (dst_reg == app_reg) {
            instrlist_set_auto_predicate(ilist, pred);
            return DRREG_ERROR_INVALID_PARAMETER;
        }
        if (dr_insert_get_stolen_reg_value(drcontext, ilist, where, dst_reg)) {
            instrlist_set_auto_predicate(ilist, pred);
            return DRREG_SUCCESS;
        }
        ASSERT(false, "internal error on getting stolen reg app value");
        instrlist_set_auto_predicate(ilist, pred);
        return DRREG_ERROR;
    }

    /* check if app_reg is an unspilled reg */
    if (pt->reg[GPR_IDX(app_reg)].native) {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": reg %s already native\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where),
            get_register_name(app_reg));
        if (dst_reg != app_reg) {
            PRE(ilist, where,
                XINST_CREATE_move(drcontext, opnd_create_reg(dst_reg),
                                  opnd_create_reg(app_reg)));
        }
        instrlist_set_auto_predicate(ilist, pred);
        return DRREG_SUCCESS;
    }

    /* we may have lost the app value for a dead reg */
    if (!pt->reg[GPR_IDX(app_reg)].ever_spilled) {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": reg %s never spilled\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where),
            get_register_name(app_reg));
        instrlist_set_auto_predicate(ilist, pred);
        return DRREG_ERROR_NO_APP_VALUE;
    }
    /* restore app value back to app_reg */
    if (pt->reg[GPR_IDX(app_reg)].xchg != DR_REG_NULL) {
        /* XXX i#511: NYI */
        instrlist_set_auto_predicate(ilist, pred);
        return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
    }
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": getting app value for %s\n",
        __FUNCTION__, pt->live_idx, get_where_app_pc(where), get_register_name(app_reg));
    /* XXX i#511: if we add .xchg support for GPR's we'll need to check them all here. */
    if (pt->aflags.xchg == app_reg) {
        /* Bail on keeping the flags in the reg. */
        drreg_move_aflags_from_reg(drcontext, ilist, where, pt, stateful);
    } else {
        restore_reg(drcontext, pt, app_reg, pt->reg[GPR_IDX(app_reg)].slot, ilist, where,
                    stateful && !pt->reg[GPR_IDX(app_reg)].in_use);
        if (stateful && !pt->reg[GPR_IDX(app_reg)].in_use)
            pt->reg[GPR_IDX(app_reg)].native = true;
    }
    instrlist_set_auto_predicate(ilist, pred);
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_get_app_value(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t app_reg,
                    reg_id_t dst_reg)
{
    return drreg_restore_app_value(drcontext, ilist, where, app_reg, dst_reg, true);
}

drreg_status_t
drreg_restore_app_values(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t opnd,
                         INOUT reg_id_t *swap)
{
    drreg_status_t res;
    bool no_app_value = false;
    int num_op = opnd_num_regs_used(opnd);
    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
    int i;

    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    for (i = 0; i < num_op; i++) {
        reg_id_t reg = opnd_get_reg_used(opnd, i);
        reg_id_t dst;
        if (!reg_is_gpr(reg))
            continue;
        reg = reg_to_pointer_sized(reg);
        dst = reg;
        if (reg == dr_get_stolen_reg()) {
            if (swap == NULL) {
                instrlist_set_auto_predicate(ilist, pred);
                return DRREG_ERROR_INVALID_PARAMETER;
            }
            if (*swap == DR_REG_NULL) {
                res = drreg_reserve_register(drcontext, ilist, where, NULL, &dst);
                if (res != DRREG_SUCCESS) {
                    instrlist_set_auto_predicate(ilist, pred);
                    return res;
                }
            } else
                dst = *swap;
            if (!opnd_replace_reg(&opnd, reg, dst)) {
                instrlist_set_auto_predicate(ilist, pred);
                return DRREG_ERROR;
            }
            *swap = dst;
        }
        res = drreg_get_app_value(drcontext, ilist, where, reg, dst);
        if (res == DRREG_ERROR_NO_APP_VALUE)
            no_app_value = true;
        else if (res != DRREG_SUCCESS) {
            instrlist_set_auto_predicate(ilist, pred);
            return res;
        }
    }
    instrlist_set_auto_predicate(ilist, pred);
    return (no_app_value ? DRREG_ERROR_NO_APP_VALUE : DRREG_SUCCESS);
}

drreg_status_t
drreg_statelessly_restore_app_value(void *drcontext, instrlist_t *ilist, reg_id_t reg,
                                    instr_t *where_restore, instr_t *where_respill,
                                    bool *restore_needed OUT, bool *respill_needed OUT)
{
    per_thread_t *pt = get_tls_data(drcontext);
    drreg_status_t res;
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where_restore), get_register_name(reg));
    if (where_restore == NULL || where_respill == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (reg == DR_REG_NULL) {
        res = drreg_restore_aflags(drcontext, ilist, where_restore, pt, false);
    } else {
        if (!reg_is_pointer_sized(reg) || reg == dr_get_stolen_reg())
            return DRREG_ERROR_INVALID_PARAMETER;
        res = drreg_restore_app_value(drcontext, ilist, where_restore, reg, reg, false);
    }
    if (restore_needed != NULL)
        *restore_needed = (res == DRREG_SUCCESS);
    if (res != DRREG_SUCCESS && res != DRREG_ERROR_NO_APP_VALUE)
        return res;
        /* XXX i#511: if we add .xchg support for GPR's we'll need to check them all here.
         */
#ifdef X86
    if (res != DRREG_ERROR_NO_APP_VALUE && reg != DR_REG_NULL && pt->aflags.xchg == reg) {
        ASSERT(reg == DR_REG_XAX, "xax is the only x86 reg that may have spilled aflags");
        /* If reg has aflags, they need to be spilled from reg to slot at `where_restore`,
         * and then restored from the slot to reg at `where_respill`. This is done locally
         * to this routine, and doesn't affect any existing drreg state (so this routine
         * continues to be 'stateless'). This spill slot is used only between
         * `where_restore` and `where_respill`; we assume there's no new drreg reservation
         * by the client in between these, which could spill to this temp slot and clobber
         * the native aflags value. The alternative to this limitation is to keep a slot
         * aside for aflags whenever aflags are reserved (currently we do not reserve any
         * spill slot for aflags when they can be stored in xax), which may reduce the
         * number of available spill slots, so we don't go for that option.
         *
         * The first step (spilling of aflags from reg to slot) is done by
         * drreg_restore_app_value above which invokes drreg_move_aflags_from_reg with
         * stateful=false, which spills aflags to some slot regardless of whether they are
         * live or not. Also, even though aflags are spilled to a slot, pt->aflags.xchg is
         * not cleared. Below, we do the second step (restore aflags from slot to reg),
         * and forget the spill slot.
         */
        ASSERT(pt->aflags.slot != MAX_SPILLS, "Aflags slot not reserved");
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " restoring aflags from slot\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where_restore));
        restore_reg(drcontext, pt, DR_REG_XAX, pt->aflags.slot, ilist, where_respill,
                    /*release=*/true);
        reset_aflags_spill_slot(pt);
        if (respill_needed != NULL)
            *respill_needed = true;
    } else
#endif
        if (respill_needed != NULL)
        *respill_needed = false;
    return res;
}

drreg_status_t
drreg_statelessly_restore_all(void *drcontext, instrlist_t *ilist, instr_t *where_restore,
                              instr_t *where_respill, bool *restore_needed OUT,
                              bool *respill_needed OUT)
{
    bool restored_any = false, respilled_any = false;
    bool restored = false, respilled = false;
    drreg_status_t res =
        drreg_statelessly_restore_app_value(drcontext, ilist, DR_REG_NULL, where_restore,
                                            where_respill, &restored, &respilled);
    if (res != DRREG_SUCCESS && res != DRREG_ERROR_NO_APP_VALUE)
        return res;
    restored_any = restored_any || restored;
    respilled_any = respilled_any || respilled;
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (reg == dr_get_stolen_reg())
            continue;
        res = drreg_statelessly_restore_app_value(drcontext, ilist, reg, where_restore,
                                                  where_respill, &restored, &respilled);
        /* We document that on an error we leave partial restores in place and do not
         * return anything in the output parameters.
         */
        if (res != DRREG_SUCCESS && res != DRREG_ERROR_NO_APP_VALUE)
            return res;
        restored_any = restored_any || restored;
        respilled_any = respilled_any || respilled;
    }
    if (restore_needed != NULL)
        *restore_needed = restored_any;
    if (respill_needed != NULL)
        *respill_needed = respilled_any;
    return res;
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
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": restoring %s\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(inst), get_register_name(reg));
        restore_reg(drcontext, pt, reg, pt->reg[GPR_IDX(reg)].slot, ilist, inst, true);
    } else {
        /* still need to release slot */
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": %s never spilled\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(inst), get_register_name(reg));
        pt->slot_use[pt->reg[GPR_IDX(reg)].slot] = DR_REG_NULL;
    }
    pt->reg[GPR_IDX(reg)].native = true;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_unreserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg)
{
    per_thread_t *pt = get_tls_data(drcontext);
    if (!pt->reg[GPR_IDX(reg)].in_use)
        return DRREG_ERROR_INVALID_PARAMETER;
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX " %s\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where), get_register_name(reg));
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        /* We have no way to lazily restore.  We do not bother at this point
         * to try and eliminate back-to-back spill/restore pairs.
         */
        dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
        drreg_status_t res;

        /* XXX i#2585: drreg should predicate spills and restores as appropriate */
        instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
        res = drreg_restore_reg_now(drcontext, ilist, where, pt, reg);
        instrlist_set_auto_predicate(ilist, pred);
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
    drreg_reserve_info_t info = {
        sizeof(info),
    };
    per_thread_t *pt = get_tls_data(drcontext);
    drreg_status_t res;
    if (!(reg >= DR_REG_START_GPR && reg <= DR_REG_STOP_GPR &&
          pt->reg[GPR_IDX(reg)].in_use) &&
        !(reg == DR_REG_NULL && pt->aflags.in_use))
        return DRREG_ERROR_INVALID_PARAMETER;
    res = drreg_reservation_info_ex(drcontext, reg, &info);
    if (res != DRREG_SUCCESS)
        return res;
    if (opnd != NULL)
        *opnd = info.opnd;
    if (is_dr_slot != NULL)
        *is_dr_slot = info.is_dr_slot;
    if (tls_offs != NULL)
        *tls_offs = info.tls_offs;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_reservation_info_ex(void *drcontext, reg_id_t reg, drreg_reserve_info_t *info OUT)
{
    per_thread_t *pt;
    reg_info_t *reg_info;
    if (info == NULL || info->size != sizeof(drreg_reserve_info_t))
        return DRREG_ERROR_INVALID_PARAMETER;
    pt = get_tls_data(drcontext);
    if (reg == DR_REG_NULL)
        reg_info = &pt->aflags;
    else {
        if (reg < DR_REG_START_GPR || reg > DR_REG_STOP_GPR)
            return DRREG_ERROR_INVALID_PARAMETER;
        reg_info = &pt->reg[GPR_IDX(reg)];
    }
    info->reserved = reg_info->in_use;
    info->holds_app_value = reg_info->native;
    if (reg_info->native) {
        info->app_value_retained = false;
        info->opnd = opnd_create_null();
        info->is_dr_slot = false;
        info->tls_offs = -1;
    } else if (reg_info->xchg != DR_REG_NULL) {
        info->app_value_retained = true;
        info->opnd = opnd_create_reg(reg_info->xchg);
        info->is_dr_slot = false;
        info->tls_offs = -1;
    } else {
        info->app_value_retained = reg_info->ever_spilled;
        uint slot = reg_info->slot;
        if ((reg == DR_REG_NULL && !reg_info->native &&
             pt->slot_use[slot] != DR_REG_NULL) ||
            (reg != DR_REG_NULL && pt->slot_use[slot] == reg)) {
            if (slot < ops.num_spill_slots) {
                info->opnd = dr_raw_tls_opnd(drcontext, tls_seg, tls_slot_offs);
                info->is_dr_slot = false;
                info->tls_offs = tls_slot_offs + slot * sizeof(reg_t);
            } else {
                dr_spill_slot_t DR_slot = (dr_spill_slot_t)(slot - ops.num_spill_slots);
                if (DR_slot < dr_max_opnd_accessible_spill_slot())
                    info->opnd = dr_reg_spill_slot_opnd(drcontext, DR_slot);
                else {
                    /* Multi-step so no single opnd */
                    info->opnd = opnd_create_null();
                }
                info->is_dr_slot = true;
                info->tls_offs = DR_slot;
            }
        } else {
            info->opnd = opnd_create_null();
            info->is_dr_slot = false;
            info->tls_offs = -1;
        }
    }
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_is_register_dead(void *drcontext, reg_id_t reg, instr_t *inst, bool *dead)
{
    per_thread_t *pt = get_tls_data(drcontext);
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

drreg_status_t
drreg_set_bb_properties(void *drcontext, drreg_bb_properties_t flags)
{
    per_thread_t *pt = get_tls_data(drcontext);
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_APP2APP &&
        drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_ANALYSIS &&
        drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_INSERTION)
        return DRREG_ERROR_FEATURE_NOT_AVAILABLE;
    /* XXX: interactions with multiple callers gets messy...for now we just or-in */
    pt->bb_props |= flags;
    LOG(drcontext, DR_LOG_ALL, 2, "%s: bb flags are now 0x%x\n", __FUNCTION__,
        pt->bb_props);
    return DRREG_SUCCESS;
}

/***************************************************************************
 * ARITHMETIC FLAGS
 */

/* The caller should only call if aflags are currently in xax.
 * If aflags are in use, moves them to TLS.
 * If not, restores aflags if necessary and restores xax.
 */
static void
drreg_move_aflags_from_reg(void *drcontext, instrlist_t *ilist, instr_t *where,
                           per_thread_t *pt, bool stateful)
{
#ifdef X86
    if (pt->aflags.in_use || !stateful) {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": moving aflags from xax to slot\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where));
        ASSERT(pt->aflags.slot == MAX_SPILLS, "aflags slot not reset");
        if (pt->aflags.slot == MAX_SPILLS) {
            pt->aflags.slot = find_free_slot(drcontext, pt, ilist, where);
        }
        spill_reg(drcontext, pt, DR_REG_XAX, pt->aflags.slot, ilist, where);
    } else if (!pt->aflags.native) {
        drreg_status_t res;
        LOG(drcontext, DR_LOG_ALL, 3,
            "%s @%d." PFX ": lazily restoring aflags for app xax\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(where));
        res = drreg_restore_aflags(drcontext, ilist, where, pt, true /*release*/);
        if (res != DRREG_SUCCESS)
            drreg_report_error(res, "failed to restore flags before app xax");
        pt->aflags.native = true;
        reset_aflags_spill_slot(pt);
    }
    LOG(drcontext, DR_LOG_ALL, 3,
        "%s @%d." PFX ": restoring xax spilled for aflags in slot %d\n", __FUNCTION__,
        pt->live_idx, get_where_app_pc(where),
        pt->reg[DR_REG_XAX - DR_REG_START_GPR].slot);
    if (ops.conservative ||
        drvector_get_entry(&pt->reg[DR_REG_XAX - DR_REG_START_GPR].live, pt->live_idx) ==
            REG_LIVE) {
        restore_reg(drcontext, pt, DR_REG_XAX,
                    pt->reg[DR_REG_XAX - DR_REG_START_GPR].slot, ilist, where, stateful);
    } else if (stateful)
        pt->slot_use[pt->reg[DR_REG_XAX - DR_REG_START_GPR].slot] = DR_REG_NULL;
    if (stateful) {
        pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use = false;
        pt->reg[DR_REG_XAX - DR_REG_START_GPR].native = true;
        pt->reg[DR_REG_XAX - DR_REG_START_GPR].ever_spilled = false;
        pt->aflags.xchg = DR_REG_NULL;
    }
#endif
}

/* May modify pt->aflags.xchg or pt->aflags.slot */
static drreg_status_t
drreg_spill_aflags(void *drcontext, instrlist_t *ilist, instr_t *where, per_thread_t *pt)
{
#ifdef X86
    uint aflags = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
    reg_id_t xax_swap = DR_REG_NULL;
    drreg_status_t res;
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX "\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where));
    /* It may be in-use for ourselves, storing the flags in xax. */
    if (pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use && pt->aflags.xchg != DR_REG_XAX) {
        /* No way to tell whoever is using xax that we need it, so we pick an
         * unreserved reg, spill it, and put xax there temporarily.  We store
         * aflags in our dedicated aflags tls slot and don't try to keep it in
         * this reg.
         */
        res = drreg_reserve_reg_internal(drcontext, ilist, where, NULL, false, &xax_swap);
        if (res != DRREG_SUCCESS)
            return res;
        LOG(drcontext, DR_LOG_ALL, 3, "  xax is in use: using %s temporarily\n",
            get_register_name(xax_swap));
        PRE(ilist, where,
            INSTR_CREATE_xchg(drcontext, opnd_create_reg(DR_REG_XAX),
                              opnd_create_reg(xax_swap)));
    }
    if (!pt->reg[DR_REG_XAX - DR_REG_START_GPR].native) {
        /* xax is unreserved but not restored */
        ASSERT(pt->slot_use[pt->reg[DR_REG_XAX - DR_REG_START_GPR].slot] == DR_REG_XAX,
               "xax tracking error");
        LOG(drcontext, DR_LOG_ALL, 3, "  using un-restored xax in slot %d\n",
            pt->reg[DR_REG_XAX - DR_REG_START_GPR].slot);
    } else if (pt->aflags.xchg != DR_REG_XAX) {
        uint xax_slot = find_free_slot(drcontext, pt, ilist, where);
        if (xax_slot == MAX_SPILLS)
            return DRREG_ERROR_OUT_OF_SLOTS;
        if (ops.conservative ||
            drvector_get_entry(&pt->reg[DR_REG_XAX - DR_REG_START_GPR].live,
                               pt->live_idx) == REG_LIVE) {
            spill_reg(drcontext, pt, DR_REG_XAX, xax_slot, ilist, where);
            pt->reg[DR_REG_XAX - DR_REG_START_GPR].ever_spilled = true;
        } else {
            /* XXX: Re-analyzing this: we shouldn't need this slot?
             * drreg_move_aflags_from_reg() undoes it for this xax-is-dead case; can
             * we remove both?
             */
            pt->slot_use[xax_slot] = DR_REG_XAX;
        }
        pt->reg[DR_REG_XAX - DR_REG_START_GPR].slot = xax_slot;
        ASSERT(pt->slot_use[xax_slot] == DR_REG_XAX, "slot should be for xax");
    }
    PRE(ilist, where, INSTR_CREATE_lahf(drcontext));
    if (TEST(EFLAGS_READ_OF, aflags)) {
        PRE(ilist, where,
            INSTR_CREATE_setcc(drcontext, OP_seto, opnd_create_reg(DR_REG_AL)));
    }
    if (xax_swap != DR_REG_NULL) {
        ASSERT(pt->aflags.slot == MAX_SPILLS, "aflags slot not reset");
        if (pt->aflags.slot == MAX_SPILLS) {
            pt->aflags.slot = find_free_slot(drcontext, pt, ilist, where);
        }
        /* We want to use xax only to spill aflags. This is to make drreg state
         * restoration a bit simpler. So, we first spill aflags to a slot using xax,
         * and then restore xax's value from xax_swap.
         */
        spill_reg(drcontext, pt, DR_REG_XAX, pt->aflags.slot, ilist, where);
        PRE(ilist, where,
            INSTR_CREATE_xchg(drcontext, opnd_create_reg(xax_swap),
                              opnd_create_reg(DR_REG_XAX)));
        res = drreg_unreserve_register(drcontext, ilist, where, xax_swap);
        if (res != DRREG_SUCCESS)
            return res; /* XXX: undo already-inserted instrs? */
    } else {
        /* As an optimization we keep the flags in xax itself until forced to move
         * them to the aflags TLS slot.
         */
        pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use = true;
        pt->reg[DR_REG_XAX - DR_REG_START_GPR].native = false;
        pt->aflags.xchg = DR_REG_XAX;
    }

#elif defined(AARCHXX)
    drreg_status_t res = DRREG_SUCCESS;
    reg_id_t scratch;
    res = drreg_reserve_reg_internal(drcontext, ilist, where, NULL, false, &scratch);
    if (res != DRREG_SUCCESS)
        return res;
    dr_save_arith_flags_to_reg(drcontext, ilist, where, scratch);
    ASSERT(pt->aflags.slot == MAX_SPILLS, "aflags slot not reset");
    if (pt->aflags.slot == MAX_SPILLS) {
        pt->aflags.slot = find_free_slot(drcontext, pt, ilist, where);
    }
    spill_reg(drcontext, pt, scratch, pt->aflags.slot, ilist, where);
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
    if (pt->aflags.native)
        return DRREG_SUCCESS;
#ifdef X86
    uint aflags = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
    uint temp_slot = 0;
    reg_id_t xax_swap = DR_REG_NULL;
    drreg_status_t res;
    LOG(drcontext, DR_LOG_ALL, 3,
        "%s @%d." PFX ": release=%d xax-in-use=%d,slot=%d xchg=%s\n", __FUNCTION__,
        pt->live_idx, get_where_app_pc(where), release,
        pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use,
        pt->reg[DR_REG_XAX - DR_REG_START_GPR].slot, get_register_name(pt->aflags.xchg));
    if (pt->aflags.xchg == DR_REG_XAX) {
        ASSERT(pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use, "eflags-in-xax error");
    } else {
        temp_slot = find_free_slot(drcontext, pt, ilist, where);
        if (temp_slot == MAX_SPILLS)
            return DRREG_ERROR_OUT_OF_SLOTS;
        if (pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use) {
            /* We pick an unreserved reg, spill it, and put xax there temporarily. */
            res = drreg_reserve_reg_internal(drcontext, ilist, where, NULL, false,
                                             &xax_swap);
            if (res != DRREG_SUCCESS)
                return res;
            LOG(drcontext, DR_LOG_ALL, 3, "  xax is in use: using %s temporarily\n",
                get_register_name(xax_swap));
            PRE(ilist, where,
                INSTR_CREATE_xchg(drcontext, opnd_create_reg(DR_REG_XAX),
                                  opnd_create_reg(xax_swap)));
        } else if (ops.conservative ||
                   drvector_get_entry(&pt->reg[DR_REG_XAX - DR_REG_START_GPR].live,
                                      pt->live_idx) == REG_LIVE)
            spill_reg(drcontext, pt, DR_REG_XAX, temp_slot, ilist, where);
        ASSERT(pt->aflags.slot != MAX_SPILLS, "Aflags slot not reserved");
        restore_reg(drcontext, pt, DR_REG_XAX, pt->aflags.slot, ilist, where, release);
    }
    if (TEST(EFLAGS_READ_OF, aflags)) {
        /* i#2351: DR's "add 0x7f, %al" is destructive.  Instead we use a
         * cmp so we can avoid messing up the value in al, which is
         * required for keeping the flags in xax.
         */
        PRE(ilist, where,
            INSTR_CREATE_cmp(drcontext, opnd_create_reg(DR_REG_AL),
                             OPND_CREATE_INT8(-127)));
    }
    PRE(ilist, where, INSTR_CREATE_sahf(drcontext));
    if (xax_swap != DR_REG_NULL) {
        PRE(ilist, where,
            INSTR_CREATE_xchg(drcontext, opnd_create_reg(xax_swap),
                              opnd_create_reg(DR_REG_XAX)));
        res = drreg_unreserve_register(drcontext, ilist, where, xax_swap);
        if (res != DRREG_SUCCESS)
            return res; /* XXX: undo already-inserted instrs? */
    } else if (pt->aflags.xchg == DR_REG_XAX) {
        if (release) {
            pt->aflags.xchg = DR_REG_NULL;
            pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use = false;
        }
    } else {
        if (ops.conservative ||
            drvector_get_entry(&pt->reg[DR_REG_XAX - DR_REG_START_GPR].live,
                               pt->live_idx) == REG_LIVE)
            restore_reg(drcontext, pt, DR_REG_XAX, temp_slot, ilist, where, true);
    }
#elif defined(AARCHXX)
    drreg_status_t res = DRREG_SUCCESS;
    reg_id_t scratch;
    res = drreg_reserve_reg_internal(drcontext, ilist, where, NULL, false, &scratch);
    if (res != DRREG_SUCCESS)
        return res;
    ASSERT(pt->aflags.slot != MAX_SPILLS, "Aflags slot not reserved");
    restore_reg(drcontext, pt, scratch, pt->aflags.slot, ilist, where, release);
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
    per_thread_t *pt = get_tls_data(drcontext);
    dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
    drreg_status_t res;
    uint aflags;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        res = drreg_forward_analysis(drcontext, where);
        if (res != DRREG_SUCCESS)
            return res;
        ASSERT(pt->live_idx == 0, "non-drmgr-insert always uses 0 index");
    }
    aflags = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
    /* Just like scratch regs, flags are exclusively owned */
    if (pt->aflags.in_use)
        return DRREG_ERROR_IN_USE;
    if (!TESTANY(EFLAGS_READ_ARITH, aflags)) {
        /* If the flags were not yet lazily restored and are now dead, clear the slot */
        if (!pt->aflags.native) {
            reset_aflags_spill_slot(pt);
        }
        pt->aflags.in_use = true;
        pt->aflags.native = true;
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": aflags are dead\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(where));
        return DRREG_SUCCESS;
    }
    /* Check for a prior reservation not yet lazily restored */
    if (!pt->aflags.native IF_X86(||
                                  (pt->reg[DR_REG_XAX - DR_REG_START_GPR].in_use &&
                                   pt->aflags.xchg == DR_REG_XAX))) {
        LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": using un-restored aflags\n",
            __FUNCTION__, pt->live_idx, get_where_app_pc(where));
        ASSERT(pt->aflags.xchg != DR_REG_NULL ||
                   (pt->aflags.slot != MAX_SPILLS &&
                    pt->slot_use[pt->aflags.slot] != DR_REG_NULL),
               "lost slot reservation");
        pt->aflags.native = false;
        pt->aflags.in_use = true;
        return DRREG_SUCCESS;
    }

    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX ": spilling aflags\n", __FUNCTION__,
        pt->live_idx, get_where_app_pc(where));
    /* drreg_spill_aflags writes to this, so clear first.  The inconsistent combo
     * xchg-null but xax-in-use won't happen b/c we'll use un-restored above.
     */
    pt->aflags.xchg = DR_REG_NULL;
    /* XXX i#2585: drreg should predicate spills and restores as appropriate */
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    res = drreg_spill_aflags(drcontext, ilist, where, pt);
    instrlist_set_auto_predicate(ilist, pred);
    if (res != DRREG_SUCCESS)
        return res;
    /* pt->aflags.xchg or pt->aflags.slot are set in drreg_spill_aflags. */
    pt->aflags.in_use = true;
    pt->aflags.native = false;
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_unreserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    per_thread_t *pt = get_tls_data(drcontext);
    if (!pt->aflags.in_use)
        return DRREG_ERROR_INVALID_PARAMETER;
    pt->aflags.in_use = false;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
        /* We have no way to lazily restore.  We do not bother at this point
         * to try and eliminate back-to-back spill/restore pairs.
         */
        /* XXX i#2585: drreg should predicate spills and restores as appropriate */
        instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
        if (pt->aflags.xchg != DR_REG_NULL)
            drreg_move_aflags_from_reg(drcontext, ilist, where, pt, true);
        else if (!pt->aflags.native) {
            drreg_restore_aflags(drcontext, ilist, where, pt, true /*release*/);
            pt->aflags.native = true;
            ASSERT(pt->aflags.slot != MAX_SPILLS, "Aflags slot not reserved");
            reset_aflags_spill_slot(pt);
        }
        ASSERT(pt->aflags.slot == MAX_SPILLS, "Aflags slot not reset");
        instrlist_set_auto_predicate(ilist, pred);
    }
    LOG(drcontext, DR_LOG_ALL, 3, "%s @%d." PFX "\n", __FUNCTION__, pt->live_idx,
        get_where_app_pc(where));
    /* We lazily restore in drreg_event_bb_insert_late(), in case
     * someone else wants the aflags locally.
     */
    return DRREG_SUCCESS;
}

drreg_status_t
drreg_aflags_liveness(void *drcontext, instr_t *inst, OUT uint *value)
{
    per_thread_t *pt = get_tls_data(drcontext);
    if (value == NULL)
        return DRREG_ERROR_INVALID_PARAMETER;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION) {
        drreg_status_t res = drreg_forward_analysis(drcontext, inst);
        if (res != DRREG_SUCCESS)
            return res;
        ASSERT(pt->live_idx == 0, "non-drmgr-insert always uses 0 index");
    }
    *value = (uint)(ptr_uint_t)drvector_get_entry(&pt->aflags.live, pt->live_idx);
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

drreg_status_t
drreg_restore_app_aflags(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    per_thread_t *pt = get_tls_data(drcontext);
    drreg_status_t res = DRREG_SUCCESS;
    if (!pt->aflags.native) {
        dr_pred_type_t pred = instrlist_get_auto_predicate(ilist);
        LOG(drcontext, DR_LOG_ALL, 3,
            "%s @%d." PFX ": restoring app aflags as requested\n", __FUNCTION__,
            pt->live_idx, get_where_app_pc(where));
        /* XXX i#2585: drreg should predicate spills and restores as appropriate */
        instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
        res = drreg_restore_aflags(drcontext, ilist, where, pt, !pt->aflags.in_use);
        instrlist_set_auto_predicate(ilist, pred);
        if (!pt->aflags.in_use) {
            pt->aflags.native = true;
            reset_aflags_spill_slot(pt);
        }
    }
    return res;
}

/***************************************************************************
 * RESTORE STATE
 */

static bool
is_our_spill_or_restore(void *drcontext, instr_t *instr, bool *spill OUT,
                        reg_id_t *reg_spilled OUT, uint *slot_out OUT, uint *offs_out OUT)
{
    bool tls;
    uint slot, offs;
    reg_id_t reg;
    if (!instr_is_reg_spill_or_restore(drcontext, instr, &tls, spill, &reg, &offs))
        return false;
    /* Is this from our raw TLS? */
    if (tls && offs >= tls_slot_offs &&
        offs < (tls_slot_offs + ops.num_spill_slots * sizeof(reg_t))) {
        slot = (offs - tls_slot_offs) / sizeof(reg_t);
    } else {
        /* We assume a DR spill slot, in TLS or thread-private mcontext */
        if (tls) {
            /* We assume the DR slots are either low-to-high or high-to-low. */
            uint DR_min_offs =
                opnd_get_disp(dr_reg_spill_slot_opnd(drcontext, SPILL_SLOT_1));
            uint DR_max_offs = opnd_get_disp(
                dr_reg_spill_slot_opnd(drcontext, dr_max_opnd_accessible_spill_slot()));
            uint max_DR_slot = (uint)dr_max_opnd_accessible_spill_slot();
            if (DR_min_offs > DR_max_offs) {
                if (offs > DR_min_offs) {
                    slot = (offs - DR_min_offs) / sizeof(reg_t);
                } else if (offs < DR_max_offs) {
                    /* Fix hidden slot regardless of low-to-high or vice versa. */
                    slot = max_DR_slot + 1;
                } else {
                    slot = (DR_min_offs - offs) / sizeof(reg_t);
                }
            } else {
                if (offs > DR_max_offs) {
                    slot = (offs - DR_max_offs) / sizeof(reg_t);
                } else if (offs < DR_min_offs) {
                    /* Fix hidden slot regardless of low-to-high or vice versa. */
                    slot = max_DR_slot + 1;
                } else {
                    slot = (offs - DR_min_offs) / sizeof(reg_t);
                }
            }
            if (slot > max_DR_slot) {
                /* This is not a drreg spill, but some TLS access by
                 * tool instrumentation (i#2035).
                 */
                return false;
            }
#ifdef X86
            if (slot > max_DR_slot - 1) {
                /* FIXME i#2933: We rule out the 3rd DR TLS slot b/c it's used by
                 * DR for purposes where there's no restore paired with a spill.
                 * Another tool component could also use the other slots that way,
                 * though: we need a more foolproof solution.  For now we have a hole
                 * and tools should allocate enough dedicated drreg TLS slots to
                 * ensure robustness.
                 */
                return false;
            }
#endif
        } else {
            /* We assume mcontext spill offs is 0-based. */
            slot = offs / sizeof(reg_t);
        }
        slot += ops.num_spill_slots;
    }
    if (reg_spilled != NULL)
        *reg_spilled = reg;
    if (slot_out != NULL)
        *slot_out = slot;
    if (offs_out != NULL)
        *offs_out = offs;
    return true;
}

static inline bool
is_aflags_spill(instr_t *inst)
{
#if defined(X86)
    return instr_get_opcode(inst) == OP_lahf;
#elif defined(AARCHXX)
    return instr_get_opcode(inst) == OP_mrs;
#elif defined(RISCV64)
    /* RISC-V does not have any integer arithmetic or compare flag registers. */
    return false;
#else
#    error Unsupported architecture
#endif
}

static inline bool
is_aflags_restore(instr_t *inst)
{
#if defined(X86)
    return instr_get_opcode(inst) == OP_sahf;
#elif defined(AARCHXX)
    return instr_get_opcode(inst) == OP_msr;
#elif defined(RISCV64)
    /* RISC-V does not have any integer arithmetic or condition flag registers. */
    return false;
#else
#    error Unsupported architecture
#endif
}

drreg_status_t
drreg_is_instr_spill_or_restore(void *drcontext, instr_t *instr, bool *spill OUT,
                                bool *restore OUT, reg_id_t *reg_spilled OUT)
{
    bool is_spill;
    if (!is_our_spill_or_restore(drcontext, instr, &is_spill, reg_spilled, NULL, NULL)) {
        if (spill != NULL)
            *spill = false;
        if (restore != NULL)
            *restore = false;
        return DRREG_SUCCESS;
    }
    if (spill != NULL)
        *spill = is_spill;
    if (restore != NULL)
        *restore = !is_spill;
    return DRREG_SUCCESS;
}

/* Restores machine state modified by drreg, without the reconstructed instrlist
 * of the faulting fragment.
 * This is a best-effort restore state logic which is used when the faulting
 * fragment's reconstructed instrlist is not available. This does not handle
 * some corner cases well, like multiple restores (i#4939), aflags in reg (i#4933)
 * and multi-phase use (i#3823). It may restore the wrong value or not restore
 * at all in these cases.
 * drreg_event_restore_state_with_ilist uses metadata inlined in the
 * reconstructed ilist to restore state in a robust manner. The metadata
 * required is essentially just a bit per instruction for whether it's an app
 * or tool instr.
 * TODO i#4937: Store required metadata for each fragment and use it here for
 * a more robust restoration of state.
 */
static bool
drreg_event_restore_state_without_ilist(void *drcontext, bool restore_memory,
                                        dr_restore_state_info_t *info)
{
    /* To achieve a clean and simple reserve-and-unreserve interface w/o specifying
     * up front how many cross-app-instr scratch regs (and then limited to whole-bb
     * regs with stored per-bb info, like Dr. Memory does), we have to pay with a
     * complex state xl8 scheme.  We need to decode the in-cache fragment and walk
     * it, recognizing our own spills and restores.  We distinguish a tool value
     * spill to a temp slot (from drreg_event_bb_insert_late()) by watching for
     * a spill of an already-spilled reg to a different slot.
     *
     */
    uint spilled_to[DR_NUM_GPR_REGS];
    uint aflags_slot = MAX_SPILLS;
    reg_id_t aflags_reg = DR_REG_NULL;
    reg_id_t reg;
    instr_t inst;
    IF_DEBUG(byte * prev_pc;)
    byte *pc = info->fragment_info.cache_start_pc;
    uint offs;
    bool spill;
    uint slot;
    if (pc == NULL)
        return true; /* fault not in cache */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++)
        spilled_to[GPR_IDX(reg)] = MAX_SPILLS;
    LOG(drcontext, DR_LOG_ALL, 3,
        "%s: processing fault @" PFX ": decoding from " PFX "\n", __FUNCTION__,
        info->raw_mcontext->pc, pc);
    instr_init(drcontext, &inst);
    while (pc < info->raw_mcontext->pc) {
        instr_reset(drcontext, &inst);
        IF_DEBUG(prev_pc = pc;)
        pc = decode(drcontext, pc, &inst);
        if (is_our_spill_or_restore(drcontext, &inst, &spill, &reg, &slot, &offs)) {
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s @" PFX " found %s to %s offs=0x%x => slot %d\n", __FUNCTION__,
                prev_pc, spill ? "spill" : "restore", get_register_name(reg), offs, slot);
            if (spill) {
                if (reg == aflags_reg) {
                    /* TODO i#4937: Aflags are re-spilled when some app instr writes
                     * them. Unlike gpr respills, aflags respills may use a different
                     * slot, or may not use a slot at all (if they are kept in xax as
                     * an optimisation). Unfortunately, without the extra metadata
                     * provided by the faulting fragment ilist, we cannot determine
                     * whether this spill was a tool aflags spill or app aflags spill.
                     * We assume the latter and update our book-keeping.
                     */
                    aflags_slot = slot;
                    /* We do not need to track this anymore. */
                    aflags_reg = DR_REG_NULL;
                } else if (spilled_to[GPR_IDX(reg)] < MAX_SPILLS &&
                           /* allow redundant spill */
                           spilled_to[GPR_IDX(reg)] != slot) {
                    /* This reg is already spilled: we assume that this new spill
                     * is to a tmp slot for preserving the tool's value.
                     */
                    LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX ": ignoring tool spill\n",
                        __FUNCTION__, pc);
                } else {
                    spilled_to[GPR_IDX(reg)] = slot;
                }
            } else {
                if (aflags_slot == slot) {
                    aflags_slot = MAX_SPILLS;
                    aflags_reg = reg;
                } else if (spilled_to[GPR_IDX(reg)] == slot) {
                    /* This suffers from i#4939: we forget the spill slot after it
                     * is read once to restore the gpr for an app read. This case is
                     * handled only if the faulting fragment's ilist is available, in
                     * drreg_event_restore_state_with_ilist.
                     */
                    spilled_to[GPR_IDX(reg)] = MAX_SPILLS;
                } else {
                    LOG(drcontext, DR_LOG_ALL, 3, "%s @" PFX ": ignoring restore\n",
                        __FUNCTION__, pc);
                }
            }
        } else if (is_aflags_spill(&inst)) {
            /* TODO i#4937: Unfortunately, without the extra metadata provided by the
             * faulting fragment ilist, we cannot determine whether this spill was a tool
             * aflags spill or app aflags spill. We assume the latter and update our
             * book-keeping.
             */
            aflags_reg = IF_X86_ELSE(DR_REG_XAX, opnd_get_reg(instr_get_dst(&inst, 0)));
        } else if (aflags_reg != DR_REG_NULL && is_aflags_restore(&inst) &&
                   opnd_get_reg(instr_get_src(&inst, 0)) == aflags_reg) {
            aflags_reg = DR_REG_NULL;
        } else if (aflags_reg != DR_REG_NULL) {
            /* If the reg storing aflags gets written to before being spilled to a slot,
             * we don't want to confuse a later spill as an aflags spill. This doesn't
             * handle all possible cases of aflags being swapped around in regs, but
             * we bail on the more complex instrumentations. The more complex cases
             * are handled only if the faulting fragment's ilist is available, in
             * drreg_event_restore_state_with_ilist.
             */
            for (int i = 0; i < instr_num_dsts(&inst); i++) {
                opnd_t opnd = instr_get_dst(&inst, i);
                if (opnd_is_reg(opnd) && opnd_get_reg(opnd) == aflags_reg) {
                    aflags_reg = DR_REG_NULL;
                }
            }
        }
    }
    instr_free(drcontext, &inst);
/* RISC-V does not have any aflags to restore */
#if !defined(RISCV64)
    if (aflags_slot < MAX_SPILLS || aflags_reg != DR_REG_NULL) {
        reg_t newval = info->mcontext->xflags;
        reg_t val;
        if (aflags_reg != DR_REG_NULL) {
#    ifdef X86
            ASSERT(aflags_reg == DR_REG_XAX, "x86 aflags can only be in xax");
            val = info->mcontext->xax;
#    else
            val = *(reg_t *)(&info->mcontext->r0 + (aflags_reg - DR_REG_R0));
#    endif
        } else {
            val = get_spilled_value(drcontext, aflags_slot);
        }
        newval = dr_merge_arith_flags(newval, val);
        LOG(drcontext, DR_LOG_ALL, 3, "%s: restoring aflags from " PFX " to " PFX "\n",
            __FUNCTION__, info->mcontext->xflags, newval);
        info->mcontext->xflags = newval;
    }
#endif
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (spilled_to[GPR_IDX(reg)] < MAX_SPILLS) {
            reg_t val = get_spilled_value(drcontext, spilled_to[GPR_IDX(reg)]);
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s: restoring %s from slot %d from " PFX " to " PFX "\n", __FUNCTION__,
                get_register_name(reg), spilled_to[GPR_IDX(reg)],
                reg_get_value(reg, info->mcontext), val);
            reg_set_value(reg, info->mcontext, val);
        }
    }
    return true;
}

/* Restores machine state modified by drreg, using the reconstructed instrlist
 * of the faulting fragment.
 * This is a more robust version of drreg_event_restore_state_without_ilist
 * that uses inlined metadata in instrlist.
 */
static bool
drreg_event_restore_state_with_ilist(void *drcontext, bool restore_memory,
                                     dr_restore_state_info_t *info)
{
    /* Tracks the slot where the app value of all gprs/aflags is spilled to.
     * The last element in the array (AFLAGS_ALIAS_REG) is for aflags.
     */
    uint spill_slot[DR_NUM_GPR_REGS + 1];
    /* Tracks the gpr where the app value of aflags is spilled to. */
    reg_id_t aflags_spill_reg;
    /* Tracks the gpr where the tool value of aflags is spilled to. We need
     * to track this so that we can detect spills of tool aflags to a slot,
     * and not confuse it with a spill of the tool_aflags_spill_reg itself.
     */
    reg_id_t tool_aflags_spill_reg;

    reg_id_t reg;
    instr_t *fault_inst, *inst;
    byte *pc = info->fragment_info.cache_start_pc;
    bool spill;
    uint slot;
    if (pc == NULL)
        return true; /* fault not in cache */

    /* Initialize all slots to MAX_SPILLS and spill regs to DR_REG_NULL to denote
     * that no spill is in effect.
     */
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR + 1; reg++) {
        spill_slot[GPR_IDX(reg)] = MAX_SPILLS;
    }
    aflags_spill_reg = DR_REG_NULL;
    tool_aflags_spill_reg = DR_REG_NULL;

    LOG(drcontext, DR_LOG_ALL, 3,
        "%s: processing fault @" PFX ": using reconstructed fragment ilist \n",
        __FUNCTION__, info->raw_mcontext->pc);
    ASSERT(info->fragment_info.ilist != NULL, "ilist required for state restoration");
    /* Find faulting instr in ilist. */
    for (fault_inst = instrlist_first(info->fragment_info.ilist);
         fault_inst != NULL && pc < info->raw_mcontext->pc;
         fault_inst = instr_get_next(fault_inst)) {
        pc += instr_length(drcontext, fault_inst);
    }
    ASSERT(fault_inst != NULL, "fault pc is beyond the given ilist");
    /* Starting from the faulting fragment ilist's end, we work our way backwards to
     * the faulting instr. drreg restores the native value of a spilled gpr/aflags at
     * the fragment's end and/or before an app read instr. We remember the spill slot
     * used by the last restore operation until we find its matching spill (which uses
     * the same spill slot), and then repeat: find the previous restore and its matching
     * spill. If we find a restore-spill pair where the spill is before the faulting
     * instr, we use that spill slot to restore the app value of that gpr/aflags. This
     * way we also are able to skip tool value spills/restores introduced by multi-phase
     * use or otherwise.
     *
     * Working our way backwards and finding the matching spills for a restore is much
     * simpler than going forwards and finding the matching restore for a spill because
     * there may be multiple restores (besides the final restore that ends the spill
     * region, there may be restores for app reads and also user prompted restores).
     *
     * Note that our reverse walk should include the faulting instr. We do not want to
     * consider it executed because it hasn't retired yet.
     */
    for (inst = instrlist_last(info->fragment_info.ilist);
         inst != NULL && instr_get_next(inst) != fault_inst;
         inst = instr_get_prev(inst)) {
        /* XXX: The check for faux spills/restores (app instrs that look like drreg
         * spills/restores) needs the ilist, but maybe we can live without that check,
         * or at least merge drreg_event_restore_state_with_ilist and
         * drreg_event_restore_state_without_ilist and check for !instr_is_app only if
         * the ilist is available.
         */
        if (!instr_is_app(inst)) {
            if (is_our_spill_or_restore(drcontext, inst, &spill, &reg, &slot, NULL)) {
                if (!spill) {
                    /* If we find a restore for app aflags/gpr, we'll record it now. While
                     * working our way back, we'll look for the matching spill (identified
                     * using the slot id). If we don't find any matching spill before
                     * reaching the faulting instr, it means that the spill happened
                     * before the fault occurred and needs to be restored.
                     */
                    if (reg == aflags_spill_reg) {
                        /* Found a restore for the app aflags from a slot to a gpr. */
                        ASSERT(spill_slot[GPR_IDX(AFLAGS_ALIAS_REG)] == MAX_SPILLS,
                               "no spill found for last seen aflags restore");
                        spill_slot[GPR_IDX(AFLAGS_ALIAS_REG)] = slot;
                        aflags_spill_reg = DR_REG_NULL;
                    } else if (reg == tool_aflags_spill_reg) {
                        /* Ignore this restore because we have already recorded the slot
                         * from where the app aflags will be restored. This restore may
                         * be for tool aflags or an app aflags restore before some app
                         * read.
                         * This should also not be confused with a restore for the reg
                         * itself; we do not record any slot for tool_aflags_spill_reg
                         * either.
                         */
                        tool_aflags_spill_reg = DR_REG_NULL;
                        LOG(drcontext, DR_LOG_ALL, 3,
                            "%s: ignoring aflags restore to reg %s from slot %d\n",
                            __FUNCTION__, get_register_name(reg), slot);
                    } else if (spill_slot[GPR_IDX(reg)] == MAX_SPILLS) {
                        /* Found a restore for an app gpr from a slot. */
                        spill_slot[GPR_IDX(reg)] = slot;
                    } else {
                        /* Ignore this restore because we have already recorded the
                         * spill slot from where this gpr will be restored. This
                         * restore may be for a tool gpr value or an app gpr restore
                         * before some app read.
                         */
                        LOG(drcontext, DR_LOG_ALL, 3,
                            "%s: ignoring gpr restore for reg %s from slot %d\n",
                            __FUNCTION__, get_register_name(reg), slot);
                    }
                } else {
                    /* If we find a matching spill here for a previously recorded aflags
                     * or gpr restore, it means that the spill lies after the faulting
                     * instr and has not happened yet, therefore doesn't need to be
                     * restored. So, we clear the recorded spill slot id.
                     */
                    if (spill_slot[GPR_IDX(AFLAGS_ALIAS_REG)] == slot) {
                        /* Found the matching app aflags spill for the previously recorded
                         * restore.
                         */
                        spill_slot[GPR_IDX(AFLAGS_ALIAS_REG)] = MAX_SPILLS;
                        aflags_spill_reg = reg;
                    } else if (spill_slot[GPR_IDX(reg)] == slot) {
                        /* Found the matching app gpr spill for the previously recorded
                         * restore.
                         */
                        spill_slot[GPR_IDX(reg)] = MAX_SPILLS;
                    } else {
                        /* Ignore this spill because we didn't record any restore for
                         * it. It may be a tool value spill, and we not record restores
                         * for tool values.
                         */
                        LOG(drcontext, DR_LOG_ALL, 3,
                            "%s: ignoring spill for reg %s from slot %d\n", __FUNCTION__,
                            get_register_name(reg), slot);
                    }
                }
            } else if (is_aflags_restore(inst)) {
                if (aflags_spill_reg == DR_REG_NULL &&
                    spill_slot[GPR_IDX(AFLAGS_ALIAS_REG)] == MAX_SPILLS) {
                    /* Found a restore for app aflags from reg. */
                    aflags_spill_reg =
                        IF_X86_ELSE(DR_REG_XAX, opnd_get_reg(instr_get_src(inst, 0)));
                } else {
                    /* We need to track movement of meta aflags so that we can ignore a
                     * restore for tool_aflags_spill_reg.
                     */
                    tool_aflags_spill_reg =
                        IF_X86_ELSE(DR_REG_XAX, opnd_get_reg(instr_get_src(inst, 0)));
                }
            } else if (is_aflags_spill(inst)) {
                if (aflags_spill_reg ==
                    IF_X86_ELSE(DR_REG_XAX, opnd_get_reg(instr_get_dst(inst, 0)))) {
                    /* Found the matching app aflags spill for the previously recorded
                     * app aflags restore.
                     */
                    aflags_spill_reg = DR_REG_NULL;
                } else if (tool_aflags_spill_reg ==
                           IF_X86_ELSE(DR_REG_XAX,
                                       opnd_get_reg(instr_get_dst(inst, 0)))) {
                    /* Found the matching tool aflags spill for the previously recorded
                     * tool aflags restore.
                     */
                    tool_aflags_spill_reg = DR_REG_NULL;
                }
            } else {
                for (int i = 0; i < instr_num_dsts(inst); i++) {
                    opnd_t opnd = instr_get_dst(inst, i);
                    if (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                        reg_to_pointer_sized(opnd_get_reg(opnd)) == aflags_spill_reg) {
                        /* As aflags_spill_reg is written by some non-drreg meta instr,
                         * it was recorded by a non-drreg sahf. Reset.
                         */
                        aflags_spill_reg = DR_REG_NULL;
                    }
                    if (opnd_is_reg(opnd) && reg_is_gpr(opnd_get_reg(opnd)) &&
                        reg_to_pointer_sized(opnd_get_reg(opnd)) ==
                            tool_aflags_spill_reg) {
                        tool_aflags_spill_reg = DR_REG_NULL;
                    }
                }
            }
        }
    }
/* RISC-V does not have any aflags to restore */
#if !defined(RISCV64)
    if (aflags_spill_reg != DR_REG_NULL ||
        spill_slot[GPR_IDX(AFLAGS_ALIAS_REG)] != MAX_SPILLS) {
        reg_t newval = info->mcontext->xflags;
        reg_t val;
        slot = spill_slot[GPR_IDX(AFLAGS_ALIAS_REG)];
        if (aflags_spill_reg != DR_REG_NULL) {
#    ifdef X86
            ASSERT(aflags_spill_reg == DR_REG_XAX, "x86 aflags can only be in xax");
            val = info->mcontext->xax;
#    else
            val = *(reg_t *)(&info->mcontext->r0 + (aflags_spill_reg - DR_REG_R0));
#    endif
            newval = dr_merge_arith_flags(newval, val);
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s: restoring aflags from reg %s " PFX " to " PFX "\n", __FUNCTION__,
                get_register_name(aflags_spill_reg), info->mcontext->xflags, newval);
        } else {
            val = get_spilled_value(drcontext, slot);
            newval = dr_merge_arith_flags(newval, val);
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s: restoring aflags from slot %d from " PFX " to " PFX "\n",
                __FUNCTION__, slot, info->mcontext->xflags, newval);
        }
        info->mcontext->xflags = newval;
    }
#endif
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        if (spill_slot[GPR_IDX(reg)] != MAX_SPILLS) {
            slot = spill_slot[GPR_IDX(reg)];
            reg_t val = get_spilled_value(drcontext, slot);
            LOG(drcontext, DR_LOG_ALL, 3,
                "%s: restoring %s from slot %d from " PFX " to " PFX "\n", __FUNCTION__,
                get_register_name(reg), slot, reg_get_value(reg, info->mcontext), val);
            reg_set_value(reg, info->mcontext, val);
        }
    }
    return true;
}

static bool
drreg_event_restore_state(void *drcontext, bool restore_memory,
                          dr_restore_state_info_t *info)
{
    if (info->fragment_info.ilist != NULL)
        return drreg_event_restore_state_with_ilist(drcontext, restore_memory, info);
    return drreg_event_restore_state_without_ilist(drcontext, restore_memory, info);
}

/***************************************************************************
 * INIT AND EXIT
 */

static int drreg_init_count;

static per_thread_t init_pt;

static per_thread_t *
get_tls_data(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    /* Support use during init (i#2910). */
    if (pt == NULL)
        return &init_pt;
    return pt;
}

static void
tls_data_init(per_thread_t *pt)
{
    reg_id_t reg;
    memset(pt, 0, sizeof(*pt));
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        drvector_init(&pt->reg[GPR_IDX(reg)].live, 20, false /*!synch*/, NULL);
        pt->reg[GPR_IDX(reg)].native = true;
    }
    pt->aflags.native = true;
    pt->aflags.slot = MAX_SPILLS;
    drvector_init(&pt->aflags.live, 20, false /*!synch*/, NULL);
}

static void
tls_data_free(per_thread_t *pt)
{
    reg_id_t reg;
    for (reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; reg++) {
        drvector_delete(&pt->reg[GPR_IDX(reg)].live);
    }
    drvector_delete(&pt->aflags.live);
}

static void
drreg_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    tls_data_init(pt);
    pt->tls_seg_base = dr_get_dr_segment_base(tls_seg);
}

static void
drreg_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    tls_data_free(pt);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

drreg_status_t
drreg_init(drreg_options_t *ops_in)
{
    uint prior_slots = ops.num_spill_slots;
    drmgr_priority_t high_priority = { sizeof(high_priority),
                                       DRMGR_PRIORITY_NAME_DRREG_HIGH, NULL, NULL,
                                       DRMGR_PRIORITY_INSERT_DRREG_HIGH };
    drmgr_priority_t low_priority = { sizeof(low_priority), DRMGR_PRIORITY_NAME_DRREG_LOW,
                                      NULL, NULL, DRMGR_PRIORITY_INSERT_DRREG_LOW };
    drmgr_priority_t fault_priority = { sizeof(fault_priority),
                                        DRMGR_PRIORITY_NAME_DRREG_FAULT, NULL, NULL,
                                        DRMGR_PRIORITY_FAULT_DRREG };

    int count = dr_atomic_add32_return_sum(&drreg_init_count, 1);
    if (count == 1) {
        drmgr_init();

        if (!drmgr_register_thread_init_event(drreg_thread_init) ||
            !drmgr_register_thread_exit_event(drreg_thread_exit))
            return DRREG_ERROR;
        tls_idx = drmgr_register_tls_field();
        if (tls_idx == -1)
            return DRREG_ERROR;

        if (!drmgr_register_bb_instrumentation_event(NULL, drreg_event_bb_insert_early,
                                                     &high_priority) ||
            !drmgr_register_bb_instrumentation_event(
                drreg_event_bb_analysis, drreg_event_bb_insert_late, &low_priority) ||
            !drmgr_register_restore_state_ex_event_ex(drreg_event_restore_state,
                                                      &fault_priority) ||
            !drmgr_register_bb_instru2instru_event(drreg_event_bb_instru2instru_late,
                                                   &low_priority))
            return DRREG_ERROR;
        dr_register_clean_call_insertion_event(drreg_event_clean_call_insertion);

#ifdef X86
        /* We get an extra slot for aflags xax, rather than just documenting that
         * clients should add 2 instead of just 1, as there are many existing clients.
         */
        ops.num_spill_slots = 1;
#endif
        /* Support use during init when there is no TLS (i#2910). */
        tls_data_init(&init_pt);
    }

    if (ops_in->struct_size < offsetof(drreg_options_t, error_callback))
        return DRREG_ERROR_INVALID_PARAMETER;

    /* Instead of allowing only one drreg_init() and all other components to be
     * passed in scratch regs, which is not always an easy-to-use
     * model, we instead consider all callers' requests, combining the option
     * fields.  We don't shift init to drreg_thread_init() or sthg b/c we really
     * want init-time error codes returning from drreg_init().
     */

    /* Sum the spill slots, honoring a new or prior do_not_sum_slots by taking
     * the max instead of summing.
     */
    if (ops_in->struct_size > offsetof(drreg_options_t, do_not_sum_slots)) {
        if (ops_in->do_not_sum_slots) {
            if (ops_in->num_spill_slots > ops.num_spill_slots)
                ops.num_spill_slots = ops_in->num_spill_slots;
        } else
            ops.num_spill_slots += ops_in->num_spill_slots;
        ops.do_not_sum_slots = ops_in->do_not_sum_slots;
    } else {
        if (ops.do_not_sum_slots) {
            if (ops_in->num_spill_slots > ops.num_spill_slots)
                ops.num_spill_slots = ops_in->num_spill_slots;
        } else
            ops.num_spill_slots += ops_in->num_spill_slots;
        ops.do_not_sum_slots = false;
    }

    /* If anyone wants to be conservative, then be conservative. */
    ops.conservative = ops.conservative || ops_in->conservative;

    /* The first callback wins. */
    if (ops_in->struct_size > offsetof(drreg_options_t, error_callback) &&
        ops.error_callback == NULL)
        ops.error_callback = ops_in->error_callback;

    if (prior_slots > 0) {
        if (!dr_raw_tls_cfree(tls_slot_offs, prior_slots))
            return DRREG_ERROR;
    }
    /* 0 spill slots is supported and just fills in tls_seg for us. */
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

    tls_data_free(&init_pt);

    if (!drmgr_unregister_thread_init_event(drreg_thread_init) ||
        !drmgr_unregister_thread_exit_event(drreg_thread_exit))
        return DRREG_ERROR;

    drmgr_unregister_tls_field(tls_idx);
    if (!drmgr_unregister_bb_insertion_event(drreg_event_bb_insert_early) ||
        !drmgr_unregister_bb_instrumentation_event(drreg_event_bb_analysis) ||
        !drmgr_unregister_restore_state_ex_event(drreg_event_restore_state))
        return DRREG_ERROR;

    drmgr_exit();

    if (ops.num_spill_slots > 0) {
        if (!dr_raw_tls_cfree(tls_slot_offs, ops.num_spill_slots))
            return DRREG_ERROR;
    }

    /* Support re-attach. */
    if (dr_is_detaching()) {
        memset(&ops, 0, sizeof(ops));
#ifdef DEBUG
        stats_max_slot = 0;
#endif
    }

    return DRREG_SUCCESS;
}
