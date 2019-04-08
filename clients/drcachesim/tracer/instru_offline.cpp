/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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

/* instru_offline: inserts instrumentation for offline traces.
 */

#include "dr_api.h"
#include "drreg.h"
#include "drutil.h"
#include "drcovlib.h"
#include "instru.h"
#include "../common/trace_entry.h"
#include <new>
#include <limits.h> /* for USHRT_MAX */
#include <stddef.h> /* for offsetof */
#include <string.h> /* for strlen */

static const ptr_uint_t MAX_INSTR_COUNT = 64 * 1024;

void *(*offline_instru_t::user_load)(module_data_t *module);
int (*offline_instru_t::user_print)(void *data, char *dst, size_t max_len);
void (*offline_instru_t::user_free)(void *data);

offline_instru_t::offline_instru_t(void (*insert_load_buf)(void *, instrlist_t *,
                                                           instr_t *, reg_id_t),
                                   bool memref_needs_info, drvector_t *reg_vector,
                                   ssize_t (*write_file)(file_t file, const void *data,
                                                         size_t count),
                                   file_t module_file)
    : instru_t(insert_load_buf, memref_needs_info, reg_vector, sizeof(offline_entry_t))
    , write_file_func(write_file)
    , modfile(module_file)
{
    drcovlib_status_t res = drmodtrack_init();
    DR_ASSERT(res == DRCOVLIB_SUCCESS);
    DR_ASSERT(write_file != NULL);
    // Ensure every compiler is packing our struct how we want:
    DR_ASSERT(sizeof(offline_entry_t) == 8);

    res = drmodtrack_add_custom_data(load_custom_module_data, print_custom_module_data,
                                     NULL, free_custom_module_data);
    DR_ASSERT(res == DRCOVLIB_SUCCESS);
}

offline_instru_t::~offline_instru_t()
{
    drcovlib_status_t res;
    size_t size = 8192;
    char *buf;
    size_t wrote;
    do {
        buf = (char *)dr_global_alloc(size);
        res = drmodtrack_dump_buf(buf, size, &wrote);
        if (res == DRCOVLIB_SUCCESS) {
            ssize_t written = write_file_func(modfile, buf, wrote - 1 /*no null*/);
            DR_ASSERT(written == (ssize_t)wrote - 1);
        }
        dr_global_free(buf, size);
        size *= 2;
    } while (res == DRCOVLIB_ERROR_BUF_TOO_SMALL);
    res = drmodtrack_exit();
    DR_ASSERT(res == DRCOVLIB_SUCCESS);
}

void *
offline_instru_t::load_custom_module_data(module_data_t *module)
{
    void *user_data = nullptr;
    if (user_load != nullptr)
        user_data = (*user_load)(module);
    const char *name = dr_module_preferred_name(module);
    // For vdso we include the entire contents so we can decode it during
    // post-processing.
    // We use placement new for better isolation, esp w/ static linkage into the app.
    if ((name != nullptr &&
         (strstr(name, "linux-gate.so") == name ||
          strstr(name, "linux-vdso.so") == name)) ||
        (module->names.file_name != NULL && strcmp(name, "[vdso]") == 0)) {
        void *alloc = dr_global_alloc(sizeof(custom_module_data_t));
        return new (alloc) custom_module_data_t((const char *)module->start,
                                                module->end - module->start, user_data);
    } else if (user_data != nullptr) {
        void *alloc = dr_global_alloc(sizeof(custom_module_data_t));
        return new (alloc) custom_module_data_t(nullptr, 0, user_data);
    }
    return nullptr;
}

