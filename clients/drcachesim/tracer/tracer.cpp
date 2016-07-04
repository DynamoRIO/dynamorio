/* ******************************************************************************
 * Copyright (c) 2011-2016 Google, Inc.  All rights reserved.
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
 * XXX i#1703: add in more optimizations to improve performance.
 * XXX i#1703: perhaps refactor and split up to make it more
 * modular.
 */

#include <stddef.h> /* for offsetof */
#include <string.h>
#include <limits.h> /* for INT_MAX/INT_MIN */
#include <string>
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "droption.h"
#include "physaddr.h"
#include "../common/trace_entry.h"
#include "../common/named_pipe.h"
#include "../common/options.h"

#ifdef ARM
# include "../../../core/unix/include/syscall_linux_arm.h" // for SYS_cacheflush
#endif

// XXX: share these instead of duplicating
#define BUFFER_SIZE_BYTES(buf)      sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf)   (BUFFER_SIZE_BYTES(buf) / sizeof((buf)[0]))
#define BUFFER_LAST_ELEMENT(buf)    (buf)[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf)  BUFFER_LAST_ELEMENT(buf) = 0

#define NOTIFY(level, ...) do {            \
    if (op_verbose.get_value() >= (level)) \
        dr_fprintf(STDERR, __VA_ARGS__);   \
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

#define MAX_NUM_DELAY_INSTRS 32
/* per bb user data during instrumentation */
typedef struct {
    app_pc last_app_pc;
    instr_t *strex;
    int num_delay_instrs;
    instr_t *delay_instrs[MAX_NUM_DELAY_INSTRS];
} user_data_t;

/* we write to a single global pipe */
static named_pipe_t ipc_pipe;

static client_id_t client_id;
static void  *mutex;    /* for multithread support */
static uint64 num_refs; /* keep a global memory reference count */

/* virtual to physical translation */
static bool have_phys;
static physaddr_t physaddr;

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
#define BUF_HDR_SLOTS_SIZE (BUF_HDR_SLOTS * sizeof(trace_entry_t))

#define MINSERT instrlist_meta_preinsert

static inline void
init_thread_entry(void *drcontext, trace_entry_t *entry)
{
    entry->type = TRACE_TYPE_THREAD;
    entry->size = sizeof(thread_id_t);
    entry->addr = (addr_t) dr_get_thread_id(drcontext);
}

static inline byte *
atomic_pipe_write(void *drcontext, byte *pipe_start, byte *pipe_end)
{
    ssize_t towrite = pipe_end - pipe_start;
    DR_ASSERT(towrite <= ipc_pipe.get_atomic_write_size() &&
              towrite > (ssize_t)BUF_HDR_SLOTS_SIZE);
    if (ipc_pipe.write((void *)pipe_start, towrite) < (ssize_t)towrite)
        DR_ASSERT(false);
    // Re-emit thread entry header
    DR_ASSERT(pipe_end - BUF_HDR_SLOTS_SIZE > pipe_start);
    pipe_start = pipe_end - BUF_HDR_SLOTS_SIZE;
    init_thread_entry(drcontext, (trace_entry_t *)pipe_start);
    return pipe_start;
}

