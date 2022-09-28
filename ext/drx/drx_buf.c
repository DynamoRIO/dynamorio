/* **********************************************************
 * Copyright (c) 2016-2019 Google, Inc.  All rights reserved.
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

/* DynamoRio eXtension Buffer Filling API */

#include "dr_api.h"
#include "drx.h"
#include "drmgr.h"
#include "drvector.h"
#include "../ext_utils.h"
#include <stddef.h> /* for offsetof */
#include <string.h> /* for memcpy */

#ifdef UNIX
#    include <signal.h>
#endif

#define TLS_SLOT(tls_base, offs) (void **)((byte *)(tls_base) + (offs))
#define BUF_PTR(tls_base, offs) *(byte **)TLS_SLOT(tls_base, offs)

#define MINSERT instrlist_meta_preinsert

/* denotes the possible buffer types */
typedef enum { DRX_BUF_CIRCULAR_FAST, DRX_BUF_CIRCULAR, DRX_BUF_TRACE } drx_buf_type_t;

typedef struct {
    byte *seg_base;
    byte *cli_base;    /* the base of the buffer from the client's perspective */
    byte *buf_base;    /* the actual base of the buffer */
    size_t total_size; /* the actual size of the buffer */
} per_thread_t;

struct _drx_buf_t {
    drx_buf_type_t buf_type;
    size_t buf_size;
    uint vec_idx; /* index into the clients vector */
    drx_buf_full_cb_t full_cb;
    /* tls implementation */
    int tls_idx;
    uint tls_offs;
    reg_id_t tls_seg;
};

/* global rwlock to lock against updates to the clients vector */
static void *global_buf_rwlock;
/* holds per-client (also per-buf) information */
static drvector_t clients;
/* A flag to avoid work when no buffers were ever created. */
static bool any_bufs_created;

/* called by drx_init() */
bool
drx_buf_init_library(void);
void
drx_buf_exit_library(void);

static drx_buf_t *
drx_buf_init(drx_buf_type_t bt, size_t bsz, drx_buf_full_cb_t full_cb);

static per_thread_t *
per_thread_init_2byte(void *drcontext, drx_buf_t *buf);
static per_thread_t *
per_thread_init_fault(void *drcontext, drx_buf_t *buf);

static void
drx_buf_insert_update_buf_ptr_2byte(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                                    instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                                    ushort stride);
static void
drx_buf_insert_update_buf_ptr_fault(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                                    instr_t *where, reg_id_t buf_ptr, ushort stride);

static void
restore_state_event(void *drcontext, void *tag, dr_mcontext_t *mcontext,
                    bool restore_memory, bool app_code_consistent);

static void
event_thread_init(void *drcontext);
static void
event_thread_exit(void *drcontext);

#ifdef WINDOWS
static bool
exception_event(void *drcontext, dr_exception_t *excpt);
#else
static dr_signal_action_t
signal_event(void *drcontext, dr_siginfo_t *info);
#endif

static reg_id_t
deduce_buf_ptr(instr_t *instr);
static bool
reset_buf_ptr(void *drcontext, dr_mcontext_t *raw_mcontext, byte *seg_base,
              byte *cli_base, drx_buf_t *buf);
static bool
fault_event_helper(void *drcontext, byte *target, dr_mcontext_t *raw_mcontext);

bool
drx_buf_init_library(void)
{
    drmgr_priority_t exit_priority = { sizeof(exit_priority),
                                       DRMGR_PRIORITY_NAME_DRX_BUF_EXIT, NULL, NULL,
                                       DRMGR_PRIORITY_THREAD_EXIT_DRX_BUF };
    drmgr_priority_t init_priority = { sizeof(init_priority),
                                       DRMGR_PRIORITY_NAME_DRX_BUF_INIT, NULL, NULL,
                                       DRMGR_PRIORITY_THREAD_INIT_DRX_BUF };

    /* We sync the vector manually, since we need to lock the vector ourselves
     * when adding a client.
     */
    if (!drvector_init(&clients, 1, false /*!synch*/, NULL) ||
        !drmgr_register_thread_init_event_ex(event_thread_init, &init_priority) ||
        !drmgr_register_thread_exit_event_ex(event_thread_exit, &exit_priority) ||
        !drmgr_register_restore_state_event(restore_state_event))
        return false;

#ifdef WINDOWS
    if (!drmgr_register_exception_event(exception_event))
        return false;
#else
    if (!drmgr_register_signal_event(signal_event))
        return false;
#endif

    global_buf_rwlock = dr_rwlock_create();
    if (global_buf_rwlock == NULL)
        return false;

    return true;
}