int
offline_instru_t::print_custom_module_data(void *data, char *dst, size_t max_len)
{
    custom_module_data_t *custom = (custom_module_data_t *)data;
    // We use ascii for the size to keep the module list human-readable except
    // for the few modules like vdso that have a binary blob.
    // We include a version #.
    if (custom == nullptr) {
        return dr_snprintf(dst, max_len, "v#%d,0,", CUSTOM_MODULE_VERSION);
    }
    char *cur = dst;
    int len = dr_snprintf(dst, max_len, "v#%d,%zu,", CUSTOM_MODULE_VERSION, custom->size);
    if (len < 0)
        return -1;
    cur += len;
    if (cur - dst + custom->size > max_len)
        return -1;
    if (custom->size > 0) {
        memcpy(cur, custom->base, custom->size);
        cur += custom->size;
    }
    if (user_print != nullptr) {
        int res = (*user_print)(custom->user_data, cur, max_len - (cur - dst));
        if (res == -1)
            return -1;
        cur += res;
    }
    return (int)(cur - dst);
}

void
offline_instru_t::free_custom_module_data(void *data)
{
    custom_module_data_t *custom = (custom_module_data_t *)data;
    if (custom == nullptr)
        return;
    if (user_free != nullptr)
        (*user_free)(custom->user_data);
    custom->~custom_module_data_t();
    dr_global_free(custom, sizeof(*custom));
}

bool
offline_instru_t::custom_module_data(void *(*load_cb)(module_data_t *module),
                                     int (*print_cb)(void *data, char *dst,
                                                     size_t max_len),
                                     void (*free_cb)(void *data))
{
    user_load = load_cb;
    user_print = print_cb;
    user_free = free_cb;
    return true;
}

trace_type_t
offline_instru_t::get_entry_type(byte *buf_ptr) const
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    switch (entry->addr.type) {
    case OFFLINE_TYPE_MEMREF: return TRACE_TYPE_READ;
    case OFFLINE_TYPE_MEMREF_HIGH: return TRACE_TYPE_READ;
    case OFFLINE_TYPE_PC: return TRACE_TYPE_INSTR;
    case OFFLINE_TYPE_THREAD: return TRACE_TYPE_THREAD;
    case OFFLINE_TYPE_PID: return TRACE_TYPE_PID;
    case OFFLINE_TYPE_TIMESTAMP: return TRACE_TYPE_THREAD; // Closest.
    case OFFLINE_TYPE_IFLUSH: return TRACE_TYPE_INSTR_FLUSH;
    }
    DR_ASSERT(false);
    return TRACE_TYPE_THREAD_EXIT; // Unknown: returning rarest entry.
}

size_t
offline_instru_t::get_entry_size(byte *buf_ptr) const
{
    // We don't know it: the post-processor adds it.
    return 0;
}

addr_t
offline_instru_t::get_entry_addr(byte *buf_ptr) const
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    return entry->addr.addr;
}

void
offline_instru_t::set_entry_addr(byte *buf_ptr, addr_t addr)
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    entry->addr.addr = addr;
}

int
offline_instru_t::append_pid(byte *buf_ptr, process_id_t pid)
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    entry->pid.type = OFFLINE_TYPE_PID;
    entry->pid.pid = pid;
    return sizeof(offline_entry_t);
}

int
offline_instru_t::append_tid(byte *buf_ptr, thread_id_t tid)
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    entry->tid.type = OFFLINE_TYPE_THREAD;
    entry->tid.tid = tid;
    return sizeof(offline_entry_t);
}

int
offline_instru_t::append_thread_exit(byte *buf_ptr, thread_id_t tid)
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    entry->extended.type = OFFLINE_TYPE_EXTENDED;
    entry->extended.ext = OFFLINE_EXT_TYPE_FOOTER;
    entry->extended.valueA = 0;
    entry->extended.valueB = 0;
    return sizeof(offline_entry_t);
}

int
offline_instru_t::append_marker(byte *buf_ptr, trace_marker_type_t type, uintptr_t val)
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    entry->extended.type = OFFLINE_TYPE_EXTENDED;
    entry->extended.ext = OFFLINE_EXT_TYPE_MARKER;
    DR_ASSERT((int)type < 1 << EXT_VALUE_B_BITS);
    entry->extended.valueB = type;
    DR_ASSERT((unsigned long long)val < 1ULL << EXT_VALUE_A_BITS);
    entry->extended.valueA = val;
    return sizeof(offline_entry_t);
}

