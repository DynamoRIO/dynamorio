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

/* instrument.cpp: tracing client for feeding data to cache simulator.
 *
 * Based on the memtrace_simple.c sample.
 * FIXME i#1703: add in optimizations to improve performance.
 * FIXME i#1703: perhaps refactor and split up to make it more
 * modular.
 */

#include <stddef.h> /* for offsetof */
#include <string.h>
#include "dr_api.h"
#include "drmgr.h"
#include "drutil.h"
#include "../common/memref.h"
#include "../common/named_pipe.h"

// XXX: share these instead of duplicating
#define BUFFER_SIZE_BYTES(buf)      sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf)   (BUFFER_SIZE_BYTES(buf) / sizeof((buf)[0]))
#define BUFFER_LAST_ELEMENT(buf)    (buf)[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf)  BUFFER_LAST_ELEMENT(buf) = 0

#define USAGE_CHECK(x, msg) DR_ASSERT_MSG(x, msg)

static int verbose;

#define NOTIFY(level, fmt, ...) do {          \
    if (verbose >= (level))                   \
        dr_fprintf(STDERR, fmt, __VA_ARGS__); \
} while (0)

#define OPTION_MAX_LENGTH MAXIMUM_PATH

// XXX i#1703: switch to separate options class
typedef struct _options_t {
    char ipc_name[MAXIMUM_PATH];
} options_t;

static options_t options;

/* Max number of mem_ref a buffer can have. It should be big enough
 * to hold all entries between clean calls.
 */
#define MAX_NUM_MEM_REFS 4096
/* The maximum size of buffer for holding mem_refs. */
#define MEM_BUF_SIZE (sizeof(memref_t) * MAX_NUM_MEM_REFS)

/* thread private buffer and counter */
typedef struct {
    byte      *seg_base;
    memref_t *buf_base;
    uint64     num_refs;
} per_thread_t;

/* we write to a single global pipe */
static named_pipe_t ipc_pipe;

static client_id_t client_id;
static void  *mutex;    /* for multithread support */
static uint64 num_refs; /* keep a global memory reference count */

/* Allocated TLS slot offsets */
enum {
    MEMTRACE_TLS_OFFS_BUF_PTR,
    MEMTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};
static reg_id_t tls_seg;
static uint     tls_offs;
static int      tls_idx;
#define TLS_SLOT(tls_base, enum_val) (void **)((byte *)(tls_base)+tls_offs+(enum_val))
#define BUF_PTR(tls_base) *(memref_t **)TLS_SLOT(tls_base, MEMTRACE_TLS_OFFS_BUF_PTR)

#define MINSERT instrlist_meta_preinsert

static void
memtrace(void *drcontext)
{
    per_thread_t *data;
    memref_t *mem_ref, *buf_ptr;
    // FIXME i#1703: we need a better thread id scheme that lets us identify
    // the process and thread easily in the simulator.  Perhaps we can
    // use the OS id here and just write an entry into a global file
    // or something identifying which process it belongs to.
    unsigned int id = (unsigned int) dr_get_thread_id(drcontext);
    size_t towrite;

    data    = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    buf_ptr = BUF_PTR(data->seg_base);
    for (mem_ref = (memref_t *)data->buf_base; mem_ref < buf_ptr; mem_ref++) {
        // FIXME i#1703: convert from virtual to physical if requested and avail
        mem_ref->id = id;
        data->num_refs++;
    }
    towrite = (byte *)buf_ptr - (byte *)data->buf_base;

    // FIXME i#1703: split up to ensure atomic if > PIPE_BUF.
    // When we split, ensure we do not split on an instr entry and not
    // a memref entry.
    if (ipc_pipe.write((void *)data->buf_base, towrite) < (ssize_t)towrite)
        DR_ASSERT(false);

    BUF_PTR(data->seg_base) = data->buf_base;
}

/* clean_call dumps the memory reference info to the log file */
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
insert_save_type(void *drcontext, instrlist_t *ilist, instr_t *where,
                 reg_id_t base, reg_id_t scratch, ushort type)
{
    scratch = reg_resize_to_opsz(scratch, OPSZ_2);
    MINSERT(ilist, where,
            XINST_CREATE_load_int(drcontext,
                                  opnd_create_reg(scratch),
                                  OPND_CREATE_INT16(type)));
    MINSERT(ilist, where,
            XINST_CREATE_store_2bytes(drcontext,
                                      OPND_CREATE_MEM16(base,
                                                        offsetof(memref_t, type)),
                                      opnd_create_reg(scratch)));
}

