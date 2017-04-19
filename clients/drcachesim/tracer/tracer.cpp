/* ******************************************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
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
 * XXX i#1703: perhaps refactor and split up to make it more
 * modular.
 */

#include <string.h>
#include <string>
#include "dr_api.h"
#include "drmgr.h"
#include "drmemtrace.h"
#include "drreg.h"
#include "drutil.h"
#include "drx.h"
#include "droption.h"
#include "instru.h"
#include "raw2trace.h"
#include "physaddr.h"
#include "../common/trace_entry.h"
#include "../common/named_pipe.h"
#include "../common/options.h"
#include "../common/utils.h"

#ifdef ARM
# include "../../../core/unix/include/syscall_linux_arm.h" // for SYS_cacheflush
#endif

/* Make sure we export function name as the symbol name without mangling. */
#ifdef __cplusplus
extern "C" {
DR_EXPORT void drmemtrace_client_main(client_id_t id, int argc, const char *argv[]);
}
#endif

#define NOTIFY(level, ...) do {            \
    if (op_verbose.get_value() >= (level)) \
        dr_fprintf(STDERR, __VA_ARGS__);   \
} while (0)

static char logsubdir[MAXIMUM_PATH];
static file_t module_file;

/* Max number of entries a buffer can have. It should be big enough
 * to hold all entries between clean calls.
 */
// XXX i#1703: use an option instead.
#define MAX_NUM_ENTRIES 4096
/* The buffer size for holding trace entries. */
static size_t trace_buf_size;
/* The redzone is allocated right after the trace buffer.
 * We fill the redzone with sentinel value to detect when the redzone
 * is reached, i.e., when the trace buffer is full.
 */
static size_t redzone_size;
static size_t max_buf_size;

/* thread private buffer and counter */
typedef struct {
    byte *seg_base;
    byte *buf_base;
    uint64 bytes_written;
    /* For offline traces */
    file_t file;
} per_thread_t;

#define MAX_NUM_DELAY_INSTRS 32
/* per bb user data during instrumentation */
typedef struct {
    app_pc last_app_pc;
    instr_t *strex;
    int num_delay_instrs;
    instr_t *delay_instrs[MAX_NUM_DELAY_INSTRS];
    bool repstr;
    void *instru_field; /* for use by instru_t */
} user_data_t;

/* For online simulation, we write to a single global pipe */
static named_pipe_t ipc_pipe;

#define MAX_INSTRU_SIZE 64  /* the max obj size of instr_t or its children */
static instru_t *instru;

static client_id_t client_id;
static void  *mutex;    /* for multithread support */
static uint64 num_bytes; /* keep a global memory reference count */

/* virtual to physical translation */
static bool have_phys;
static physaddr_t physaddr;

/* file operations functions */
struct file_ops_func_t {
    drmemtrace_open_file_func_t  open_file;
    drmemtrace_read_file_func_t  read_file;
    drmemtrace_write_file_func_t write_file;
    drmemtrace_close_file_func_t close_file;
    drmemtrace_create_dir_func_t create_dir;
};
static struct file_ops_func_t file_ops_func = {
    dr_open_file, dr_read_file, dr_write_file, dr_close_file, dr_create_dir,
};

