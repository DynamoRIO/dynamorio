/* ******************************************************************************
 * Copyright (c) 2011-2015 Google, Inc.  All rights reserved.
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
 * Based on the memtrace_opt.c sample.
 * FIXME i#1703: add in more optimizations to improve performance.
 * FIXME i#1703: perhaps refactor and split up to make it more
 * modular.
 */

#include <stddef.h> /* for offsetof */
#include <string.h>
#include <limits.h> /* for INT_MAX/INT_MIN */
#include <string>
#include "dr_api.h"
#include "drmgr.h"
#include "drutil.h"
#include "droption.h"
#include "../common/trace_entry.h"
#include "../common/named_pipe.h"
#include "../common/options.h"

// XXX: share these instead of duplicating
#define BUFFER_SIZE_BYTES(buf)      sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf)   (BUFFER_SIZE_BYTES(buf) / sizeof((buf)[0]))
#define BUFFER_LAST_ELEMENT(buf)    (buf)[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf)  BUFFER_LAST_ELEMENT(buf) = 0

#define NOTIFY(level, fmt, ...) do {          \
    if (verbose.get_value() >= (level))           \
        dr_fprintf(STDERR, fmt, __VA_ARGS__); \
} while (0)

/* Max number of entries a buffer can have. It should be big enough
 * to hold all entries between clean calls.
 */
// XXX i#1703: use an option instead.
#define MAX_NUM_ENTRIES 4096
/* The buffer size for holding trace entries. */
#define TRACE_BUF_SIZE (sizeof(trace_entry_t) * MAX_NUM_ENTRIES)
/* The redzone is allocated right after the trace buffer.
 * We fill the redzone with sentinel value to detect when the redzone
 * is reached, i.e., when the trace buffer is full.
 */
#define REDZONE_SIZE (sizeof(trace_entry_t) * MAX_NUM_ENTRIES)
#define MAX_BUF_SIZE (TRACE_BUF_SIZE + REDZONE_SIZE)

/* thread private buffer and counter */
typedef struct {
    byte *seg_base;
    trace_entry_t *buf_base;
    uint64 num_refs;
} per_thread_t;

/* per bb user data during instrumentation */
typedef struct {
    bool clean_call_inserted;
    app_pc last_app_pc;
} user_data_t;

/* we write to a single global pipe */
static named_pipe_t ipc_pipe;

static client_id_t client_id;
static void  *mutex;    /* for multithread support */
static uint64 num_refs; /* keep a global memory reference count */

static dr_spill_slot_t slot_ptr = SPILL_SLOT_2; /* TLS slot for reg_ptr */
static dr_spill_slot_t slot_tmp = SPILL_SLOT_3; /* TLS slot for reg_tmp/reg_addr */

/* Allocated TLS slot offsets */
enum {
    MEMTRACE_TLS_OFFS_BUF_PTR,
    MEMTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};
static reg_id_t tls_seg;
static uint     tls_offs;
static int      tls_idx;
#define TLS_SLOT(tls_base, enum_val) (void **)((byte *)(tls_base)+tls_offs+(enum_val))
#define BUF_PTR(tls_base) *(trace_entry_t **)TLS_SLOT(tls_base, MEMTRACE_TLS_OFFS_BUF_PTR)
/* We leave a slot at the start so we can easily insert a header entry */
#define BUF_HDR_SLOTS 1

#define MINSERT instrlist_meta_preinsert

static inline void
init_thread_entry(void *drcontext, trace_entry_t *entry)
{
    entry->type = TRACE_TYPE_THREAD;
    entry->size = sizeof(thread_id_t);
    entry->addr = (addr_t) dr_get_thread_id(drcontext);
}