static void
insert_save_size(void *drcontext, instrlist_t *ilist, instr_t *where,
                 reg_id_t base, reg_id_t scratch, ushort size)
{
    scratch = reg_resize_to_opsz(scratch, OPSZ_2);
    MINSERT(ilist, where,
            XINST_CREATE_load_int(drcontext,
                                  opnd_create_reg(scratch),
                                  OPND_CREATE_INT16(size)));
    MINSERT(ilist, where,
            XINST_CREATE_store_2bytes(drcontext,
                                      OPND_CREATE_MEM16(base,
                                                        offsetof(memref_t, size)),
                                      opnd_create_reg(scratch)));
}

static void
insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where,
               reg_id_t base, reg_id_t scratch, app_pc pc)
{
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
                               OPND_CREATE_MEMPTR(base,
                                                  offsetof(memref_t, addr)),
                               opnd_create_reg(scratch)));
}

static void
insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                 opnd_t ref, reg_id_t reg_ptr, reg_id_t reg_addr)
{
    bool ok;
    /* we use reg_ptr as scratch to get addr */
    ok = drutil_insert_get_mem_addr(drcontext, ilist, where, ref, reg_addr, reg_ptr);
    DR_ASSERT(ok);
    insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext,
                               OPND_CREATE_MEMPTR(reg_ptr,
                                                  offsetof(memref_t, addr)),
                               opnd_create_reg(reg_addr)));
}

/* insert inline code to add an instruction entry into the buffer */
static void
instrument_instr(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    reg_id_t reg_ptr = IF_X86_ELSE(DR_REG_XCX, DR_REG_R1);
    reg_id_t reg_tmp = IF_X86_ELSE(DR_REG_XBX, DR_REG_R2);
    dr_spill_slot_t slot_ptr = SPILL_SLOT_2;
    dr_spill_slot_t slot_tmp = SPILL_SLOT_3;

    /* We need two scratch registers */
    dr_save_reg(drcontext, ilist, where, reg_ptr, slot_ptr);
    dr_save_reg(drcontext, ilist, where, reg_tmp, slot_tmp);

    insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    insert_save_type(drcontext, ilist, where, reg_ptr, reg_tmp,
                     (ushort)instr_get_opcode(where));
    insert_save_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                     (ushort)instr_length(drcontext, where));
    insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp,
                   instr_get_app_pc(where));
    insert_update_buf_ptr(drcontext, ilist, where, reg_ptr, sizeof(memref_t));
    /* restore scratch registers */
    dr_restore_reg(drcontext, ilist, where, reg_ptr, slot_ptr);
    dr_restore_reg(drcontext, ilist, where, reg_tmp, slot_tmp);
}

/* insert inline code to add a memory reference info entry into the buffer */
static void
instrument_mem(void *drcontext, instrlist_t *ilist, instr_t *where,
               opnd_t ref, bool write)
{
    reg_id_t reg_ptr = IF_X86_ELSE(DR_REG_XCX, DR_REG_R1);
    reg_id_t reg_tmp = IF_X86_ELSE(DR_REG_XBX, DR_REG_R2);
    dr_spill_slot_t slot_ptr = SPILL_SLOT_2;
    dr_spill_slot_t slot_tmp = SPILL_SLOT_3;

    /* We need two scratch registers */
    dr_save_reg(drcontext, ilist, where, reg_ptr, slot_ptr);
    dr_save_reg(drcontext, ilist, where, reg_tmp, slot_tmp);

    /* save_addr should be called first as reg_ptr or reg_tmp maybe used in ref */
    insert_save_addr(drcontext, ilist, where, ref, reg_ptr, reg_tmp);
    insert_save_type(drcontext, ilist, where, reg_ptr, reg_tmp,
                     write ? REF_TYPE_WRITE : REF_TYPE_READ);
    insert_save_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                     (ushort)drutil_opnd_mem_size_in_bytes(ref, where));
    insert_update_buf_ptr(drcontext, ilist, where, reg_ptr, sizeof(memref_t));

    /* restore scratch registers */
    dr_restore_reg(drcontext, ilist, where, reg_ptr, slot_ptr);
    dr_restore_reg(drcontext, ilist, where, reg_tmp, slot_tmp);
}