void
drx_buf_exit_library(void)
{
#ifdef WINDOWS
    drmgr_unregister_exception_event(exception_event);
#else
    drmgr_unregister_signal_event(signal_event);
#endif

    drmgr_unregister_restore_state_event(restore_state_event);
    drmgr_unregister_thread_init_event(event_thread_init);
    drmgr_unregister_thread_exit_event(event_thread_exit);
    drvector_delete(&clients);
    dr_rwlock_destroy(global_buf_rwlock);
}

DR_EXPORT
drx_buf_t *
drx_buf_create_circular_buffer(size_t buf_size)
{
    /* We can optimize circular buffers that are this size */
    drx_buf_type_t buf_type = (buf_size == DRX_BUF_FAST_CIRCULAR_BUFSZ)
        ? DRX_BUF_CIRCULAR_FAST
        : DRX_BUF_CIRCULAR;
    return drx_buf_init(buf_type, buf_size, NULL);
}

DR_EXPORT
drx_buf_t *
drx_buf_create_trace_buffer(size_t buf_size, drx_buf_full_cb_t full_cb)
{
    return drx_buf_init(DRX_BUF_TRACE, buf_size, full_cb);
}

static drx_buf_t *
drx_buf_init(drx_buf_type_t bt, size_t bsz, drx_buf_full_cb_t full_cb)
{
    drx_buf_t *new_client;
    int tls_idx;
    uint tls_offs;
    reg_id_t tls_seg;

    /* allocate raw TLS so we can access it from the code cache */
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, 1, 0))
        return NULL;

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return NULL;

    /* init the client struct */
    new_client = dr_global_alloc(sizeof(*new_client));
    new_client->buf_type = bt;
    new_client->buf_size = bsz;
    new_client->tls_offs = tls_offs;
    new_client->tls_seg = tls_seg;
    new_client->tls_idx = tls_idx;
    new_client->full_cb = full_cb;
    dr_rwlock_write_lock(global_buf_rwlock);
    /* We don't attempt to re-use NULL entries (presumably which
     * have already been freed), for simplicity.
     */
    new_client->vec_idx = clients.entries;
    drvector_append(&clients, new_client);
    dr_rwlock_write_unlock(global_buf_rwlock);

    /* We don't need the usual setup for buffers if we're using
     * the optimized circular buffer.
     */
    if (!any_bufs_created && bt != DRX_BUF_CIRCULAR_FAST)
        any_bufs_created = true;

    return new_client;
}

DR_EXPORT
bool
drx_buf_free(drx_buf_t *buf)
{
    dr_rwlock_write_lock(global_buf_rwlock);
    if (!(buf != NULL && drvector_get_entry(&clients, buf->vec_idx) == buf)) {
        dr_rwlock_write_unlock(global_buf_rwlock);
        return false;
    }
    /* NULL out the entry in the vector */
    ((drx_buf_t **)clients.array)[buf->vec_idx] = NULL;
    dr_rwlock_write_unlock(global_buf_rwlock);

    if (!drmgr_unregister_tls_field(buf->tls_idx) || !dr_raw_tls_cfree(buf->tls_offs, 1))
        return false;
    dr_global_free(buf, sizeof(*buf));

    return true;
}

DR_EXPORT
void *
drx_buf_get_buffer_ptr(void *drcontext, drx_buf_t *buf)
{
    per_thread_t *data = drmgr_get_tls_field(drcontext, buf->tls_idx);
    return BUF_PTR(data->seg_base, buf->tls_offs);
}

