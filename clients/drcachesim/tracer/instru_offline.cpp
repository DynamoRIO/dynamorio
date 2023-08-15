/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

#include <stddef.h> /* for offsetof */
#include <string.h> /* for strlen */
#include <sys/types.h>

#include <atomic>
#include <cstdint>
#include <new>

#include "dr_api.h"
#include "drcovlib.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drvector.h"
#include "trace_entry.h"
#include "utils.h"
#include "instru.h"

namespace dynamorio {
namespace drmemtrace {

static const uint MAX_INSTR_COUNT = 64 * 1024;

void *(*offline_instru_t::user_load_)(module_data_t *module, int seg_idx);
int (*offline_instru_t::user_print_)(void *data, char *dst, size_t max_len);
void (*offline_instru_t::user_free_)(void *data);

// This constructor is for use in post-processing when we just need the
// elision utility functions.
offline_instru_t::offline_instru_t()
    : instru_t(nullptr, nullptr, sizeof(offline_entry_t))
    , write_file_func_(nullptr)
{
    // We can't use drmgr in standalone mode, but for post-processing it's just us,
    // so we just pick a note value.
    elide_memref_note_ = 1;
    standalone_ = true;
}

offline_instru_t::offline_instru_t(
    void (*insert_load_buf)(void *, instrlist_t *, instr_t *, reg_id_t),
    drvector_t *reg_vector,
    ssize_t (*write_file)(file_t file, const void *data, size_t count),
    file_t module_file, file_t encoding_file, bool disable_optimizations,
    void (*log)(uint level, const char *fmt, ...))
    : instru_t(insert_load_buf, reg_vector, sizeof(offline_entry_t),
               disable_optimizations)
    , write_file_func_(write_file)
    , modfile_(module_file)
    , log_(log)
    , encoding_file_(encoding_file)
{
    drcovlib_status_t res = drmodtrack_init();
    DR_ASSERT(res == DRCOVLIB_SUCCESS);
    DR_ASSERT(write_file != NULL);
    // Ensure every compiler is packing our struct how we want:
    DR_ASSERT(sizeof(offline_entry_t) == 8);

    res = drmodtrack_add_custom_data(load_custom_module_data, print_custom_module_data,
                                     NULL, free_custom_module_data);
    DR_ASSERT(res == DRCOVLIB_SUCCESS);

    if (!drmgr_init())
        DR_ASSERT(false);
    elide_memref_note_ = drmgr_reserve_note_range(1);
    DR_ASSERT(elide_memref_note_ != DRMGR_NOTE_NONE);

    uint64 max_bb_instrs;
    if (!dr_get_integer_option("max_bb_instrs", &max_bb_instrs))
        max_bb_instrs = 256; /* current default */
    max_block_encoding_size_ = static_cast<int>(max_bb_instrs) * MAX_INSTR_LENGTH;
    encoding_lock_ = dr_mutex_create();
    encoding_buf_sz_ = ALIGN_FORWARD(max_block_encoding_size_ * 10, dr_page_size());
    encoding_buf_start_ = reinterpret_cast<byte *>(
        dr_raw_mem_alloc(encoding_buf_sz_, DR_MEMPROT_READ | DR_MEMPROT_WRITE, nullptr));
    encoding_buf_ptr_ = encoding_buf_start_;
    // Write out the header which is just a 64-bit version.
    *reinterpret_cast<uint64_t *>(encoding_buf_ptr_) = ENCODING_FILE_VERSION;
    encoding_buf_ptr_ += sizeof(uint64_t);
}

offline_instru_t::~offline_instru_t()
{
    if (standalone_)
        return;

    dr_mutex_lock(encoding_lock_);
    flush_instr_encodings();
    dr_raw_mem_free(encoding_buf_start_, encoding_buf_sz_);
    dr_mutex_unlock(encoding_lock_);
    dr_mutex_destroy(encoding_lock_);
    log_(1, "Wrote " UINT64_FORMAT_STRING " bytes to encoding file\n",
         encoding_bytes_written_);

    drcovlib_status_t res;
    size_t size = 8192;
    char *buf;
    size_t wrote;
    do {
        buf = (char *)dr_global_alloc(size);
        res = drmodtrack_dump_buf(buf, size, &wrote);
        if (res == DRCOVLIB_SUCCESS) {
            ssize_t written = write_file_func_(modfile_, buf, wrote - 1 /*no null*/);
            DR_ASSERT(written == (ssize_t)wrote - 1);
        }
        dr_global_free(buf, size);
        size *= 2;
    } while (res == DRCOVLIB_ERROR_BUF_TOO_SMALL);
    res = drmodtrack_exit();
    DR_ASSERT(res == DRCOVLIB_SUCCESS);
    drmgr_exit();
}

void *
offline_instru_t::load_custom_module_data(module_data_t *module, int seg_idx)
{
    void *user_data = nullptr;
    if (user_load_ != nullptr)
        user_data = (*user_load_)(module, seg_idx);
    const char *name = dr_module_preferred_name(module);
    // For vdso we include the entire contents so we can decode it during
    // post-processing.
    // We use placement new for better isolation, esp w/ static linkage into the app.
    if ((name != nullptr &&
         (strstr(name, "linux-gate.so") == name ||
          strstr(name, "linux-vdso.so") == name)) ||
        (module->names.file_name != NULL && strcmp(name, "[vdso]") == 0)) {
        void *alloc = dr_global_alloc(sizeof(custom_module_data_t));
#ifdef WINDOWS
        byte *start = module->start;
        byte *end = module->end;
#else
        byte *start =
            (module->num_segments > 0) ? module->segments[seg_idx].start : module->start;
        byte *end =
            (module->num_segments > 0) ? module->segments[seg_idx].end : module->end;
#endif
        return new (alloc)
            custom_module_data_t((const char *)start, end - start, user_data);
    } else if (user_data != nullptr) {
        void *alloc = dr_global_alloc(sizeof(custom_module_data_t));
        return new (alloc) custom_module_data_t(nullptr, 0, user_data);
    }
    return nullptr;
}

int
offline_instru_t::print_module_data_fields(
    char *dst, size_t max_len, const void *custom_data, size_t custom_size,
    int (*user_print_cb)(void *data, char *dst, size_t max_len), void *user_cb_data)
{
    char *cur = dst;
    int len = dr_snprintf(dst, max_len, "v#%d,%zu,", CUSTOM_MODULE_VERSION, custom_size);
    if (len < 0)
        return -1;
    cur += len;
    if (cur - dst + custom_size > max_len)
        return -1;
    if (custom_size > 0) {
        memcpy(cur, custom_data, custom_size);
        cur += custom_size;
    }
    if (user_print_cb != nullptr) {
        int res = (*user_print_cb)(user_cb_data, cur, max_len - (cur - dst));
        if (res == -1)
            return -1;
        cur += res;
    }
    return (int)(cur - dst);
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
    return print_module_data_fields(dst, max_len, custom->base, custom->size, user_print_,
                                    custom->user_data);
}

void
offline_instru_t::free_custom_module_data(void *data)
{
    custom_module_data_t *custom = (custom_module_data_t *)data;
    if (custom == nullptr)
        return;
    if (user_free_ != nullptr)
        (*user_free_)(custom->user_data);
    custom->~custom_module_data_t();
    dr_global_free(custom, sizeof(*custom));
}

bool
offline_instru_t::custom_module_data(void *(*load_cb)(module_data_t *module, int seg_idx),
                                     int (*print_cb)(void *data, char *dst,
                                                     size_t max_len),
                                     void (*free_cb)(void *data))
{
    user_load_ = load_cb;
    user_print_ = print_cb;
    user_free_ = free_cb;
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
    case OFFLINE_TYPE_EXTENDED: return TRACE_TYPE_MARKER; // Closest.
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

int
offline_instru_t::get_instr_count(byte *buf_ptr) const
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    if (entry->addr.type != OFFLINE_TYPE_PC)
        return 0;
    // TODO i#3995: We should *not* count "non-fetched" instrs so we'll match
    // hardware performance counters.
    // Xref i#4948 and i#4915 on getting rid of "non-fetched" instrs.
    return entry->pc.instr_count;
}

addr_t
offline_instru_t::get_entry_addr(void *drcontext, byte *buf_ptr) const
{
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    if (entry->addr.type == OFFLINE_TYPE_PC) {
        // XXX i#4014: Use caching to avoid lookup for last queried modbase.
        app_pc modbase;
        if (drmodtrack_lookup_pc_from_index(drcontext, entry->pc.modidx, &modbase) !=
            DRCOVLIB_SUCCESS)
            return 0;
        return reinterpret_cast<addr_t>(modbase) + static_cast<addr_t>(entry->pc.modoffs);
    }
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
    int extra_size = 0;
#ifdef X64
    if ((unsigned long long)val >= 1ULL << EXT_VALUE_A_BITS) {
        // We need two entries.
        // XXX: What we should do is change these types to signed so we can avoid
        // two entries for small negative numbers.  That requires a version bump
        // though which adds complexity for backward compatibility.
        DR_ASSERT(type != TRACE_MARKER_TYPE_SPLIT_VALUE);
        extra_size = append_marker(buf_ptr, TRACE_MARKER_TYPE_SPLIT_VALUE, val >> 32);
        buf_ptr += extra_size;
        val = (uint)val;
    }
#else
    // XXX i#5634: We're truncating timestamps and other values by limiting to
    // pointer-sized payloads: what we should do is use multiple markers (need up to 3)
    // to support 64-bit values in 32-bit builds.  However, this means we need an
    // analysis-tool-visible extended-payload marker type, or maybe make the reader
    // hide that from the user.
#endif
    offline_entry_t *entry = (offline_entry_t *)buf_ptr;
    entry->extended.valueA = val;
    DR_ASSERT(entry->extended.valueA == val);
    entry->extended.type = OFFLINE_TYPE_EXTENDED;
    entry->extended.ext = OFFLINE_EXT_TYPE_MARKER;
    DR_ASSERT((uint)type < 1 << EXT_VALUE_B_BITS);
    entry->extended.valueB = type;
    return sizeof(offline_entry_t) + extra_size;
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
offline_instru_t::append_thread_header(byte *buf_ptr, thread_id_t tid,
                                       offline_file_type_t file_type)
{
    byte *new_buf = buf_ptr;
    offline_entry_t *entry = (offline_entry_t *)new_buf;
    entry->extended.type = OFFLINE_TYPE_EXTENDED;
    entry->extended.ext = OFFLINE_EXT_TYPE_HEADER;
    entry->extended.valueA = file_type;
    entry->extended.valueB = OFFLINE_FILE_VERSION;
    new_buf += sizeof(*entry);
    new_buf += append_tid(new_buf, tid);
    new_buf += append_pid(new_buf, dr_get_process_id());
    new_buf += append_marker(new_buf, TRACE_MARKER_TYPE_CACHE_LINE_SIZE,
                             proc_get_cache_line_size());
    new_buf += append_marker(new_buf, TRACE_MARKER_TYPE_PAGE_SIZE, dr_page_size());
    return (int)(new_buf - buf_ptr);
}

int
offline_instru_t::append_thread_header(byte *buf_ptr, thread_id_t tid)
{
    return append_thread_header(buf_ptr, tid, OFFLINE_FILE_TYPE_DEFAULT);
}

int
offline_instru_t::append_unit_header(byte *buf_ptr, thread_id_t tid, ptr_int_t window)
{
    byte *new_buf = buf_ptr;
    offline_entry_t *entry = (offline_entry_t *)new_buf;
    entry->timestamp.type = OFFLINE_TYPE_TIMESTAMP;
    uint64 frozen = frozen_timestamp_.load(std::memory_order_acquire);
    entry->timestamp.usec = frozen != 0 ? frozen : instru_t::get_timestamp();
    new_buf += sizeof(*entry);
    if (window >= 0)
        new_buf += append_marker(new_buf, TRACE_MARKER_TYPE_WINDOW_ID, (uintptr_t)window);
    new_buf += append_marker(new_buf, TRACE_MARKER_TYPE_CPU_ID, instru_t::get_cpu_id());
    return (int)(new_buf - buf_ptr);
}

bool
offline_instru_t::refresh_unit_header_timestamp(byte *buf_ptr, uint64 min_timestamp)
{
    offline_entry_t *stamp = reinterpret_cast<offline_entry_t *>(buf_ptr);
    DR_ASSERT(stamp->timestamp.type == OFFLINE_TYPE_TIMESTAMP);
    if (stamp->timestamp.usec < min_timestamp) {
        log_(2, "%s: replacing " UINT64_FORMAT_STRING " with " UINT64_FORMAT_STRING "\n",
             __FUNCTION__, stamp->timestamp.usec, min_timestamp);
        stamp->timestamp.usec = min_timestamp;
        return true;
    }
    return false;
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

uint64_t
offline_instru_t::get_modoffs(void *drcontext, app_pc pc, OUT uint *modidx)
{
    app_pc modbase;
    if (drmodtrack_lookup(drcontext, pc, modidx, &modbase) != DRCOVLIB_SUCCESS)
        return 0;
    return pc - modbase;
}

// Caller must hold the encoding_lock.
void
offline_instru_t::flush_instr_encodings()
{
    DR_ASSERT(dr_mutex_self_owns(encoding_lock_));
    size_t size = encoding_buf_ptr_ - encoding_buf_start_;
    if (size == 0)
        return;
    ssize_t written = write_file_func_(encoding_file_, encoding_buf_start_, size);
    log_(2, "%s: Wrote %zu/%zu bytes to encoding file\n", __FUNCTION__, written, size);
    DR_ASSERT(written == static_cast<ssize_t>(size));
    encoding_buf_ptr_ = encoding_buf_start_;
    encoding_bytes_written_ += written;
}

void
offline_instru_t::record_instr_encodings(void *drcontext, app_pc tag_pc,
                                         per_block_t *per_block, instrlist_t *ilist)
{
    dr_mutex_lock(encoding_lock_);
    per_block->id = encoding_id_++;

    if (encoding_buf_ptr_ + max_block_encoding_size_ >=
        encoding_buf_start_ + encoding_buf_sz_) {
        flush_instr_encodings();
    }
    byte *buf_start = encoding_buf_ptr_;
    byte *buf = buf_start;
    buf += sizeof(encoding_entry_t);

    bool in_emulation_region = false;
    for (instr_t *instr = instrlist_first(ilist); instr != NULL;
         instr = instr_get_next(instr)) {
        instr_t *to_copy = nullptr;
        emulated_instr_t emulation_info = { sizeof(emulation_info), 0 };
        if (in_emulation_region) {
            if (drmgr_is_emulation_end(instr))
                in_emulation_region = false;
        } else if (drmgr_is_emulation_start(instr)) {
            bool ok = drmgr_get_emulated_instr_data(instr, &emulation_info);
            DR_ASSERT(ok);
            to_copy = emulation_info.instr;
            in_emulation_region = true;
        } else if (instr_is_app(instr)) {
            to_copy = instr;
        }
        if (to_copy == nullptr)
            continue;
        // To handle application code hooked by DR we cannot just copy from
        // instr_get_app_pc(): we have to encode.  Nearly all the time this
        // will be a pure memcpy so this only incurs an actual encoding walk
        // for the hooked level 4 instrs.
        byte *end_pc =
            instr_encode_to_copy(drcontext, to_copy, buf, instr_get_app_pc(to_copy));
        DR_ASSERT(end_pc != nullptr);
        buf = end_pc;
        DR_ASSERT(buf < encoding_buf_start_ + encoding_buf_sz_);
    }

    encoding_entry_t *enc = reinterpret_cast<encoding_entry_t *>(buf_start);
    enc->length = buf - buf_start;
    enc->id = per_block->id;
    // We put the ARM vs Thumb mode into the modoffs to ensure proper decoding.
    enc->start_pc = reinterpret_cast<uint64_t>(
        dr_app_pc_as_jump_target(instr_get_isa_mode(instrlist_first(ilist)), tag_pc));
    log_(2, "%s: Recorded %zu bytes for id " UINT64_FORMAT_STRING " @ %p\n", __FUNCTION__,
         enc->length, enc->id, tag_pc);

    encoding_buf_ptr_ += enc->length;
    dr_mutex_unlock(encoding_lock_);
}

int
offline_instru_t::insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where,
                                 reg_id_t reg_ptr, reg_id_t scratch, int adjust,
                                 app_pc pc, uint instr_count, per_block_t *per_block)
{
    offline_entry_t entry;
    entry.pc.type = OFFLINE_TYPE_PC;
    app_pc modbase;
    uint modidx;
    uint64_t modoffs;
    if (drmodtrack_lookup(drcontext, pc, &modidx, &modbase) == DRCOVLIB_SUCCESS) {
        // TODO i#2062: We need to also identify modified library code and record
        // its encodings.  The plan is to augment drmodtrack to track this for us;
        // for now we will incorrectly use the original bits in the trace.
        //
        // We put the ARM vs Thumb mode into the modoffs to ensure proper decoding.
        modoffs = dr_app_pc_as_jump_target(instr_get_isa_mode(where), pc) - modbase;
        DR_ASSERT(modidx != PC_MODIDX_INVALID);
    } else {
        modidx = PC_MODIDX_INVALID;
        // For generated code we store the id for matching with the encodings recorded
        // into the encoding file.
        modoffs = per_block->id;
    }
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
    } else if (instr_is_flush(app)) {
        type = instru_t::instr_to_flush_type(app);
    }
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_MEMINFO;
    entry.extended.valueB = type;
    entry.extended.valueA = size;
    return insert_save_entry(drcontext, ilist, where, reg_ptr, scratch, adjust, &entry);
}

bool
offline_instru_t::opnd_disp_is_elidable(opnd_t memop)
{
    return !disable_optimizations_ && opnd_is_near_base_disp(memop) &&
        opnd_get_base(memop) != DR_REG_NULL &&
        opnd_get_index(memop) == DR_REG_NULL
#ifdef AARCH64
        /* On AArch64 we cannot directly store SP to memory. */
        && opnd_get_base(memop) != DR_REG_SP
#elif defined(AARCH32)
        /* Avoid complexities with PC bases which are completely elided separately. */
        && opnd_get_base(memop) != DR_REG_PC
#endif
        ;
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
    if (opnd_disp_is_elidable(ref)) {
        /* Optimization: to avoid needing a scratch reg to lea into, we simply
         * store the base reg directly and add the disp during post-processing.
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
    if (!have_addr) {
        res = drreg_reserve_register(drcontext, ilist, where, reg_vector_, &reg_addr);
        DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
        reserved = true;
        bool reg_ptr_used;
        insert_obtain_addr(drcontext, ilist, where, reg_addr, reg_ptr, ref,
                           &reg_ptr_used);
        if (reg_ptr_used) {
            // Re-load because reg_ptr was clobbered.
            insert_load_buf_ptr_(drcontext, ilist, where, reg_ptr);
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
offline_instru_t::instrument_memref(void *drcontext, void *bb_field, instrlist_t *ilist,
                                    instr_t *where, reg_id_t reg_ptr, int adjust,
                                    instr_t *app, opnd_t ref, int ref_index, bool write,
                                    dr_pred_type_t pred, bool memref_needs_full_info)
{
    // Check whether we can elide this address.
    // We expect our labels to be at "where" due to drbbdup's handling of block-final
    // instrs, but for exclusive store post-instr insertion we make sure we walk
    // across that app instr.
    for (instr_t *prev = instr_get_prev(where);
         prev != nullptr && (!instr_is_app(prev) || instr_is_exclusive_store(prev));
         prev = instr_get_prev(prev)) {
        int elided_index;
        bool elided_is_store;
        if (label_marks_elidable(prev, &elided_index, nullptr, &elided_is_store,
                                 nullptr) &&
            elided_index == ref_index && elided_is_store == write) {
            return adjust;
        }
    }
    // Post-processor distinguishes read, write, prefetch, flush, and finds size.
    if (!memref_needs_full_info) // For full info we skip this for !pred
        instrlist_set_auto_predicate(ilist, pred);
    // We allow either 0 or all 1's as the type so no need to write anything else,
    // unless a filter is in place in which case we need a PC entry.
    if (memref_needs_full_info) {
        per_block_t *per_block = reinterpret_cast<per_block_t *>(bb_field);
        reg_id_t reg_tmp;
        drreg_status_t res =
            drreg_reserve_register(drcontext, ilist, where, reg_vector_, &reg_tmp);
        DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
        adjust += insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp, adjust,
                                 instr_get_app_pc(app), 0, per_block);
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

// We stored the instr count in bb_field==per_block_t in bb_analysis().
int
offline_instru_t::instrument_instr(void *drcontext, void *tag, void *bb_field,
                                   instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr,
                                   int adjust, instr_t *app, bool memref_needs_full_info,
                                   uintptr_t mode)
{
    per_block_t *per_block = reinterpret_cast<per_block_t *>(bb_field);
    app_pc pc;
    reg_id_t reg_tmp;
    if (!memref_needs_full_info) {
        // We write just once per bb, if not filtering.
        if (per_block->instr_count > MAX_INSTR_COUNT)
            return adjust;
        pc = dr_fragment_app_pc(tag);
    } else {
        DR_ASSERT(instr_is_app(app));
        pc = instr_get_app_pc(app);
    }
    drreg_status_t res =
        drreg_reserve_register(drcontext, ilist, where, reg_vector_, &reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    adjust += insert_save_pc(
        drcontext, ilist, where, reg_ptr, reg_tmp, adjust, pc,
        memref_needs_full_info ? 1 : static_cast<uint>(per_block->instr_count),
        per_block);
    if (!memref_needs_full_info)
        per_block->instr_count = MAX_INSTR_COUNT + 1;
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

int
offline_instru_t::instrument_instr_encoding(void *drcontext, void *tag, void *bb_field,
                                            instrlist_t *ilist, instr_t *where,
                                            reg_id_t reg_ptr, int adjust, instr_t *app)
{
    // We emit non-module-code or modified-module-code encodings separately in
    // record_instr_encodings().  Encodings for static code are added in the
    // post-processor.
    return adjust;
}

int
offline_instru_t::instrument_rseq_entry(void *drcontext, instrlist_t *ilist,
                                        instr_t *where, instr_t *rseq_label,
                                        reg_id_t reg_ptr, int adjust)
{
    dr_instr_label_data_t *data = instr_get_label_data_area(rseq_label);
    reg_id_t reg_tmp;
    drreg_status_t res =
        drreg_reserve_register(drcontext, ilist, where, reg_vector_, &reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    // We may need 2 entries for our marker.  We write the entry marker with payload
    // data[0]==rseq end.  We do not use a separate marker to write data[1]==rseq
    // handler as an abort marker will have the handler.
    static constexpr int RSEQ_LABEL_END_PC_INDEX = 0;
    offline_entry_t entries[2];
    int size = append_marker((byte *)entries, TRACE_MARKER_TYPE_RSEQ_ENTRY,
                             data->data[RSEQ_LABEL_END_PC_INDEX]);
    DR_ASSERT(size % sizeof(offline_entry_t) == 0);
    size /= sizeof(offline_entry_t);
    DR_ASSERT(size <= static_cast<int>(sizeof(entries)));
    for (int i = 0; i < size; i++) {
        adjust += insert_save_entry(drcontext, ilist, where, reg_ptr, reg_tmp, adjust,
                                    &entries[i]);
    }
    res = drreg_unreserve_register(drcontext, ilist, where, reg_tmp);
    DR_ASSERT(res == DRREG_SUCCESS); // Can't recover.
    return adjust;
}

void
offline_instru_t::bb_analysis(void *drcontext, void *tag, void **bb_field,
                              instrlist_t *ilist, bool repstr_expanded,
                              bool memref_needs_full_info)
{
    per_block_t *per_block =
        reinterpret_cast<per_block_t *>(dr_thread_alloc(drcontext, sizeof(*per_block)));
    *bb_field = per_block;

    per_block->instr_count = instru_t::count_app_instrs(ilist);

    identify_elidable_addresses(drcontext, ilist, OFFLINE_FILE_VERSION,
                                memref_needs_full_info);

    app_pc tag_pc = dr_fragment_app_pc(tag);
    if (drmodtrack_lookup(drcontext, tag_pc, nullptr, nullptr) != DRCOVLIB_SUCCESS) {
        // For (unmodified) library code we do not need to record encodings as we
        // rely on access to the binary during post-processing.
        //
        // TODO i#2062: We need to also identify modified library code and record
        // its encodings.  The plan is to augment drmodtrack to track this for us;
        // for now we will incorrectly use the original bits in the trace.
        record_instr_encodings(drcontext, tag_pc, per_block, ilist);
    }
}

void
offline_instru_t::bb_analysis_cleanup(void *drcontext, void *bb_field)
{
    dr_thread_free(drcontext, bb_field, sizeof(per_block_t));
}

bool
offline_instru_t::opnd_is_elidable(opnd_t memop, OUT reg_id_t &base, int version)
{
    if (version <= OFFLINE_FILE_VERSION_NO_ELISION)
        return false;
    // When adding new elision cases, be sure to check "version" to keep backward
    // compatibility.  For OFFLINE_FILE_VERSION_ELIDE_UNMOD_BASE we elide a
    // base register that has not changed since a prior stored address (with no
    // index register).  We include rip-relative in this category.
    // Here we look for rip-relative and no-index operands: opnd_check_elidable()
    // checks for an unchanged prior instance.
    if (IF_REL_ADDRS(opnd_is_near_rel_addr(memop) ||) opnd_is_near_abs_addr(memop)) {
        base = DR_REG_NULL;
        return true;
    }
    if (!opnd_is_near_base_disp(memop) ||
        // We're assuming displacements are all factored out, such that we can share
        // a base across all uses without subtracting the original disp.
        // TODO(i#4898): This is blocking elision of SP bases on AArch64.  We should
        // add disp subtraction by storing the disp along with reg_vals in raw2trace
        // for AArch64.
        !opnd_disp_is_elidable(memop) ||
        (opnd_get_base(memop) != DR_REG_NULL && opnd_get_index(memop) != DR_REG_NULL))
        return false;
    base = opnd_get_base(memop);
    if (base == DR_REG_NULL)
        base = opnd_get_index(memop);
    return true;
}

void
offline_instru_t::opnd_check_elidable(void *drcontext, instrlist_t *ilist, instr_t *instr,
                                      opnd_t memop, int op_index, int memop_index,
                                      bool write, int version, reg_id_set_t &saw_base)
{
    // We elide single-register (base or index) operands that only differ in
    // displacement, as well as rip-relative or absolute-address operands.
    reg_id_t base;
    if (!opnd_is_elidable(memop, base, version))
        return;
    // When adding new elision cases, be sure to check "version" to keep backward
    // compatibility.  See the opnd_is_elidable() notes.  Here we insert a label if
    // we find a base that has not changed or a rip-relative operand.
    if (base == DR_REG_NULL || saw_base.find(base) != saw_base.end()) {
        instr_t *note = INSTR_CREATE_label(drcontext);
        instr_set_note(note, (void *)elide_memref_note_);
        dr_instr_label_data_t *data = instr_get_label_data_area(note);
        data->data[LABEL_DATA_ELIDED_INDEX] = op_index;
        data->data[LABEL_DATA_ELIDED_MEMOP_INDEX] = memop_index;
        data->data[LABEL_DATA_ELIDED_IS_WRITE] = write;
        data->data[LABEL_DATA_ELIDED_NEEDS_BASE] = (base != DR_REG_NULL);
        MINSERT(ilist, instr, note);
    } else
        saw_base.insert(base);
}

bool
offline_instru_t::label_marks_elidable(instr_t *instr, OUT int *opnd_index,
                                       OUT int *memopnd_index, OUT bool *is_write,
                                       OUT bool *needs_base)
{
    if (!instr_is_label(instr))
        return false;
    if (instr_get_note(instr) != (void *)elide_memref_note_)
        return false;
    dr_instr_label_data_t *data = instr_get_label_data_area(instr);
    if (opnd_index != nullptr)
        *opnd_index = static_cast<int>(data->data[LABEL_DATA_ELIDED_INDEX]);
    if (memopnd_index != nullptr)
        *memopnd_index = static_cast<int>(data->data[LABEL_DATA_ELIDED_MEMOP_INDEX]);
    // The !! is to work around MSVC's warning C4800 about casting int to bool.
    if (is_write != nullptr)
        *is_write = static_cast<bool>(!!data->data[LABEL_DATA_ELIDED_IS_WRITE]);
    if (needs_base != nullptr)
        *needs_base = static_cast<bool>(!!data->data[LABEL_DATA_ELIDED_NEEDS_BASE]);
    return true;
}

void
offline_instru_t::identify_elidable_addresses(void *drcontext, instrlist_t *ilist,
                                              int version, bool memref_needs_full_info)
{
    // Analysis for eliding redundant addresses we can reconstruct during
    // post-processing.
    if (disable_optimizations_)
        return;
    // We can't elide when doing filtering.
    if (memref_needs_full_info)
        return;
    reg_id_set_t saw_base;
    for (instr_t *instr = instrlist_first(ilist); instr != NULL;
         instr = instr_get_next(instr)) {
        // XXX: We turn off address elision for bbs containing emulation sequences
        // or instrs that are expanded into emulation sequences like scatter/gather
        // and rep stringop. As instru_offline and raw2trace see different instrs in
        // these bbs (expanded seq vs original app instr), there may be mismatches in
        // identifying elision opportunities. We can possibly provide a consistent
        // view by expanding the instr in raw2trace (e.g. using
        // drx_expand_scatter_gather) when building the ilist.
        if (drutil_instr_is_stringop_loop(instr)
            // TODO i#3837: Scatter/gather support NYI on ARM/AArch64.
            IF_X86(|| instr_is_scatter(instr) || instr_is_gather(instr))) {
            return;
        }
        if (drmgr_is_emulation_start(instr) || drmgr_is_emulation_end(instr)) {
            return;
        }
    }
    for (instr_t *instr = instrlist_first_app(ilist); instr != NULL;
         instr = instr_get_next_app(instr)) {
        // For now we bail at predication.
        if (instr_get_predicate(instr) != DR_PRED_NONE) {
            saw_base.clear();
            continue;
        }
        // Use instr_{reads,writes}_memory() to rule out LEA and NOP.
        if (instr_reads_memory(instr) || instr_writes_memory(instr)) {
            int mem_count = 0;
            for (int i = 0; i < instr_num_srcs(instr); i++) {
                if (opnd_is_memory_reference(instr_get_src(instr, i))) {
                    opnd_check_elidable(drcontext, ilist, instr, instr_get_src(instr, i),
                                        i, mem_count, false, version, saw_base);
                    ++mem_count;
                }
            }
            // Rule out sharing with any dest if the base is written to.  The ISA
            // does not specify the ordering of multiple dests.
            auto reg_it = saw_base.begin();
            while (reg_it != saw_base.end()) {
                if (instr_writes_to_reg(instr, *reg_it, DR_QUERY_INCLUDE_COND_DSTS))
                    reg_it = saw_base.erase(reg_it);
                else
                    ++reg_it;
            }
            mem_count = 0;
            for (int i = 0; i < instr_num_dsts(instr); i++) {
                if (opnd_is_memory_reference(instr_get_dst(instr, i))) {
                    opnd_check_elidable(drcontext, ilist, instr, instr_get_dst(instr, i),
                                        i, mem_count, true, version, saw_base);
                    ++mem_count;
                }
            }
        }
        // Rule out sharing with subsequent instrs if the base is written to.
        // TODO(i#2001): Add special support for eliding the xsp base of push+pop
        // instructions.
        auto reg_it = saw_base.begin();
        while (reg_it != saw_base.end()) {
            if (instr_writes_to_reg(instr, *reg_it, DR_QUERY_INCLUDE_COND_DSTS))
                reg_it = saw_base.erase(reg_it);
            else
                ++reg_it;
        }
    }
}

} // namespace drmemtrace
} // namespace dynamorio
