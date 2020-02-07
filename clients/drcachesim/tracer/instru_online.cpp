/* **********************************************************
 * Copyright (c) 2016-2020 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* instru_online: inserts instrumentation for online traces.
 */

#include "dr_api.h"
#include "drreg.h"
#include "drutil.h"
#include "instru.h"
#include "../common/trace_entry.h"
#include <limits.h> /* for USHRT_MAX */
#include <stddef.h> /* for offsetof */

online_instru_t::online_instru_t(void (*insert_load_buf)(void *, instrlist_t *, instr_t *,
                                                         reg_id_t),
                                 bool memref_needs_info, drvector_t *reg_vector)
    : instru_t(insert_load_buf, memref_needs_info, reg_vector, sizeof(trace_entry_t))
{
}

online_instru_t::~online_instru_t()
{
}

trace_type_t
online_instru_t::get_entry_type(byte *buf_ptr) const
{
    trace_entry_t *entry = (trace_entry_t *)buf_ptr;
    return (trace_type_t)entry->type;
}

size_t
online_instru_t::get_entry_size(byte *buf_ptr) const
{
    trace_entry_t *entry = (trace_entry_t *)buf_ptr;
    return entry->size;
}

addr_t
online_instru_t::get_entry_addr(byte *buf_ptr) const
{
    trace_entry_t *entry = (trace_entry_t *)buf_ptr;
    return entry->addr;
}

void
online_instru_t::set_entry_addr(byte *buf_ptr, addr_t addr)
{
    trace_entry_t *entry = (trace_entry_t *)buf_ptr;
    entry->addr = addr;
}

int
online_instru_t::append_pid(byte *buf_ptr, process_id_t pid)
{
    trace_entry_t *entry = (trace_entry_t *)buf_ptr;
    entry->type = TRACE_TYPE_PID;
    entry->size = sizeof(process_id_t);
    entry->addr = (addr_t)pid;
    return sizeof(trace_entry_t);
}

int
online_instru_t::append_tid(byte *buf_ptr, thread_id_t tid)
{
    trace_entry_t *entry = (trace_entry_t *)buf_ptr;
    entry->type = TRACE_TYPE_THREAD;
    entry->size = sizeof(thread_id_t);
    entry->addr = (addr_t)tid;
    return sizeof(trace_entry_t);
}

int
online_instru_t::append_thread_exit(byte *buf_ptr, thread_id_t tid)
{
    trace_entry_t *entry = (trace_entry_t *)buf_ptr;
    entry->type = TRACE_TYPE_THREAD_EXIT;
    entry->size = sizeof(thread_id_t);
    entry->addr = (addr_t)tid;
    return sizeof(trace_entry_t);
}

int
online_instru_t::append_marker(byte *buf_ptr, trace_marker_type_t type, uintptr_t val)
{
    trace_entry_t *entry = (trace_entry_t *)buf_ptr;
    entry->type = TRACE_TYPE_MARKER;
    entry->size = (ushort)type;
    entry->addr = (addr_t)val;
    return sizeof(trace_entry_t);
}

int
online_instru_t::append_iflush(byte *buf_ptr, addr_t start, size_t size)
{
    trace_entry_t *entry = (trace_entry_t *)buf_ptr;
    entry->type = TRACE_TYPE_INSTR_FLUSH;
    entry->addr = start;
    entry->size = (size <= USHRT_MAX) ? (ushort)size : 0;
    // If the flush size is too large, we use two entries for start/end.
    if (entry->size == 0) {
        ++entry;
        entry->type = TRACE_TYPE_INSTR_FLUSH_END;
        entry->addr = start + size;
        entry->size = 0;
    }
    return (int)((byte *)entry + sizeof(trace_entry_t) - buf_ptr);
}

int
online_instru_t::append_thread_header(byte *buf_ptr, thread_id_t tid)
{
    byte *new_buf = buf_ptr;
    new_buf += append_tid(new_buf, tid);
    new_buf += append_pid(new_buf, dr_get_process_id());
    return (int)(new_buf - buf_ptr);
}

int
online_instru_t::append_unit_header(byte *buf_ptr, thread_id_t tid)
{
    byte *new_buf = buf_ptr;
    new_buf += append_tid(new_buf, tid);
    new_buf += append_marker(new_buf, TRACE_MARKER_TYPE_TIMESTAMP,
                             // Truncated to 32 bits for 32-bit: we live with it.
                             (uintptr_t)instru_t::get_timestamp());
    new_buf += append_marker(new_buf, TRACE_MARKER_TYPE_CPU_ID, instru_t::get_cpu_id());
    return (int)(new_buf - buf_ptr);
}

