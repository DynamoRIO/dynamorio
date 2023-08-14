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

/* tracer.cpp: tracing client for feeding data to cache simulator.
 *
 * Originally built from the memtrace_opt.c sample.
 * XXX i#1703, i#2001: add in more optimizations to improve performance.
 */

#include "tracer.h"

#include <string.h>
#include <sys/types.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <new>
#include <string>

#include "dr_api.h"
#include "drbbdup.h"
#include "drmemtrace.h"
#include "drmgr.h"
#include "droption.h"
#include "drreg.h"
#include "drstatecmp.h"
#include "drutil.h"
#include "drvector.h"
#include "drwrap.h"
#include "drx.h"
#include "func_trace.h"
#include "instr_counter.h"
#include "instru.h"
#include "named_pipe.h"
#include "options.h"
#include "output.h"
#include "physaddr.h"
#include "raw2trace.h"
#include "reader.h"
#include "trace_entry.h"
#include "utils.h"

#ifdef ARM
#    include "../../../core/unix/include/syscall_linux_arm.h" // for SYS_cacheflush
#elif defined(LINUX)
#    include <syscall.h>
#endif

#ifdef BUILD_PT_TRACER
#    include "drpttracer.h"
#    include "syscall_pt_trace.h"
#    include "kcore_copy.h"
#endif

/* Make sure we export function name as the symbol name without mangling. */
#ifdef __cplusplus
extern "C" {
DR_EXPORT void
drmemtrace_client_main(client_id_t id, int argc, const char *argv[]);
}
#endif

/* Request debug-build checks on use of malloc mid-run which will break statically
 * linking this client into an app.
 */
DR_DISALLOW_UNSAFE_STATIC

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;

char logsubdir[MAXIMUM_PATH];
#ifdef BUILD_PT_TRACER
char kernel_pt_logsubdir[MAXIMUM_PATH];
#endif
char subdir_prefix[MAXIMUM_PATH]; /* Holds op_subdir_prefix. */

static file_t module_file;
static file_t funclist_file = INVALID_FILE;
static file_t encoding_file = INVALID_FILE;

/* Max number of entries a buffer can have. It should be big enough
 * to hold all entries between clean calls.
 */
// XXX i#1703: use an option instead.
#define MAX_NUM_ENTRIES 4096
/* The buffer size for holding trace entries. */
size_t trace_buf_size;
/* The redzone is allocated right after the trace buffer.
 * We fill the redzone with sentinel value to detect when the redzone
 * is reached, i.e., when the trace buffer is full.
 */
size_t redzone_size;
size_t max_buf_size;

std::atomic<uint64> attached_timestamp;

static drvector_t scratch_reserve_vec;

/* per bb user data during instrumentation */
typedef struct {
    app_pc last_app_pc;
    instr_t *strex;
    int num_delay_instrs;
    instr_t *delay_instrs[MAX_NUM_DELAY_INSTRS];
    bool repstr;
    bool scatter_gather;
    void *instru_field;        /* For use by instru_t. */
    bool recorded_instr;       /* For offline single-PC-per-block. */
    int bb_instr_count;        /* For filtered traces. */
    bool recorded_instr_count; /* For filtered traces. */
} user_data_t;

/* For online simulation, we write to a single global pipe */
named_pipe_t ipc_pipe;

#define MAX_INSTRU_SIZE 256 /* The max instance size of instru_t or its children. */
instru_t *instru;

static client_id_t client_id;
void *mutex;                 /* for multithread support */
uint64 num_refs_racy;        /* racy global memory reference count */
uint64 num_filter_refs_racy; /* racy global memory reference count in warmup mode */
static uint64 num_refs;      /* keep a global memory reference count */
static uint64 num_writeouts;
static uint64 num_v2p_writeouts;
static uint64 num_phys_markers;

static drmgr_priority_t pri_pre_bbdup = { sizeof(drmgr_priority_t),
                                          DRMGR_PRIORITY_NAME_MEMTRACE, NULL, NULL,
                                          DRMGR_PRIORITY_APP2APP_DRBBDUP - 1 };

reg_id_t tls_seg;
uint tls_offs;
int tls_idx;
/* We leave slot(s) at the start so we can easily insert a header entry */
size_t buf_hdr_slots_size;

static bool (*should_trace_thread_cb)(thread_id_t tid, void *user_data);
static void *trace_thread_cb_user_data;
static bool thread_filtering_enabled;
bool attached_midway;

#ifdef AARCH64
static bool reported_sg_warning = false;
#endif

static bool
bbdup_instr_counting_enabled()
{
    // XXX: with no other options -trace_for_instrs switches to counting mode once tracing
    // is done, so return true. Now that we have a NOP mode this could be changed.
    return op_trace_after_instrs.get_value() > 0 || op_trace_for_instrs.get_value() > 0 ||
        op_retrace_every_instrs.get_value() > 0;
}

static bool
bbdup_duplication_enabled()
{
    return attached_midway || bbdup_instr_counting_enabled();
}

// If we have both BBDUP_MODE_TRACE and BBDUP_MODE_L0_FILTER, then L0 filter is active
// only when mode is BBDUP_MODE_L0_FILTER
void
get_L0_filters_enabled(uintptr_t mode, OUT bool *l0i_enabled, OUT bool *l0d_enabled)
{
    if (op_L0_filter_until_instrs.get_value()) {
        if (mode != BBDUP_MODE_L0_FILTER) {
            *l0i_enabled = false;
            *l0d_enabled = false;
            return;
        }
    }

    *l0i_enabled = op_L0I_filter.get_value();
    *l0d_enabled = op_L0D_filter.get_value();
    return;
}

std::atomic<ptr_int_t> tracing_window;

struct file_ops_func_t file_ops_func;

static char modlist_path[MAXIMUM_PATH];
static char funclist_path[MAXIMUM_PATH];
static char encoding_path[MAXIMUM_PATH];

/* clean_call sends the memory reference info to the simulator */
static void
clean_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    process_and_output_buffer(drcontext, false);
}

void
instru_notify(uint level, const char *fmt, ...)
{
    if (op_verbose.get_value() < level)
        return;
    va_list args;
    va_start(args, fmt);
    dr_vfprintf(STDERR, fmt, args);
    va_end(args);
}

/***************************************************************************
 * Alternating tracing-no-tracing feature.
 */

/* This holds one of the BBDUP_MODE_ enum values, but drbbdup requires that it
 * be pointer-sized.
 */
std::atomic<ptr_int_t> tracing_mode;

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void **user_data, uintptr_t mode);

static dr_emit_flags_t
event_bb_analysis_cleanup(void *drcontext, void *user_data);

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      instr_t *where, bool for_trace, bool translating, uintptr_t mode,
                      void *orig_analysis_data, void *user_data);

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating);

static bool
event_filter_syscall(void *drcontext, int sysnum);

static bool
event_pre_syscall(void *drcontext, int sysnum);

static void
event_post_syscall(void *drcontext, int sysnum);

static void
event_kernel_xfer(void *drcontext, const dr_kernel_xfer_info_t *info);

static uintptr_t
event_bb_setup(void *drbbdup_ctx, void *drcontext, void *tag, instrlist_t *bb,
               bool *enable_dups, bool *enable_dynamic_handling, void *user_data)
{
    DR_ASSERT(enable_dups != NULL && enable_dynamic_handling != NULL);
    if (bbdup_duplication_enabled()) {
        *enable_dups = true;
        // Make sure to update opts.non_default_case_limit if adding an encoding here.
        drbbdup_status_t res;
        if (align_attach_detach_endpoints()) {
            res = drbbdup_register_case_encoding(drbbdup_ctx, BBDUP_MODE_NOP);
            DR_ASSERT(res == DRBBDUP_SUCCESS);
        }
        if (bbdup_instr_counting_enabled()) {
            res = drbbdup_register_case_encoding(drbbdup_ctx, BBDUP_MODE_COUNT);
            DR_ASSERT(res == DRBBDUP_SUCCESS);
        }
        if (op_L0_filter_until_instrs.get_value()) {
            res = drbbdup_register_case_encoding(drbbdup_ctx, BBDUP_MODE_L0_FILTER);
            DR_ASSERT(res == DRBBDUP_SUCCESS);
        }
        // XXX i#2039: We have possible future use cases for BBDUP_MODE_FUNC_ONLY
        // to track functions during no-tracing periods, possibly replacing the
        // NOP mode for some of those.  For now it is not enabled.
    } else {
        /* Tracing is always on, so we have just one type of instrumentation and
         * do not need block duplication.
         */
        *enable_dups = false;
    }
    *enable_dynamic_handling = false;
    return BBDUP_MODE_TRACE;
}

static void
event_bb_retrieve_mode(void *drcontext, void *tag, instrlist_t *bb, instr_t *where,
                       void *user_data, void *orig_analysis_data)
{
    /* Nothing to do.  We would pass nullptr for this but drbbdup makes it required. */
}

bool
is_first_nonlabel(void *drcontext, instr_t *instr)
{
    bool is_first_nonlabel = false;
    if (drbbdup_is_first_nonlabel_instr(drcontext, instr, &is_first_nonlabel) !=
        DRBBDUP_SUCCESS)
        DR_ASSERT(false);
    return is_first_nonlabel;
}

static dr_emit_flags_t
event_bb_analyze_case(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                      bool translating, uintptr_t mode, void *user_data,
                      void *orig_analysis_data, void **analysis_data)
{
    if (is_in_tracing_mode(mode)) {
        return event_bb_analysis(drcontext, tag, bb, for_trace, translating,
                                 analysis_data, mode);
    } else if (mode == BBDUP_MODE_COUNT) {
        return event_inscount_bb_analysis(drcontext, tag, bb, for_trace, translating,
                                          analysis_data);
    } else if (mode == BBDUP_MODE_FUNC_ONLY) {
        return DR_EMIT_DEFAULT;
    } else if (mode == BBDUP_MODE_NOP) {
        return DR_EMIT_DEFAULT;
    } else
        DR_ASSERT(false);
    return DR_EMIT_DEFAULT;
}

static void
event_bb_analyze_case_cleanup(void *drcontext, uintptr_t mode, void *user_data,
                              void *orig_analysis_data, void *analysis_data)
{
    if (is_in_tracing_mode(mode))
        event_bb_analysis_cleanup(drcontext, analysis_data);
    else if (mode == BBDUP_MODE_COUNT)
        ; /* no cleanup needed */
    else if (mode == BBDUP_MODE_FUNC_ONLY)
        ; /* no cleanup needed */
    else if (mode == BBDUP_MODE_NOP)
        ; /* no cleanup needed */
    else
        DR_ASSERT(false);
}

static dr_emit_flags_t
event_app_instruction_case(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                           instr_t *where, bool for_trace, bool translating,
                           uintptr_t mode, void *user_data, void *orig_analysis_data,
                           void *analysis_data)
{
    if (is_in_tracing_mode(mode)) {
        return event_app_instruction(drcontext, tag, bb, instr, where, for_trace,
                                     translating, mode, orig_analysis_data,
                                     analysis_data);
    } else if (mode == BBDUP_MODE_COUNT) {
        // This includes func_trace_disabled_instrument_event() for drwrap cleanup.
        return event_inscount_app_instruction(drcontext, tag, bb, instr, where, for_trace,
                                              translating, orig_analysis_data,
                                              analysis_data);
    } else if (mode == BBDUP_MODE_FUNC_ONLY) {
        return func_trace_enabled_instrument_event(drcontext, tag, bb, instr, where,
                                                   for_trace, translating, NULL);
    } else if (mode == BBDUP_MODE_NOP) {
        // We still need drwrap to clean up b/c we're using intrusive optimizations.
        return func_trace_disabled_instrument_event(drcontext, tag, bb, instr, where,
                                                    for_trace, translating, NULL);
    } else
        DR_ASSERT(false);
    return DR_EMIT_DEFAULT;
}