DR_EXPORT
void
drx_buf_set_buffer_ptr(void *drcontext, drx_buf_t *buf, void *new_ptr)
{
    per_thread_t *data = drmgr_get_tls_field(drcontext, buf->tls_idx);
    BUF_PTR(data->seg_base, buf->tls_offs) = new_ptr;
}

DR_EXPORT
void *
drx_buf_get_buffer_base(void *drcontext, drx_buf_t *buf)
{
    per_thread_t *data = drmgr_get_tls_field(drcontext, buf->tls_idx);
    return data->cli_base;
}

DR_EXPORT
size_t
drx_buf_get_buffer_size(void *drcontext, drx_buf_t *buf)
{
    return buf->buf_size;
}

void
event_thread_init(void *drcontext)
{
    unsigned int i;
    dr_rwlock_read_lock(global_buf_rwlock);
    for (i = 0; i < clients.entries; ++i) {
        per_thread_t *data;
        drx_buf_t *buf = drvector_get_entry(&clients, i);
        if (buf != NULL) {
            if (buf->buf_type == DRX_BUF_CIRCULAR_FAST)
                data = per_thread_init_2byte(drcontext, buf);
            else
                data = per_thread_init_fault(drcontext, buf);
            drmgr_set_tls_field(drcontext, buf->tls_idx, data);
            BUF_PTR(data->seg_base, buf->tls_offs) = data->cli_base;
        }
    }
    dr_rwlock_read_unlock(global_buf_rwlock);
}

void
event_thread_exit(void *drcontext)
{
    unsigned int i;
    dr_rwlock_read_lock(global_buf_rwlock);
    for (i = 0; i < clients.entries; ++i) {
        drx_buf_t *buf = drvector_get_entry(&clients, i);
        if (buf != NULL) {
            per_thread_t *data = drmgr_get_tls_field(drcontext, buf->tls_idx);
            byte *cli_ptr = BUF_PTR(data->seg_base, buf->tls_offs);
            /* buffer has not yet been deleted, call user callback(s) */
            if (buf->full_cb != NULL) {
                (*buf->full_cb)(drcontext, data->cli_base,
                                (size_t)(cli_ptr - data->cli_base));
            }
            dr_raw_mem_free(data->buf_base, data->total_size);
            dr_thread_free(drcontext, data, sizeof(per_thread_t));
        }
    }
    dr_rwlock_read_unlock(global_buf_rwlock);
}

static per_thread_t *
per_thread_init_2byte(void *drcontext, drx_buf_t *buf)
{
    per_thread_t *per_thread = dr_thread_alloc(drcontext, sizeof(per_thread_t));
    byte *ret;
    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    per_thread->seg_base = dr_get_dr_segment_base(buf->tls_seg);
    /* We allocate twice the amount necessary to make sure we
     * have a buffer with a 65336-byte aligned starting address.
     */
    per_thread->total_size = 2 * buf->buf_size;
    ret = dr_raw_mem_alloc(per_thread->total_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE,
                           NULL);
    per_thread->buf_base = ret;
    per_thread->cli_base = (void *)ALIGN_FORWARD(ret, buf->buf_size);
    return per_thread;
}

static per_thread_t *
per_thread_init_fault(void *drcontext, drx_buf_t *buf)
{
    size_t page_size = dr_page_size();
    per_thread_t *per_thread = dr_thread_alloc(drcontext, sizeof(per_thread_t));
    byte *ret;
    bool ok;
    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    per_thread->seg_base = dr_get_dr_segment_base(buf->tls_seg);
    /* We construct a buffer right before a fault by allocating as
     * many pages as needed to fit the buffer, plus another read-only
     * page. Then, we return an address such that we have exactly
     * buf_size bytes usable before we hit the ro page.
     */
    per_thread->total_size = ALIGN_FORWARD(buf->buf_size, page_size) + page_size;
    ret = dr_raw_mem_alloc(per_thread->total_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE,
                           NULL);
    ok = dr_memory_protect(ret + per_thread->total_size - page_size, page_size,
                           DR_MEMPROT_READ);
    DR_ASSERT(ok);
    per_thread->buf_base = ret;
    per_thread->cli_base = ret + ALIGN_FORWARD(buf->buf_size, page_size) - buf->buf_size;
    return per_thread;
}