int
offline_instru_t::append_iflush(byte *buf_ptr, addr_t start, size_t size)
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    entry->addr.type = OFFLINE_TYPE_IFLUSH;
    entry->addr.addr = start;
    ++entry;
    entry->addr.type = OFFLINE_TYPE_IFLUSH;
    entry->addr.addr = start + size;
    return 2 * sizeof(offline_entry_t);
}

int
offline_instru_t::append_thread_header(byte *buf_ptr, thread_id_t tid)
{
    byte *new_buf = buf_ptr;
    offline_entry_t *entry = (offline_entry_t *)new_buf;
    entry->extended.type = OFFLINE_TYPE_EXTENDED;
    entry->extended.ext = OFFLINE_EXT_TYPE_HEADER;
    entry->extended.valueA = OFFLINE_FILE_VERSION;
    entry->extended.valueB = 0;
    new_buf += sizeof(*entry);
    new_buf += append_tid(new_buf, tid);
    new_buf += append_pid(new_buf, dr_get_process_id());
    return (int)(new_buf - buf_ptr);
}

int
offline_instru_t::append_unit_header(byte *buf_ptr, thread_id_t tid)
{
    byte *new_buf = buf_ptr;
    offline_entry_t *entry = (offline_entry_t *)new_buf;
    entry->timestamp.type = OFFLINE_TYPE_TIMESTAMP;
    entry->timestamp.usec = instru_t::get_timestamp();
    new_buf += sizeof(*entry);
    new_buf += append_marker(new_buf, TRACE_MARKER_TYPE_CPU_ID, instru_t::get_cpu_id());
    return (int)(new_buf - buf_ptr);
}

int
offline_instru_t::insert_save_entry(void *drcontext, instrlist_t *ilist, instr_t *where,
                                    reg_id_t reg_ptr, reg_id_t scratch, int adjust,
                                    offline_entry_t *entry)
{
    int disp = adjust;
#ifdef X64
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)entry->combined_value,
                                     opnd_create_reg(scratch), ilist, where, NULL, NULL);
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, disp),
                               opnd_create_reg(scratch)));
#else
    instrlist_insert_mov_immed_ptrsz(drcontext, (int)entry->combined_value,
                                     opnd_create_reg(scratch), ilist, where, NULL, NULL);
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, disp),
                               opnd_create_reg(scratch)));
    instrlist_insert_mov_immed_ptrsz(drcontext, (int)(entry->combined_value >> 32),
                                     opnd_create_reg(scratch), ilist, where, NULL, NULL);
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, disp + 4),
                               opnd_create_reg(scratch)));
#endif
    return sizeof(offline_entry_t);
}

int
offline_instru_t::insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where,
                                 reg_id_t reg_ptr, reg_id_t scratch, int adjust,
                                 app_pc pc, uint instr_count)
{
    app_pc modbase;
    uint modidx;
    if (drmodtrack_lookup(drcontext, pc, &modidx, &modbase) != DRCOVLIB_SUCCESS) {
        // FIXME i#2062: add non-module support.  The plan for instrs is to have
        // one entry w/ the start abs pc, and subsequent entries that pack the instr
        // length for 10 instrs, 4 bits each, into a pc.modoffs field.  We will
        // also need to store the type (read/write/prefetch*) and size for the
        // memrefs.
        modidx = 0;
        modbase = pc;
    }
    offline_entry_t entry;
    entry.pc.type = OFFLINE_TYPE_PC;
    // We put the ARM vs Thumb mode into the modoffs to ensure proper decoding.
    uint64_t modoffs = dr_app_pc_as_jump_target(instr_get_isa_mode(where), pc) - modbase;
    // Check that the values we want to assign to the bitfields in offline_entry_t do not
    // overflow. In i#2956 we observed an overflow for the modidx field.
    DR_ASSERT(modoffs < uint64_t(1) << PC_MODOFFS_BITS);
    DR_ASSERT(modidx < uint64_t(1) << PC_MODIDX_BITS);
    DR_ASSERT(instr_count < uint64_t(1) << PC_INSTR_COUNT_BITS);
    entry.pc.modoffs = modoffs;
    entry.pc.modidx = modidx;
    entry.pc.instr_count = instr_count;
    return insert_save_entry(drcontext, ilist, where, reg_ptr, scratch, adjust, &entry);
}

