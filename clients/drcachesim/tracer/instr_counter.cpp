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

#include <limits>
#include <stddef.h>
#include <atomic>
#include <cstdint>

// These libraries are safe to use during initialization only.
// See api/docs/deployment.dox sec_static_DR.
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "dr_api.h"
#include "dr_defines.h"
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

static client_id_t client_id;
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

static std::atomic<bool> reached_trace_after_instrs(false);
std::atomic<uint64> retrace_start_timestamp;

static std::atomic<uint> irregular_window_idx(0);
static drvector_t irregular_windows_list;
static uint num_irregular_windows = 0;

void
delete_instr_window_lists()
{
    if (num_irregular_windows == 0)
        return;
    if (!drvector_delete(&irregular_windows_list))
        FATAL("Fatal error: irregular_windows_list global vector was not deleted.");
}

void
maybe_increment_irregular_window_index()
{
    if (irregular_window_idx.load(std::memory_order_acquire) < num_irregular_windows)
        irregular_window_idx.fetch_add(1, std::memory_order_release);
}

struct irregular_window_t {
    uint64 no_trace_for_instrs = 0;
    uint64 trace_for_instrs = 0;
};

uint64
get_initial_no_trace_for_instrs_value()
{
    if (op_trace_after_instrs.get_value() > 0)
        return (uint64)op_trace_after_instrs.get_value();
    if (num_irregular_windows > 0) {
        void *irregular_window_ptr = drvector_get_entry(&irregular_windows_list, 0);
        if (irregular_window_ptr == nullptr)
            FATAL("Fatal error: irregular window not found at index 0.");
        return ((irregular_window_t *)irregular_window_ptr)->no_trace_for_instrs;
    }
    return 0;
}

uint64
get_current_trace_for_instrs_value()
{
    if (op_trace_for_instrs.get_value() > 0)
        return (uint64)op_trace_for_instrs.get_value();
    if (num_irregular_windows > 0) {
        uint i = irregular_window_idx.load(std::memory_order_acquire);
        void *irregular_window_ptr = drvector_get_entry(&irregular_windows_list, i);
        if (irregular_window_ptr == nullptr)
            FATAL("Fatal error: irregular window not found at index %d.", i);
        return ((irregular_window_t *)irregular_window_ptr)->trace_for_instrs;
    }
    return 0;
}

// This function returns the no_trace interval for all windows except the first one.
// The no_trace interval for the first window is returned by
// get_initial_no_trace_for_instrs_value().
uint64
get_current_no_trace_for_instrs_value()
{
    if (op_retrace_every_instrs.get_value() > 0)
        return (uint64)op_retrace_every_instrs.get_value();
    if (num_irregular_windows > 0) {
        uint i = irregular_window_idx.load(std::memory_order_acquire);
        void *irregular_window_ptr = drvector_get_entry(&irregular_windows_list, i);
        if (irregular_window_ptr == nullptr)
            FATAL("Fatal error: irregular window not found at index %d.", i);
        return ((irregular_window_t *)irregular_window_ptr)->no_trace_for_instrs;
    }
    return 0;
}

static bool
has_instr_count_threshold_to_enable_tracing()
{
    if (get_initial_no_trace_for_instrs_value() > 0 &&
        !reached_trace_after_instrs.load(std::memory_order_acquire))
        return true;
    if (get_current_no_trace_for_instrs_value() > 0)
        return true;
    return false;
}

static uint64
instr_count_threshold()
{
    uint64 initial_no_trace_for_instrs = get_initial_no_trace_for_instrs_value();
    if (initial_no_trace_for_instrs > 0 &&
        !reached_trace_after_instrs.load(std::memory_order_acquire))
        return initial_no_trace_for_instrs;
    uint64 current_no_trace_for_instrs = get_current_no_trace_for_instrs_value();
    if (current_no_trace_for_instrs > 0)
        return current_no_trace_for_instrs;
    return DELAY_FOREVER_THRESHOLD;
}

// Enables tracing if we've reached the delay point.
// For tracing windows going in the reverse direction and disabling tracing,
// see reached_traced_instrs_threshold(). On Linux, call this function only from a clean
// call. This is because it might invoke dr_redirect_execution() after a nudge to ensure
// a cache exit. Refer to dr_nudge_client() for more details.
// This function will not return when dr_redirect_execution() is called.
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
#ifdef LINUX
    bool redirect_execution = false;