static void
instrumentation_exit()
{
    dr_unregister_filter_syscall_event(event_filter_syscall);
    if (!drmgr_unregister_pre_syscall_event(event_pre_syscall) ||
        !drmgr_unregister_kernel_xfer_event(event_kernel_xfer) ||
        !drmgr_unregister_bb_app2app_event(event_bb_app2app))
        DR_ASSERT(false);
#ifdef DELAYED_CHECK_INLINED
    drx_exit();
#endif
    drbbdup_status_t res = drbbdup_exit();
    DR_ASSERT(res == DRBBDUP_SUCCESS);
}

static void
instrumentation_drbbdup_init()
{
    drbbdup_options_t opts = {
        sizeof(opts),
    };
    opts.set_up_bb_dups = event_bb_setup;
    opts.insert_encode = event_bb_retrieve_mode;
    opts.analyze_case_ex = event_bb_analyze_case;
    opts.destroy_case_analysis = event_bb_analyze_case_cleanup;
    opts.instrument_instr_ex = event_app_instruction_case;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&tracing_mode, OPSZ_PTR);
    opts.atomic_load_encoding = true;
    // Save memory by asking drbbdup to not keep per-block bookkeeping
    // unless we need it (the cases below).
    opts.non_default_case_limit = 0;
    if (align_attach_detach_endpoints())
        ++opts.non_default_case_limit; // BBDUP_MODE_NOP.
    if (bbdup_instr_counting_enabled())
        ++opts.non_default_case_limit; // BBDUP_MODE_COUNT.
    if (op_L0_filter_until_instrs.get_value())
        ++opts.non_default_case_limit; // BBDUP_MODE_L0_FILTER.
    // Save per-thread heap for a feature we do not need.
    opts.never_enable_dynamic_handling = true;
    drbbdup_status_t res = drbbdup_init(&opts);
    DR_ASSERT(res == DRBBDUP_SUCCESS);
    /* We just want barriers and atomic ops: no locks b/c they are not safe. */
    DR_ASSERT(tracing_mode.is_lock_free());
}

static void
instrumentation_init()
{
    instrumentation_drbbdup_init();
    if (!drmgr_register_pre_syscall_event(event_pre_syscall) ||
        !drmgr_register_post_syscall_event(event_post_syscall) ||
        !drmgr_register_kernel_xfer_event(event_kernel_xfer) ||
        !drmgr_register_bb_app2app_event(event_bb_app2app, &pri_pre_bbdup))
        DR_ASSERT(false);
    dr_register_filter_syscall_event(event_filter_syscall);

    if (align_attach_detach_endpoints())
        tracing_mode.store(BBDUP_MODE_NOP, std::memory_order_release);
    else if (op_trace_after_instrs.get_value() != 0)
        tracing_mode.store(BBDUP_MODE_COUNT, std::memory_order_release);
    else if (op_L0_filter_until_instrs.get_value())
        tracing_mode.store(BBDUP_MODE_L0_FILTER, std::memory_order_release);

#ifdef DELAYED_CHECK_INLINED
    drx_init();
#endif
}

static void
event_post_attach()
{
    DR_ASSERT(attached_midway);
    if (!align_attach_detach_endpoints())
        return;
    uint64 timestamp = instru_t::get_timestamp();
    attached_timestamp.store(timestamp, std::memory_order_release);
    NOTIFY(1, "Fully-attached timestamp is " UINT64_FORMAT_STRING "\n", timestamp);
    if (op_trace_after_instrs.get_value() != 0) {
        NOTIFY(1, "Switching to counting mode after attach\n");
        tracing_mode.store(BBDUP_MODE_COUNT, std::memory_order_release);
    } else if (op_L0_filter_until_instrs.get_value()) {
        NOTIFY(1, "Switching to filter mode after attach\n");
        tracing_mode.store(BBDUP_MODE_L0_FILTER, std::memory_order_release);
    } else {
        NOTIFY(1, "Switching to tracing mode after attach\n");
        tracing_mode.store(BBDUP_MODE_TRACE, std::memory_order_release);
    }
}

static void
event_pre_detach()
{
    if (align_attach_detach_endpoints()) {
        NOTIFY(1, "Switching to no-tracing mode during detach\n");
        // Keep all final thread output at the detach timestamp.
        // With timestamps added at buffer start instead of output we generally do not
        // add new timestamps during detach, but for a window being closed and the
        // thread exit in the new window or similar cases it can happen, so we avoid any
        // possible post-detach timestamp by freezing.
        instru->set_frozen_timestamp(instru_t::get_timestamp());
        tracing_mode.store(BBDUP_MODE_NOP, std::memory_order_release);
    }
}

/***************************************************************************
 * Tracing instrumentation.
 */

static void
append_marker_seg_base(void *drcontext, func_trace_entry_vector_t *vec)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (BUF_PTR(data->seg_base) == NULL)
        return; /* This thread was filtered out. */
    for (int i = 0; i < vec->size; i++) {
        BUF_PTR(data->seg_base) +=
            instru->append_marker(BUF_PTR(data->seg_base), vec->entries[i].marker_type,
                                  vec->entries[i].marker_value);
    }
    /* In a filtered data-only trace, a block with no memrefs today still has
     * a redzone check at the end guarding a clean call to memtrace(), but to
     * be a litte safer in case that changes we also do a redzone check here.
     */
    if (BUF_PTR(data->seg_base) - data->buf_base > static_cast<ssize_t>(trace_buf_size))
        process_and_output_buffer(drcontext, false);
}

static void
insert_load_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr)
{
    dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                           tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_BUF_PTR,
                           reg_ptr);
}

static void
insert_update_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, dr_pred_type_t pred, int adjust, uintptr_t mode)
{
    if (adjust == 0)
        return;
    bool is_L0I_enabled, is_L0D_enabled;
    get_L0_filters_enabled(mode, &is_L0I_enabled, &is_L0D_enabled);
    if (!(is_L0I_enabled || is_L0D_enabled)) // Filter skips over this for !pred.
        instrlist_set_auto_predicate(ilist, pred);
    MINSERT(
        ilist, where,
        XINST_CREATE_add(drcontext, opnd_create_reg(reg_ptr), OPND_CREATE_INT16(adjust)));
    dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                            tls_offs + MEMTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
}

static int
instrument_delay_instrs(void *drcontext, void *tag, instrlist_t *ilist, user_data_t *ud,
                        instr_t *where, reg_id_t reg_ptr, int adjust, bool is_L0I_enabled,
                        uintptr_t mode)
{
    // Instrument to add a full instr entry for the first instr.
    if (op_instr_encodings.get_value()) {
        adjust = instru->instrument_instr_encoding(drcontext, tag, ud->instru_field,
                                                   ilist, where, reg_ptr, adjust,
                                                   ud->delay_instrs[0]);
    }
    adjust =
        instru->instrument_instr(drcontext, tag, ud->instru_field, ilist, where, reg_ptr,
                                 adjust, ud->delay_instrs[0], is_L0I_enabled, mode);
    if (op_use_physical.get_value() || op_instr_encodings.get_value()) {
        // No instr bundle if physical-2-virtual since instr bundle may
        // cross page bundary, and no bundles for encodings so we can easily
        // insert encoding entries.
        int i;
        for (i = 1; i < ud->num_delay_instrs; i++) {
            if (op_instr_encodings.get_value()) {
                adjust = instru->instrument_instr_encoding(
                    drcontext, tag, ud->instru_field, ilist, where, reg_ptr, adjust,
                    ud->delay_instrs[i]);
            }
            adjust = instru->instrument_instr(drcontext, tag, ud->instru_field, ilist,
                                              where, reg_ptr, adjust, ud->delay_instrs[i],
                                              is_L0I_enabled, mode);
        }
    } else {
        adjust =
            instru->instrument_ibundle(drcontext, ilist, where, reg_ptr, adjust,
                                       ud->delay_instrs + 1, ud->num_delay_instrs - 1);
    }
    ud->num_delay_instrs = 0;
    return adjust;
}

/* Inserts a conditional branch that jumps to skip_label if reg_skip_if_zero's
 * value is zero.
 * "*reg_tmp" must start out as DR_REG_NULL. It will hold a temp reg that must be passed
 * to any subsequent call here as well as to insert_conditional_skip_target() at
 * the point where skip_label should be inserted.  Additionally, the
 * app_regs_at_skip set must be empty prior to calling and it must be passed
 * to insert_conditional_skip_target().
 * reg_skip_if_zero must be DR_REG_XCX on x86.
 */
static void
insert_conditional_skip(void *drcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t reg_skip_if_zero, reg_id_t *reg_tmp INOUT,
                        instr_t *skip_label, bool short_reaches,
                        reg_id_set_t &app_regs_at_skip)
{
    // Record the registers that will need barriers at the skip target.
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; ++reg) {
        drreg_reserve_info_t info = { sizeof(info) };
        drreg_status_t res = drreg_reservation_info_ex(drcontext, reg, &info);
        DR_ASSERT(res == DRREG_SUCCESS);
        if (info.holds_app_value) {
            app_regs_at_skip.insert(reg);
        }
    }

#ifdef X86
    DR_ASSERT(reg_skip_if_zero == DR_REG_XCX);
    if (short_reaches) {
        MINSERT(ilist, where,
                INSTR_CREATE_jecxz(drcontext, opnd_create_instr(skip_label)));
    } else {
        instr_t *should_skip = INSTR_CREATE_label(drcontext);
        instr_t *no_skip = INSTR_CREATE_label(drcontext);
        MINSERT(ilist, where,
                INSTR_CREATE_jecxz(drcontext, opnd_create_instr(should_skip)));
        MINSERT(ilist, where,
                INSTR_CREATE_jmp_short(drcontext, opnd_create_instr(no_skip)));
        /* XXX i#2825: we need this to not match instr_is_cti_short_rewrite() */
        MINSERT(ilist, where, INSTR_CREATE_nop(drcontext));
        MINSERT(ilist, where, should_skip);
        MINSERT(ilist, where, INSTR_CREATE_jmp(drcontext, opnd_create_instr(skip_label)));
        MINSERT(ilist, where, no_skip);
    }
#elif defined(ARM)
    if (dr_get_isa_mode(drcontext) == DR_ISA_ARM_THUMB) {
        instr_t *noskip = INSTR_CREATE_label(drcontext);
        /* XXX: clean call is too long to use cbz to skip. */
        DR_ASSERT(reg_skip_if_zero <= DR_REG_R7); /* cbnz can't take r8+ */
        MINSERT(ilist, where,
                INSTR_CREATE_cbnz(drcontext, opnd_create_instr(noskip),
                                  opnd_create_reg(reg_skip_if_zero)));
        MINSERT(ilist, where,
                XINST_CREATE_jump(drcontext, opnd_create_instr(skip_label)));
        MINSERT(ilist, where, noskip);
    } else {
        /* There is no jecxz/cbz like instr on ARM-A32 mode, so we have to
         * save aflags to a temp reg before the cmp.
         * XXX optimization: use drreg to avoid aflags save/restore.
         */
        if (*reg_tmp != DR_REG_NULL) {
            /* A prior call has already saved the flags. */
        } else {
            if (drreg_reserve_register(drcontext, ilist, where, &scratch_reserve_vec,
                                       reg_tmp) != DRREG_SUCCESS)
                FATAL("Fatal error: failed to reserve reg.");
            dr_save_arith_flags_to_reg(drcontext, ilist, where, *reg_tmp);
        }
        MINSERT(ilist, where,
                INSTR_CREATE_cmp(drcontext, opnd_create_reg(reg_skip_if_zero),
                                 OPND_CREATE_INT(0)));
        MINSERT(
            ilist, where,
            instr_set_predicate(
                XINST_CREATE_jump(drcontext, opnd_create_instr(skip_label)), DR_PRED_EQ));
    }
#elif defined(AARCH64)
    MINSERT(ilist, where,
            INSTR_CREATE_cbz(drcontext, opnd_create_instr(skip_label),
                             opnd_create_reg(reg_skip_if_zero)));