static void
memtrace(void *drcontext)
{
    per_thread_t *data = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    trace_entry_t *mem_ref, *buf_ptr;
    size_t towrite;

    buf_ptr = BUF_PTR(data->seg_base);
    /* The initial slot is left empty for the thread entry, which we add here */
    init_thread_entry(drcontext, data->buf_base);

    for (mem_ref = (trace_entry_t *)data->buf_base; mem_ref < buf_ptr; mem_ref++) {
        // FIXME i#1703: convert from virtual to physical if requested and avail
        data->num_refs++;
#ifdef VERBOSE // XXX: add a runtime option for this?
        dr_printf("SEND: type=%d, sz=%d, addr=%p\n", mem_ref->type, mem_ref->size,
                  mem_ref->addr);
#endif
    }
    towrite = (byte *)buf_ptr - (byte *)data->buf_base;

    // FIXME i#1703: split up to ensure atomic if > PIPE_BUF.
    // When we split, ensure we re-emit any headers (like thread id) after the
    // split and that we don't split in the middle of an instr fetch-memref
    // sequence or a thread id-process id sequence.
    if (ipc_pipe.write((void *)data->buf_base, towrite) < (ssize_t)towrite)
        DR_ASSERT(false);

    /* clear trace buffer */
    memset(data->buf_base, 0, REDZONE_SIZE);
    if (towrite > REDZONE_SIZE) {
        /* set sentinel (non-zero) value in redzone */
        memset((byte *)data->buf_base + REDZONE_SIZE, -1, towrite - REDZONE_SIZE);
    }
    BUF_PTR(data->seg_base) = data->buf_base + BUF_HDR_SLOTS;
}

/* clean_call sends the memory reference info to the simulator */
static void
clean_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    memtrace(drcontext);
}

static void
insert_load_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where,
                    reg_id_t reg_ptr)
{
    dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                           tls_offs + MEMTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
}

static void
insert_update_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, int adjust)
{
    MINSERT(ilist, where,
            XINST_CREATE_add(drcontext,
                             opnd_create_reg(reg_ptr),
                             OPND_CREATE_INT16(adjust)));
    dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                            tls_offs + MEMTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
}

static void
insert_save_type_and_size(void *drcontext, instrlist_t *ilist, instr_t *where,
                          reg_id_t base, reg_id_t scratch, ushort type, ushort size,
                          int adjust)
{
    int disp;
    if (offsetof(trace_entry_t, type) + 2 != offsetof(trace_entry_t, size)) {
        /* there is padding between type and size, so save them separately */
        disp = adjust + offsetof(trace_entry_t, type);
        scratch = reg_resize_to_opsz(scratch, OPSZ_2);
        /* save type */
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext,
                                      opnd_create_reg(scratch),
                                      OPND_CREATE_INT16(type)));
        MINSERT(ilist, where,
                XINST_CREATE_store_2bytes(drcontext,
                                          OPND_CREATE_MEM16(base, disp),
                                          opnd_create_reg(scratch)));
        /* save size */
        disp = adjust + offsetof(trace_entry_t, size);
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext,
                                      opnd_create_reg(scratch),
                                      OPND_CREATE_INT16(size)));
        MINSERT(ilist, where,
                XINST_CREATE_store_2bytes(drcontext,
                                          OPND_CREATE_MEM16(base, disp),
                                          opnd_create_reg(scratch)));
    } else {
        /* no padding, save type and size together */
        disp = adjust + offsetof(trace_entry_t, type);
#ifdef X86
        MINSERT(ilist, where,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM32(base, disp),
                                    OPND_CREATE_INT32(type | (size << 16))));
#elif defined(ARM)
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext,
                                      opnd_create_reg(scratch),
                                      OPND_CREATE_INT(type)));
        MINSERT(ilist, where,
                INSTR_CREATE_movt(drcontext,
                                  opnd_create_reg(scratch),
                                  OPND_CREATE_INT(size)));
        MINSERT(ilist, where,
                XINST_CREATE_store(drcontext,
                                   OPND_CREATE_MEM32(base, disp),
                                   opnd_create_reg(scratch)));
#endif
    }
}

static void
insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where,
               reg_id_t base, reg_id_t scratch, app_pc pc, int adjust)
{
    int disp = adjust + offsetof(trace_entry_t, addr);
#ifdef X86
    ptr_int_t val = (ptr_int_t)pc;
    MINSERT(ilist, where,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM32(base, disp),
                                OPND_CREATE_INT32((int)val)));
# ifdef X64
    if (val > INT_MAX || val < INT_MIN) {
        MINSERT(ilist, where,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM32(base, disp + 4),
                                    OPND_CREATE_INT32((int)(val >> 32))));
    }