void
online_instru_t::insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where,
                                reg_id_t base, reg_id_t scratch, app_pc pc, int adjust)
{
    int disp = adjust + offsetof(trace_entry_t, addr);
#ifdef X86_32
    ptr_int_t val = (ptr_int_t)pc;
    MINSERT(ilist, where,
            INSTR_CREATE_mov_st(drcontext, OPND_CREATE_MEM32(base, disp),
                                OPND_CREATE_INT32((int)val)));
#else
    // For X86_64, we can't write the PC immed directly to memory and
    // skip the top half for a <4GB PC b/c if we're in the sentinel
    // region of the buffer we'll be leaving 0xffffffff in the top
    // half (i#1735).  Thus we go through a register on x86 (where we
    // can skip the top half), just like on ARM.
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)pc, opnd_create_reg(scratch),
                                     ilist, where, NULL, NULL);
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(base, disp),
                               opnd_create_reg(scratch)));
#endif
}

void
online_instru_t::insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                                  reg_id_t reg_ptr, reg_id_t reg_addr, int adjust,
                                  opnd_t ref)
{
    int disp = adjust + offsetof(trace_entry_t, addr);
    bool reg_ptr_used;
    insert_obtain_addr(drcontext, ilist, where, reg_addr, reg_ptr, ref, &reg_ptr_used);
    if (reg_ptr_used) {
        // drutil_insert_get_mem_addr clobbered reg_ptr, so we need to reload reg_ptr.
        insert_load_buf_ptr_(drcontext, ilist, where, reg_ptr);
    }
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, disp),
                               opnd_create_reg(reg_addr)));
}

void
online_instru_t::insert_save_type_and_size(void *drcontext, instrlist_t *ilist,
                                           instr_t *where, reg_id_t base,
                                           reg_id_t scratch, ushort type, ushort size,
                                           int adjust)
{
    int disp;
    if (offsetof(trace_entry_t, type) + 2 != offsetof(trace_entry_t, size)) {
        /* there is padding between type and size, so save them separately */
        disp = adjust + offsetof(trace_entry_t, type);
        scratch = reg_resize_to_opsz(scratch, OPSZ_2);
        /* save type */
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
                                      OPND_CREATE_INT16(type)));
        MINSERT(ilist, where,
                XINST_CREATE_store_2bytes(drcontext, OPND_CREATE_MEM16(base, disp),
                                          opnd_create_reg(scratch)));
        /* save size */
        disp = adjust + offsetof(trace_entry_t, size);
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
                                      OPND_CREATE_INT16(size)));
        MINSERT(ilist, where,
                XINST_CREATE_store_2bytes(drcontext, OPND_CREATE_MEM16(base, disp),
                                          opnd_create_reg(scratch)));
    } else {
        /* no padding, save type and size together */
        disp = adjust + offsetof(trace_entry_t, type);
#ifdef X86
        MINSERT(ilist, where,
                INSTR_CREATE_mov_st(drcontext, OPND_CREATE_MEM32(base, disp),
                                    OPND_CREATE_INT32(type | (size << 16))));
#elif defined(ARM)
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
                                      OPND_CREATE_INT(type)));
        MINSERT(ilist, where,
                INSTR_CREATE_movt(drcontext, opnd_create_reg(scratch),
                                  OPND_CREATE_INT(size)));
        MINSERT(ilist, where,
                XINST_CREATE_store(drcontext, OPND_CREATE_MEM32(base, disp),
                                   opnd_create_reg(scratch)));
#elif defined(AARCH64)
        scratch = reg_resize_to_opsz(scratch, OPSZ_4);
        /* MOVZ scratch, #type */
        MINSERT(ilist, where,
                INSTR_CREATE_movz(drcontext, opnd_create_reg(scratch),
                                  OPND_CREATE_INT(type), OPND_CREATE_INT8(0)));
        /* MOVK scratch, #size, LSL #16 */
        MINSERT(ilist, where,
                INSTR_CREATE_movk(drcontext, opnd_create_reg(scratch),
                                  OPND_CREATE_INT(size), OPND_CREATE_INT8(16)));
        MINSERT(ilist, where,
                XINST_CREATE_store(drcontext, OPND_CREATE_MEM32(base, disp),
                                   opnd_create_reg(scratch)));
#endif
    }
}