#endif
}

/* Should be called at the point where skip_label should be inserted.
 * reg_tmp must be the "*reg_tmp" output value from insert_conditional_skip().
 * Inserts a barrier for all app-valued registers at the jump point
 * (stored in app_regs_at_skip), to help avoid problems with different
 * paths having different lazy reg restoring from drreg.
 */
static void
insert_conditional_skip_target(void *drcontext, instrlist_t *ilist, instr_t *where,
                               instr_t *skip_label, reg_id_t reg_tmp,
                               reg_id_set_t &app_regs_at_skip)
{
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; ++reg) {
        if (app_regs_at_skip.find(reg) != app_regs_at_skip.end() &&
            /* We spilled reg_tmp in insert_conditional_skip() *before* the jump,
             * so we do *not* want to restore it before the target.
             */
            reg != reg_tmp &&
            /* We're not allowed to restore the stolen register this way, but drreg
             * won't hand it out as a scratch register in any case, so we don't need
             * a barrier for it.
             */
            reg != dr_get_stolen_reg()) {
            drreg_status_t res = drreg_get_app_value(drcontext, ilist, where, reg, reg);
            if (res != DRREG_ERROR_NO_APP_VALUE && res != DRREG_SUCCESS)
                FATAL("Fatal error: failed to restore reg.");
        }
    }
    MINSERT(ilist, where, skip_label);
#ifdef ARM
    if (reg_tmp != DR_REG_NULL) {
        dr_restore_arith_flags_from_reg(drcontext, ilist, where, reg_tmp);
        if (drreg_unreserve_register(drcontext, ilist, where, reg_tmp) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to unreserve reg.\n");
    }
#endif
}

static void
insert_mode_comparison(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_ptr, void *addr, uint slot)
{
    reg_id_t reg_mine = DR_REG_NULL, reg_global = DR_REG_NULL;
    if (drreg_reserve_register(drcontext, ilist, where, NULL, &reg_mine) !=
            DRREG_SUCCESS ||
        drreg_reserve_register(drcontext, ilist, where, NULL, &reg_global) !=
            DRREG_SUCCESS)
        FATAL("Fatal error: failed to reserve reg.");
#ifdef AARCHXX
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)addr,
                                     opnd_create_reg(reg_global), ilist, where, NULL,
                                     NULL);
#    ifdef AARCH64
    MINSERT(ilist, where,
            INSTR_CREATE_ldar(drcontext, opnd_create_reg(reg_global),
                              OPND_CREATE_MEMPTR(reg_global, 0)));
#    else
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext, opnd_create_reg(reg_global),
                              OPND_CREATE_MEMPTR(reg_global, 0)));
    MINSERT(ilist, where, INSTR_CREATE_dmb(drcontext, OPND_CREATE_INT(DR_DMB_ISH)));
#    endif
#else
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext, opnd_create_reg(reg_global),
                              OPND_CREATE_ABSMEM(addr, OPSZ_PTR)));
#endif
    dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                           tls_offs + sizeof(void *) * slot, reg_mine);
#ifdef AARCHXX
    MINSERT(ilist, where,
            XINST_CREATE_sub(drcontext, opnd_create_reg(reg_mine),
                             opnd_create_reg(reg_global)));
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    DR_ASSERT_MSG(false, "Not implemented on RISC-V");
#else
    // Our version of a flags-free reg-reg subtraction: 1's complement one reg
    // plus 1 and then add using base+index of LEA.
    MINSERT(ilist, where, INSTR_CREATE_not(drcontext, opnd_create_reg(reg_global)));
    MINSERT(ilist, where,
            INSTR_CREATE_lea(drcontext, opnd_create_reg(reg_global),
                             OPND_CREATE_MEM_lea(reg_global, DR_REG_NULL, 0, 1)));
    MINSERT(ilist, where,
            INSTR_CREATE_lea(drcontext, opnd_create_reg(reg_mine),
                             OPND_CREATE_MEM_lea(reg_mine, reg_global, 1, 0)));
#endif
    // To avoid writing a 0 on top of the redzone, we read the buffer value and add
    // that to the local ("mine") window minus the global window.  The redzone is
    // -1, so if we do mine minus global which will always be non-positive, we'll
    // never write 0 for a redzone slot (and thus possibly overflowing).
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext, opnd_create_reg(reg_global),
                              OPND_CREATE_MEMPTR(reg_ptr, 0)));
    MINSERT(ilist, where,
            XINST_CREATE_add(drcontext, opnd_create_reg(reg_mine),
                             opnd_create_reg(reg_global)));
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, 0),
                               opnd_create_reg(reg_mine)));
    if (drreg_unreserve_register(drcontext, ilist, where, reg_global) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, ilist, where, reg_mine) != DRREG_SUCCESS)
        FATAL("Fatal error: failed to unreserve scratch reg.\n");
}

/* We insert code to read from trace buffer and check whether the redzone
 * is reached. If redzone is reached, the clean call will be called.
 * Additionally, for tracing windows, we also check for a mode switch and
 * invoke the clean call if our tracing window is over.
 */
static void
instrument_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, uintptr_t mode)
{
    instr_t *skip_call = INSTR_CREATE_label(drcontext);
    bool short_reaches = true;
#ifdef X86
    DR_ASSERT(reg_ptr == DR_REG_XCX);
    /* i#2049: we use DR_CLEANCALL_ALWAYS_OUT_OF_LINE to ensure our jecxz
     * reaches across the clean call (o/w we need 2 jmps to invert the jecxz).
     * Long-term we should try a fault instead (xref drx_buf) or a lean
     * proc to clean call gencode.
     */
    /* i#2147: -prof_pcs adds extra cleancall code that makes jecxz not reach.
     * XXX: it would be nice to have a more robust solution than this explicit check!
     */
    if (dr_is_tracking_where_am_i())
        short_reaches = false;
#elif defined(ARM)
    /* XXX: clean call is too long to use cbz to skip. */
    short_reaches = false;
#endif

    if (has_tracing_windows()) {
        // We need to do the clean call if the mode has changed back to counting.  To
        // detect a double-change we compare the TLS-stored last window to the
        // current tracing_window.  To avoid flags and avoid another branch (jumping
        // over the filter and redzone checks, e.g.), our strategy is to arrange for
        // the redzone load to trigger the call for us if the two window values are
        // not equal by writing their difference onto the next buffer slot.  This
        // requires a store and two scratch regs so perhaps this should be measured
        // against a branch-based scheme, but we assume we're i/o bound and so this will
        // not affect overhead.
        insert_mode_comparison(drcontext, ilist, where, reg_ptr, &tracing_window,
                               MEMTRACE_TLS_OFFS_WINDOW);
    }

    if (op_L0_filter_until_instrs.get_value()) {
        // Force a clean call when another thread changes tracing mode, so that
        // we can update our trace appropriately.
        insert_mode_comparison(drcontext, ilist, where, reg_ptr, &tracing_mode,
                               MEMTRACE_TLS_OFFS_MODE);
    }

    reg_id_t reg_tmp = DR_REG_NULL, reg_tmp2 = DR_REG_NULL;
    instr_t *skip_thread = INSTR_CREATE_label(drcontext);
    reg_id_set_t app_regs_at_skip_thread;
    bool is_L0I_enabled, is_L0D_enabled;
    get_L0_filters_enabled(mode, &is_L0I_enabled, &is_L0D_enabled);
    if ((is_L0I_enabled || is_L0D_enabled) && thread_filtering_enabled) {
        insert_conditional_skip(drcontext, ilist, where, reg_ptr, &reg_tmp2, skip_thread,
                                short_reaches, app_regs_at_skip_thread);
    }
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext, opnd_create_reg(reg_ptr),
                              OPND_CREATE_MEMPTR(reg_ptr, 0)));
    reg_id_set_t app_regs_at_skip_call;
    insert_conditional_skip(drcontext, ilist, where, reg_ptr, &reg_tmp, skip_call,
                            short_reaches, app_regs_at_skip_call);

    dr_insert_clean_call_ex(drcontext, ilist, where, (void *)clean_call,
                            DR_CLEANCALL_ALWAYS_OUT_OF_LINE, 0);
    insert_conditional_skip_target(drcontext, ilist, where, skip_call, reg_tmp,
                                   app_regs_at_skip_call);
    insert_conditional_skip_target(drcontext, ilist, where, skip_thread, reg_tmp2,
                                   app_regs_at_skip_thread);
}

// Called before writing to the trace buffer.
// reg_ptr is treated as scratch and may be clobbered by this routine.
// Returns DR_REG_NULL to indicate *not* to insert the instrumentation to
// write to the trace buffer.  Otherwise, returns a register that the caller
// must restore *after* the skip target.  The caller must also restore the
// aflags after the skip target.  (This is for parity on all paths per drreg
// limitations.)
static reg_id_t
insert_filter_addr(void *drcontext, instrlist_t *ilist, instr_t *where, user_data_t *ud,
                   reg_id_t reg_ptr, opnd_t ref, instr_t *app, instr_t *skip,
                   dr_pred_type_t pred, uintptr_t mode)
{
    bool is_L0I_enabled, is_L0D_enabled;
    get_L0_filters_enabled(mode, &is_L0I_enabled, &is_L0D_enabled);
    // Our "level 0" inlined direct-mapped cache filter.
    DR_ASSERT((is_L0I_enabled || is_L0D_enabled));
    reg_id_t reg_idx;
    bool is_icache = opnd_is_null(ref);
    uint64 cache_size = is_icache ? op_L0I_size.get_value() : op_L0D_size.get_value();
    if (cache_size == 0)
        return DR_REG_NULL; // Skip instru.
    ptr_int_t mask = (ptr_int_t)(cache_size / op_line_size.get_value()) - 1;
    int line_bits = compute_log2(op_line_size.get_value());
    uint offs = is_icache ? MEMTRACE_TLS_OFFS_ICACHE : MEMTRACE_TLS_OFFS_DCACHE;
    reg_id_t reg_addr;
    if (is_icache) {
        // For filtering the icache, we disable bundles + delays and call here on
        // every instr.  We skip if we're still on the same cache line.
        if (ud->last_app_pc != NULL) {
            ptr_uint_t prior_line = ((ptr_uint_t)ud->last_app_pc >> line_bits) & mask;
            // FIXME i#2439: we simplify and ignore a 2nd cache line touched by an
            // instr that straddles cache lines.  However, that is not uncommon on
            // x86 and we should check the L0 cache for both lines, do regular instru
            // if either misses, and have some flag telling the regular instru to
            // only do half the instr if only one missed (for offline this flag would
            // have to propagate to raw2trace; for online we could use a mid-instr PC
            // and size).
            ptr_uint_t new_line = ((ptr_uint_t)instr_get_app_pc(app) >> line_bits) & mask;
            if (prior_line == new_line)
                return DR_REG_NULL; // Skip instru.
        }
        ud->last_app_pc = instr_get_app_pc(app);
    }
    if (drreg_reserve_register(drcontext, ilist, where, &scratch_reserve_vec,
                               &reg_addr) != DRREG_SUCCESS)
        FATAL("Fatal error: failed to reserve scratch reg\n");
    if (drreg_reserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS)
        FATAL("Fatal error: failed to reserve aflags\n");
    // We need a 3rd scratch register.  We can avoid clobbering the app address
    // if we either get a 4th scratch or keep re-computing the tag and the mask
    // but it's better to keep the common path shorter, so we clobber reg_addr
    // with the tag and recompute on a miss.
    if (drreg_reserve_register(drcontext, ilist, where, NULL, &reg_idx) != DRREG_SUCCESS)
        FATAL("Fatal error: failed to reserve 3rd scratch register\n");
#ifdef ARM
    if (instr_predicate_is_cond(pred)) {
        // We can't mark everything as predicated b/c we have a cond branch.
        // Instead we jump over it if the memref won't be executed.
        // We have to do that after spilling the regs for parity on all paths.
        // This means we don't have to restore app flags for later predicate prefixes.
        MINSERT(ilist, where,
                XINST_CREATE_jump_cond(drcontext, instr_invert_predicate(pred),
                                       opnd_create_instr(skip)));
    }
#endif
    if (thread_filtering_enabled) {
        // Similarly to the predication case, we need reg parity, so we incur
        // the cost of spilling the regs before we skip for this thread so the
        // lazy restores are the same on all paths.
        // XXX: do better!
        insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
        MINSERT(
            ilist, where,
            XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_ptr), OPND_CREATE_INT32(0)));
        MINSERT(ilist, where,
                XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(skip)));
    }
    // First get the cache slot and load what's currently stored there.
    // XXX i#2439: we simplify and ignore a memref that straddles cache lines.
    // That will only happen for unaligned accesses.
    if (is_icache) {
        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)instr_get_app_pc(app),
                                         opnd_create_reg(reg_addr), ilist, where, NULL,
                                         NULL);
    } else
        instru->insert_obtain_addr(drcontext, ilist, where, reg_addr, reg_ptr, ref);
    MINSERT(ilist, where,
            XINST_CREATE_slr_s(drcontext, opnd_create_reg(reg_addr),
                               OPND_CREATE_INT8(line_bits)));
    MINSERT(ilist, where,
            XINST_CREATE_move(drcontext, opnd_create_reg(reg_idx),
                              opnd_create_reg(reg_addr)));