DR_EXPORT
void
drx_buf_insert_load_buf_ptr(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                            instr_t *where, reg_id_t buf_ptr)
{
    dr_insert_read_raw_tls(drcontext, ilist, where, buf->tls_seg, buf->tls_offs, buf_ptr);
}

DR_EXPORT
void
drx_buf_insert_update_buf_ptr(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                              instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                              ushort stride)
{
    if (buf->buf_type == DRX_BUF_CIRCULAR_FAST) {
        drx_buf_insert_update_buf_ptr_2byte(drcontext, buf, ilist, where, buf_ptr,
                                            scratch, stride);
    } else {
        drx_buf_insert_update_buf_ptr_fault(drcontext, buf, ilist, where, buf_ptr,
                                            stride);
    }
}

static void
drx_buf_insert_update_buf_ptr_2byte(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                                    instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                                    ushort stride)
{
#ifdef X86
    /* To get the "rotating" effect, we update only the bottom bits of the register. */
    if (drx_aflags_are_dead(where)) {
        /* if aflags are dead, we use add directly */
        MINSERT(ilist, where,
                INSTR_CREATE_add(drcontext,
                                 opnd_create_far_base_disp(buf->tls_seg, DR_REG_NULL,
                                                           DR_REG_NULL, 0, buf->tls_offs,
                                                           OPSZ_2),
                                 OPND_CREATE_INT16(stride)));
    } else {
        reg_id_t scratch2 = reg_resize_to_opsz(buf_ptr, OPSZ_2);
        /* use lea to avoid dealing with aflags */
        MINSERT(ilist, where,
                INSTR_CREATE_lea(
                    drcontext, opnd_create_reg(scratch2),
                    opnd_create_base_disp(buf_ptr, DR_REG_NULL, 0, stride, OPSZ_lea)));
        dr_insert_write_raw_tls(drcontext, ilist, where, buf->tls_seg, buf->tls_offs,
                                buf_ptr);
    }
#elif defined(ARM)
    if (stride > 255) {
        /* fall back to XINST_CREATE_load_int() if stride doesn't fit in 1 byte */
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
                                      OPND_CREATE_INT16(stride)));
        MINSERT(ilist, where,
                XINST_CREATE_add(drcontext, opnd_create_reg(buf_ptr),
                                 opnd_create_reg(scratch)));
    } else {
        MINSERT(ilist, where,
                XINST_CREATE_add(drcontext, opnd_create_reg(buf_ptr),
                                 OPND_CREATE_INT16(stride)));
    }
    MINSERT(ilist, where,
            XINST_CREATE_store_2bytes(drcontext,
                                      OPND_CREATE_MEM16(buf->tls_seg, buf->tls_offs),
                                      opnd_create_reg(buf_ptr)));
#elif defined(AARCH64)
    if (stride > 0xfff) {
        /* Fall back to XINST_CREATE_load_int() if stride has more than 12 bits.
         * Another possibility, avoiding a scratch register, would be:
         * add x4, x4, #0x1, lsl #12
         * add x4, x4, #0x234
         */
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
                                      OPND_CREATE_INT16(stride)));
        MINSERT(ilist, where,
                XINST_CREATE_add(drcontext, opnd_create_reg(buf_ptr),
                                 opnd_create_reg(scratch)));
    } else {
        MINSERT(ilist, where,
                XINST_CREATE_add(drcontext, opnd_create_reg(buf_ptr),
                                 OPND_CREATE_INT16(stride)));
    }
    MINSERT(ilist, where,
            XINST_CREATE_store_2bytes(drcontext,
                                      OPND_CREATE_MEM16(buf->tls_seg, buf->tls_offs),
                                      opnd_create_reg(reg_64_to_32(buf_ptr))));
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    DR_ASSERT_MSG(false, "Not implemented");
#else
#    error NYI
#endif
}