int
online_instru_t::instrument_memref(void *drcontext, instrlist_t *ilist, instr_t *where,
                                   reg_id_t reg_ptr, int adjust, instr_t *app, opnd_t ref,
                                   int ref_index, bool write, dr_pred_type_t pred)
{
    ushort type = (ushort)(write ? TRACE_TYPE_WRITE : TRACE_TYPE_READ);
    ushort size = (ushort)drutil_opnd_mem_size_in_bytes(ref, app);
    reg_id_t reg_tmp;
    drreg_status_t res =
        drreg_reserve_register(drcontext, ilist, where, reg_vector_, &reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    if (!memref_needs_full_info_)    // For full info we skip this for !pred
        instrlist_set_auto_predicate(ilist, pred);
    if (memref_needs_full_info_) {
        // When filtering we have to insert a PC entry for every memref.
        // The 0 size indicates it's a non-icache entry.
        insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                                  TRACE_TYPE_INSTR, 0, adjust);
        insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp,
                       // XXX: For repstr do we want tag insted of skipping rep prefix?
                       instr_get_app_pc(app), adjust);
        adjust += sizeof(trace_entry_t);
    }
    insert_save_addr(drcontext, ilist, where, reg_ptr, reg_tmp, adjust, ref);
    // Special handling for prefetch instruction
    if (instr_is_prefetch(app)) {
        type = instru_t::instr_to_prefetch_type(app);
        // Prefetch instruction may have zero sized mem reference.
        size = 1;
    } else if (instru_t::instr_is_flush(app)) {
        // XXX: OP_clflush invalidates all levels of the processor cache
        // hierarchy (data and instruction)
        type = TRACE_TYPE_DATA_FLUSH;
    }
    insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp, type, size,
                              adjust);
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    res = drreg_unreserve_register(drcontext, ilist, where, reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    return (adjust + sizeof(trace_entry_t));
}

int
online_instru_t::instrument_instr(void *drcontext, void *tag, void **bb_field,
                                  instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr,
                                  int adjust, instr_t *app)
{
    bool repstr_expanded = *bb_field != 0; // Avoid cl warning C4800.
    app_pc pc = repstr_expanded ? dr_fragment_app_pc(tag) : instr_get_app_pc(app);
    reg_id_t reg_tmp;
    drreg_status_t res =
        drreg_reserve_register(drcontext, ilist, where, reg_vector_, &reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    // To handle zero-iter repstr loops this routine is called at the top of the bb
    // where "app" is jecxz so we have to hardcode the rep str type and get length
    // from the tag.
    ushort type = repstr_expanded ? TRACE_TYPE_INSTR_MAYBE_FETCH
                                  : instr_to_instr_type(app, repstr_expanded);
    ushort size = repstr_expanded
        ? (ushort)decode_sizeof(drcontext, pc, NULL _IF_X86_64(NULL))
        : (ushort)instr_length(drcontext, app);
    insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp, type, size,
                              adjust);
    insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp, pc, adjust);
    res = drreg_unreserve_register(drcontext, ilist, where, reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    return (adjust + sizeof(trace_entry_t));
}

int
online_instru_t::instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                                    reg_id_t reg_ptr, int adjust, instr_t **delay_instrs,
                                    int num_delay_instrs)
{
    // Create and instrument for INSTR_BUNDLE
    trace_entry_t entry;
    int i;
    reg_id_t reg_tmp;
    drreg_status_t res =
        drreg_reserve_register(drcontext, ilist, where, reg_vector_, &reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    entry.type = TRACE_TYPE_INSTR_BUNDLE;
    entry.size = 0;
    for (i = 0; i < num_delay_instrs; i++) {
        // Fill instr size into bundle entry
        entry.length[entry.size++] = (char)instr_length(drcontext, delay_instrs[i]);
        // Instrument to add an INSTR_BUNDLE entry if bundle is full or last instr
        if (entry.size == sizeof(entry.length) || i == num_delay_instrs - 1) {
            insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                                      entry.type, entry.size, adjust);
            insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp, (app_pc)entry.addr,
                           adjust);
            adjust += sizeof(trace_entry_t);
            entry.size = 0;
        }
    }
    res = drreg_unreserve_register(drcontext, ilist, where, reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    return adjust;
}

void
online_instru_t::bb_analysis(void *drcontext, void *tag, void **bb_field,
                             instrlist_t *ilist, bool repstr_expanded)
{
    *bb_field = (void *)repstr_expanded;
}