#ifndef X86
    /* Unfortunately the mask is likely too big for an immediate (32K cache and
     * 64-byte line => 0x1ff mask, and A32 and T32 have an 8-bit limit).
     */
    MINSERT(ilist, where,
            XINST_CREATE_load_int(drcontext, opnd_create_reg(reg_ptr),
                                  OPND_CREATE_INT32(mask)));
#endif
    MINSERT(ilist, where,
            XINST_CREATE_and_s(
                drcontext, opnd_create_reg(reg_idx),
                IF_X86_ELSE(OPND_CREATE_INT32(mask), opnd_create_reg(reg_ptr))));
    dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                           tls_offs + sizeof(void *) * offs, reg_ptr);
    // While we can load from a base reg + scaled index reg on x86 and arm, we
    // have to clobber the index reg as the dest, and we need the final address again
    // to store on a miss.  Thus we take a step to compute the final
    // cache addr in a register.
    MINSERT(ilist, where,
            XINST_CREATE_add_sll(drcontext, opnd_create_reg(reg_ptr),
                                 opnd_create_reg(reg_ptr), opnd_create_reg(reg_idx),
                                 compute_log2(sizeof(app_pc))));
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext, opnd_create_reg(reg_idx),
                              OPND_CREATE_MEMPTR(reg_ptr, 0)));
    // Now see whether it's a hit or a miss.
    MINSERT(
        ilist, where,
        XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_idx), opnd_create_reg(reg_addr)));
    MINSERT(ilist, where,
            XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(skip)));
    // On a miss, replace the cache entry with the new cache line.
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, 0),
                               opnd_create_reg(reg_addr)));

    // Restore app value b/c the caller will re-compute the app addr.
    // We can avoid clobbering the app address if we either get a 4th scratch or
    // keep re-computing the tag and the mask but it's better to keep the common
    // path shorter, so we clobber reg_addr with the tag and recompute on a miss.
    if (!is_icache && opnd_uses_reg(ref, reg_idx))
        drreg_get_app_value(drcontext, ilist, where, reg_idx, reg_idx);
    if (drreg_unreserve_register(drcontext, ilist, where, reg_addr) != DRREG_SUCCESS)
        FATAL("Fatal error: failed to unreserve scratch reg\n");
    return reg_idx;
}

static int
instrument_memref(void *drcontext, user_data_t *ud, instrlist_t *ilist, instr_t *where,
                  reg_id_t reg_ptr, int adjust, instr_t *app, opnd_t ref, int ref_index,
                  bool write, dr_pred_type_t pred, uintptr_t mode)
{
    if (op_instr_only_trace.get_value()) {
        return adjust;
    }
    instr_t *skip = INSTR_CREATE_label(drcontext);
    reg_id_t reg_third = DR_REG_NULL;
    bool is_L0I_enabled, is_L0D_enabled;
    get_L0_filters_enabled(mode, &is_L0I_enabled, &is_L0D_enabled);

    if (is_L0D_enabled) {
        reg_third = insert_filter_addr(drcontext, ilist, where, ud, reg_ptr, ref, NULL,
                                       skip, pred, mode);
        if (reg_third == DR_REG_NULL) {
            instr_destroy(drcontext, skip);
            return adjust;
        }
    }
    // XXX: If we're filtering only instrs, not data, then we can possibly
    // avoid loading the buf ptr for each memref. We skip this optimization for
    // now for simplicity.
    if ((is_L0I_enabled || is_L0D_enabled))
        insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    adjust = instru->instrument_memref(drcontext, ud->instru_field, ilist, where, reg_ptr,
                                       adjust, app, ref, ref_index, write, pred,
                                       is_L0I_enabled);
    if ((is_L0I_enabled || is_L0D_enabled) && adjust != 0) {
        // When filtering we can't combine buf_ptr adjustments.
        insert_update_buf_ptr(drcontext, ilist, where, reg_ptr, pred, adjust, mode);
        adjust = 0;
    }
    MINSERT(ilist, where, skip);
    if (is_L0D_enabled) {
        // drreg requires parity on all paths, so we need to restore the scratch regs
        // for the filter *after* the skip target.
        if (reg_third != DR_REG_NULL &&
            drreg_unreserve_register(drcontext, ilist, where, reg_third) != DRREG_SUCCESS)
            DR_ASSERT(false);
        if (drreg_unreserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS)
            DR_ASSERT(false);
    }
    return adjust;
}

static int
instrument_instr(void *drcontext, void *tag, user_data_t *ud, instrlist_t *ilist,
                 instr_t *where, reg_id_t reg_ptr, int adjust, instr_t *app,
                 uintptr_t mode)
{
    instr_t *skip = INSTR_CREATE_label(drcontext);
    reg_id_t reg_third = DR_REG_NULL;
    bool is_L0I_enabled, is_L0D_enabled;
    get_L0_filters_enabled(mode, &is_L0I_enabled, &is_L0D_enabled);
    if (is_L0I_enabled) {
        // Count dynamic instructions per thread.
        // It is too expensive to increment per instruction, so we increment once
        // per block by the instruction count for that block.
        if (!ud->recorded_instr_count) {
            ud->recorded_instr_count = true;
            // On x86 we could do this in one instruction if we clobber the flags: but
            // then we'd have to preserve the flags before our same-line skip in
            // insert_filter_addr().
            dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                                   tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNT,
                                   reg_ptr);
            MINSERT(ilist, where,
                    XINST_CREATE_add(drcontext, opnd_create_reg(reg_ptr),
                                     OPND_CREATE_INT16(ud->bb_instr_count)));
            dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                                    tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNT,
                                    reg_ptr);
        }
        reg_third = insert_filter_addr(drcontext, ilist, where, ud, reg_ptr,
                                       opnd_create_null(), app, skip, DR_PRED_NONE, mode);
        if (reg_third == DR_REG_NULL) {
            instr_destroy(drcontext, skip);
            return adjust;
        }
    }
    if ((is_L0I_enabled || is_L0D_enabled)) // Else already loaded.
        insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    if (op_instr_encodings.get_value()) {
        adjust = instru->instrument_instr_encoding(drcontext, tag, ud->instru_field,
                                                   ilist, where, reg_ptr, adjust, app);
    }
    adjust = instru->instrument_instr(drcontext, tag, ud->instru_field, ilist, where,
                                      reg_ptr, adjust, app, is_L0I_enabled, mode);
    if ((is_L0I_enabled || is_L0D_enabled) && adjust != 0) {
        // When filtering we can't combine buf_ptr adjustments.
        insert_update_buf_ptr(drcontext, ilist, where, reg_ptr, DR_PRED_NONE, adjust,
                              mode);
        adjust = 0;
    }
    MINSERT(ilist, where, skip);
    if (is_L0I_enabled) {
        // drreg requires parity on all paths, so we need to restore the scratch regs
        // for the filter *after* the skip target.
        if (reg_third != DR_REG_NULL &&
            drreg_unreserve_register(drcontext, ilist, where, reg_third) != DRREG_SUCCESS)
            DR_ASSERT(false);
        if (drreg_unreserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS)
            DR_ASSERT(false);
    }
    return adjust;
}

static bool
is_last_instr(void *drcontext, instr_t *instr)
{
    bool is_last = false;
    return drbbdup_is_last_instr(drcontext, instr, &is_last) == DRBBDUP_SUCCESS &&
        is_last;
}