static void
drx_buf_insert_update_buf_ptr_fault(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                                    instr_t *where, reg_id_t buf_ptr, ushort stride)
{
    /* straightforward, just increment buf_ptr */
    MINSERT(
        ilist, where,
        XINST_CREATE_add(drcontext, opnd_create_reg(buf_ptr), OPND_CREATE_INT16(stride)));
    dr_insert_write_raw_tls(drcontext, ilist, where, buf->tls_seg, buf->tls_offs,
                            buf_ptr);
}

static bool
drx_buf_insert_buf_store_1byte(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                               instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                               opnd_t opnd, short offset)
{
    instr_t *instr;
    if (!opnd_is_reg(opnd) && !opnd_is_immed(opnd))
        return false;
    if (opnd_is_immed(opnd)) {
#ifdef X86
        instr =
            XINST_CREATE_store_1byte(drcontext, OPND_CREATE_MEM8(buf_ptr, offset), opnd);
#elif defined(AARCHXX)
        /* this will certainly not fault, so don't set a translation */
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch), opnd));
        instr = XINST_CREATE_store_1byte(drcontext, OPND_CREATE_MEM8(buf_ptr, offset),
                                         opnd_create_reg(scratch));
#elif defined(RISCV64)
        /* FIXME i#3544: Not implemented */
        DR_ASSERT_MSG(false, "Not implemented");
        return false;
#else
#    error NYI
#endif
    } else {
        instr =
            XINST_CREATE_store_1byte(drcontext, OPND_CREATE_MEM8(buf_ptr, offset), opnd);
    }
    INSTR_XL8(instr, instr_get_app_pc(where));
    MINSERT(ilist, where, instr);
    return true;
}

static bool
drx_buf_insert_buf_store_2bytes(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                                instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                                opnd_t opnd, short offset)
{
    instr_t *instr;
    if (!opnd_is_reg(opnd) && !opnd_is_immed(opnd))
        return false;
    if (opnd_is_immed(opnd)) {
#ifdef X86
        instr = XINST_CREATE_store_2bytes(drcontext, OPND_CREATE_MEM16(buf_ptr, offset),
                                          opnd);
#elif defined(AARCHXX)
        /* this will certainly not fault, so don't set a translation */
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch), opnd));
        instr = XINST_CREATE_store_2bytes(drcontext, OPND_CREATE_MEM16(buf_ptr, offset),
                                          opnd_create_reg(scratch));
#elif defined(RISCV64)
        /* FIXME i#3544: Not implemented */
        DR_ASSERT_MSG(false, "Not implemented");
        return false;
#else
#    error NYI
#endif
    } else {
        instr = XINST_CREATE_store_2bytes(drcontext, OPND_CREATE_MEM16(buf_ptr, offset),
                                          opnd);
    }
    INSTR_XL8(instr, instr_get_app_pc(where));
    MINSERT(ilist, where, instr);
    return true;
}

#if defined(X86_64) || defined(AARCH64)
/* only valid on platforms where OPSZ_PTR != OPSZ_4 */
static bool
drx_buf_insert_buf_store_4bytes(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                                instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                                opnd_t opnd, short offset)
{
    instr_t *instr;
    if (!opnd_is_reg(opnd) && !opnd_is_immed(opnd))
        return false;
    if (opnd_is_immed(opnd)) {
#    ifdef X86_64
        instr = XINST_CREATE_store(drcontext, OPND_CREATE_MEM32(buf_ptr, offset), opnd);
#    elif defined(AARCH64)
        /* this will certainly not fault, so don't set a translation */
        instrlist_insert_mov_immed_ptrsz(drcontext, opnd_get_immed_int(opnd),
                                         opnd_create_reg(scratch), ilist, where, NULL,
                                         NULL);
        instr = XINST_CREATE_store(drcontext, OPND_CREATE_MEM32(buf_ptr, offset),
                                   opnd_create_reg(scratch));
#    endif
    } else {
        instr = XINST_CREATE_store(drcontext, OPND_CREATE_MEM32(buf_ptr, offset), opnd);
    }
    INSTR_XL8(instr, instr_get_app_pc(where));
    MINSERT(ilist, where, instr);
    return true;
}
#endif