# endif
#elif defined(ARM)
    instr_t *mov1, *mov2;
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)pc,
                                     opnd_create_reg(scratch),
                                     ilist, where, &mov1, &mov2);
    DR_ASSERT(mov1 != NULL);
    instr_set_meta(mov1);
    if (mov2 != NULL)
        instr_set_meta(mov2);
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext,
                               OPND_CREATE_MEMPTR(base, disp),
                               opnd_create_reg(scratch)));
#endif
}

static void
insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                 opnd_t ref, reg_id_t reg_ptr, reg_id_t reg_addr, int adjust)
{
    bool ok;
    int disp = adjust + offsetof(trace_entry_t, addr);
    if (opnd_uses_reg(ref, reg_ptr))
        dr_restore_reg(drcontext, ilist, where, reg_ptr, slot_ptr);
    if (opnd_uses_reg(ref, reg_addr))
        dr_restore_reg(drcontext, ilist, where, reg_addr, slot_tmp);
    /* we use reg_ptr as scratch to get addr */
    ok = drutil_insert_get_mem_addr(drcontext, ilist, where, ref, reg_addr, reg_ptr);
    DR_ASSERT(ok);
    insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext,
                               OPND_CREATE_MEMPTR(reg_ptr, disp),
                               opnd_create_reg(reg_addr)));
}

/* insert inline code to add an instruction entry into the buffer */
static void
instrument_instr(void *drcontext, instrlist_t *ilist, instr_t *where,
                 reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust)
{
    insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                              TRACE_TYPE_INSTR,
                              (ushort)instr_length(drcontext, where), adjust);
    insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp,
                   instr_get_app_pc(where), adjust);
}

/* insert inline code to add a memory reference info entry into the buffer */
static void
instrument_mem(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t ref,
               bool write, reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust)
{
    insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                              write ? TRACE_TYPE_WRITE : TRACE_TYPE_READ,
                              (ushort)drutil_opnd_mem_size_in_bytes(ref, where), adjust);
    insert_save_addr(drcontext, ilist, where, ref, reg_ptr, reg_tmp, adjust);
}

/* We insert code to read from trace buffer and check whether the redzone
 * is reached. If redzone is reached, the clean call will be called.
 */
static void
instrument_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, reg_id_t reg_tmp)
{
    instr_t *label = INSTR_CREATE_label(drcontext);
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext,
                              opnd_create_reg(reg_ptr),
                              OPND_CREATE_MEMPTR(reg_ptr, 0)));
#ifdef X86
    DR_ASSERT(reg_ptr == DR_REG_XCX);
    MINSERT(ilist, where,
            INSTR_CREATE_jecxz(drcontext, opnd_create_instr(label)));
#elif defined(ARM)
    if (dr_get_isa_mode(drcontext) == DR_ISA_ARM_THUMB) {
        instr_t *noskip = INSTR_CREATE_label(drcontext);
        /* XXX: clean call is too long to use cbz to skip. */
        MINSERT(ilist, where,
                INSTR_CREATE_cbnz(drcontext,
                                 opnd_create_instr(noskip),
                                 opnd_create_reg(reg_ptr)));
        MINSERT(ilist, where,
                XINST_CREATE_jump(drcontext,
                                  opnd_create_instr(label)));
        MINSERT(ilist, where, noskip);
    } else {
        /* There is no jecxz/cbz like instr on ARM-A32 mode, so we have to
         * save aflags to reg_tmp before check.
         * XXX optimization: use drreg to avoid aflags save/restore.
         */
        dr_save_arith_flags_to_reg(drcontext, ilist, where, reg_tmp);
        MINSERT(ilist, where,
                INSTR_CREATE_cmp(drcontext,
                                 opnd_create_reg(reg_ptr),
                                 OPND_CREATE_INT(0)));
        MINSERT(ilist, where,
                instr_set_predicate(XINST_CREATE_jump(drcontext,
                                                      opnd_create_instr(label)),
                                    DR_PRED_EQ));
    }
#endif
    dr_insert_clean_call(drcontext, ilist, where, (void *)clean_call, false, 0);
    MINSERT(ilist, where, label);
#ifdef ARM
    if (dr_get_isa_mode(drcontext) == DR_ISA_ARM_A32)
        dr_restore_arith_flags_from_reg(drcontext, ilist, where, reg_tmp);
#endif
}