/* For each memory reference app instr, we insert inline code to fill the buffer
 * with an instruction entry and memory reference entries.
 */
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      instr_t *where, bool for_trace, bool translating, uintptr_t mode,
                      void *orig_analysis_data, void *user_data)
{
    int i, adjust = 0;
    user_data_t *ud = (user_data_t *)user_data;
    reg_id_t reg_ptr;
    drvector_t rvec;
    dr_emit_flags_t flags = DR_EMIT_DEFAULT;
    bool is_L0I_enabled, is_L0D_enabled;
    get_L0_filters_enabled(mode, &is_L0I_enabled, &is_L0D_enabled);

    // We need drwrap's instrumentation to go first so that function trace
    // entries will not be appended to the middle of a BB's PC and Memory Access
    // trace entries. Assumption made here is that every drwrap function pre/post
    // callback always happens at the first instruction of a BB.
    dr_emit_flags_t func_flags = func_trace_enabled_instrument_event(
        drcontext, tag, bb, instr, where, for_trace, translating, NULL);
    flags = static_cast<dr_emit_flags_t>(flags | func_flags);

    drmgr_disable_auto_predication(drcontext, bb);

    if ((is_L0I_enabled || is_L0D_enabled) && ud->repstr &&
        is_first_nonlabel(drcontext, instr)) {
        // XXX: the control flow added for repstr ends up jumping over the
        // aflags spill for the memref, yet it hits the lazily-delayed aflags
        // restore.  We don't have a great solution (repstr violates drreg's
        // symmetric-paths requirement) so we work around it by forcing a
        // spill up front before the internal jump.
        if (drreg_reserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve aflags\n");
        if (drreg_unreserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve aflags\n");
    }

    bool need_rseq_instru = instr_is_label(instr) &&
        instr_get_note(instr) == (void *)DR_NOTE_RSEQ_ENTRY &&
        // For filtered instructions we can't adjust rseq regions as we do not have
        // the fill instruction sequence so we reduce overhead by not outputting the
        // entry markers.
        !op_L0I_filter.get_value();

    // Use emulation-aware queries to accurately trace rep string and
    // scatter-gather expansions.  Getting this wrong can result in significantly
    // incorrect ifetch stats (i#2011).
    instr_t *instr_fetch = drmgr_orig_app_instr_for_fetch(drcontext);
    instr_t *instr_operands = drmgr_orig_app_instr_for_operands(drcontext);
    if (instr_fetch == NULL &&
        (instr_operands == NULL ||
         !(instr_reads_memory(instr_operands) || instr_writes_memory(instr_operands))) &&
        // Ensure we reach the code below for post-strex instru.
        ud->strex == NULL &&
        // Do not skip misc cases that need instrumentation.
        !need_rseq_instru &&
        // Avoid dropping trailing bundled instrs or missing the block-final clean call.
        !is_last_instr(drcontext, instr))
        return flags;

    // i#1698: there are constraints for code between ldrex/strex pairs.
    // Normally DR will have done its -ldstex2cas, but in case that's disabled,
    // we attempt to handle ldrex/strex pairs here as a best-effort attempt.
    // However there is no way to completely avoid the instrumentation in between,
    // so we reduce the instrumentation in between by moving strex instru
    // from before the strex to after the strex.
    instr_t *strex = NULL;
    if (instr_fetch != NULL && instr_is_exclusive_store(instr_fetch))
        strex = instr_fetch;
    else if (instr_operands != NULL && instr_is_exclusive_store(instr_operands))
        strex = instr_operands;
    if (ud->strex == NULL && strex != NULL) {
        opnd_t dst = instr_get_dst(strex, 0);
        DR_ASSERT(opnd_is_base_disp(dst));
        if (!instr_writes_to_reg(strex, opnd_get_base(dst), DR_QUERY_INCLUDE_COND_DSTS)) {
            ud->strex = strex;
            ud->last_app_pc = instr_get_app_pc(strex);
        } else {
            NOTIFY(0,
                   "Exclusive store clobbering base not supported: skipping address\n");
        }
        if (is_last_instr(drcontext, instr)) {
            // We need our block-final call below.
            NOTIFY(0, "Block-final exclusive store: may hang");
            ud->strex = NULL;
        } else
            return flags;
    } else if (strex != NULL) {
        // Assuming there are no consecutive strex instructions, otherwise we
        // will insert instrumentation code at the second strex instruction.
        NOTIFY(0, "Consecutive exclusive stores not supported: may hang.");
    }

    // Optimization: delay the simple instr trace instrumentation if possible.
    // For offline traces we want a single instr entry for the start of the bb.
    if (instr_fetch != NULL && (!op_offline.get_value() || ud->recorded_instr) &&
        (instr_operands == NULL ||
         !(instr_reads_memory(instr_operands) || instr_writes_memory(instr_operands))) &&
        // Avoid dropping trailing bundled instrs or missing the block-final clean call.
        !is_last_instr(drcontext, instr) &&
        // Avoid bundling instrs whose types we separate.
        (instru_t::instr_to_instr_type(instr_fetch, ud->repstr) == TRACE_TYPE_INSTR ||
         // We avoid overhead of skipped bundling for online unless the user requested
         // instr types.  We could use different types for
         // bundle-ends-in-this-branch-type to avoid this but for now it's not worth it.
         (!op_offline.get_value() && !op_online_instr_types.get_value())) &&
        ud->strex == NULL &&
        // Don't bundle emulated instructions, as they sometimes have internal control
        // flow and other complications that could cause us to skip an instruction.
        !drmgr_in_emulation_region(drcontext, NULL) &&
        // We can't bundle with a filter.
        !(is_L0I_enabled || is_L0D_enabled) &&
        // The delay instr buffer is not full.
        ud->num_delay_instrs < MAX_NUM_DELAY_INSTRS) {
        ud->delay_instrs[ud->num_delay_instrs++] = instr_fetch;
        return flags;
    }

    /* We usually need two scratch registers, but not always, so we push the 2nd
     * out into the instru_t routines.
     * reg_ptr must be ECX or RCX for jecxz on x86, and must be <= r7 for cbnz on ARM.
     */
    drreg_init_and_fill_vector(&rvec, false);
#ifdef X86
    drreg_set_vector_entry(&rvec, DR_REG_XCX, true);
#elif defined(RISCV64)
    /* FIXME i#3544: Check if scratch reg can be used here. */
    drreg_set_vector_entry(&rvec, DR_REG_T2, true);
#else
    for (reg_ptr = DR_REG_R0; reg_ptr <= DR_REG_R7; reg_ptr++)
        drreg_set_vector_entry(&rvec, reg_ptr, true);
#endif
    if (drreg_reserve_register(drcontext, bb, where, &rvec, &reg_ptr) != DRREG_SUCCESS) {
        // We can't recover.
        FATAL("Fatal error: failed to reserve scratch registers\n");
    }
    drvector_delete(&rvec);

    /* Load buf ptr into reg_ptr, unless we're cache-filtering. */
    instr_t *skip_instru = INSTR_CREATE_label(drcontext);
    reg_id_t reg_skip = DR_REG_NULL;
    reg_id_set_t app_regs_at_skip;
    if (!(is_L0I_enabled || is_L0D_enabled)) {
        insert_load_buf_ptr(drcontext, bb, where, reg_ptr);
        if (thread_filtering_enabled) {
            bool short_reaches = false;
#ifdef X86
            if (ud->num_delay_instrs == 0 && !is_last_instr(drcontext, instr)) {
                /* jecxz should reach (really we want "smart jecxz" automation here) */
                short_reaches = true;
            }
#endif
            insert_conditional_skip(drcontext, bb, where, reg_ptr, &reg_skip, skip_instru,
                                    short_reaches, app_regs_at_skip);
        }
    }

    if (ud->num_delay_instrs != 0) {
        adjust = instrument_delay_instrs(drcontext, tag, bb, ud, where, reg_ptr, adjust,
                                         is_L0I_enabled, mode);
    }

    if (ud->strex != NULL) {
        DR_ASSERT(instr_is_exclusive_store(ud->strex));
        adjust = instrument_instr(drcontext, tag, ud, bb, where, reg_ptr, adjust,
                                  ud->strex, mode);
        adjust = instrument_memref(drcontext, ud, bb, where, reg_ptr, adjust, ud->strex,
                                   instr_get_dst(ud->strex, 0), 0, true,
                                   instr_get_predicate(ud->strex), mode);
        ud->strex = NULL;
    }

    if (need_rseq_instru) {
        DR_ASSERT(!op_L0I_filter.get_value()); // Excluded from need_rseq_instru.
        if (op_L0D_filter.get_value())
            insert_load_buf_ptr(drcontext, bb, where, reg_ptr);
        adjust =
            instru->instrument_rseq_entry(drcontext, bb, where, instr, reg_ptr, adjust);
        if (op_L0D_filter.get_value()) {
            // When filtering we can't combine buf_ptr adjustments.
            insert_update_buf_ptr(drcontext, bb, where, reg_ptr, DR_PRED_NONE, adjust,
                                  mode);
            adjust = 0;
        }
    }

    /* Instruction entry for instr fetch trace.  This does double-duty by
     * also providing the PC for subsequent data ref entries.
     */
    if (instr_fetch != NULL && (!op_offline.get_value() || !ud->recorded_instr)) {
        adjust = instrument_instr(drcontext, tag, ud, bb, where, reg_ptr, adjust,
                                  instr_fetch, mode);
        ud->recorded_instr = true;
        ud->last_app_pc = instr_get_app_pc(instr_fetch);
    }

    /* Data entries. */
    if (instr_operands != NULL &&
        (instr_reads_memory(instr_operands) || instr_writes_memory(instr_operands))) {
        dr_pred_type_t pred = instr_get_predicate(instr_operands);
        if (pred != DR_PRED_NONE && adjust != 0) {
            // Update buffer ptr and reset adjust to 0, because
            // we may not execute the inserted code below.
            insert_update_buf_ptr(drcontext, bb, where, reg_ptr, DR_PRED_NONE, adjust,
                                  mode);
            adjust = 0;
        }

        /* insert code to add an entry for each memory reference opnd */
        for (i = 0; i < instr_num_srcs(instr_operands); i++) {
            const opnd_t src = instr_get_src(instr_operands, i);
            if (opnd_is_memory_reference(src)) {
#ifdef AARCH64
                /* TODO i#5844: Memory references involving SVE registers are not
                 * supported yet. To be implemented as part of scatter/gather work.
                 */
                if (opnd_is_base_disp(src) &&
                    (reg_is_z(opnd_get_base(src)) || reg_is_z(opnd_get_index(src)))) {
                    if (!reported_sg_warning) {
                        NOTIFY(
                            0,
                            "WARNING: Scatter/gather is not supported, results will be "
                            "inaccurate\n");
                        reported_sg_warning = true;
                    }
                    continue;
                }
#endif
                adjust = instrument_memref(drcontext, ud, bb, where, reg_ptr, adjust,
                                           instr_operands, src, i, false, pred, mode);
            }
        }

        for (i = 0; i < instr_num_dsts(instr_operands); i++) {
            const opnd_t dst = instr_get_dst(instr_operands, i);
            if (opnd_is_memory_reference(dst)) {
#ifdef AARCH64
                /* TODO i#5844: Memory references involving SVE registers are not
                 * supported yet. To be implemented as part of scatter/gather work.
                 */
                if (opnd_is_base_disp(dst) &&
                    (reg_is_z(opnd_get_base(dst)) || reg_is_z(opnd_get_index(dst)))) {
                    if (!reported_sg_warning) {
                        NOTIFY(
                            0,
                            "WARNING: Scatter/gather is not supported, results will be "
                            "inaccurate\n");
                        reported_sg_warning = true;
                    }
                    continue;
                }
#endif
                adjust = instrument_memref(drcontext, ud, bb, where, reg_ptr, adjust,
                                           instr_operands, dst, i, true, pred, mode);
            }
        }
        if (adjust != 0)
            insert_update_buf_ptr(drcontext, bb, where, reg_ptr, pred, adjust, mode);
    } else if (adjust != 0)
        insert_update_buf_ptr(drcontext, bb, where, reg_ptr, DR_PRED_NONE, adjust, mode);

    /* Insert code to call clean_call for processing the buffer.
     * We restore the registers after the clean call, which should be ok
     * assuming the clean call does not need the two register values.
     */
    if (is_last_instr(drcontext, instr)) {
        if ((is_L0I_enabled || is_L0D_enabled))
            insert_load_buf_ptr(drcontext, bb, where, reg_ptr);
        instrument_clean_call(drcontext, bb, where, reg_ptr, mode);
    }

    insert_conditional_skip_target(drcontext, bb, where, skip_instru, reg_skip,
                                   app_regs_at_skip);

    /* restore scratch register */
    if (drreg_unreserve_register(drcontext, bb, where, reg_ptr) != DRREG_SUCCESS)
        DR_ASSERT(false);
    return flags;
}

/* We transform string loops into regular loops so we can more easily
 * monitor every memory reference they make.
 */
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating)
{
    /* drbbdup doesn't pass the user_data from this stage so we use TLS.
     * XXX i#5400: Integrating drbbdup into drmgr would provide user_data here.
     */
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (!drutil_expand_rep_string_ex(drcontext, bb, &pt->repstr, NULL)) {
        DR_ASSERT(false);
        /* in release build, carry on: we'll just miss per-iter refs */
    }
    if (!drx_expand_scatter_gather(drcontext, bb, &pt->scatter_gather)) {
        DR_ASSERT(false);
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void **user_data, uintptr_t mode)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    user_data_t *ud = (user_data_t *)dr_thread_alloc(drcontext, sizeof(user_data_t));
    memset(ud, 0, sizeof(*ud));
    ud->repstr = pt->repstr;
    ud->scatter_gather = pt->scatter_gather;
    *user_data = (void *)ud;

    instru->bb_analysis(drcontext, tag, &ud->instru_field, bb, ud->repstr,
                        mode == BBDUP_MODE_L0_FILTER);

    ud->bb_instr_count = instru_t::count_app_instrs(bb);
    // As elsewhere in this code, we want the single-instr original and not
    // the expanded 6 labeled-app instrs for repstr loops (i#2011).
    // The new emulation support should have solved that for us.
    DR_ASSERT((!ud->repstr && !pt->scatter_gather) || ud->bb_instr_count == 1);

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis_cleanup(void *drcontext, void *user_data)
{
    user_data_t *ud = reinterpret_cast<user_data_t *>(user_data);
    instru->bb_analysis_cleanup(drcontext, ud->instru_field);

    dr_thread_free(drcontext, user_data, sizeof(user_data_t));
    return DR_EMIT_DEFAULT;
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true;
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (!is_in_tracing_mode(tracing_mode.load(std::memory_order_acquire)))
        return true;
    if (BUF_PTR(data->seg_base) == NULL)
        return true; /* This thread was filtered out. */

    // If we just switched to BBDUP_MODE_TRACE in this thread in a block ending in a
    // syscall, we can end up here without having traced the syscall instr, which we
    // want to avoid.  We look for an empty new-window buffer (not just empty, because
    // it could be empty just due to the syscall instr filling up the prior buffer).
    // (The converse can happen, with a tracing window ending in a syscall but the mode
    // having changed, causing us to exit up above and not emit a marker: we solve that
    // by removing syscalls without markers in raw2trace.)
    if (is_new_window_buffer_empty(data))
        return true;

    // Output system call numbers if we have a full instruction trace.
    // Since the instruction fetch has already been output, this will be
    // appended to the block-final syscall instr.
    if (!op_L0I_filter.get_value()) {
        BUF_PTR(data->seg_base) += instru->append_marker(
            BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_SYSCALL, sysnum);
#ifdef LINUX
        if (sysnum == SYS_futex) {
            static constexpr int FUTEX_ARG_COUNT = 6;
            BUF_PTR(data->seg_base) += instru->append_marker(
                BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_FUNC_ID,
                static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) +
                    IF_X64_ELSE(sysnum, (sysnum & 0xffff)));
            for (int i = 0; i < FUTEX_ARG_COUNT; ++i) {
                BUF_PTR(data->seg_base) += instru->append_marker(
                    BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_FUNC_ARG,
                    dr_syscall_get_param(drcontext, i));
            }
        }
#endif
    }

#ifdef ARM
    // On Linux ARM, cacheflush syscall takes 3 params: start, end, and 0.
    if (sysnum == SYS_cacheflush) {
        addr_t start = (addr_t)dr_syscall_get_param(drcontext, 0);
        addr_t end = (addr_t)dr_syscall_get_param(drcontext, 1);
        if (end > start) {
            BUF_PTR(data->seg_base) +=
                instru->append_iflush(BUF_PTR(data->seg_base), start, end - start);
        }
    }
#endif
    if (file_ops_func.handoff_buf == NULL)
        process_and_output_buffer(drcontext, false);

#ifdef BUILD_PT_TRACER
    if (op_offline.get_value() && op_enable_kernel_tracing.get_value()) {
        if (data->syscall_pt_trace.get_cur_recording_sysnum() != INVALID_SYSNUM) {
            ASSERT(false, "last tracing isn't stopped");
            if (!data->syscall_pt_trace.stop_syscall_pt_trace()) {
                ASSERT(false, "failed to stop syscall pt trace");
                return false;
            }
        }

        if (!syscall_pt_trace_t::is_syscall_pt_trace_enabled(sysnum)) {
            return true;
        }

        /* Write a marker to userspace raw trace. */
        trace_marker_type_t marker_type = TRACE_MARKER_TYPE_SYSCALL_IDX;
        uintptr_t marker_val = data->syscall_pt_trace.get_traced_syscall_idx();
        BUF_PTR(data->seg_base) +=
            instru->append_marker(BUF_PTR(data->seg_base), marker_type, marker_val);

        if (!data->syscall_pt_trace.start_syscall_pt_trace(sysnum)) {
            ASSERT(false, "failed to start syscall pt trace");
            return false;
        }
    }
#endif
    return true;
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (tracing_mode.load(std::memory_order_acquire) != BBDUP_MODE_TRACE)
        return;
    if (BUF_PTR(data->seg_base) == NULL)
        return; /* This thread was filtered out. */

#ifdef LINUX
    if (!op_L0I_filter.get_value()) { /* No syscall data unless full instr trace. */
        if (sysnum == SYS_futex) {
            dr_syscall_result_info_t info = {
                sizeof(info),
            };
            dr_syscall_get_result_ex(drcontext, &info);
            BUF_PTR(data->seg_base) += instru->append_marker(
                BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_FUNC_ID,
                static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) +
                    IF_X64_ELSE(sysnum, (sysnum & 0xffff)));
            /* XXX i#5843: Return values are complex and can include more than just
             * the primary register value.  Since we care mostly just about failure,
             * we use the "succeeded" field.  However, this is not accurate for all
             * syscalls.  Plus, would the scheduler want to know about various
             * successful return values which indicate how many waiters were woken
             * up and other data?
             */
            BUF_PTR(data->seg_base) += instru->append_marker(
                BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_FUNC_RETVAL, info.succeeded);
        }
    }
#endif

#ifdef BUILD_PT_TRACER
    if (!op_offline.get_value() || !op_enable_kernel_tracing.get_value())
        return;
    if (!is_in_tracing_mode(tracing_mode.load(std::memory_order_acquire)))
        return;
    if (!syscall_pt_trace_t::is_syscall_pt_trace_enabled(sysnum))
        return;

    if (data->syscall_pt_trace.get_cur_recording_sysnum() == INVALID_SYSNUM) {
        ASSERT(false, "last syscall is not traced");
        return;
    }

    ASSERT(data->syscall_pt_trace.get_cur_recording_sysnum() == sysnum,
           "last tracing isn't for the expected sysnum");
    if (!data->syscall_pt_trace.stop_syscall_pt_trace()) {
        ASSERT(false, "failed to stop syscall pt trace");
        return;
    }
#endif
}