/* For each memory reference app instr, we insert inline code to fill the buffer
 * with an instruction entry and memory reference entries.
 */
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      instr_t *instr, bool for_trace,
                      bool translating, void *user_data)
{
    int i;

    if (!instr_is_app(instr))
        return DR_EMIT_DEFAULT;
    if (!instr_reads_memory(instr) && !instr_writes_memory(instr))
        return DR_EMIT_DEFAULT;

    /* insert code to add an entry for app instruction */
    instrument_instr(drcontext, bb, instr);

    /* insert code to add an entry for each memory reference opnd */
    for (i = 0; i < instr_num_srcs(instr); i++) {
        if (opnd_is_memory_reference(instr_get_src(instr, i)))
            instrument_mem(drcontext, bb, instr, instr_get_src(instr, i), false);
    }

    for (i = 0; i < instr_num_dsts(instr); i++) {
        if (opnd_is_memory_reference(instr_get_dst(instr, i)))
            instrument_mem(drcontext, bb, instr, instr_get_dst(instr, i), true);
    }

    /* insert code to call clean_call for processing the buffer */
    if (/* XXX i#1702: it is ok to skip a few clean calls on predicated instructions,
         * since the buffer will be dumped later by other clean calls.
         */
        IF_X86_ELSE(true, !instr_is_predicated(instr))
        /* FIXME i#1698: there are constraints for code between ldrex/strex pairs,
         * so we minimize the instrumentation in between by skipping the clean call.
         * However, there is still a chance that the instrumentation code may clear the
         * exclusive monitor state.
         */
        IF_ARM(&& !instr_is_exclusive_store(instr)))
        dr_insert_clean_call(drcontext, bb, instr, (void *)clean_call, false, 0);

    return DR_EMIT_DEFAULT;
}

/* We transform string loops into regular loops so we can more easily
 * monitor every memory reference they make.
 */
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb,
                 bool for_trace, bool translating)
{
    if (!drutil_expand_rep_string(drcontext, bb)) {
        DR_ASSERT(false);
        /* in release build, carry on: we'll just miss per-iter refs */
    }
    return DR_EMIT_DEFAULT;
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)
        dr_thread_alloc(drcontext, sizeof(per_thread_t));
    DR_ASSERT(data != NULL);
    drmgr_set_tls_field(drcontext, tls_idx, data);

    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    data->seg_base = (byte *) dr_get_dr_segment_base(tls_seg);
    data->buf_base = (memref_t *)
        dr_raw_mem_alloc(MEM_BUF_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    DR_ASSERT(data->seg_base != NULL && data->buf_base != NULL);
    /* put buf_base to TLS as starting buf_ptr */
    BUF_PTR(data->seg_base) = data->buf_base;

    data->num_refs = 0;
}

static void
event_thread_exit(void *drcontext)
{
    // FIXME i#1703: write a special thread-exiting msg to the pipe,
    // unless we use a thread id scheme that doesn't need it.
    per_thread_t *data = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    dr_mutex_lock(mutex);
    num_refs += data->num_refs;
    dr_mutex_unlock(mutex);
    dr_raw_mem_free(data->buf_base, MEM_BUF_SIZE);
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
        !drmgr_unregister_bb_app2app_event(event_bb_app2app) ||
        !drmgr_unregister_bb_insertion_event(event_app_instruction))
        DR_ASSERT(false);

    dr_mutex_destroy(mutex);
    drutil_exit();
    drmgr_exit();
}

static void
options_init(client_id_t id)
{
    const char *opstr = dr_get_options(id);
    const char *s;
    char token[OPTION_MAX_LENGTH];

    /* default values: none right now */

    for (s = dr_get_token(opstr, token, BUFFER_SIZE_ELEMENTS(token));
         s != NULL;
         s = dr_get_token(s, token, BUFFER_SIZE_ELEMENTS(token))) {
        if (strcmp(token, "-ipc") == 0) {
            s = dr_get_token(s, options.ipc_name,
                             BUFFER_SIZE_ELEMENTS(options.ipc_name));
            USAGE_CHECK(s != NULL, "missing ipc name");
        } else {
            NOTIFY(0, "UNRECOGNIZED OPTION: \"%s\"\n", token);
            USAGE_CHECK(false, "invalid option");
        }
    }
    USAGE_CHECK(options.ipc_name[0] != '\0', "-ipc <name> is required");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_set_client_name("DynamoRIO Cache Simulator Tracer",
                       "http://dynamorio.org/issues");

    options_init(id);

    if (!ipc_pipe.set_name(options.ipc_name))
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
        !drmgr_register_bb_app2app_event(event_bb_app2app, NULL) ||
        !drmgr_register_bb_instrumentation_event(NULL /*analysis_func*/,
                                                 event_app_instruction,
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