static void
memtrace(void *drcontext)
{
    per_thread_t *data = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    trace_entry_t *mem_ref, *buf_ptr;
    byte *pipe_start, *pipe_end, *redzone;

    buf_ptr = BUF_PTR(data->seg_base);
    /* The initial slot is left empty for the thread entry, which we add here */
    init_thread_entry(drcontext, data->buf_base);
    pipe_start = (byte *)data->buf_base;
    pipe_end = pipe_start;

    for (mem_ref = data->buf_base + BUF_HDR_SLOTS; mem_ref < buf_ptr; mem_ref++) {
        data->num_refs++;
        if (have_phys && op_use_physical.get_value()) {
            if (mem_ref->type != TRACE_TYPE_THREAD &&
                mem_ref->type != TRACE_TYPE_THREAD_EXIT &&
                mem_ref->type != TRACE_TYPE_PID) {
                addr_t phys = physaddr.virtual2physical(mem_ref->addr);
                DR_ASSERT(mem_ref->type != TRACE_TYPE_INSTR_BUNDLE);
                if (phys != 0)
                    mem_ref->addr = phys;
                else {
                    // XXX i#1735: use virtual address and continue?
                    // There are cases the xl8 fail, e.g.,:
                    // - vsyscall/kernel page,
                    // - wild access (NULL or very large bogus address) by app
                    NOTIFY(1, "virtual2physical translation failure for "
                           "<%2d, %2d, " PFX">\n",
                           mem_ref->type, mem_ref->size, mem_ref->addr);
                }
            }
        }
        // Split up the buffer into multiple writes to ensure atomic pipe writes.
        // We can only split before TRACE_TYPE_INSTR, assuming only a few data
        // entries in between instr entries.
        if (mem_ref->type == TRACE_TYPE_INSTR) {
            if (((byte *)mem_ref - pipe_start) > ipc_pipe.get_atomic_write_size())
                pipe_start = atomic_pipe_write(drcontext, pipe_start, pipe_end);
            // Advance pipe_end pointer
            pipe_end = (byte *)mem_ref;
        }
    }
    // Write the rest to pipe
    // The last few entries (e.g., instr + refs) may exceed the atomic write size,
    // so we may need two writes.
    if (((byte *)buf_ptr - pipe_start) > ipc_pipe.get_atomic_write_size())
        pipe_start = atomic_pipe_write(drcontext, pipe_start, pipe_end);
    if (((byte *)buf_ptr - pipe_start) > (ssize_t)BUF_HDR_SLOTS_SIZE)
        atomic_pipe_write(drcontext, pipe_start, (byte *)buf_ptr);

    // Our instrumentation reads from buffer and skips the clean call if the
    // content is 0, so we need set zero in the trace buffer and set non-zero
    // in redzone.
    memset(data->buf_base, 0, TRACE_BUF_SIZE);
    redzone = (byte *)data->buf_base + TRACE_BUF_SIZE;
    if ((byte *)buf_ptr > redzone) {
        // Set sentinel (non-zero) value in redzone
        memset(redzone, -1, (byte *)buf_ptr - redzone);
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
                      reg_id_t reg_ptr, dr_pred_type_t pred, int adjust)
{
    instr_t *label = INSTR_CREATE_label(drcontext);
    MINSERT(ilist, where, label);
    MINSERT(ilist, where,
            XINST_CREATE_add(drcontext,
                             opnd_create_reg(reg_ptr),
                             OPND_CREATE_INT16(adjust)));
    dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                            tls_offs + MEMTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
#ifdef ARM // X86 does not support general predicated execution
    if (pred != DR_PRED_NONE) {
        instr_t *instr;
        for (instr  = instr_get_prev(where);
             instr != label;
             instr  = instr_get_prev(instr)) {
            DR_ASSERT(!instr_is_predicated(instr));
            instr_set_predicate(instr, pred);
        }
    }
#endif
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
#ifdef X86_32
    ptr_int_t val = (ptr_int_t)pc;
    MINSERT(ilist, where,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM32(base, disp),
                                OPND_CREATE_INT32((int)val)));
#else
    // For X86_64, we can't write the PC immed directly to memory and
    // skip the top half for a <4GB PC b/c if we're in the sentinel
    // region of the buffer we'll be leaving 0xffffffff in the top
    // half (i#1735).  Thus we go through a register on x86 (where we
    // can skip the top half), just like on ARM.
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
        drreg_get_app_value(drcontext, ilist, where, reg_ptr, reg_ptr);
    if (opnd_uses_reg(ref, reg_addr))
        drreg_get_app_value(drcontext, ilist, where, reg_addr, reg_addr);
    /* we use reg_ptr as scratch to get addr */
    ok = drutil_insert_get_mem_addr(drcontext, ilist, where, ref, reg_addr, reg_ptr);
    DR_ASSERT(ok);
    // drutil_insert_get_mem_addr may clobber reg_ptr, so we need reload reg_ptr
    insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext,
                               OPND_CREATE_MEMPTR(reg_ptr, disp),
                               opnd_create_reg(reg_addr)));
}