static void
event_kernel_xfer(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    trace_marker_type_t marker_type;
    uintptr_t marker_val = 0;
    if (!is_in_tracing_mode(tracing_mode.load(std::memory_order_acquire)))
        return;
    if (BUF_PTR(data->seg_base) == NULL)
        return; /* This thread was filtered out. */
    switch (info->type) {
    case DR_XFER_APC_DISPATCHER:
        /* Do not bother with a marker for the thread init routine. */
        if (data->num_refs == 0 &&
            BUF_PTR(data->seg_base) == data->buf_base + data->init_header_size)
            return;
        ANNOTATE_FALLTHROUGH;
    case DR_XFER_SIGNAL_DELIVERY:
    case DR_XFER_EXCEPTION_DISPATCHER:
    case DR_XFER_RAISE_DISPATCHER:
    case DR_XFER_CALLBACK_DISPATCHER:
    case DR_XFER_RSEQ_ABORT: marker_type = TRACE_MARKER_TYPE_KERNEL_EVENT; break;
    case DR_XFER_SIGNAL_RETURN:
    case DR_XFER_CALLBACK_RETURN:
    case DR_XFER_CONTINUE:
    case DR_XFER_SET_CONTEXT_THREAD: marker_type = TRACE_MARKER_TYPE_KERNEL_XFER; break;
    case DR_XFER_CLIENT_REDIRECT: return;
    default: DR_ASSERT(false && "unknown kernel xfer type"); return;
    }
    NOTIFY(2, "%s: type %d, sig %d\n", __FUNCTION__, info->type, info->sig);
    /* TODO i#3937: We need something similar to what raw2trace does with this info
     * for online too, to place signals inside instr bundles.
     */
    /* XXX i#4041,i#5954: For rseq abort, offline post-processing rolls back the
     * committing store so the abort happens at a reasonable point.  We don't have a
     * solution for online though.
     */
    if (info->source_mcontext != nullptr) {
        app_pc mcontext_pc = info->source_mcontext->pc;
        /* When a signal arrives at the end of functions wrapped using the
         * DRWRAP_REPLACE_RETADDR drwrap strategy, the mcontext PC on the stack
         * is the address of the internal replace_retaddr_sentinel() instead
         * of the actual return address of the wrapped function. For the
         * kernel xfer marker to contain the correct value, we must handle
         * this case by allowing drwrap to replace the address with the
         * correct one.
         */
        drwrap_get_retaddr_if_sentinel(drcontext, &mcontext_pc);
        /* Enable post-processing to figure out the ordering of this xfer vs
         * non-memref instrs in the bb, and also to give core simulators the
         * interrupted PC -- primarily for a kernel event arriving right
         * after a branch to give a core simulator the branch target.
         * For non-module code we don't have the block id but the absolute PC
         * unambiguously points to the most recently executed encoding at
         * that PC, even if another thread modifies it right after the signal.
         */
        marker_val = reinterpret_cast<uintptr_t>(mcontext_pc);
        NOTIFY(3, "%s: source pc " PFX " => marker val " PIFX "\n", __FUNCTION__,
               mcontext_pc, marker_val);
    }
    if (info->type == DR_XFER_RSEQ_ABORT) {
        BUF_PTR(data->seg_base) += instru->append_marker(
            BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_RSEQ_ABORT, marker_val);
    }
    BUF_PTR(data->seg_base) +=
        instru->append_marker(BUF_PTR(data->seg_base), marker_type, marker_val);
    if (file_ops_func.handoff_buf == NULL)
        process_and_output_buffer(drcontext, false);
}

/***************************************************************************
 * Top level.
 */

/* Initializes a thread either at process init or fork init, where we want
 * a new offline file or a new thread,process registration pair for online.
 */