static bool
drx_buf_insert_buf_store_ptrsz(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                               instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                               opnd_t opnd, short offset)
{
    if (!opnd_is_reg(opnd) && !opnd_is_immed(opnd))
        return false;
    if (opnd_is_immed(opnd)) {
        instr_t *first, *last;
        ptr_int_t immed = opnd_get_immed_int(opnd);
#ifdef X86
        instrlist_insert_mov_immed_ptrsz(drcontext, immed,
                                         OPND_CREATE_MEMPTR(buf_ptr, offset), ilist,
                                         where, &first, &last);
        for (;; first = instr_get_next(first)) {
            INSTR_XL8(first, instr_get_app_pc(where));
            if (last == NULL || first == last)
                break;
        }
#elif defined(AARCHXX)
        instr_t *instr;
        instrlist_insert_mov_immed_ptrsz(drcontext, immed, opnd_create_reg(scratch),
                                         ilist, where, &first, &last);
        instr = XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(buf_ptr, offset),
                                   opnd_create_reg(scratch));
        INSTR_XL8(instr, instr_get_app_pc(where));
        MINSERT(ilist, where, instr);
#elif defined(RISCV64)
        /* FIXME i#3544: Not implemented */
        DR_ASSERT_MSG(false, "Not implemented");
        /* Marking as unused to silence -Wunused-variable. */
        (void)first;
        (void)last;
        (void)immed;
        return false;
#else
#    error NYI
#endif
    } else {
        instr_t *instr =
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(buf_ptr, offset), opnd);
        INSTR_XL8(instr, instr_get_app_pc(where));
        MINSERT(ilist, where, instr);
    }
    return true;
}

DR_EXPORT
bool
drx_buf_insert_buf_store(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                         instr_t *where, reg_id_t buf_ptr, reg_id_t scratch, opnd_t opnd,
                         opnd_size_t opsz, short offset)
{
    switch (opsz) {
    case OPSZ_1:
        return drx_buf_insert_buf_store_1byte(drcontext, buf, ilist, where, buf_ptr,
                                              scratch, opnd, offset);
    case OPSZ_2:
        return drx_buf_insert_buf_store_2bytes(drcontext, buf, ilist, where, buf_ptr,
                                               scratch, opnd, offset);
#if defined(X86_64) || defined(AARCH64)
    case OPSZ_4:
        return drx_buf_insert_buf_store_4bytes(drcontext, buf, ilist, where, buf_ptr,
                                               scratch, opnd, offset);
#endif
    case OPSZ_PTR:
        return drx_buf_insert_buf_store_ptrsz(drcontext, buf, ilist, where, buf_ptr,
                                              scratch, opnd, offset);
    default: return false;
    }
}

static void
insert_load(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t dst,
            reg_id_t src, opnd_size_t opsz)
{
    switch (opsz) {
    case OPSZ_1:
        MINSERT(ilist, where,
                XINST_CREATE_load_1byte(
                    drcontext, opnd_create_reg(reg_resize_to_opsz(dst, opsz)),
                    opnd_create_base_disp(src, DR_REG_NULL, 0, 0, opsz)));
        break;
    case OPSZ_2:
        MINSERT(ilist, where,
                XINST_CREATE_load_2bytes(
                    drcontext, opnd_create_reg(reg_resize_to_opsz(dst, opsz)),
                    opnd_create_base_disp(src, DR_REG_NULL, 0, 0, opsz)));
        break;
    case OPSZ_4:
#if defined(X86_64) || defined(AARCH64)
    case OPSZ_8:
#endif
        MINSERT(ilist, where,
                XINST_CREATE_load(drcontext,
                                  opnd_create_reg(reg_resize_to_opsz(dst, opsz)),
                                  opnd_create_base_disp(src, DR_REG_NULL, 0, 0, opsz)));
        break;
    default: DR_ASSERT(false); break;
    }
}

/* Performs a drx_buf-compatible memcpy which handles its own fault.
 * Note that on a fault we simply reset the buffer pointer with no partial write.
 */