drmemtrace_status_t
drmemtrace_replace_file_ops(drmemtrace_open_file_func_t  open_file_func,
                            drmemtrace_read_file_func_t  read_file_func,
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

static char modlist_path[MAXIMUM_PATH];

drmemtrace_status_t
drmemtrace_get_modlist_path(OUT const char **path)
{
    if (path == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    *path = modlist_path;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_custom_module_data(void * (*load_cb)(module_data_t *module),
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

/* sideline threading pool */
/* We use a circular buffer to implement a producer-consumer queue.
 * The tracer puts traces from application threads into the queue,
 * and the sideline threads consume traces the queue and write them into files.
 */
static drvector_t circular_buf_queue;
/* The number of traces can be cached in the queue (vector length), must be power of 2. */
static uint queue_size = 0;
/* queue_size - 1, for fast circular buffer index computation */
static uint queue_size_mask = 0;
/* The index for the first entry to be written. */
static volatile uint queue_put = 0;
/* The index for the first entry to be read. */
static volatile uint queue_get = 0;
static void **sideline_exit_events;
struct buf_entry_t {
    void   *data;  // output data pointer, NULL means the end of data for this file.
    ssize_t size;  // output data size.
    thread_id_t tid;
};
struct sideline_arg_t {
    ptr_int_t id;    // sideline thread id
    file_t    file;  // file used by sideline thread
};
static sideline_arg_t *sideline_args;

/* Allocated TLS slot offsets */
enum {
    MEMTRACE_TLS_OFFS_BUF_PTR,
    MEMTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};
static reg_id_t tls_seg;
static uint     tls_offs;
static int      tls_idx;
#define TLS_SLOT(tls_base, enum_val) (void **)((byte *)(tls_base)+tls_offs+(enum_val))
#define BUF_PTR(tls_base) *(byte **)TLS_SLOT(tls_base, MEMTRACE_TLS_OFFS_BUF_PTR)
/* We leave some space at the start so we can easily insert a header entry */
static size_t buf_hdr_slots_size;

static inline byte *
atomic_pipe_write(thread_id_t tid, byte *pipe_start, byte *pipe_end);

static inline void
init_buf_entry(buf_entry_t *entry)
{
    entry->data = NULL;
    entry->size = 0;
    entry->tid  = 0;
}

static void
free_buf_entry(void *entry)
{
    dr_global_free(entry, sizeof(buf_entry_t));
}

static inline void
circular_buf_enqueue(void *data, ssize_t size, thread_id_t tid)
{
    bool done = false;
    buf_entry_t *entry;
    do {
        /* quick racy check without locking */
        while (((queue_put + 1) & queue_size_mask) == queue_get) {
            /* the queue is full, waiting */;
            dr_thread_yield();
        }
        drvector_lock(&circular_buf_queue);
        if (((queue_put + 1) & queue_size_mask) != queue_get) {
            /* queue_put pointing to the first empty entry for write */
            entry = (buf_entry_t *)drvector_get_entry(&circular_buf_queue, queue_put);
            DR_ASSERT(entry->data == NULL && entry->size == 0);
            if (data != NULL) {  /* not special END entry */
                /* Update the timestamp to the enqueue time to make sure
                 * that the timestamp changes monotonically.
                 */
                instru->append_timestamp((byte *)data);
            }
            entry->data = data;
            entry->size = size;
            entry->tid = tid;
            drvector_set_entry(&circular_buf_queue, queue_put, entry);
            queue_put = (queue_put + 1) & queue_size_mask;
            done = true;
        }
        drvector_unlock(&circular_buf_queue);
    } while (!done);
}

static inline bool
circular_buf_dequeue(buf_entry_t *entry)
{
    bool get = false;
    drvector_lock(&circular_buf_queue);
    if (queue_get != queue_put) {
        buf_entry_t *tmp;
        tmp = (buf_entry_t *)drvector_get_entry(&circular_buf_queue, queue_get);
        *entry = *tmp;
        init_buf_entry(tmp);
        queue_get = (queue_get + 1) & queue_size_mask;
        get = true;
    }
    drvector_unlock(&circular_buf_queue);
    return get;
}

static void
trace_traverse(byte *buf_base, ssize_t size,
               bool vaddr2paddr, bool do_write, thread_id_t tid)
{
    byte *mem_ref, *buf_ptr;
    byte *pipe_start, *pipe_end;
    pipe_start = buf_base;
    pipe_end = pipe_start;
    buf_ptr = buf_base + size;
    for (mem_ref  = buf_base + buf_hdr_slots_size;
         mem_ref  < buf_ptr;
         mem_ref += instru->sizeof_entry()) {
        if (vaddr2paddr) {
            trace_type_t type = instru->get_entry_type(mem_ref);
            if (type != TRACE_TYPE_THREAD &&
                type != TRACE_TYPE_THREAD_EXIT &&
                type != TRACE_TYPE_PID) {
                addr_t virt = instru->get_entry_addr(mem_ref);
                addr_t phys = physaddr.virtual2physical(virt);
                DR_ASSERT(type != TRACE_TYPE_INSTR_BUNDLE);
                if (phys != 0)
                    instru->set_entry_addr(mem_ref, phys);
                else {
                    // XXX i#1735: use virtual address and continue?
                    // There are cases the xl8 fail, e.g.,:
                    // - vsyscall/kernel page,
                    // - wild access (NULL or very large bogus address) by app
                    NOTIFY(1, "virtual2physical translation failure for "
                           "<%2d, %2d, " PFX">\n",
                           type, instru->get_entry_size(mem_ref), virt);
                }
            }
        }
        if (do_write) {
            // Split up the buffer into multiple writes to ensure
            // atomic pipe writes.
            // We can only split before TRACE_TYPE_INSTR, assuming only a few
            // data entries in between instr entries.
            if (instru->get_entry_type(mem_ref) == TRACE_TYPE_INSTR) {
                if ((mem_ref - pipe_start) > ipc_pipe.get_atomic_write_size())
                    pipe_start = atomic_pipe_write(tid, pipe_start, pipe_end);
                // Advance pipe_end pointer
                pipe_end = mem_ref;
            }
        }
    }
    if (do_write) {
        // Write the rest to pipe
        // The last few entries (e.g., instr + refs) may exceed the atomic write size,
        // so we may need two writes.
        if ((buf_ptr - pipe_start) > ipc_pipe.get_atomic_write_size())
            pipe_start = atomic_pipe_write(tid, pipe_start, pipe_end);
        if ((buf_ptr - pipe_start) > (ssize_t)buf_hdr_slots_size)
            atomic_pipe_write(tid, pipe_start, buf_ptr);
    }
}

static void
sideline_run(void *arg)
{
    sideline_arg_t *sa = (sideline_arg_t *)arg;
    buf_entry_t entry;
    bool profiling = true;
    bool offline = op_offline.get_value();
    /* consume the queue and write data to file */
    do {
        /* quick racy check without locking */
        while (queue_get == queue_put) {
            /* the queue is empty, waiting */
            dr_thread_yield();
        }
        if (!circular_buf_dequeue(&entry))
            continue;
        if (entry.data != NULL) {
            if (offline)
                file_ops_func.write_file(sa->file, entry.data, entry.size);
            else {
                DR_ASSERT(op_num_threads.get_value() == 1);
                trace_traverse((byte *)entry.data, entry.size,
                               false, /* !vaddr2paddr */
                               true, /* do_write */
                               entry.tid);
            }
            dr_raw_mem_free(entry.data, max_buf_size);
        } else {
            DR_ASSERT(entry.size == 0);
            profiling = false;
        }
    } while (profiling);

    dr_mutex_lock(mutex);
    dr_mutex_unlock(mutex);
    dr_event_signal(sideline_exit_events[sa->id]);
}

static void
create_buffer(per_thread_t *data)
{
    data->buf_base = (byte *)
        dr_raw_mem_alloc(max_buf_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    if (data->buf_base == NULL) {
        NOTIFY(0, "Out of memory: truncating further tracing.\n");
        dr_abort();
    }
    /* dr_raw_mem_alloc guarantees to give us zeroed memory, so no need for a memset */
    /* set sentinel (non-zero) value in redzone */
    memset(data->buf_base + trace_buf_size, -1, redzone_size);
}

static inline byte *
atomic_pipe_write(thread_id_t tid, byte *pipe_start, byte *pipe_end)
{
    ssize_t towrite = pipe_end - pipe_start;
    DR_ASSERT(towrite <= ipc_pipe.get_atomic_write_size() &&
              towrite > (ssize_t)buf_hdr_slots_size);
    if (ipc_pipe.write((void *)pipe_start, towrite) < (ssize_t)towrite)
        DR_ASSERT(false);
    // Re-emit thread entry header
    DR_ASSERT(pipe_end - buf_hdr_slots_size > pipe_start);
    pipe_start = pipe_end - buf_hdr_slots_size;
    instru->append_tid(pipe_start, tid);
    return pipe_start;
}

static inline byte *
write_trace_data(void *drcontext, byte *towrite_start, byte *towrite_end)
{
    if (op_num_threads.get_value() > 0) {
        circular_buf_enqueue(towrite_start, towrite_end - towrite_start,
                             dr_get_thread_id(drcontext));
        return towrite_start;
    }
    if (op_offline.get_value()) {
        per_thread_t *data = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
        ssize_t size = towrite_end - towrite_start;
        if (file_ops_func.write_file(data->file, towrite_start, size) < size) {
            NOTIFY(0, "Fatal error: failed to write trace\n");
            dr_abort();
        }
        return towrite_start;
    } else
        return atomic_pipe_write(dr_get_thread_id(drcontext), towrite_start, towrite_end);
}

static void
memtrace(void *drcontext, bool skip_size_cap)
{
    per_thread_t *data = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    byte *buf_ptr;
    byte *pipe_start, *pipe_end, *redzone;
    bool do_write = true;

    buf_ptr = BUF_PTR(data->seg_base);
    /* The initial slot is left empty for the header entry, which we add here. */
    instru->append_unit_header(data->buf_base, dr_get_thread_id(drcontext));
    pipe_start = data->buf_base;
    pipe_end = pipe_start;
    if (!skip_size_cap && op_max_trace_size.get_value() > 0 &&
        data->bytes_written > op_max_trace_size.get_value()) {
        /* We don't guarantee to match the limit exactly so we allow one buffer
         * beyond.  We also don't put much effort into reducing overhead once
         * beyond the limit: we still instrument and come here.
         */
        do_write = false;
    } else
        data->bytes_written += buf_ptr - pipe_start;

    if (do_write) {
        /* We only need walk the trace if we need convert vaddr to paddr
         * or synchronized online simulation with atomic pipe write.
         */
        if ((have_phys && op_use_physical.get_value()) /* need convert addr */||
            (!op_offline.get_value() && op_num_threads.get_value() == 0)) {
            trace_traverse(data->buf_base,
                           buf_ptr - data->buf_base,
                           have_phys && op_use_physical.get_value(),
                           !op_offline.get_value() &&
                           op_num_threads.get_value() == 0,
                           dr_get_thread_id(drcontext));
        }
        if (op_num_threads.get_value() > 0) {
            write_trace_data(drcontext, pipe_start, buf_ptr);
            // The buf is in the queue, and we get a new one.
            create_buffer(data);
        }
    }
    if (!do_write || op_num_threads.get_value() == 0) {
        // Our instrumentation reads from buffer and skips the clean call if the
        // content is 0, so we need set zero in the trace buffer and set non-zero
        // in redzone.
        memset(data->buf_base, 0, trace_buf_size);
        redzone = data->buf_base + trace_buf_size;
        if (buf_ptr > redzone) {
            // Set sentinel (non-zero) value in redzone
            memset(redzone, -1, buf_ptr - redzone);
        }
    }
    BUF_PTR(data->seg_base) = data->buf_base + buf_hdr_slots_size;
}

/* clean_call sends the memory reference info to the simulator */
static void
clean_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    memtrace(drcontext, false);
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

static int
instrument_delay_instrs(void *drcontext, void *tag, instrlist_t *ilist,
                        user_data_t *ud, instr_t *where,
                        reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust)
{
    if (ud->repstr) {
        // We assume that drutil restricts repstr to a single bb on its own, and
        // we avoid its mix of translations resulting in incorrect ifetch stats
        // (it can be significant: i#2011).  The original app bb has just one instr,
        // which is a memref, so the pre-memref entry will suffice.
        //
        // XXX i#2051: we also need to limit repstr loops to a single ifetch for the
        // whole loop, instead of an ifetch per iteration.  For offline we remove
        // the extras in post-processing, but for online we'll need extra instru...
        ud->num_delay_instrs = 0;
        return adjust;
    }
    // Instrument to add a full instr entry for the first instr.
    adjust = instru->instrument_instr(drcontext, tag, &ud->instru_field,
                                      ilist, where, reg_ptr, reg_tmp, adjust,
                                      ud->delay_instrs[0]);
    if (have_phys && op_use_physical.get_value()) {
        // No instr bundle if physical-2-virtual since instr bundle may
        // cross page bundary.
        int i;
        for (i = 1; i < ud->num_delay_instrs; i++) {
            adjust = instru->instrument_instr(drcontext, tag, &ud->instru_field,
                                              ilist, where, reg_ptr, reg_tmp,
                                              adjust, ud->delay_instrs[i]);
        }
    } else {
        adjust = instru->instrument_ibundle(drcontext, ilist, where, reg_ptr, reg_tmp,
                                            adjust, ud->delay_instrs + 1,
                                            ud->num_delay_instrs - 1);
    }
    ud->num_delay_instrs = 0;
    return adjust;
}

/* We insert code to read from trace buffer and check whether the redzone
 * is reached. If redzone is reached, the clean call will be called.
 */
static void
instrument_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, reg_id_t reg_tmp)
{
    instr_t *skip_call = INSTR_CREATE_label(drcontext);
    IF_X86(uint64 prof_pcs;)
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext,
                              opnd_create_reg(reg_ptr),
                              OPND_CREATE_MEMPTR(reg_ptr, 0)));
#ifdef X86
    DR_ASSERT(reg_ptr == DR_REG_XCX);
    /* i#2049: we use DR_CLEANCALL_ALWAYS_OUT_OF_LINE to ensure our jecxz
     * reaches across the clean call (o/w we need 2 jmps to invert the jecxz).
     * Long-term we should try a fault instead (xref drx_buf) or a lean
     * proc to clean call gencode.
     */
    /* i#2147: -prof_pcs adds extra cleancall code that makes jecxz not reach.
     * XXX: it would be nice to have a more robust solution than this explicit check
     * for that DR option!
     */
    if (dr_get_integer_option("profile_pcs", &prof_pcs) && prof_pcs) {
        instr_t *should_skip = INSTR_CREATE_label(drcontext);
        instr_t *no_skip = INSTR_CREATE_label(drcontext);
        MINSERT(ilist, where,
                INSTR_CREATE_jecxz(drcontext, opnd_create_instr(should_skip)));
        MINSERT(ilist, where,
                INSTR_CREATE_jmp(drcontext, opnd_create_instr(no_skip)));
        MINSERT(ilist, where, should_skip);
        MINSERT(ilist, where,
                INSTR_CREATE_jmp(drcontext, opnd_create_instr(skip_call)));
        MINSERT(ilist, where, no_skip);
    } else {
        MINSERT(ilist, where,
                INSTR_CREATE_jecxz(drcontext, opnd_create_instr(skip_call)));
    }
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
#elif defined(AARCH64)
    MINSERT(ilist, where,
            INSTR_CREATE_cbz(drcontext,
                             opnd_create_instr(skip_call),
                             opnd_create_reg(reg_ptr)));
#endif
    dr_insert_clean_call_ex(drcontext, ilist, where, (void *)clean_call,
                            DR_CLEANCALL_ALWAYS_OUT_OF_LINE, 0);
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
    bool is_memref;

    if ((!instr_is_app(instr) ||
         /* Skip identical app pc, which happens with rep str expansion.
          * XXX: the expansion means our instr fetch trace is not perfect,
          * but we live with having the wrong instr length.
          */
         ud->last_app_pc == instr_get_app_pc(instr)) &&
        ud->strex == NULL &&
        // Ensure we have an instr entry for the start of the bb, for offline.
        (!op_offline.get_value() || !drmgr_is_first_instr(drcontext, instr)))
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

    // Optimization: delay the simple instr trace instrumentation if possible.
    // For offline traces we want a single instr entry for the start of the bb.
    if ((!op_offline.get_value() || !drmgr_is_first_instr(drcontext, instr)) &&
        !(instr_reads_memory(instr) ||instr_writes_memory(instr)) &&
        // Avoid dropping trailing instrs
        !drmgr_is_last_instr(drcontext, instr) &&
        // Avoid bundling instrs whose types we separate.
        (instru_t::instr_to_instr_type(instr) == TRACE_TYPE_INSTR ||
         // We avoid overhead of skipped bundling for online unless the user requested
         // instr types.  We could use different types for
         // bundle-ends-in-this-branch-type to avoid this but for now it's not worth it.
         (!op_offline.get_value() && !op_online_instr_types.get_value())) &&
        ud->strex == NULL &&
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
        NOTIFY(0, "Fatal error: failed to reserve scratch registers\n");
        dr_abort();
    }
    drvector_delete(&rvec);
    /* load buf ptr into reg_ptr */
    insert_load_buf_ptr(drcontext, bb, instr, reg_ptr);

    if (ud->num_delay_instrs != 0) {
        adjust = instrument_delay_instrs(drcontext, tag, bb, ud, instr,
                                         reg_ptr, reg_tmp, adjust);
    }

    if (ud->strex != NULL) {
        DR_ASSERT(instr_is_exclusive_store(ud->strex));
        adjust = instru->instrument_instr(drcontext, tag, &ud->instru_field, bb,
                                          instr, reg_ptr, reg_tmp, adjust, ud->strex);
        adjust = instru->instrument_memref(drcontext, bb, instr, reg_ptr, reg_tmp,
                                           adjust, instr_get_dst(ud->strex, 0),
                                           true, instr_get_predicate(ud->strex));
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
    is_memref = instr_reads_memory(instr) || instr_writes_memory(instr);
    // See comment in instrument_delay_instrs: we only want the original string
    // ifetch and not any of the expansion instrs.
    if (is_memref || !ud->repstr) {
        adjust = instru->instrument_instr(drcontext, tag, &ud->instru_field, bb,
                                          instr, reg_ptr, reg_tmp, adjust, instr);
    }
    ud->last_app_pc = instr_get_app_pc(instr);

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
                adjust = instru->instrument_memref(drcontext, bb, instr, reg_ptr,
                                                   reg_tmp, adjust,
                                                   instr_get_src(instr, i), false, pred);
            }
        }

        for (i = 0; i < instr_num_dsts(instr); i++) {
            if (opnd_is_memory_reference(instr_get_dst(instr, i))) {
                adjust = instru->instrument_memref(drcontext, bb, instr, reg_ptr,
                                                   reg_tmp, adjust,
                                                   instr_get_dst(instr, i), true, pred);
            }
        }
        insert_update_buf_ptr(drcontext, bb, instr, reg_ptr, pred, adjust);
    } else if (adjust != 0)
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
    data->instru_field = NULL;
    *user_data = (void *)data;
    if (!drutil_expand_rep_string_ex(drcontext, bb, &data->repstr, NULL)) {
        DR_ASSERT(false);
        /* in release build, carry on: we'll just miss per-iter refs */
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating, void *user_data)
{
    user_data_t *ud = (user_data_t *) user_data;
    instru->bb_analysis(drcontext, tag, &ud->instru_field, bb, ud->repstr);
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
        addr_t start = (addr_t)dr_syscall_get_param(drcontext, 0);
        addr_t end   = (addr_t)dr_syscall_get_param(drcontext, 1);
        data = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
        if (end > start) {
            BUF_PTR(data->seg_base) +=
                instru->append_iflush(BUF_PTR(data->seg_base), start, end - start);
        }
    }
#endif
    memtrace(drcontext, false);
    return true;
}