static void
init_thread_in_process(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

    init_thread_io(drcontext);

    if (op_L0D_filter.get_value() && op_L0D_size.get_value() > 0) {
        data->l0_dcache = (byte *)dr_raw_mem_alloc(
            (size_t)op_L0D_size.get_value() / op_line_size.get_value() * sizeof(void *),
            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        *(byte **)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_DCACHE) = data->l0_dcache;
    }
    if (op_L0I_filter.get_value() && op_L0I_size.get_value() > 0) {
        data->l0_icache = (byte *)dr_raw_mem_alloc(
            (size_t)op_L0I_size.get_value() / op_line_size.get_value() * sizeof(void *),
            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        *(byte **)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICACHE) = data->l0_icache;
    }

#ifdef BUILD_PT_TRACER
    if (op_offline.get_value() && op_enable_kernel_tracing.get_value()) {
        data->syscall_pt_trace.init(
            drcontext, kernel_pt_logsubdir,
            // XXX i#5505: This should be per-thread and per-window; once we've
            // finalized the PT output scheme we should pass those parameters.
            [](const char *fname, uint mode_flags) {
                return file_ops_func.open_process_file(fname, mode_flags);
            },
            file_ops_func.write_file, file_ops_func.close_file);
    }
#endif
    // XXX i#1729: gather and store an initial callstack for the thread.
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    DR_ASSERT(data != NULL);
    *data = {}; // We must safely zero due to the C++ class member.
    data->file = INVALID_FILE;
    drmgr_set_tls_field(drcontext, tls_idx, data);

    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    data->seg_base = (byte *)dr_get_dr_segment_base(tls_seg);
    DR_ASSERT(data->seg_base != NULL);

    event_inscount_thread_init(drcontext);

    if ((should_trace_thread_cb != NULL &&
         !(*should_trace_thread_cb)(dr_get_thread_id(drcontext),
                                    trace_thread_cb_user_data)) ||
        is_num_refs_beyond_global_max())
        BUF_PTR(data->seg_base) = NULL;
    else {
        init_buffers(data);
        init_thread_in_process(drcontext);
    }
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

    if (BUF_PTR(data->seg_base) != NULL) {
        /* This thread was *not* filtered out. */

#ifdef BUILD_PT_TRACER
        if (op_offline.get_value() && op_enable_kernel_tracing.get_value()) {
            int cur_recording_sysnum = data->syscall_pt_trace.get_cur_recording_sysnum();
            if (cur_recording_sysnum != INVALID_SYSNUM) {
                NOTIFY(0,
                       "ERROR: The last recorded syscall %d of thread T%d wasn't be "
                       "stopped.\n",
                       cur_recording_sysnum, dr_get_thread_id(drcontext));
                ASSERT(cur_recording_sysnum, "syscall recording is not stopped");
                if (!data->syscall_pt_trace.stop_syscall_pt_trace()) {
                    ASSERT(false, "failed to stop syscall pt trace");
                }
            }
        }
#endif

        /* let the simulator know this thread has exited */
        if (is_bytes_written_beyond_trace_max(data)) {
            // If over the limit, we still want to write the footer, but nothing else.
            BUF_PTR(data->seg_base) = data->buf_base + buf_hdr_slots_size;
        }
        if (op_L0I_filter.get_value()) {
            // Include the final instruction count.
            // It might be useful to include the count with each miss as well, but
            // in experiments that adds non-trivial space and time overheads (as
            // a separate marker; squished into the instr_count field might be
            // better but at complexity costs, plus we may need that field for
            // offset-within-block info to adjust the per-block count) and
            // would likely need to be under an off-by-default option and have
            // a mandated use case to justify adding it.
            uintptr_t icount =
                *(uintptr_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNT);
            BUF_PTR(data->seg_base) += instru->append_marker(
                BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_INSTRUCTION_COUNT, icount);
        }

        if (op_L0D_filter.get_value()) {
            if (op_L0D_size.get_value() > 0) {
                dr_raw_mem_free(data->l0_dcache,
                                (size_t)op_L0D_size.get_value() /
                                    op_line_size.get_value() * sizeof(void *));
            }
        }
        if (op_L0I_filter.get_value()) {
            if (op_L0I_size.get_value() > 0) {
                dr_raw_mem_free(data->l0_icache,
                                (size_t)op_L0I_size.get_value() /
                                    op_line_size.get_value() * sizeof(void *));
            }
        }

        exit_thread_io(drcontext);

        dr_mutex_lock(mutex);
        num_refs += data->num_refs;
        num_writeouts += data->num_writeouts;
        num_v2p_writeouts += data->num_v2p_writeouts;
        num_phys_markers += data->num_phys_markers;
        dr_mutex_unlock(mutex);
        dr_raw_mem_free(data->buf_base, max_buf_size);
        if (data->reserve_buf != NULL)
            dr_raw_mem_free(data->reserve_buf, max_buf_size);
    }
    data->~per_thread_t();
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static void
event_exit(void)
{
#ifdef BUILD_PT_TRACER
    if (op_offline.get_value() && op_enable_kernel_tracing.get_value()) {
        drpttracer_exit();
        /* Copy kcore and kallsyms to {kernel_pt_logsubdir}. */
        kcore_copy_t kcore_copy(file_ops_func.open_file, file_ops_func.write_file,
                                file_ops_func.close_file);
        if (!kcore_copy.copy(kernel_pt_logsubdir)) {
            NOTIFY(0, "WARNING: failed to copy kcore and kallsyms to %s\n",
                   kernel_pt_logsubdir);
        }
    }
#endif
    dr_log(NULL, DR_LOG_ALL, 1, "drcachesim num refs seen: " UINT64_FORMAT_STRING "\n",
           num_refs);
    NOTIFY(1,
           "drmemtrace exiting process " PIDFMT "; traced " UINT64_FORMAT_STRING
           " references in " UINT64_FORMAT_STRING " writeouts.\n",
           dr_get_process_id(), num_refs, num_writeouts);
    if (op_use_physical.get_value()) {
        dr_log(NULL, DR_LOG_ALL, 1,
               "drcachesim num physical address markers emitted: " UINT64_FORMAT_STRING
               "\n",
               num_phys_markers);
        NOTIFY(1,
               "drmemtrace emitted " UINT64_FORMAT_STRING
               " physical address markers in " UINT64_FORMAT_STRING " writeouts.\n",
               num_phys_markers, num_v2p_writeouts);
    }
    /* we use placement new for better isolation */
    instru->~instru_t();
    dr_global_free(instru, MAX_INSTRU_SIZE);

    if (op_offline.get_value()) {
        file_ops_func.close_file(module_file);
        if (funclist_file != INVALID_FILE)
            file_ops_func.close_file(funclist_file);
        if (encoding_file != INVALID_FILE)
            file_ops_func.close_file(encoding_file);
    } else
        ipc_pipe.close();

    if (file_ops_func.exit_cb != NULL)
        (*file_ops_func.exit_cb)(file_ops_func.exit_arg);

    if (!dr_raw_tls_cfree(tls_offs, MEMTRACE_TLS_COUNT))
        DR_ASSERT(false);

    drvector_delete(&scratch_reserve_vec);

    instrumentation_exit();

    if (!drmgr_unregister_tls_field(tls_idx) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        drreg_exit() != DRREG_SUCCESS)
        DR_ASSERT(false);
    if (op_enable_drstatecmp.get_value()) {
        if (drstatecmp_exit() != DRSTATECMP_SUCCESS) {
            DR_ASSERT(false);
        }
    }
    dr_unregister_exit_event(event_exit);

    /* Clear callbacks and globals to support re-attach when linked statically. */
    file_ops_func = file_ops_func_t();
    if (!offline_instru_t::custom_module_data(nullptr, nullptr, nullptr))
        DR_ASSERT(false && "failed to clear custom module callbacks");
    should_trace_thread_cb = nullptr;
    trace_thread_cb_user_data = nullptr;
    thread_filtering_enabled = false;
    num_refs = 0;
    num_refs_racy = 0;
    num_filter_refs_racy = 0;

    exit_io();

    dr_mutex_destroy(mutex);
    drutil_exit();
    drmgr_exit();
    func_trace_exit();
    drx_exit();
    /* Avoid accumulation of option values on static-link re-attach. */
    droption_parser_t::clear_values();
}

static bool
init_offline_dir(void)
{
    char buf[MAXIMUM_PATH];
    int i;
    const int NUM_OF_TRIES = 10000;
    /* open unique dir */
    /* We cannot use malloc mid-run (to support static client use).  We thus need
     * to move all std::strings into plain buffers at init time.
     */
    dr_snprintf(subdir_prefix, BUFFER_SIZE_ELEMENTS(subdir_prefix), "%s",
                op_subdir_prefix.get_value().c_str());
    NULL_TERMINATE_BUFFER(subdir_prefix);
    /* We do not need to call drx_init before using drx_open_unique_appid_file. */
    for (i = 0; i < NUM_OF_TRIES; i++) {
        /* We use drx_open_unique_appid_file with DRX_FILE_SKIP_OPEN to get a
         * directory name for creation.  Retry if the same name directory already
         * exists.  Abort if we fail too many times.
         */
        drx_open_unique_appid_file(op_outdir.get_value().c_str(), dr_get_process_id(),
                                   subdir_prefix, "dir", DRX_FILE_SKIP_OPEN, buf,
                                   BUFFER_SIZE_ELEMENTS(buf));
        NULL_TERMINATE_BUFFER(buf);
        /* open the dir */
        if (file_ops_func.create_dir(buf))
            break;
    }
    if (i == NUM_OF_TRIES)
        return false;
    /* We group the raw thread files in a further subdir to isolate from the
     * processed trace file.
     */
    dr_snprintf(logsubdir, BUFFER_SIZE_ELEMENTS(logsubdir), "%s%s%s", buf, DIRSEP,
                OUTFILE_SUBDIR);
    NULL_TERMINATE_BUFFER(logsubdir);
    if (!file_ops_func.create_dir(logsubdir))
        return false;

#ifdef BUILD_PT_TRACER
    dr_snprintf(kernel_pt_logsubdir, BUFFER_SIZE_ELEMENTS(kernel_pt_logsubdir), "%s%s%s",
                buf, DIRSEP, DRMEMTRACE_KERNEL_PT_SUBDIR);
    NULL_TERMINATE_BUFFER(kernel_pt_logsubdir);
    if (op_offline.get_value() && op_enable_kernel_tracing.get_value()) {
        if (!file_ops_func.create_dir(kernel_pt_logsubdir))
            return false;
    }
#endif
    if (has_tracing_windows())
        open_new_window_dir(tracing_window.load(std::memory_order_acquire));
    /* If the ops are replaced, it's up the replacer to notify the user.
     * In some cases data is sent over the network and the replaced create_dir is
     * a nop that returns true, in which case we don't want this message.
     */
    if (file_ops_func.create_dir == dr_create_dir)
        NOTIFY(1, "Log directory is %s\n", logsubdir);
    dr_snprintf(modlist_path, BUFFER_SIZE_ELEMENTS(modlist_path), "%s%s%s", logsubdir,
                DIRSEP, DRMEMTRACE_MODULE_LIST_FILENAME);
    NULL_TERMINATE_BUFFER(modlist_path);
    module_file = file_ops_func.open_process_file(
        modlist_path, DR_FILE_WRITE_REQUIRE_NEW IF_UNIX(| DR_FILE_CLOSE_ON_FORK));

    dr_snprintf(funclist_path, BUFFER_SIZE_ELEMENTS(funclist_path), "%s%s%s", logsubdir,
                DIRSEP, DRMEMTRACE_FUNCTION_LIST_FILENAME);
    NULL_TERMINATE_BUFFER(funclist_path);
    funclist_file = file_ops_func.open_process_file(
        funclist_path, DR_FILE_WRITE_REQUIRE_NEW IF_UNIX(| DR_FILE_CLOSE_ON_FORK));

    dr_snprintf(encoding_path, BUFFER_SIZE_ELEMENTS(encoding_path), "%s%s%s", logsubdir,
                DIRSEP, DRMEMTRACE_ENCODING_FILENAME);
    NULL_TERMINATE_BUFFER(encoding_path);
    encoding_file = file_ops_func.open_process_file(
        encoding_path, DR_FILE_WRITE_REQUIRE_NEW IF_UNIX(| DR_FILE_CLOSE_ON_FORK));

    return (module_file != INVALID_FILE && funclist_file != INVALID_FILE &&
            encoding_file != INVALID_FILE);
}

#ifdef UNIX
static void
fork_init(void *drcontext)
{
    if (op_offline.get_value()) {
        /* XXX i#4660: droption references at fork init time use malloc.
         * We do not fully support static link with fork at this time.
         */
        dr_allow_unsafe_static_behavior();
#    ifdef DRMEMTRACE_STATIC
        NOTIFY(0, "-offline across a fork is unsafe with statically linked clients\n");
#    endif
    }
    /* We use DR_FILE_CLOSE_ON_FORK, and we dumped outstanding data prior to the
     * fork syscall, so we just need to create a new subdir, new module log, and
     * a new initial thread file for offline, or register the new process for
     * online.
     */
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    /* Only count refs in the new process (plus, we use this to set up the
     * initial header in process_and_output_buffer() for offline).
     */
    data->num_refs = 0;
    if (op_offline.get_value()) {
        data->file = INVALID_FILE;
        if (!init_offline_dir()) {
            FATAL("Failed to create a subdir in %s\n", op_outdir.get_value().c_str());
        }
    }
    init_thread_in_process(drcontext);
}
#endif

drmemtrace_status_t
drmemtrace_replace_file_ops(drmemtrace_open_file_func_t open_file_func,
                            drmemtrace_read_file_func_t read_file_func,
                            drmemtrace_write_file_func_t write_file_func,
                            drmemtrace_close_file_func_t close_file_func,
                            drmemtrace_create_dir_func_t create_dir_func)
{
    /* We don't check op_offline b/c option parsing may not have happened yet. */
    if (open_file_func != NULL)
        file_ops_func.open_file = open_file_func;
    if (read_file_func != NULL)
        file_ops_func.read_file = read_file_func;
    if (write_file_func != NULL)
        file_ops_func.write_file = write_file_func;
    if (close_file_func != NULL)
        file_ops_func.close_file = close_file_func;
    if (create_dir_func != NULL)
        file_ops_func.create_dir = create_dir_func;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_replace_file_ops_ex(drmemtrace_replace_file_ops_t *ops)
{
    if (ops == nullptr || ops->size != sizeof(drmemtrace_replace_file_ops_t))
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    if (ops->write_file_func != nullptr && ops->handoff_buf_func != nullptr)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    if (ops->open_file_ex_func != nullptr) {
        file_ops_func.open_file_ex = ops->open_file_ex_func;
        file_ops_func.open_file = nullptr;
    }
    if (ops->read_file_func != nullptr)
        file_ops_func.read_file = ops->read_file_func;
    if (ops->write_file_func != nullptr)
        file_ops_func.write_file = ops->write_file_func;
    if (ops->close_file_func != nullptr)
        file_ops_func.close_file = ops->close_file_func;
    if (ops->create_dir_func != nullptr)
        file_ops_func.create_dir = ops->create_dir_func;
    if (ops->handoff_buf_func != nullptr)
        file_ops_func.handoff_buf = ops->handoff_buf_func;
    if (ops->exit_func != nullptr) {
        file_ops_func.exit_cb = ops->exit_func;
        file_ops_func.exit_arg = ops->exit_arg;
    }
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_buffer_handoff(drmemtrace_handoff_func_t handoff_func,
                          drmemtrace_exit_func_t exit_func, void *exit_func_arg)
{
    /* We don't check op_offline b/c option parsing may not have happened yet. */
    file_ops_func.handoff_buf = handoff_func;
    file_ops_func.exit_cb = exit_func;
    file_ops_func.exit_arg = exit_func_arg;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_get_output_path(OUT const char **path)
{
    if (path == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    *path = logsubdir;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_get_modlist_path(OUT const char **path)
{
    if (path == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    *path = modlist_path;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_get_funclist_path(OUT const char **path)
{
    if (path == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    *path = funclist_path;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_get_encoding_path(OUT const char **path)
{
    if (path == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    *path = encoding_path;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_custom_module_data(void *(*load_cb)(module_data_t *module, int seg_idx),
                              int (*print_cb)(void *data, char *dst, size_t max_len),
                              void (*free_cb)(void *data))
{
    /* We want to support this being called prior to initializing us, so we use
     * a static routine and do not check -offline.
     */
    if (offline_instru_t::custom_module_data(load_cb, print_cb, free_cb))
        return DRMEMTRACE_SUCCESS;
    else
        return DRMEMTRACE_ERROR;
}

drmemtrace_status_t
drmemtrace_filter_threads(bool (*should_trace_thread)(thread_id_t tid, void *user_data),
                          void *user_value)
{
    if (should_trace_thread == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    /* We document that this should be called once at init time: i.e., we do not
     * handle races here.
     * XXX: to filter out the calling thread (the initial application thread) we suggest
     * that this be called prior to DR initialization, but that's only feasible for
     * the start/stop API.  We should consider remembering the main thread's
     * per_thread_t and freeing it here if the filter routine says to skip it.
     */
    should_trace_thread_cb = should_trace_thread;
    trace_thread_cb_user_data = user_value;
    thread_filtering_enabled = true;
    return DRMEMTRACE_SUCCESS;
}

/* We export drmemtrace_client_main so that a global dr_client_main can initialize
 * drmemtrace client by calling drmemtrace_client_main in a statically linked
 * multi-client executable.
 */
DR_EXPORT void
drmemtrace_client_main(client_id_t id, int argc, const char *argv[])
{
    /* We need 2 reg slots beyond drreg's eflags slots => 3 slots */
    drreg_options_t ops = { sizeof(ops), 3, false };
    byte buf[MAXIMUM_PATH];

    dr_set_client_name("DynamoRIO Cache Simulator Tracer", "http://dynamorio.org/issues");

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, &parse_err,
                                       NULL)) {
        FATAL("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
              droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }
    if (!op_offline.get_value() && op_ipc_name.get_value().empty()) {
        FATAL("Usage error: ipc name is required\nUsage:\n%s",
              droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    } else if (op_offline.get_value() && op_outdir.get_value().empty()) {
        FATAL("Usage error: outdir is required\nUsage:\n%s",
              droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    } else if (!op_offline.get_value() &&
               (op_record_heap.get_value() || !op_record_function.get_value().empty())) {
        FATAL("Usage error: function recording is only supported for -offline\n");
    }
    if (op_L0_filter_until_instrs.get_value()) {
        if (!op_L0D_filter.get_value() && !op_L0I_filter.get_value()) {
            NOTIFY(
                0,
                "Assuming both L0D_filter and L0I_filter for L0_filter_until_instrs\n");
            op_L0D_filter.set_value(true);
            op_L0I_filter.set_value(true);
        }
    }

    if (op_L0_filter_deprecated.get_value()) {
        op_L0D_filter.set_value(true);
        op_L0I_filter.set_value(true);
    }
    if ((op_L0I_filter.get_value() && !IS_POWER_OF_2(op_L0I_size.get_value()) &&
         op_L0I_size.get_value() != 0) ||
        (op_L0D_filter.get_value() && !IS_POWER_OF_2(op_L0D_size.get_value()) &&
         op_L0D_size.get_value() != 0)) {
        FATAL("Usage error: L0I_size and L0D_size must be 0 or powers of 2.");
    }
    // We cannot elide addresses or ignore offsets when we need to translate
    // all addresses during tracing or when instruction or data address entries
    // are being filtered.
    if (op_use_physical.get_value() || op_L0I_filter.get_value() ||
        op_L0D_filter.get_value())
        op_disable_optimizations.set_value(true);

    event_inscount_init();
    init_io();

    DR_ASSERT(std::atomic_is_lock_free(&tracing_mode));
    DR_ASSERT(std::atomic_is_lock_free(&tracing_window));

    drreg_init_and_fill_vector(&scratch_reserve_vec, true);
#ifdef X86
    if (op_L0I_filter.get_value() || op_L0D_filter.get_value()) {
        /* We need to preserve the flags so we need xax. */
        drreg_set_vector_entry(&scratch_reserve_vec, DR_REG_XAX, false);
    }
#endif

    if (op_offline.get_value()) {
        void *placement;
        if (!init_offline_dir()) {
            FATAL("Failed to create a subdir in %s\n", op_outdir.get_value().c_str());
        }
        /* we use placement new for better isolation */
        DR_ASSERT(MAX_INSTRU_SIZE >= sizeof(offline_instru_t));
        placement = dr_global_alloc(MAX_INSTRU_SIZE);
        instru = new (placement)
            offline_instru_t(insert_load_buf_ptr, &scratch_reserve_vec,
                             file_ops_func.write_file, module_file, encoding_file,
                             op_disable_optimizations.get_value(), instru_notify);
    } else {
        void *placement;
        /* we use placement new for better isolation */
        DR_ASSERT(MAX_INSTRU_SIZE >= sizeof(online_instru_t));
        placement = dr_global_alloc(MAX_INSTRU_SIZE);
        instru = new (placement) online_instru_t(
            insert_load_buf_ptr, insert_update_buf_ptr, &scratch_reserve_vec);
        if (!ipc_pipe.set_name(op_ipc_name.get_value().c_str()))
            DR_ASSERT(false);
#ifdef UNIX
        /* we want an isolated fd so we don't use ipc_pipe.open_for_write() */
        int fd = dr_open_file(ipc_pipe.get_pipe_path().c_str(), DR_FILE_WRITE_ONLY);
        DR_ASSERT(fd != INVALID_FILE);
        if (!ipc_pipe.set_fd(fd))
            DR_ASSERT(false);
#else
        if (!ipc_pipe.open_for_write()) {
            if (GetLastError() == ERROR_PIPE_BUSY) {
                // FIXME i#1727: add multi-process support to Windows named_pipe_t.
                FATAL("Fatal error: multi-process applications not yet supported "
                      "for drcachesim on Windows\n");
            } else {
                FATAL("Fatal error: Failed to open pipe %s.\n",
                      op_ipc_name.get_value().c_str());
            }
        }
#endif
        if (!ipc_pipe.maximize_buffer())
            NOTIFY(1, "Failed to maximize pipe buffer: performance may suffer.\n");
    }

    if (op_offline.get_value() &&
        !func_trace_init(append_marker_seg_base, file_ops_func.write_file,
                         funclist_file)) {
        FATAL("Failed to initialized function tracing.\n");
    }

    /* We need an extra for -L0I_filter and -L0D_filter. */
    if (op_L0I_filter.get_value() || op_L0D_filter.get_value())
        ++ops.num_spill_slots;
    // We use the buf pointer reg plus 2 more in instrument_clean_call, so we want
    // 3 total: thus 1 more than the base.
    if (has_tracing_windows())
        ++ops.num_spill_slots;

    if (!drmgr_init() || !drutil_init() || drreg_init(&ops) != DRREG_SUCCESS ||
        !drx_init())
        DR_ASSERT(false);
    if (op_enable_drstatecmp.get_value()) {
        drstatecmp_options_t drstatecmp_ops = { NULL };
        if (drstatecmp_init(&drstatecmp_ops) != DRSTATECMP_SUCCESS) {
            DR_ASSERT(false);
        }
    }

    /* register events */
    dr_register_exit_event(event_exit);
#ifdef UNIX
    dr_register_fork_init_event(fork_init);
#endif
    attached_midway = dr_register_post_attach_event(event_post_attach);
    dr_register_pre_detach_event(event_pre_detach);

    /* We need our thread exit event to run *before* drmodtrack's as we may
     * need to translate physical addresses for the thread's final buffer.
     */
    drmgr_priority_t pri_thread_exit = { sizeof(drmgr_priority_t), "", nullptr, nullptr,
                                         -100 };
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event_ex(event_thread_exit, &pri_thread_exit))
        DR_ASSERT(false);

    instrumentation_init();

    trace_buf_size = instru->sizeof_entry() * MAX_NUM_ENTRIES;

    /* The redzone needs to hold one bb's worth of data, until we
     * reach the clean call at the bottom of the bb that dumps the
     * buffer if full.  We leave room for each of the maximum count of
     * instructions accessing memory once, which is fairly
     * pathological as by default that's 256 memrefs for one bb.  We double
     * it to ensure we cover skipping clean calls for sthg like strex.
     * We also check here that the max_bb_instrs can fit in the instr_count
     * bitfield in offline_entry_t.
     */
    uint64 max_bb_instrs;
    if (!dr_get_integer_option("max_bb_instrs", &max_bb_instrs))
        max_bb_instrs = 256; /* current default */
    DR_ASSERT(max_bb_instrs < uint64(1) << PC_INSTR_COUNT_BITS);
    redzone_size = instru->sizeof_entry() * (size_t)max_bb_instrs * 2;

    max_buf_size = ALIGN_FORWARD(trace_buf_size + redzone_size, dr_page_size());
    /* Mark any padding as redzone as well */
    redzone_size = max_buf_size - trace_buf_size;
    /* Append a throwaway header to get its size. */
    buf_hdr_slots_size = append_unit_header(
        NULL /*no TLS yet*/, buf, 0 /*doesn't matter*/, has_tracing_windows() ? 0 : -1);
    DR_ASSERT(BUFFER_SIZE_BYTES(buf) >= buf_hdr_slots_size);

    client_id = id;
    mutex = dr_mutex_create();

    tls_idx = drmgr_register_tls_field();
    DR_ASSERT(tls_idx != -1);
    /* The TLS field provided by DR cannot be directly accessed from the code cache.
     * For better performance, we allocate raw TLS so that we can directly
     * access and update it with a single instruction.
     */
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, MEMTRACE_TLS_COUNT, 0))
        DR_ASSERT(false);

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "drcachesim client initializing\n");

    init_io();

    if (op_max_global_trace_refs.get_value() > 0) {
        /* We need the same is-buffer-zero checks in the instrumentation. */
        thread_filtering_enabled = true;
    }

    if (op_use_physical.get_value() && !physaddr_t::global_init())
        FATAL("Unable to open pagemap for physical addresses: check privileges.\n");

#ifdef BUILD_PT_TRACER
    if (op_offline.get_value() && op_enable_kernel_tracing.get_value()) {
        if (!drpttracer_init())
            FATAL("Failed to initialize drpttracer.\n");
    }
#endif
}

} // namespace drmemtrace
} // namespace dynamorio

/***************************************************************************
 * Outside of namespace.
 */

/* To support statically linked multiple clients, we add drmemtrace_client_main
 * as the real client init function and make dr_client_main a weak symbol.
 * We could also use alias to link dr_client_main to drmemtrace_client_main.
 * A simple call won't add too much overhead, and works both in Windows and Linux.
 * To automate the process and minimize the code change, we should investigate the
 * approach that uses command-line link option to alias two symbols.
 */
DR_EXPORT WEAK void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dynamorio::drmemtrace::drmemtrace_client_main(id, argc, argv);
}