static unsigned short
instr_to_prefetch_type(instr_t *instr)
{
    int opcode = instr_get_opcode(instr);
    DR_ASSERT(instr_is_prefetch(instr));
    switch (opcode) {
#ifdef X86
    case OP_prefetcht0:
        return TRACE_TYPE_PREFETCHT0;
    case OP_prefetcht1:
        return TRACE_TYPE_PREFETCHT1;
    case OP_prefetcht2:
        return TRACE_TYPE_PREFETCHT2;
    case OP_prefetchnta:
        return TRACE_TYPE_PREFETCHNTA;
#endif
#ifdef ARM
    case OP_pld:
        return TRACE_TYPE_PREFETCH_READ;
    case OP_pldw:
        return TRACE_TYPE_PREFETCH_WRITE;
    case OP_pli:
        return TRACE_TYPE_PREFETCH_INSTR;
#endif
    default:
        return TRACE_TYPE_PREFETCH;
    }
}

/* insert inline code to add an instruction entry into the buffer */
static int
instrument_instr(void *drcontext, instrlist_t *ilist,
                 instr_t *app, instr_t *where,
                 reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust)
{
    insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                              TRACE_TYPE_INSTR,
                              (ushort)instr_length(drcontext, app), adjust);
    insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp,
                   instr_get_app_pc(app), adjust);
    return (adjust + sizeof(trace_entry_t));
}

static int
instrument_trace_entry(void *drcontext, instrlist_t *ilist,
                       const trace_entry_t &entry, instr_t *where,
                       reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust)
{
    insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                              entry.type, entry.size, adjust);
    insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp,
                   (app_pc)entry.addr, adjust);
    return (adjust + sizeof(trace_entry_t));
}

static int
instrument_delay_instrs(void *drcontext, instrlist_t *ilist,
                        user_data_t *ud, instr_t *where,
                        reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust)
{
    trace_entry_t entry;
    int i;

    // Instrument to add an INSTR_TRACE entry
    adjust = instrument_instr(drcontext, ilist, ud->delay_instrs[0], where,
                              reg_ptr, reg_tmp, adjust);
    if (have_phys && op_use_physical.get_value()) {
        // No instr bundle if physical-2-virtual since instr bundle may
        // cross page bundary.
        for (i = 1; i < ud->num_delay_instrs; i++) {
            adjust = instrument_instr(drcontext, ilist, ud->delay_instrs[i],
                                      where, reg_ptr, reg_tmp, adjust);
        }
    } else {
        // Create and instrument for INSTR_BUNDLE
        entry.type = TRACE_TYPE_INSTR_BUNDLE;
        entry.size = 0;
        for (i = 1; i < ud->num_delay_instrs; i++) {
            // Fill instr size into bundle entry
            entry.length[entry.size++] = instr_length(drcontext, ud->delay_instrs[i]);
            // Instrument to add an INSTR_BUNDLE entry if bundle is full or last instr
            if (entry.size == sizeof(entry.length) || i == ud->num_delay_instrs - 1) {
                adjust = instrument_trace_entry(drcontext, ilist, entry, where,
                                                reg_ptr, reg_tmp, adjust);
                entry.size = 0;
            }
        }
    }
    ud->num_delay_instrs = 0;
    return adjust;
}

static bool
instr_is_flush(instr_t *instr)
{
    // Assuming we won't see any privileged instructions.
#ifdef X86
    if (instr_get_opcode(instr) == OP_clflush)
        return true;
#endif
    return false;
}

/* insert inline code to add a memory reference info entry into the buffer */
static int
instrument_mem(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t ref,
               bool write, reg_id_t reg_ptr, reg_id_t reg_tmp,
               dr_pred_type_t pred, int adjust)
{
    ushort type = write ? TRACE_TYPE_WRITE : TRACE_TYPE_READ;
    ushort size = (ushort)drutil_opnd_mem_size_in_bytes(ref, where);
    instr_t *label = INSTR_CREATE_label(drcontext);
    MINSERT(ilist, where, label);
    // Special handling for prefetch instruction
    if (instr_is_prefetch(where)) {
        type = instr_to_prefetch_type(where);
        // Prefetch instruction may have zero sized mem reference.
        size = 1;
    } else if (instr_is_flush(where)) {
        // XXX: OP_clflush invalidates all levels of the processor cache
        // hierarchy (data and instruction)
        type = TRACE_TYPE_DATA_FLUSH;
    }
    insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                              type, size, adjust);
    insert_save_addr(drcontext, ilist, where, ref, reg_ptr, reg_tmp, adjust);