int
offline_instru_t::insert_save_type_and_size(void *drcontext, instrlist_t *ilist,
                                            instr_t *where, reg_id_t reg_ptr,
                                            reg_id_t scratch, int adjust, instr_t *app,
                                            opnd_t ref, bool write)
{
    ushort type = (ushort)(write ? TRACE_TYPE_WRITE : TRACE_TYPE_READ);
    ushort size = (ushort)drutil_opnd_mem_size_in_bytes(ref, app);
    if (instr_is_prefetch(app)) {
        type = instru_t::instr_to_prefetch_type(app);
        // Prefetch instruction may have zero sized mem reference.
        size = 1;
    }
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_MEMINFO;
    entry.extended.valueB = type;
    entry.extended.valueA = size;
    return insert_save_entry(drcontext, ilist, where, reg_ptr, scratch, adjust, &entry);
}

int
offline_instru_t::insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                                   reg_id_t reg_ptr, int adjust, opnd_t ref, bool write)
{
    int disp = adjust;
    reg_id_t reg_addr = DR_REG_NULL;
    bool reserved = false;
    bool have_addr = false;
    drreg_status_t res;
#ifdef X86
    if (opnd_is_near_base_disp(ref) && opnd_get_base(ref) != DR_REG_NULL &&
        opnd_get_index(ref) == DR_REG_NULL) {
        /* Optimization: to avoid needing a scratch reg to lea into, we simply
         * store the base reg directly and add the disp during post-processing.
         * We only do this for x86 for now to avoid dealing with complexities of
         * PC bases.
         */
        reg_addr = opnd_get_base(ref);
        if (opnd_get_base(ref) == reg_ptr) {
            /* Here we do need a scratch reg, and raw2trace can't identify this case:
             * so we set disp to 0 and use the regular path below.
             */
            opnd_set_disp(&ref, 0);
        } else
            have_addr = true;
    }
#endif
    if (!have_addr) {
        res = drreg_reserve_register(drcontext, ilist, where, reg_vector, &reg_addr);
        DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
        reserved = true;
        bool reg_ptr_used;
        insert_obtain_addr(drcontext, ilist, where, reg_addr, reg_ptr, ref,
                           &reg_ptr_used);
        if (reg_ptr_used) {
            // Re-load because reg_ptr was clobbered.
            insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
        }
        reserved = true;
    }
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, disp),
                               opnd_create_reg(reg_addr)));
    if (reserved) {
        res = drreg_unreserve_register(drcontext, ilist, where, reg_addr);
        DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    }
    return sizeof(offline_entry_t);
}

// The caller should already have verified that either instr_reads_memory() or
// instr_writes_memory().
bool
offline_instru_t::instr_has_multiple_different_memrefs(instr_t *instr)
{
    int count = 0;
    opnd_t first_memref = opnd_create_null();
    for (int i = 0; i < instr_num_srcs(instr); i++) {
        opnd_t op = instr_get_src(instr, i);
        if (opnd_is_memory_reference(op)) {
            if (count == 0)
                first_memref = op;
            else if (!opnd_same(op, first_memref))
                return true;
            ++count;
        }
    }
    for (int i = 0; i < instr_num_dsts(instr); i++) {
        opnd_t op = instr_get_dst(instr, i);
        if (opnd_is_memory_reference(op)) {
            if (count == 0)
                first_memref = op;
            else if (!opnd_same(op, first_memref))
                return true;
            ++count;
        }
    }
    return false;
}