/* For each memory reference app instr, we insert inline code to fill the buffer
 * with an instruction entry and memory reference entries.
 */
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      instr_t *instr, bool for_trace,
                      bool translating, void *user_data)
{
    reg_id_t reg_ptr = IF_X86_ELSE(DR_REG_XCX, DR_REG_R1);
    reg_id_t reg_tmp = IF_X86_ELSE(DR_REG_XBX, DR_REG_R2);
    int i, adjust = 0;
    user_data_t *ud = (user_data_t *) user_data;

    if (!instr_is_app(instr) ||
        /* Skip identical app pc, which happens with rep str expansion.
         * XXX: the expansion means our instr fetch trace is not perfect,
         * but we live with having the wrong instr length.
         */
        ud->last_app_pc == instr_get_app_pc(instr))
        return DR_EMIT_DEFAULT;

    /* opt: save/restore reg per instr instead of per entry */
    /* We need two scratch registers */
    dr_save_reg(drcontext, bb, instr, reg_ptr, slot_ptr);
    dr_save_reg(drcontext, bb, instr, reg_tmp, slot_tmp);
    /* load buf ptr into reg_ptr */
    insert_load_buf_ptr(drcontext, bb, instr, reg_ptr);

    /* Instruction entry for instr fetch trace.  This does double-duty by
     * also providing the PC for subsequent data ref entries.
     */
    /* XXX i#1703: we may want to put the instr fetch under an option, in
     * case the user only cares about data references.
     * Note that in that case we may want to still provide the PC for
     * memory references, and it may be better to add a PC field to
     * trace_entry_t than require a separate instr entry for every memref
     * instr (if average # of memrefs per instr is < 2, PC field is better).
     */
    instrument_instr(drcontext, bb, instr, reg_ptr, reg_tmp, adjust);
    adjust += sizeof(trace_entry_t);
    ud->last_app_pc = instr_get_app_pc(instr);

    if (instr_reads_memory(instr) || instr_writes_memory(instr)) {
        /* insert code to add an entry for each memory reference opnd */
        for (i = 0; i < instr_num_srcs(instr); i++) {
            if (opnd_is_memory_reference(instr_get_src(instr, i))) {
                instrument_mem(drcontext, bb, instr, instr_get_src(instr, i),
                               false, reg_ptr, reg_tmp, adjust);
                adjust += sizeof(trace_entry_t);
            }
        }

        for (i = 0; i < instr_num_dsts(instr); i++) {
            if (opnd_is_memory_reference(instr_get_dst(instr, i))) {
                instrument_mem(drcontext, bb, instr, instr_get_dst(instr, i),
                               true, reg_ptr, reg_tmp, adjust);
                adjust += sizeof(trace_entry_t);
            }
        }
    }

    /* opt: update buf ptr once per instr instead of per entry */
    insert_update_buf_ptr(drcontext, bb, instr, reg_ptr, adjust);

    /* Insert code to call clean_call for processing the buffer.
     * We restore the registers after the clean call, which should be ok
     * assuming the clean call does not need the two register values.
     */
    if (!ud->clean_call_inserted
        /* XXX i#1702: it is ok to skip a few clean calls on predicated instructions,
         * since the buffer will be dumped later by other clean calls.
         */
        IF_ARM(&& !instr_is_predicated(instr))
        /* FIXME i#1698: there are constraints for code between ldrex/strex pairs,
         * so we minimize the instrumentation in between by skipping the clean call.
         * However, there is still a chance that the instrumentation code may clear the
         * exclusive monitor state.
         */
        IF_ARM(&& !instr_is_exclusive_store(instr))) {
        instrument_clean_call(drcontext, bb, instr, reg_ptr, reg_tmp);
        ud->clean_call_inserted = true;
    }

    /* restore scratch registers */
    dr_restore_reg(drcontext, bb, instr, reg_ptr, slot_ptr);
    dr_restore_reg(drcontext, bb, instr, reg_tmp, slot_tmp);
    return DR_EMIT_DEFAULT;
}