#ifdef ARM // X86 does not support general predicated execution
    if (pred != DR_PRED_NONE) {
        instr_t *instr;
        for (instr  = instr_get_prev(where);
             instr != label;
             instr  = instr_get_prev(instr)) {
            DR_ASSERT(!instr_is_predicated(instr));
            instr_set_predicate(instr, pred);
        }
    }
#endif
    return (adjust + sizeof(trace_entry_t));
}

/* We insert code to read from trace buffer and check whether the redzone
 * is reached. If redzone is reached, the clean call will be called.
 */
static void
instrument_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, reg_id_t reg_tmp)
{
    instr_t *skip_call = INSTR_CREATE_label(drcontext);
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext,
                              opnd_create_reg(reg_ptr),
                              OPND_CREATE_MEMPTR(reg_ptr, 0)));
#ifdef X86
    DR_ASSERT(reg_ptr == DR_REG_XCX);
    MINSERT(ilist, where,
            INSTR_CREATE_jecxz(drcontext, opnd_create_instr(skip_call)));
#elif defined(ARM)
    if (dr_get_isa_mode(drcontext) == DR_ISA_ARM_THUMB) {
        instr_t *noskip = INSTR_CREATE_label(drcontext);
        /* XXX: clean call is too long to use cbz to skip. */
        DR_ASSERT(reg_ptr <= DR_REG_R7); /* cbnz can't take r8+ */
        MINSERT(ilist, where,
                INSTR_CREATE_cbnz(drcontext,
                                 opnd_create_instr(noskip),
                                 opnd_create_reg(reg_ptr)));
        MINSERT(ilist, where,
                XINST_CREATE_jump(drcontext,
                                  opnd_create_instr(skip_call)));
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
                                                      opnd_create_instr(skip_call)),
                                    DR_PRED_EQ));
    }
#endif
    dr_insert_clean_call(drcontext, ilist, where, (void *)clean_call, false, 0);
    MINSERT(ilist, where, skip_call);
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
    int i, adjust = 0;
    user_data_t *ud = (user_data_t *) user_data;
    dr_pred_type_t pred;
    reg_id_t reg_ptr, reg_tmp = DR_REG_NULL;
    drvector_t rvec;

    if (!instr_is_app(instr) ||
        /* Skip identical app pc, which happens with rep str expansion.
         * XXX: the expansion means our instr fetch trace is not perfect,
         * but we live with having the wrong instr length.
         */
        ud->last_app_pc == instr_get_app_pc(instr))
        return DR_EMIT_DEFAULT;

    // FIXME i#1698: there are constraints for code between ldrex/strex pairs.
    // However there is no way to completely avoid the instrumentation in between,
    // so we reduce the instrumentation in between by moving strex instru
    // from before the strex to after the strex.
    if (ud->strex == NULL && instr_is_exclusive_store(instr)) {
        opnd_t dst = instr_get_dst(instr, 0);
        DR_ASSERT(opnd_is_base_disp(dst));
        // Assuming there are no consecutive strex instructions, otherwise we
        // will insert instrumentation code at the second strex instruction.
        if (!instr_writes_to_reg(instr, opnd_get_base(dst),
                                 DR_QUERY_INCLUDE_COND_DSTS)) {
            ud->strex = instr;
            ud->last_app_pc = instr_get_app_pc(instr);
        }
        return DR_EMIT_DEFAULT;
    }

    // Optimization: delay the simple instr trace instrumentation if possible
    if (!(instr_reads_memory(instr) ||instr_writes_memory(instr)) &&
        // Avoid dropping trailing instrs
        !drmgr_is_last_instr(drcontext, instr) &&
        // The delay instr buffer is not full.
        ud->num_delay_instrs < MAX_NUM_DELAY_INSTRS) {
        ud->delay_instrs[ud->num_delay_instrs++] = instr;
        return DR_EMIT_DEFAULT;
    }

    pred = instr_get_predicate(instr);
    /* opt: save/restore reg per instr instead of per entry */
    /* We need two scratch registers.
     * reg_ptr must be ECX or RCX for jecxz on x86, and must be <= r7 for cbnz on ARM.
     */