static void
safe_memcpy(drx_buf_t *buf, void *src, size_t len)
{
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *data = drmgr_get_tls_field(drcontext, buf->tls_idx);
    byte *cli_ptr = BUF_PTR(data->seg_base, buf->tls_offs);

    DR_ASSERT_MSG(buf->buf_size >= len,
                  "buffer was too small to fit requested memcpy() operation");
    /* try to perform a safe memcpy */
    if (!dr_safe_write(cli_ptr, len, src, NULL)) {
        /* we overflowed the client buffer, so flush it and try again */
        byte *cli_base = data->cli_base;
        BUF_PTR(data->seg_base, buf->tls_offs) = cli_base;
        if (buf->full_cb != NULL)
            (*buf->full_cb)(drcontext, cli_base, (size_t)(cli_ptr - cli_base));
        memcpy(cli_base, src, len);
    }
}

DR_EXPORT
void
drx_buf_insert_buf_memcpy(void *drcontext, drx_buf_t *buf, instrlist_t *ilist,
                          instr_t *where, reg_id_t dst, reg_id_t src, ushort len)
{
    DR_ASSERT_MSG(buf->buf_type != DRX_BUF_CIRCULAR_FAST,
                  "drx_buf_insert_buf_memcpy does not support the fast circular buffer");
    if (len > sizeof(app_pc)) {
        opnd_t buf_opnd = OPND_CREATE_INTPTR(buf);
        opnd_t src_opnd = opnd_create_reg(src);
        opnd_t len_opnd = OPND_CREATE_INTPTR((short)len);
        dr_insert_clean_call(drcontext, ilist, where, (void *)safe_memcpy, false, 3,
                             buf_opnd, src_opnd, len_opnd);
    } else {
        opnd_size_t opsz = opnd_size_from_bytes(len);
        opnd_t src_opnd;
        bool ok;

        /* fast path, we are able to directly perform the load/store */
        if (IF_X86_ELSE(reg_resize_to_opsz(src, opsz) == DR_REG_NULL, false)) {
            /* This could happen if, for example we tried to resize the base
             * pointer to an operand size of length 1. Unfortunately drreg can
             * give us registers like this on 32-bit.
             */
            /* We change the operand size to OPSZ_4, and we just perform the
             * load and store as normal but zextend the register in between. We
             * rely on little-endian behavior to do the right thing when storing
             * a word as opposed to a byte, as the first byte written is the
             * least significant byte.
             * XXX: The load may fault if for example this read is along a page
             * boundary, but it's very unlkely and we ignore it for now. There
             * are no problems with the store, because even if we fault the
             * client should have a way to recover from partial writes anyway.
             */
            DR_ASSERT(opsz == OPSZ_1);
            opsz = OPSZ_4;
            MINSERT(ilist, where,
                    XINST_CREATE_load_1byte_zext4(
                        drcontext, opnd_create_reg(reg_resize_to_opsz(src, opsz)),
                        opnd_create_base_disp(src, DR_REG_NULL, 0, 0, OPSZ_1)));
        } else
            insert_load(drcontext, ilist, where, src, src, opsz);
        src_opnd = opnd_create_reg(reg_resize_to_opsz(src, opsz));
        ok = drx_buf_insert_buf_store(drcontext, buf, ilist, where, dst, DR_REG_NULL,
                                      src_opnd, opsz, 0);
        DR_ASSERT(ok);
    }
    /* update buf ptr, so that client does not have to */
    drx_buf_insert_update_buf_ptr(drcontext, buf, ilist, where, dst, src, len);
}

/* assumes that the instruction writes memory relative to some buffer pointer */
static reg_id_t
deduce_buf_ptr(instr_t *instr)
{
    ushort opcode = (ushort)instr_get_opcode(instr);

#if defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    DR_ASSERT_MSG(false, "Not implemented");
#endif
    /* drx_buf will only emit these instructions to store a value */
    if (IF_X86_ELSE(opcode == OP_mov_st,
                    IF_RISCV64_ELSE(opcode == OP_sb || opcode == OP_sw || opcode == OP_sd,
                                    opcode == OP_str || opcode == OP_strb ||
                                        opcode == OP_strh))) {
        int i;
        for (i = 0; i < instr_num_dsts(instr); ++i) {
            opnd_t dst = instr_get_dst(instr, i);
            if (opnd_is_memory_reference(dst))
                return opnd_get_base(dst);
        }
        DR_ASSERT_MSG(false,
                      "fault occured, but instruction did not have "
                      "memory reference destination operand");
    } else {
        DR_ASSERT_MSG(false,
                      "fault occured, but instruction was not compatible "
                      "with drx_buf");
    }
    /* If we got here, then the write had no base reg, and there's
     * nothing for us to do.
     */
    return DR_REG_NULL;
}

