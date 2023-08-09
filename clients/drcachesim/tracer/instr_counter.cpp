/* ******************************************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
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

/***************************************************************************
 * Instruction counting mode where we do not record any trace data.
 */

#include "instr_counter.h"

#include <limits.h>
#include <stddef.h>

#include <atomic>
#include <cstdint>

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "options.h"
#include "utils.h"
#include "drmemtrace.h"
#include "func_trace.h"
#include "instru.h"
#include "output.h"
#include "tracer.h"
#include "droption.h"
#include "drx.h"

namespace dynamorio {
namespace drmemtrace {

static uint64 instr_count;
/* For performance, we only increment the global instr_count exactly for
 * small thresholds.  If -trace_after_instrs is larger than this value, we
 * instead use thread-private counters and add to the global every
 * ~DELAY_COUNTDOWN_UNIT instructions.
 */
#define DELAY_EXACT_THRESHOLD (10 * 1024 * 1024)
// We use the same value we use for tracing windows.
#define DELAY_COUNTDOWN_UNIT INSTR_COUNT_LOCAL_UNIT
// For -trace_for_instrs without -retrace_every_instrs we count forever,
// but to avoid the complexity of different instrumentation we need a threshold.
#define DELAY_FOREVER_THRESHOLD (1024 * 1024 * 1024)

static std::atomic<bool> reached_trace_after_instrs;

static bool
has_instr_count_threshold_to_enable_tracing()
{
    if (op_trace_after_instrs.get_value() > 0 &&
        !reached_trace_after_instrs.load(std::memory_order_acquire))
        return true;
    if (op_retrace_every_instrs.get_value() > 0)
        return true;
    return false;
}

static uint64
instr_count_threshold()
{
    if (op_trace_after_instrs.get_value() > 0 &&
        !reached_trace_after_instrs.load(std::memory_order_acquire))
        return op_trace_after_instrs.get_value();
    if (op_retrace_every_instrs.get_value() > 0)
        return op_retrace_every_instrs.get_value();
    return DELAY_FOREVER_THRESHOLD;
}

// Enables tracing if we've reached the delay point.
// For tracing windows going in the reverse direction and disabling tracing,
// see reached_traced_instrs_threshold().
static void
hit_instr_count_threshold(app_pc next_pc)
{
    uintptr_t mode;
    if (!has_instr_count_threshold_to_enable_tracing())
        return;
#ifdef DELAYED_CHECK_INLINED
    /* XXX: We could do the same thread-local counters for non-inlined.
     * We'd then switch to std::atomic or something for 32-bit.
     */
    if (instr_count_threshold() > DELAY_EXACT_THRESHOLD) {
        void *drcontext = dr_get_current_drcontext();
        per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        int64 myval = *(int64 *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNTDOWN);
        uint64 newval = (uint64)dr_atomic_add64_return_sum((volatile int64 *)&instr_count,
                                                           DELAY_COUNTDOWN_UNIT - myval);
        *(uintptr_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNTDOWN) =
            DELAY_COUNTDOWN_UNIT;
        if (newval < instr_count_threshold())
            return;
    }
#endif
    dr_mutex_lock(mutex);
    if (is_in_tracing_mode(tracing_mode.load(std::memory_order_acquire))) {
        // Another thread already changed the mode.
        dr_mutex_unlock(mutex);
        return;
    }
    if (op_trace_after_instrs.get_value() > 0 &&
        !reached_trace_after_instrs.load(std::memory_order_acquire))
        NOTIFY(0, "Hit delay threshold: enabling tracing.\n");
    else {
        NOTIFY(0, "Hit retrace threshold: enabling tracing for window #%zd.\n",
               tracing_window.load(std::memory_order_acquire));
        if (op_offline.get_value())
            open_new_window_dir(tracing_window.load(std::memory_order_acquire));
    }
    if (!reached_trace_after_instrs.load(std::memory_order_acquire)) {
        reached_trace_after_instrs.store(true, std::memory_order_release);
    }
    // Reset for -retrace_every_instrs.
#ifdef X64
    dr_atomic_store64((volatile int64 *)&instr_count, 0);
#else
    // dr_atomic_store64 is not implemented for 32-bit, and it's technically not
    // portably safe to take the address of std::atomic, so we rely on our mutex.
    instr_count = 0;
#endif
    DR_ASSERT(tracing_mode.load(std::memory_order_acquire) == BBDUP_MODE_COUNT);

    if (op_L0_filter_until_instrs.get_value())
        mode = BBDUP_MODE_L0_FILTER;
    else
        mode = BBDUP_MODE_TRACE;
    tracing_mode.store(mode, std::memory_order_release);
    dr_mutex_unlock(mutex);
}

#ifndef DELAYED_CHECK_INLINED
static void
check_instr_count_threshold(uint incby, app_pc next_pc)
{
    if (!has_instr_count_threshold_to_enable_tracing())
        return;
    /* XXX i#5030: This is racy.  We could make std::atomic, or, better, go and
     * implement the inlining and i#5026's thread-private counting.
     */
    instr_count += incby;
    if (instr_count > instr_count_threshold())
        hit_instr_count_threshold(next_pc);
}
#endif

dr_emit_flags_t
event_inscount_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                           bool translating, void **user_data)
{
    instr_t *instr;
    uint num_instrs;
    for (instr = instrlist_first_app(bb), num_instrs = 0; instr != NULL;
         instr = instr_get_next_app(instr)) {
        num_instrs++;
    }
    *user_data = (void *)(ptr_uint_t)num_instrs;
    return DR_EMIT_DEFAULT;
}

dr_emit_flags_t
event_inscount_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                               instr_t *instr, instr_t *where, bool for_trace,
                               bool translating, void *orig_analysis_data,
                               void *user_data)
{
    uint num_instrs;
    dr_emit_flags_t flags = DR_EMIT_DEFAULT;

    // Give drwrap a chance to clean up, even when we're not actively wrapping.
    dr_emit_flags_t func_flags = func_trace_disabled_instrument_event(
        drcontext, tag, bb, instr, where, for_trace, translating, NULL);
    flags = static_cast<dr_emit_flags_t>(flags | func_flags);

    if (!is_first_nonlabel(drcontext, instr))
        return flags;

    num_instrs = (uint)(ptr_uint_t)user_data;
    drmgr_disable_auto_predication(drcontext, bb);
#ifdef DELAYED_CHECK_INLINED
#    if defined(X86_64) || defined(AARCH64)
    instr_t *skip_call = INSTR_CREATE_label(drcontext);
#        ifdef X86_64
    reg_id_t scratch = DR_REG_NULL;
    if (instr_count_threshold() > DELAY_EXACT_THRESHOLD) {
        /* Contention on a global counter causes high overheads.  We approximate the
         * count by using thread-local counters and only merging into the global
         * every so often.
         */
        if (drreg_reserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve aflags");
        MINSERT(
            bb, where,
            INSTR_CREATE_sub(
                drcontext,
                dr_raw_tls_opnd(drcontext, tls_seg,
                                tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNTDOWN),
                OPND_CREATE_INT32(num_instrs)));
        MINSERT(bb, where,
                INSTR_CREATE_jcc(drcontext, OP_jns, opnd_create_instr(skip_call)));
    } else {
        if (!drx_insert_counter_update(
                drcontext, bb, where, (dr_spill_slot_t)(SPILL_SLOT_MAX + 1) /*use drmgr*/,
                &instr_count, num_instrs, DRX_COUNTER_64BIT))
            DR_ASSERT(false);

        if (drreg_reserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve aflags");
        if (instr_count_threshold() < INT_MAX) {
            MINSERT(bb, where,
                    XINST_CREATE_cmp(drcontext, OPND_CREATE_ABSMEM(&instr_count, OPSZ_8),
                                     OPND_CREATE_INT32(instr_count_threshold())));
        } else {
            if (drreg_reserve_register(drcontext, bb, where, NULL, &scratch) !=
                DRREG_SUCCESS)
                FATAL("Fatal error: failed to reserve scratch register");
            instrlist_insert_mov_immed_ptrsz(drcontext, instr_count_threshold(),
                                             opnd_create_reg(scratch), bb, where, NULL,
                                             NULL);
            MINSERT(bb, where,
                    XINST_CREATE_cmp(drcontext, OPND_CREATE_ABSMEM(&instr_count, OPSZ_8),
                                     opnd_create_reg(scratch)));
        }
        MINSERT(bb, where,
                INSTR_CREATE_jcc(drcontext, OP_jl, opnd_create_instr(skip_call)));
    }
#        elif defined(AARCH64)
    reg_id_t scratch1, scratch2 = DR_REG_NULL;
    if (instr_count_threshold() > DELAY_EXACT_THRESHOLD) {
        /* See the x86_64 comment on using thread-local counters to avoid contention. */
        if (drreg_reserve_register(drcontext, bb, where, NULL, &scratch1) !=
            DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve scratch register");
        dr_insert_read_raw_tls(drcontext, bb, where, tls_seg,
                               tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNTDOWN,
                               scratch1);
        /* We're counting down for an aflags-free comparison. */
        MINSERT(bb, where,
                XINST_CREATE_sub(drcontext, opnd_create_reg(scratch1),
                                 OPND_CREATE_INT(num_instrs)));
        dr_insert_write_raw_tls(drcontext, bb, where, tls_seg,
                                tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNTDOWN,
                                scratch1);
        MINSERT(bb, where,
                INSTR_CREATE_tbz(drcontext, opnd_create_instr(skip_call),
                                 /* If the top bit is still zero, skip the call. */
                                 opnd_create_reg(scratch1), OPND_CREATE_INT(63)));
    } else {
        /* We're counting down for an aflags-free comparison. */
        if (!drx_insert_counter_update(
                drcontext, bb, where, (dr_spill_slot_t)(SPILL_SLOT_MAX + 1) /*use drmgr*/,
                (dr_spill_slot_t)(SPILL_SLOT_MAX + 1), &instr_count, -num_instrs,
                DRX_COUNTER_64BIT | DRX_COUNTER_REL_ACQ))
            DR_ASSERT(false);

        if (drreg_reserve_register(drcontext, bb, where, NULL, &scratch1) !=
            DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve scratch register");
        if (drreg_reserve_register(drcontext, bb, where, NULL, &scratch2) !=
            DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve scratch register");

        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)&instr_count,
                                         opnd_create_reg(scratch1), bb, where, NULL,
                                         NULL);
        MINSERT(bb, where,
                XINST_CREATE_load(drcontext, opnd_create_reg(scratch2),
                                  OPND_CREATE_MEMPTR(scratch1, 0)));
        MINSERT(bb, where,
                INSTR_CREATE_tbz(drcontext, opnd_create_instr(skip_call),
                                 /* If the top bit is still zero, skip the call. */
                                 opnd_create_reg(scratch2), OPND_CREATE_INT(63)));
    }
#        endif

    dr_insert_clean_call_ex(drcontext, bb, where, (void *)hit_instr_count_threshold,
                            static_cast<dr_cleancall_save_t>(
                                DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_MULTIPATH),
                            1, OPND_CREATE_INTPTR((ptr_uint_t)instr_get_app_pc(instr)));
    MINSERT(bb, where, skip_call);

#        ifdef X86_64
    if (drreg_unreserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
        DR_ASSERT(false);
    if (scratch != DR_REG_NULL) {
        if (drreg_unreserve_register(drcontext, bb, where, scratch) != DRREG_SUCCESS)
            DR_ASSERT(false);
    }
#        elif defined(AARCH64)
    if (drreg_unreserve_register(drcontext, bb, where, scratch1) != DRREG_SUCCESS ||
        (scratch2 != DR_REG_NULL &&
         drreg_unreserve_register(drcontext, bb, where, scratch2) != DRREG_SUCCESS))
        DR_ASSERT(false);
#        endif
#    else
#        error NYI
#    endif
#else
    /* XXX: drx_insert_counter_update doesn't support 64-bit counters for ARM_32, and
     * inlining of check_instr_count_threshold is not implemented for i386. For now we pay
     * the cost of a clean call every time for 32-bit architectures.
     */
    dr_insert_clean_call_ex(drcontext, bb, where, (void *)check_instr_count_threshold,
                            DR_CLEANCALL_READS_APP_CONTEXT, 2,
                            OPND_CREATE_INT32(num_instrs),
                            OPND_CREATE_INTPTR((ptr_uint_t)instr_get_app_pc(instr)));
#endif
    return flags;
}

void
event_inscount_thread_init(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    *(uintptr_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNTDOWN) =
        DELAY_COUNTDOWN_UNIT;
}

void
event_inscount_init()
{
    DR_ASSERT(std::atomic_is_lock_free(&reached_trace_after_instrs));
}

} // namespace drmemtrace
} // namespace dynamorio