#ifdef X86
    drreg_init_and_fill_vector(&rvec, false);
    drreg_set_vector_entry(&rvec, DR_REG_XCX, true);
#else
    drreg_init_and_fill_vector(&rvec, false);
    for (reg_ptr = DR_REG_R0; reg_ptr <= DR_REG_R7; reg_ptr++)
        drreg_set_vector_entry(&rvec, reg_ptr, true);
#endif
    if (drreg_reserve_register(drcontext, bb, instr, &rvec, &reg_ptr) != DRREG_SUCCESS ||
        drreg_reserve_register(drcontext, bb, instr, NULL, &reg_tmp) != DRREG_SUCCESS) {
        // We can't recover.
        NOTIFY(0, "Fatal error: failed to reserve scratch registers");
        dr_abort();
    }
    drvector_delete(&rvec);
    /* load buf ptr into reg_ptr */
    insert_load_buf_ptr(drcontext, bb, instr, reg_ptr);

    if (ud->num_delay_instrs != 0) {
        adjust = instrument_delay_instrs(drcontext, bb, ud, instr,
                                         reg_ptr, reg_tmp, adjust);
    }

    if (ud->strex != NULL) {
        DR_ASSERT(instr_is_exclusive_store(ud->strex));
        adjust = instrument_instr(drcontext, bb, ud->strex, instr,
                                  reg_ptr, reg_tmp, adjust);
        adjust = instrument_mem(drcontext, bb, instr,
                                instr_get_dst(ud->strex, 0),
                                true, reg_ptr, reg_tmp,
                                instr_get_predicate(ud->strex), adjust);
        ud->strex = NULL;
    }

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
    adjust = instrument_instr(drcontext, bb, instr, instr, reg_ptr, reg_tmp, adjust);
    ud->last_app_pc = instr_get_app_pc(instr);

    // FIXME i#1703: add OP_clflush handling for cache flush on X86
    if (instr_reads_memory(instr) || instr_writes_memory(instr)) {
        if (pred != DR_PRED_NONE) {
            // Update buffer ptr and reset adjust to 0, because
            // we may not execute the inserted code below.
            insert_update_buf_ptr(drcontext, bb, instr, reg_ptr,
                                  DR_PRED_NONE, adjust);
            adjust = 0;
        }

        /* insert code to add an entry for each memory reference opnd */
        for (i = 0; i < instr_num_srcs(instr); i++) {
            if (opnd_is_memory_reference(instr_get_src(instr, i))) {
                adjust = instrument_mem(drcontext, bb, instr,
                                        instr_get_src(instr, i),
                                        false, reg_ptr, reg_tmp,
                                        pred, adjust);
            }
        }

        for (i = 0; i < instr_num_dsts(instr); i++) {
            if (opnd_is_memory_reference(instr_get_dst(instr, i))) {
                adjust = instrument_mem(drcontext, bb, instr,
                                        instr_get_dst(instr, i),
                                        true, reg_ptr, reg_tmp,
                                        pred, adjust);
            }
        }
        insert_update_buf_ptr(drcontext, bb, instr, reg_ptr, pred, adjust);
    } else
        insert_update_buf_ptr(drcontext, bb, instr, reg_ptr, DR_PRED_NONE, adjust);

    /* Insert code to call clean_call for processing the buffer.
     * We restore the registers after the clean call, which should be ok
     * assuming the clean call does not need the two register values.
     */
    if (drmgr_is_last_instr(drcontext, instr))
        instrument_clean_call(drcontext, bb, instr, reg_ptr, reg_tmp);

    /* restore scratch registers */
    if (drreg_unreserve_register(drcontext, bb, instr, reg_ptr) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, bb, instr, reg_tmp) != DRREG_SUCCESS)
        DR_ASSERT(false);
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
    data->last_app_pc = NULL;
    data->strex = NULL;
    data->num_delay_instrs = 0;
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

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
#ifdef ARM
    // On Linux ARM, cacheflush syscall takes 3 params: start, end, and 0.
    if (sysnum == SYS_cacheflush) {
        per_thread_t *data;
        trace_entry_t *buf_ptr;
        addr_t start = (addr_t)dr_syscall_get_param(drcontext, 0);
        addr_t end   = (addr_t)dr_syscall_get_param(drcontext, 1);
        data = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
        if (end > start) {
            buf_ptr = BUF_PTR(data->seg_base);
            buf_ptr->type = TRACE_TYPE_INSTR_FLUSH;
            buf_ptr->addr = start;
            buf_ptr->size = ((end - start) <= USHRT_MAX) ? (end - start) : 0;
            // if flush size is too large, we use two entries for start/end
            if (buf_ptr->size == 0) {
                ++buf_ptr;
                buf_ptr->type = TRACE_TYPE_INSTR_FLUSH_END;
                buf_ptr->addr = end;
                buf_ptr->size = 0;
            }
            BUF_PTR(data->seg_base) = ++buf_ptr;
        }
    }