static inline file_t
create_thread_file(ptr_int_t id)
{
    /* We do not need to call drx_init before using drx_open_unique_appid_file.
     * Since we're now in a subdir we could make the name simpler but this
     * seems nice and complete.
     */
    int i, size;
    file_t file;
    char buf[MAXIMUM_PATH];
    const int NUM_OF_TRIES = 10000;
    uint flags = IF_UNIX(DR_FILE_CLOSE_ON_FORK |)
        DR_FILE_ALLOW_LARGE | DR_FILE_WRITE_REQUIRE_NEW;
    /* We use drx_open_unique_appid_file with DRX_FILE_SKIP_OPEN to get a
     * file name for creation.  Retry if the same name file already exists.
     * Abort if we fail too many times.
     */
    for (i = 0; i < NUM_OF_TRIES; i++) {
        drx_open_unique_appid_file(logsubdir, id,
                                   OUTFILE_PREFIX, OUTFILE_SUFFIX,
                                   DRX_FILE_SKIP_OPEN,
                                   buf, BUFFER_SIZE_ELEMENTS(buf));
        NULL_TERMINATE_BUFFER(buf);
        file = file_ops_func.open_file(buf, flags);
        if (file != INVALID_FILE)
            break;
    }
    if (i == NUM_OF_TRIES) {
        NOTIFY(0, "Fatal error: failed to create trace file %s\n", buf);
        dr_abort();
    }
    NOTIFY(2, "Created thread trace file %s\n", buf);
    /* write file header */
    size  = instru->append_thread_file_header((byte *)buf);

    DR_ASSERT(size > 0 && size < MAXIMUM_PATH);
    file_ops_func.write_file(file, buf, size);
    return file;
}