int
offline_instru_t::instrument_memref(void *drcontext, instrlist_t *ilist, instr_t *where,
                                    reg_id_t reg_ptr, int adjust, instr_t *app,
                                    opnd_t ref, bool write, dr_pred_type_t pred)
{
    // Post-processor distinguishes read, write, prefetch, flush, and finds size.
    if (!memref_needs_full_info) // For full info we skip this for !pred
        instrlist_set_auto_predicate(ilist, pred);
    // We allow either 0 or all 1's as the type so no need to write anything else,
    // unless a filter is in place in which case we need a PC entry.
    if (memref_needs_full_info) {
        reg_id_t reg_tmp;
        drreg_status_t res =
            drreg_reserve_register(drcontext, ilist, where, reg_vector, &reg_tmp);
        DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
        adjust += insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp, adjust,
                                 instr_get_app_pc(app), 0);
        if (instr_has_multiple_different_memrefs(app)) {
            // i#2756: post-processing can't determine which memref this is, so we
            // insert a type entry.  (For instrs w/ identical memrefs, like an ALU
            // operation, the addresses are the same and the load will pass the
            // filter first and be found first in post-processing.)
            adjust += insert_save_type_and_size(drcontext, ilist, where, reg_ptr, reg_tmp,
                                                adjust, app, ref, write);
        }
        res = drreg_unreserve_register(drcontext, ilist, where, reg_tmp);
        DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    }
    adjust += insert_save_addr(drcontext, ilist, where, reg_ptr, adjust, ref, write);
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    return adjust;
}

// We stored the instr count in *bb_field in bb_analysis().
int
offline_instru_t::instrument_instr(void *drcontext, void *tag, void **bb_field,
                                   instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr,
                                   int adjust, instr_t *app)
{
    app_pc pc;
    reg_id_t reg_tmp;
    if (!memref_needs_full_info) {
        // We write just once per bb, if not filtering.
        if ((ptr_uint_t)*bb_field > MAX_INSTR_COUNT)
            return adjust;
        pc = dr_fragment_app_pc(tag);
    } else {
        // XXX: For repstr do we want tag insted of skipping rep prefix?
        pc = instr_get_app_pc(app);
    }
    drreg_status_t res =
        drreg_reserve_register(drcontext, ilist, where, reg_vector, &reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    adjust += insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp, adjust, pc,
                             memref_needs_full_info ? 1 : (uint)(ptr_uint_t)*bb_field);
    if (!memref_needs_full_info)
        *(ptr_uint_t *)bb_field = MAX_INSTR_COUNT + 1;
    res = drreg_unreserve_register(drcontext, ilist, where, reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    return adjust;
}

int
offline_instru_t::instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                                     reg_id_t reg_ptr, int adjust, instr_t **delay_instrs,
                                     int num_delay_instrs)
{
    // The post-processor fills in all instr info other than our once-per-bb entry.
    return adjust;
}

void
offline_instru_t::bb_analysis(void *drcontext, void *tag, void **bb_field,
                              instrlist_t *ilist, bool repstr_expanded)
{
    instr_t *instr;
    ptr_uint_t count = 0;
    app_pc last_xl8 = NULL;
    if (repstr_expanded) {
        // The same-translation check below is not sufficient as drutil uses
        // two different translations to deal with complexities.
        // Thus we hardcode this.
        count = 1;
    } else {
        for (instr = instrlist_first_app(ilist); instr != NULL;
             instr = instr_get_next_app(instr)) {
            // To deal with app2app changes, we do not double-count consecutive instrs
            // with the same translation.
            if (instr_get_app_pc(instr) != last_xl8)
                ++count;
            last_xl8 = instr_get_app_pc(instr);
        }
    }
    *bb_field = (void *)count;
}