#endif
    memtrace(drcontext);
    return true;
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
    memset(data->buf_base, 0, TRACE_BUF_SIZE);
    /* set sentinel (non-zero) value in redzone */
    memset((byte *)data->buf_base + TRACE_BUF_SIZE, -1, REDZONE_SIZE);
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

    /* let the simulator know this thread has exited */
    trace_entry_t *buf_ptr = BUF_PTR(data->seg_base);
    buf_ptr->type = TRACE_TYPE_THREAD_EXIT;
    buf_ptr->size = sizeof(thread_id_t);
    buf_ptr->addr = (addr_t) dr_get_thread_id(drcontext);
    BUF_PTR(data->seg_base) = ++buf_ptr;

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
    dr_log(NULL, LOG_ALL, 1, "drcachesim num refs seen: " SZFMT"\n", num_refs);
    ipc_pipe.close();
    if (!dr_raw_tls_cfree(tls_offs, MEMTRACE_TLS_COUNT))
        DR_ASSERT(false);

    if (!drmgr_unregister_tls_field(tls_idx) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        !drmgr_unregister_pre_syscall_event(event_pre_syscall) ||
        !drmgr_unregister_bb_instrumentation_ex_event(event_bb_app2app,
                                                      event_bb_analysis,
                                                      event_app_instruction,
                                                      event_bb_instru2instru) ||
        drreg_exit() != DRREG_SUCCESS)
        DR_ASSERT(false);

    dr_mutex_destroy(mutex);
    drutil_exit();
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    /* We need 2 reg slots beyond drreg's eflags slots => 3 slots */
    drreg_options_t ops = {sizeof(ops), 3, false};

    dr_set_client_name("DynamoRIO Cache Simulator Tracer",
                       "http://dynamorio.org/issues");

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv,
                                       &parse_err, NULL)) {
        NOTIFY(0, "Usage error: %s\nUsage:\n%s", parse_err.c_str(),
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }
    if (op_ipc_name.get_value().empty()) {
        NOTIFY(0, "Usage error: ipc name is required\nUsage:\n%s",
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }

    if (!ipc_pipe.set_name(op_ipc_name.get_value().c_str()))
        DR_ASSERT(false);
    /* we want an isolated fd so we don't use ipc_pipe.open_for_write() */
    int fd = dr_open_file(ipc_pipe.get_pipe_path().c_str(), DR_FILE_WRITE_ONLY);
    DR_ASSERT(fd != INVALID_FILE);
    if (!ipc_pipe.set_fd(fd))
        DR_ASSERT(false);
    if (!ipc_pipe.maximize_buffer())
        NOTIFY(1, "Failed to maximize pipe buffer: performance may suffer.\n");

    if (!drmgr_init() || !drutil_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_pre_syscall_event(event_pre_syscall) ||
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

    if (op_use_physical.get_value()) {
        have_phys = physaddr.init();
        if (!have_phys)
            NOTIFY(0, "Unable to open pagemap: using virtual addresses.\n");
    }
}