static void
close_thread_file(file_t file)
{
    if (!op_offline.get_value())
        return;
    file_ops_func.close_file(file);
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)
        dr_thread_alloc(drcontext, sizeof(per_thread_t));
    DR_ASSERT(data != NULL);
    memset(data, 0, sizeof(*data));
    drmgr_set_tls_field(drcontext, tls_idx, data);

    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    data->seg_base = (byte *) dr_get_dr_segment_base(tls_seg);
    DR_ASSERT(data->seg_base != NULL);
    create_buffer(data);

    if (op_offline.get_value()) {
        if (op_num_threads.get_value() == 0)
            data->file = create_thread_file(dr_get_thread_id(drcontext));
        else
            data->file = INVALID_FILE;
    } else {
        /* pass pid and tid to the simulator to register current thread */
        byte *buf = (byte *)
            dr_raw_mem_alloc(max_buf_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        int size = instru->append_thread_file_header(buf);
        DR_ASSERT(max_buf_size > (uint)size);
        write_trace_data(drcontext, buf, buf + size);
    }
    /* put buf_base to TLS plus header slots as starting buf_ptr */
    BUF_PTR(data->seg_base) = data->buf_base + buf_hdr_slots_size;

    // XXX i#1729: gather and store an initial callstack for the thread.
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    /* let the simulator know this thread has exited */
    if (op_max_trace_size.get_value() > 0 &&
        data->bytes_written > op_max_trace_size.get_value()) {
        // If over the limit, we still want to write the footer, but nothing else.
        BUF_PTR(data->seg_base) = data->buf_base + buf_hdr_slots_size;
    }
    BUF_PTR(data->seg_base) +=
        instru->append_thread_exit(BUF_PTR(data->seg_base), dr_get_thread_id(drcontext));

    memtrace(drcontext, true);

    if (op_offline.get_value() && op_num_threads.get_value() == 0) {
        file_ops_func.close_file(data->file);
    }

    dr_mutex_lock(mutex);
    num_bytes += data->bytes_written;
    dr_mutex_unlock(mutex);
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static void
event_exit(void)
{
    uint i;
    dr_log(NULL, LOG_ALL, 1, "drcachesim profile size: " SZFMT" bytes\n", num_bytes);
    NOTIFY(1, "drmemtrace exiting process " PIDFMT"; traced " SZFMT" bytes.\n",
           dr_get_process_id(), num_bytes);


    for (i = 0; i < op_num_threads.get_value(); i++) {
        /* Enqueue empty entries to indicate the end of profiling,
         * one entry per sideline thread.
         */
        circular_buf_enqueue(NULL, 0, 0);
    }
    for (i = 0; i < op_num_threads.get_value(); i++) {
        dr_event_wait(sideline_exit_events[i]);
        dr_event_destroy(sideline_exit_events[i]);
        if (op_offline.get_value())
            close_thread_file(sideline_args[i].file);
    }
    if (op_num_threads.get_value() > 0) {
        drvector_delete(&circular_buf_queue);
        dr_global_free(sideline_args,
                       op_num_threads.get_value() * sizeof(sideline_args[0]));
        dr_global_free(sideline_exit_events,
                       op_num_threads.get_value() * sizeof(sideline_exit_events[0]));
    }

    /* ~instru_t writes to module_file, we assume that it will not be affected the
     * sideline threads.
     */
    /* we use placement new for better isolation */
    instru->~instru_t();
    if (op_offline.get_value()) {
        /* ~instru_t writes to module_file, so we close module_file after. */
        file_ops_func.close_file(module_file);
    } else {
        ipc_pipe.close();
    }
    dr_global_free(instru, MAX_INSTRU_SIZE);

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
    dr_unregister_exit_event(event_exit);

    dr_mutex_destroy(mutex);
    drutil_exit();
    drmgr_exit();
}

static bool
init_offline_dir(void)
{
    char buf[MAXIMUM_PATH];
    int i;
    const int NUM_OF_TRIES = 10000;
    /* open unique dir */
    /* We do not need to call drx_init before using drx_open_unique_appid_file. */
    for (i = 0; i < NUM_OF_TRIES; i++) {
        /* We use drx_open_unique_appid_file with DRX_FILE_SKIP_OPEN to get a
         * directory name for creation.  Retry if the same name directory already
         * exists.  Abort if we fail too many times.
         */
        drx_open_unique_appid_file(op_outdir.get_value().c_str(),
                                   dr_get_process_id(),
                                   OUTFILE_PREFIX, "dir",
                                   DRX_FILE_SKIP_OPEN,
                                   buf, BUFFER_SIZE_ELEMENTS(buf));
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
    /* If the ops are replaced, it's up the replacer to notify the user.
     * In some cases data is sent over the network and the replaced create_dir is
     * a nop that returns true, in which case we don't want this message.
     */
    if (file_ops_func.create_dir == dr_create_dir)
        NOTIFY(1, "Log directory is %s\n", logsubdir);
    dr_snprintf(modlist_path, BUFFER_SIZE_ELEMENTS(modlist_path),
                "%s%s%s", logsubdir, DIRSEP, DRMEMTRACE_MODULE_LIST_FILENAME);
    NULL_TERMINATE_BUFFER(modlist_path);
    module_file = file_ops_func.open_file(modlist_path, DR_FILE_WRITE_REQUIRE_NEW
                                          IF_UNIX(| DR_FILE_CLOSE_ON_FORK));
    return (module_file != INVALID_FILE);
}

static bool
init_online_pipe(void)
{
    DR_ASSERT(!op_offline.get_value());
    if (!ipc_pipe.set_name(op_ipc_name.get_value().c_str())) {
        DR_ASSERT(false);
        return false;
    }
#ifdef UNIX
    /* we want an isolated fd so we don't use ipc_pipe.open_for_write() */
    int fd = dr_open_file(ipc_pipe.get_pipe_path().c_str(), DR_FILE_WRITE_ONLY);
    DR_ASSERT(fd != INVALID_FILE);
    if (!ipc_pipe.set_fd(fd)) {
        DR_ASSERT(false);
        return false;
    }
#else
    if (!ipc_pipe.open_for_write()) {
        if (GetLastError() == ERROR_PIPE_BUSY) {
            // FIXME i#1727: add multi-process support to Windows named_pipe_t.
            NOTIFY(0, "Fatal error: multi-process applications not yet supported "
                   "for drcachesim on Windows\n");
        } else {
            NOTIFY(0, "Fatal error: Failed to open pipe %s.\n",
                   op_ipc_name.get_value().c_str());
        }
        return false;
    }
#endif
    if (!ipc_pipe.maximize_buffer())
        NOTIFY(1, "Failed to maximize pipe buffer: performance may suffer.\n");
    return true;
}

static bool
init_thread_pool(void)
{
    uint i;

    queue_put = 0;
    queue_get = 0;
    queue_size = (unsigned int)op_queue_capacity.get_value() / max_buf_size;
    // We round up queue_size to the nearest power of 2.
    for (i = 1; i <= queue_size; i <<= 1)
        /*do nothing*/;
    queue_size = i;
    if (queue_size < 2) {
        NOTIFY(1, "Usage error: queue size too small\n");
        return false;
    }
    queue_size_mask = queue_size - 1;
    drvector_init(&circular_buf_queue, queue_size, false, free_buf_entry);
    for (i = 0; i < queue_size; i++) {
        buf_entry_t *entry = (buf_entry_t *)dr_global_alloc(sizeof(*entry));
        init_buf_entry(entry);
        drvector_set_entry(&circular_buf_queue, i, entry);
    }
    sideline_args = (sideline_arg_t *)
        dr_global_alloc(op_num_threads.get_value() * sizeof(sideline_args[0]));
    sideline_exit_events = (void **)
        dr_global_alloc(op_num_threads.get_value() * sizeof(sideline_exit_events[0]));
    for (i = 0; i < op_num_threads.get_value(); i++) {
        sideline_exit_events[i] = dr_event_create();
        sideline_args[i].id   = i;
        if (op_offline.get_value())
            sideline_args[i].file = create_thread_file((ptr_int_t)i);
        dr_create_client_thread(sideline_run, &sideline_args[i]);
    }
    return true;
}

static void
circular_buf_reset()
{
    uint i = 0;
    DR_ASSERT(op_num_threads.get_value() > 0);
    for (i = 0; i < queue_size; i++) {
        buf_entry_t *entry = (buf_entry_t *)drvector_get_entry(&circular_buf_queue, i);
        dr_raw_mem_free(entry->data, max_buf_size);
        init_buf_entry(entry);
    }
    dr_global_free(sideline_exit_events,
                   op_num_threads.get_value() * sizeof(sideline_exit_events[0]));
}

void event_fork_init(void *drcontext)
{
    uint i;
    /* clear the queue */
    if (op_num_threads.get_value() > 0) {
        circular_buf_reset();
    }
    /* recreate sideline threads */
    for (i = 0; i < op_num_threads.get_value(); i++) {
        if (op_offline.get_value())
            sideline_args[i].file = create_thread_file((ptr_int_t)i);
        dr_create_client_thread(sideline_run, &sideline_args[i]);
    }
}

/* We export drmemtrace_client_main so that a global dr_client_main can initialize
 * drmemtrace client by calling drmemtrace_client_main in a statically linked
 * multi-client executable.
 */
DR_EXPORT void
drmemtrace_client_main(client_id_t id, int argc, const char *argv[])
{
    char buf[16];
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
    if (!op_offline.get_value()) {
        if (op_ipc_name.get_value().empty()) {
            NOTIFY(0, "Usage error: ipc name is required\nUsage:\n%s",
                   droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
            dr_abort();
        }
        if (op_num_threads.get_value() > 1) {
            NOTIFY(0, "Usage error: only one sideline thread is supported "
                   "for online simulation\n");
            dr_abort();
        }
    } else if (op_outdir.get_value().empty()) {
        NOTIFY(0, "Usage error: outdir is required\nUsage:\n%s",
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }

    if (op_offline.get_value()) {
        void *buf;
        if (!init_offline_dir()) {
            NOTIFY(0, "Failed to create a subdir in %s\n", op_outdir.get_value().c_str());
            dr_abort();
        }
        /* we use placement new for better isolation */
        DR_ASSERT(MAX_INSTRU_SIZE >= sizeof(offline_instru_t));
        buf = dr_global_alloc(MAX_INSTRU_SIZE);
        instru = new(buf) offline_instru_t(insert_load_buf_ptr,
                                           file_ops_func.write_file,
                                           module_file);
    } else {
        void *buf;
        if (!init_online_pipe()) {
            NOTIFY(0, "Failed to setup pipe for online simulation\n");
            dr_abort();
        }
        /* we use placement new for better isolation */
        DR_ASSERT(MAX_INSTRU_SIZE >= sizeof(online_instru_t));
        buf = dr_global_alloc(MAX_INSTRU_SIZE);
        instru = new(buf) online_instru_t(insert_load_buf_ptr);
    }

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

    trace_buf_size = instru->sizeof_entry() * MAX_NUM_ENTRIES;
    redzone_size = instru->sizeof_entry() * MAX_NUM_ENTRIES;
    max_buf_size = trace_buf_size + redzone_size;
    /* get the header size by faking an unit header write */
    buf_hdr_slots_size = instru->append_unit_header((byte *)buf, 0);
    DR_ASSERT(buf_hdr_slots_size <= 16);

    /* setup sideline threads and queue */
    if (op_num_threads.get_value() > 0) {
        if (!init_thread_pool()) {
            NOTIFY(0, "Failed to setup threading pool\n");
            dr_abort();
        }
        dr_register_fork_init_event(event_fork_init);
    }

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
    drmemtrace_client_main(id, argc, argv);
}