/* returns true if we won't intercept the fault, false otherwise */
static bool
reset_buf_ptr(void *drcontext, dr_mcontext_t *raw_mcontext, byte *seg_base,
              byte *cli_base, drx_buf_t *buf)
{
    instr_t *instr;
    reg_id_t buf_ptr;
    app_pc tmp_base;

    /* decode the instruction to extract the base register */
    instr = instr_create(drcontext);
    decode(drcontext, raw_mcontext->pc, instr);
    buf_ptr = deduce_buf_ptr(instr);
    instr_destroy(drcontext, instr);
    if (buf_ptr == DR_REG_NULL)
        return true;

    /* We set the buffer pointer before the callback so it's easier
     * for the user to override it in the callback.
     */
    tmp_base = BUF_PTR(seg_base, buf->tls_offs);
    BUF_PTR(seg_base, buf->tls_offs) = cli_base;
    if (buf->full_cb != NULL)
        (*buf->full_cb)(drcontext, cli_base, (size_t)(tmp_base - cli_base));

    /* change contents of buf_ptr and retry the instruction */
    reg_set_value(buf_ptr, raw_mcontext, (reg_t)BUF_PTR(seg_base, buf->tls_offs));
    return false;
}

/* returns true if we won't intercept the fault, false otherwise */
static bool
fault_event_helper(void *drcontext, byte *target, dr_mcontext_t *raw_mcontext)
{
    size_t page_size = dr_page_size();
    per_thread_t *data;
    drx_buf_t *buf;
    unsigned int i;

    /* were we executing instead of writing? */
    if (raw_mcontext->pc == target)
        return true;

    /* check bounds of write to see which buffer this event belongs to */
    dr_rwlock_read_lock(global_buf_rwlock);
    for (i = 0; i < clients.entries; ++i) {
        buf = drvector_get_entry(&clients, i);
        if (buf != NULL && buf->buf_type != DRX_BUF_CIRCULAR_FAST) {
            byte *ro_lo;
            data = drmgr_get_tls_field(drcontext, buf->tls_idx);
            ro_lo = data->cli_base + buf->buf_size;

            /* we found the right client */
            if (target >= ro_lo && target < ro_lo + page_size) {
                bool ret = reset_buf_ptr(drcontext, raw_mcontext, data->seg_base,
                                         data->cli_base, buf);
                dr_rwlock_read_unlock(global_buf_rwlock);
                return ret;
            }
        }
    }
    dr_rwlock_read_unlock(global_buf_rwlock);
    return true;
}

#ifdef WINDOWS
bool
exception_event(void *drcontext, dr_exception_t *excpt)
{
    /* fast fail if it wasn't a seg fault */
    if (!any_bufs_created || excpt->record->ExceptionCode != STATUS_ACCESS_VIOLATION)
        return true;

    /* The second entry in the exception information array handily holds the target
     * write address.
     */
    return fault_event_helper(drcontext, (byte *)excpt->record->ExceptionInformation[1],
                              excpt->raw_mcontext);
}
#else
dr_signal_action_t
signal_event(void *drcontext, dr_siginfo_t *info)
{
    /* fast fail if it wasn't a regular seg fault */
    if (!any_bufs_created || info->sig != SIGSEGV || !info->raw_mcontext_valid)
        return DR_SIGNAL_DELIVER;

    return fault_event_helper(drcontext, info->access_address, info->raw_mcontext)
        ? DR_SIGNAL_DELIVER
        : DR_SIGNAL_SUPPRESS;
}
#endif

void
restore_state_event(void *drcontext, void *tag, dr_mcontext_t *mcontext,
                    bool restore_memory, bool app_code_consistent)
{
    /* no-op */
}