/* We transform string loops into regular loops so we can more easily
 * monitor every memory reference they make.
 */
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb,
                 bool for_trace, bool translating, OUT void **user_data)
{
    user_data_t *data = (user_data_t *) dr_thread_alloc(drcontext, sizeof(user_data_t));
    data->clean_call_inserted = false;
    data->last_app_pc = NULL;
    *user_data = (void *)data;
    if (!drutil_expand_rep_string(drcontext, bb)) {
        DR_ASSERT(false);
        /* in release build, carry on: we'll just miss per-iter refs */
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating, void *user_data)
{
    /* do nothing */
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_instru2instru(void *drcontext, void *tag, instrlist_t *bb,
                       bool for_trace, bool translating,
                       void *user_data)
{
    dr_thread_free(drcontext, user_data, sizeof(user_data_t));
    return DR_EMIT_DEFAULT;
}

static void
event_thread_init(void *drcontext)
{
    trace_entry_t pid_info[2];
    per_thread_t *data = (per_thread_t *)
        dr_thread_alloc(drcontext, sizeof(per_thread_t));
    DR_ASSERT(data != NULL);
    drmgr_set_tls_field(drcontext, tls_idx, data);

    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    data->seg_base = (byte *) dr_get_dr_segment_base(tls_seg);
    data->buf_base = (trace_entry_t *)
        dr_raw_mem_alloc(MAX_BUF_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    DR_ASSERT(data->seg_base != NULL && data->buf_base != NULL);
    /* clear trace buffer */
    memset(data->buf_base, 0, REDZONE_SIZE);
    /* set sentinel (non-zero) value in redzone */
    memset((byte *)data->buf_base + REDZONE_SIZE, -1, MAX_BUF_SIZE - REDZONE_SIZE);
    /* put buf_base to TLS plus header slots as starting buf_ptr */
    BUF_PTR(data->seg_base) = data->buf_base + BUF_HDR_SLOTS;

    /* pass pid and tid to the simulator to register current thread */
    init_thread_entry(drcontext, &pid_info[0]);
    pid_info[1].type = TRACE_TYPE_PID;
    pid_info[1].size = sizeof(process_id_t);
    pid_info[1].addr = (addr_t) dr_get_process_id();
    if (ipc_pipe.write((void *)pid_info, sizeof(pid_info)) < (ssize_t)sizeof(pid_info))
        DR_ASSERT(false);
    data->num_refs = 0;
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    memtrace(drcontext);
    dr_mutex_lock(mutex);
    num_refs += data->num_refs;
    dr_mutex_unlock(mutex);
    dr_raw_mem_free(data->buf_base, MAX_BUF_SIZE);
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static void
event_exit(void)
{
    dr_log(NULL, LOG_ALL, 1, "drcachesim num refs seen: "SZFMT"\n", num_refs);
    ipc_pipe.close();
    if (!dr_raw_tls_cfree(tls_offs, MEMTRACE_TLS_COUNT))
        DR_ASSERT(false);

    if (!drmgr_unregister_tls_field(tls_idx) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        !drmgr_unregister_bb_instrumentation_ex_event(event_bb_app2app,
                                                      event_bb_analysis,
                                                      event_app_instruction,
                                                      event_bb_instru2instru))
        DR_ASSERT(false);

    dr_mutex_destroy(mutex);
    drutil_exit();
    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_set_client_name("DynamoRIO Cache Simulator Tracer",
                       "http://dynamorio.org/issues");

    std::string parse_err;
    if (!dr_parse_options(id, &parse_err, NULL)) {
        NOTIFY(0, "Usage error: %s\nUsage:\n%s", parse_err.c_str(),
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }
    if (ipc_name.get_value().empty()) {
        NOTIFY(0, "Usage error: ipc name is required\nUsage:\n%s",
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }

    if (!ipc_pipe.set_name(ipc_name.get_value().c_str()))
        DR_ASSERT(false);
    /* we want an isolated fd so we don't use ipc_pipe.open_for_write() */
    int fd = dr_open_file(ipc_pipe.get_pipe_path().c_str(), DR_FILE_WRITE_ONLY);
    DR_ASSERT(fd != INVALID_FILE);
    if (!ipc_pipe.set_fd(fd))
        DR_ASSERT(false);
    if (!ipc_pipe.maximize_buffer())
        DR_ASSERT(false);

    if (!drmgr_init() || !drutil_init())
        DR_ASSERT(false);

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_bb_instrumentation_ex_event(event_bb_app2app,
                                                    event_bb_analysis,
                                                    event_app_instruction,
                                                    event_bb_instru2instru,
                                                    NULL))
        DR_ASSERT(false);

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
    dr_log(NULL, LOG_ALL, 1, "drcachesim client initializing\n");
}