#endif
    if (get_initial_no_trace_for_instrs_value() > 0 &&
        !reached_trace_after_instrs.load(std::memory_order_acquire)) {
        NOTIFY(0, "Hit delay threshold: enabling tracing.\n");
        if (op_memdump_on_window.get_value()) {
            dr_nudge_client(
                client_id,
                (static_cast<uint64>(TRACER_NUDGE_MEM_DUMP) << TRACER_NUDGE_TYPE_SHIFT) |
                    tracing_window.load(std::memory_order_acquire));
#ifdef LINUX
            redirect_execution = true;
#endif
        }
        retrace_start_timestamp.store(instru_t::get_timestamp());
    } else {
        NOTIFY(0, "Hit retrace threshold: enabling tracing for window #%zd.\n",
               tracing_window.load(std::memory_order_acquire));
        if (op_memdump_on_window.get_value()) {
            dr_nudge_client(
                client_id,
                (static_cast<uint64>(TRACER_NUDGE_MEM_DUMP) << TRACER_NUDGE_TYPE_SHIFT) |
                    tracing_window.load(std::memory_order_acquire));
#ifdef LINUX
            redirect_execution = true;
#endif
        }
        retrace_start_timestamp.store(instru_t::get_timestamp());
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
#ifdef LINUX
    // On Linux, the nudge is not delivered until this thread exits the code cache.
    // As this is a clean call, `dr_redirect_execution()` is used to force a cache exit
    // and ensure timely nudge delivery.
    if (redirect_execution) {
        void *drcontext = dr_get_current_drcontext();
        dr_mcontext_t mcontext;
        mcontext.size = sizeof(mcontext);
        mcontext.flags = DR_MC_ALL;
        dr_get_mcontext(drcontext, &mcontext);
        mcontext.pc = dr_app_pc_as_jump_target(dr_get_isa_mode(drcontext), next_pc);
        dr_redirect_execution(&mcontext);
        ASSERT(false, "dr_redirect_execution should not return");
    }
#endif
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

// Represents an interval as a <start,duration> pair in terms of number of instructions.
struct instr_interval_t {
    instr_interval_t(uint64 start, uint64 duration)
        : start(start)
        , duration(duration)
    {
    }

    uint64 start = 0;
    uint64 duration = 0;
};

// Function to order instruction intervals by start time in ascending order.
static bool
cmp_start_instr(const instr_interval_t &l, const instr_interval_t &r)
{
    return l.start < r.start;
}

static std::vector<instr_interval_t>
parse_instr_intervals_file(std::string path_to_file)
{
    std::vector<instr_interval_t> instr_intervals;
    std::ifstream file(path_to_file);
    if (!file.is_open())
        FATAL("Fatal error: failed to open file %s.\n", path_to_file.c_str());

    std::string line;
    while (std::getline(file, line)) {
        // Ignore empty lines, if any.
        if (line.empty())
            continue;
        std::stringstream ss(line);
        std::string elem;
        if (!std::getline(ss, elem, ','))
            FATAL("Fatal error: start instruction not found.\n");
        uint64 start = std::stoull(elem);
        if (!std::getline(ss, elem, ','))
            FATAL("Fatal error: instruction duration not found.\n");
        uint64 duration = std::stoull(elem);
        // Ignore the remaining comma-separated elements, if any.

        instr_intervals.emplace_back(start, duration);
    }
    file.close();

    if (instr_intervals.empty()) {
        FATAL("Fatal error: -trace_instr_intervals_file %s contains no intervals.\n",
              path_to_file.c_str());
    }

    // Enforcing constraints on intervals:
    // 1) They need to be ordered by start time.
    std::sort(instr_intervals.begin(), instr_intervals.end(), cmp_start_instr);

    // 2) Overlapping intervals must be merged.
    std::vector<instr_interval_t> instr_intervals_merged;
    instr_intervals_merged.emplace_back(instr_intervals[0]);
    for (instr_interval_t &interval : instr_intervals) {
        uint64 end = interval.start + interval.duration;
        instr_interval_t &last_interval = instr_intervals_merged.back();
        uint64 last_end = last_interval.start + last_interval.duration;
        if (interval.start <= last_end) {
            uint64 max_end = last_end > end ? last_end : end;
            last_interval.duration = max_end - last_interval.start;
        } else {
            instr_intervals_merged.emplace_back(interval);
        }
    }

    return instr_intervals_merged;
}

static void
free_trace_window_entry(void *entry)
{
    dr_global_free(entry, sizeof(irregular_window_t));
}

// Transforms instruction intervals from <start,duration> pairs to trace and no_trace
// number of instructions. Has the side effect of populating the read-only, global vector
// irregular_windows_list, and the global num_irregular_windows.
static void
compute_irregular_trace_windows(std::vector<instr_interval_t> &instr_intervals)
{
    if (instr_intervals.empty())
        return;

    uint num_intervals = (uint)instr_intervals.size();
    num_irregular_windows = num_intervals + 1;

    // We don't need to synch accesses because this global vector is initialized here and
    // then it's only read.
    drvector_init(&irregular_windows_list, num_irregular_windows, /* synch = */ false,
                  free_trace_window_entry);

    irregular_window_t *irregular_window_ptr =
        (irregular_window_t *)dr_global_alloc(sizeof(irregular_window_t));
    irregular_window_ptr->no_trace_for_instrs = instr_intervals[0].start;
    irregular_window_ptr->trace_for_instrs = instr_intervals[0].duration;
    drvector_set_entry(&irregular_windows_list, 0, irregular_window_ptr);

    for (uint i = 1; i < num_intervals; ++i) {
        uint64 no_trace_for_instrs = instr_intervals[i].start -
            (instr_intervals[i - 1].start + instr_intervals[i - 1].duration);
        uint64 trace_for_instrs = instr_intervals[i].duration;

        irregular_window_ptr =
            (irregular_window_t *)dr_global_alloc(sizeof(irregular_window_t));
        irregular_window_ptr->no_trace_for_instrs = no_trace_for_instrs;
        irregular_window_ptr->trace_for_instrs = trace_for_instrs;
        drvector_set_entry(&irregular_windows_list, i, irregular_window_ptr);
    }

    // Last window. We are done setting all the irregular windows of the csv file. We
    // generate one last non-tracing window in case the target program is still running.
    // If the user wants to finish with a tracing window, the last window in the csv file
    // must have a duration long enough to cover the end of the program.
    irregular_window_ptr =
        (irregular_window_t *)dr_global_alloc(sizeof(irregular_window_t));
    // DELAY_FOREVER_THRESHOLD might be too small for long traces, but it doesn't matter
    // because we trace_for_instrs = 0, so no window is created anyway.
    irregular_window_ptr->no_trace_for_instrs = DELAY_FOREVER_THRESHOLD;
    irregular_window_ptr->trace_for_instrs = 0;
    drvector_set_entry(&irregular_windows_list, num_intervals, irregular_window_ptr);
}

static void
init_irregular_trace_windows()
{
    std::string path_to_file = op_trace_instr_intervals_file.get_value();
    if (!path_to_file.empty()) {
        // Other instruction interval options (i.e., -trace_after_instrs,
        // -trace_for_instrs, -retrace_every_instrs) are not compatible with
        // -trace_instr_intervals_file. Check that they are not set.
        if (op_trace_after_instrs.get_value() > 0 ||
            op_trace_for_instrs.get_value() > 0 ||
            op_retrace_every_instrs.get_value() > 0) {
            FATAL("Fatal error: -trace_instr_intervals_file cannot be used with "
                  "-trace_after_instrs, -trace_for_instrs, or -retrace_every_instrs.\n");
        }
        // Parse intervals file.
        std::vector<instr_interval_t> instr_intervals =
            parse_instr_intervals_file(op_trace_instr_intervals_file.get_value());
        // Populate the irregular_windows_list global vector and num_irregular_windows.
        compute_irregular_trace_windows(instr_intervals);
    }
}

void
event_inscount_init(client_id_t id)
{
    client_id = id;
    init_irregular_trace_windows();
    DR_ASSERT(std::atomic_is_lock_free(&reached_trace_after_instrs));
}

} // namespace drmemtrace
} // namespace dynamorio
