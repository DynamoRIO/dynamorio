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

/* instru: inserts instrumentation to write to a trace stream.
 */

#ifndef _INSTRU_H_
#define _INSTRU_H_ 1

#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <atomic>
#include <iterator>
#include <utility>

#include "dr_allocator.h"
#include "dr_api.h"
#include "drvector.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

#define MINSERT instrlist_meta_preinsert

// Versioning for our drmodtrack custom module fields.
#define CUSTOM_MODULE_VERSION 1

// A std::unordered_set, even using dr_allocator_t, raises transparency risks when
// statically linked on Windows (from lock functions and other non-allocator
// resources).  We thus create our own resource-isolated class to track GPR register
// inclusion, which can easily be implemented with an array.
// The burst_traceopts test contains unit tests for this class.
class reg_id_set_t {
public:
    reg_id_set_t()
    {
        clear();
    }

    void
    clear()
    {
        memset(present_, 0, sizeof(present_));
    }

    class reg_id_set_iterator_t
        : public std::iterator<std::input_iterator_tag, reg_id_t> {
    public:
        reg_id_set_iterator_t(reg_id_set_t *set)
            : set_(set)
            , index_(-1)
        {
        }
        reg_id_set_iterator_t(reg_id_set_t *set, int index)
            : set_(set)
            , index_(index)
        {
        }

        reg_id_t
        operator*()
        {
            return static_cast<reg_id_t>(DR_REG_START_GPR + index_);
        }

        bool
        operator==(const reg_id_set_iterator_t &rhs) const
        {
            return index_ == rhs.index_;
        }
        bool
        operator!=(const reg_id_set_iterator_t &rhs) const
        {
            return index_ != rhs.index_;
        }

        reg_id_set_iterator_t &
        operator++()
        {
            while (++index_ < DR_NUM_GPR_REGS && !set_->present_[index_]) {
                /* Nothing. */
            }
            if (index_ >= DR_NUM_GPR_REGS)
                index_ = -1;
            return *this;
        }

    private:
        friend class reg_id_set_t;
        reg_id_set_t *set_;
        int index_;
    };

    reg_id_set_iterator_t
    begin()
    {
        reg_id_set_iterator_t iter(this);
        return ++iter;
    }

    reg_id_set_iterator_t
    end()
    {
        return reg_id_set_iterator_t(this);
    }

    reg_id_set_iterator_t
    erase(const reg_id_set_iterator_t pos)
    {
        if (pos.index_ == -1)
            return end();
        reg_id_set_iterator_t iter(this, pos.index_);
        present_[pos.index_] = false;
        return ++iter;
    }

    std::pair<reg_id_set_iterator_t, bool>
    insert(reg_id_t id)
    {
        if (id < DR_REG_START_GPR || id > DR_REG_STOP_GPR)
            return std::make_pair(end(), false);
        index_ = id - DR_REG_START_GPR;
        bool exists = present_[index_];
        if (!exists)
            present_[index_] = true;
        return std::make_pair(reg_id_set_iterator_t(this, index_), !exists);
    }

    reg_id_set_iterator_t
    find(reg_id_t id)
    {
        if (id < DR_REG_START_GPR || id > DR_REG_STOP_GPR)
            return end();
        index_ = id - DR_REG_START_GPR;
        if (!present_[index_])
            return end();
        return reg_id_set_iterator_t(this, index_);
    }

private:
    bool present_[DR_NUM_GPR_REGS];
    int index_;
};

class instru_t {
public:
    // The one dependence we have on the user is that we need to know how
    // to insert code to re-load the current trace buffer pointer into a register.
    // We require that this is passed at construction time:
    instru_t(void (*insert_load_buf)(void *, instrlist_t *, instr_t *, reg_id_t),
             drvector_t *reg_vector, size_t instruction_size, bool disable_opts = false)
        : insert_load_buf_ptr_(insert_load_buf)
        , reg_vector_(reg_vector)
        , disable_optimizations_(disable_opts)
        , instr_size_(instruction_size)
    {
        frozen_timestamp_.store(0, std::memory_order_release);
    }
    virtual ~instru_t()
    {
    }

    instru_t &
    operator=(instru_t &) = delete;

    size_t
    sizeof_entry() const
    {
        return instr_size_;
    }

    virtual trace_type_t
    get_entry_type(byte *buf_ptr) const = 0;
    virtual size_t
    get_entry_size(byte *buf_ptr) const = 0;
    virtual int
    get_instr_count(byte *buf_ptr) const = 0;
    virtual addr_t
    get_entry_addr(void *drcontext, byte *buf_ptr) const = 0;
    virtual void
    set_entry_addr(byte *buf_ptr, addr_t addr) = 0;

    // All of these return how many bytes to advance the buffer pointer.

    virtual int
    append_pid(byte *buf_ptr, process_id_t pid) = 0;
    virtual int
    append_tid(byte *buf_ptr, thread_id_t tid) = 0;
    virtual int
    append_thread_exit(byte *buf_ptr, thread_id_t tid) = 0;
    virtual int
    append_marker(byte *buf_ptr, trace_marker_type_t type, uintptr_t val) = 0;
    virtual int
    append_iflush(byte *buf_ptr, addr_t start, size_t size) = 0;
    virtual int
    append_thread_header(byte *buf_ptr, thread_id_t tid) = 0;
    // This is a per-buffer-writeout header.
    virtual int
    append_unit_header(byte *buf_ptr, thread_id_t tid, ptr_int_t window) = 0;
    // The entry at buf_ptr must be a timestamp.
    // If the timestamp value is < min_timestamp, replaces it with min_timestamp
    // and returns true; else returns false.
    virtual bool
    refresh_unit_header_timestamp(byte *buf_ptr, uint64 min_timestamp) = 0;
    virtual void
    set_frozen_timestamp(uint64 timestamp)
    {
        frozen_timestamp_.store(timestamp, std::memory_order_release);
    }

    // These insert inlined code to add an entry into the trace buffer.
    virtual int
    instrument_memref(void *drcontext, void *bb_field, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, int adjust, instr_t *app, opnd_t ref,
                      int ref_index, bool write, dr_pred_type_t pred,
                      bool memref_needs_full_info) = 0;
    // Note that the 'mode' parameter is an opaque value passed to the
    // insert_update_buf_ptr_ callback function.
    virtual int
    instrument_instr(void *drcontext, void *tag, void *bb_field, instrlist_t *ilist,
                     instr_t *where, reg_id_t reg_ptr, int adjust, instr_t *app,
                     bool memref_needs_full_info, uintptr_t mode) = 0;
    virtual int
    instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_ptr, int adjust, instr_t **delay_instrs,
                       int num_delay_instrs) = 0;
    virtual int
    instrument_instr_encoding(void *drcontext, void *tag, void *bb_field,
                              instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr,
                              int adjust, instr_t *app) = 0;
    virtual int
    instrument_rseq_entry(void *drcontext, instrlist_t *ilist, instr_t *where,
                          instr_t *rseq_label, reg_id_t reg_ptr, int adjust) = 0;

    virtual void
    bb_analysis(void *drcontext, void *tag, void **bb_field, instrlist_t *ilist,
                bool repstr_expanded, bool memref_needs_full_info) = 0;
    virtual void
    bb_analysis_cleanup(void *drcontext, void *bb_field) = 0;

    // Utilities.
#ifdef AARCH64
    static unsigned short
    get_aarch64_prefetch_target_policy(ptr_int_t prfop);
    static unsigned short
    get_aarch64_prefetch_op_type(ptr_int_t prfop);
    static unsigned short
    get_aarch64_prefetch_type(ptr_int_t prfop);
    static bool
    is_aarch64_icache_flush_op(instr_t *instr);
    static bool
    is_aarch64_dcache_flush_op(instr_t *instr);
    static bool
    is_aarch64_dc_zva_instr(instr_t *instr);
#endif
    static unsigned short
    instr_to_prefetch_type(instr_t *instr);
    static unsigned short
    instr_to_instr_type(instr_t *instr, bool repstr_expanded = false);
    static bool
    instr_is_flush(instr_t *instr);
    static unsigned short
    instr_to_flush_type(instr_t *instr);
    static int
    get_cpu_id();
    static uint64
    get_timestamp();
    static int
    count_app_instrs(instrlist_t *ilist);
    virtual void
    insert_obtain_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_addr, reg_id_t reg_scratch, opnd_t ref,
                       OUT bool *scratch_used = NULL);

protected:
    void (*insert_load_buf_ptr_)(void *, instrlist_t *, instr_t *, reg_id_t);
    // Whether each data ref needs its own PC and type entry (i.e.,
    // this info cannot be inferred from surrounding icache entries).
    drvector_t *reg_vector_;
    bool disable_optimizations_;
    // Stores a timestamp to use for all future unit headers.  This is meant for
    // avoiding time gaps for -align_endpoints or max-limit scenarios (i#5021).
    std::atomic<uint64> frozen_timestamp_;

private:
    instru_t()
        : instr_size_(0)
    {
    }

    const size_t instr_size_;
};

class online_instru_t : public instru_t {
public:
    online_instru_t(void (*insert_load_buf)(void *, instrlist_t *, instr_t *, reg_id_t),
                    void (*insert_update_buf_ptr)(void *, instrlist_t *, instr_t *,
                                                  reg_id_t, dr_pred_type_t, int,
                                                  uintptr_t),
                    drvector_t *reg_vector);
    virtual ~online_instru_t();

    trace_type_t
    get_entry_type(byte *buf_ptr) const override;
    size_t
    get_entry_size(byte *buf_ptr) const override;
    int
    get_instr_count(byte *buf_ptr) const override;
    addr_t
    get_entry_addr(void *drcontext, byte *buf_ptr) const override;
    void
    set_entry_addr(byte *buf_ptr, addr_t addr) override;

    int
    append_pid(byte *buf_ptr, process_id_t pid) override;
    int
    append_tid(byte *buf_ptr, thread_id_t tid) override;
    int
    append_thread_exit(byte *buf_ptr, thread_id_t tid) override;
    int
    append_marker(byte *buf_ptr, trace_marker_type_t type, uintptr_t val) override;
    int
    append_iflush(byte *buf_ptr, addr_t start, size_t size) override;
    int
    append_thread_header(byte *buf_ptr, thread_id_t tid) override;
    virtual int
    append_thread_header(byte *buf_ptr, thread_id_t tid, offline_file_type_t file_type);
    int
    append_unit_header(byte *buf_ptr, thread_id_t tid, ptr_int_t window) override;
    bool
    refresh_unit_header_timestamp(byte *buf_ptr, uint64 min_timestamp) override;

    int
    instrument_memref(void *drcontext, void *bb_field, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, int adjust, instr_t *app, opnd_t ref,
                      int ref_index, bool write, dr_pred_type_t pred,
                      bool memref_needs_full_info) override;
    int
    instrument_instr(void *drcontext, void *tag, void *bb_field, instrlist_t *ilist,
                     instr_t *where, reg_id_t reg_ptr, int adjust, instr_t *app,
                     bool memref_needs_full_info, uintptr_t mode) override;
    int
    instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_ptr, int adjust, instr_t **delay_instrs,
                       int num_delay_instrs) override;
    int
    instrument_instr_encoding(void *drcontext, void *tag, void *bb_field,
                              instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr,
                              int adjust, instr_t *app) override;
    int
    instrument_rseq_entry(void *drcontext, instrlist_t *ilist, instr_t *where,
                          instr_t *rseq_label, reg_id_t reg_ptr, int adjust) override;

    void
    bb_analysis(void *drcontext, void *tag, void **bb_field, instrlist_t *ilist,
                bool repstr_expanded, bool memref_needs_full_info) override;
    void
    bb_analysis_cleanup(void *drcontext, void *bb_field) override;

private:
    void
    insert_save_immed(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
                      reg_id_t scratch, ptr_int_t immed, int adjust);
    void
    insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                     reg_id_t reg_ptr, reg_id_t reg_addr, int adjust, opnd_t ref);
    void
    insert_save_type_and_size(void *drcontext, instrlist_t *ilist, instr_t *where,
                              reg_id_t base, reg_id_t scratch, ushort type, ushort size,
                              int adjust);

protected:
    void (*insert_update_buf_ptr_)(void *, instrlist_t *, instr_t *, reg_id_t,
                                   dr_pred_type_t, int, uintptr_t);
};

class offline_instru_t : public instru_t {
public:
    offline_instru_t();
    offline_instru_t(void (*insert_load_buf)(void *, instrlist_t *, instr_t *, reg_id_t),
                     drvector_t *reg_vector,
                     ssize_t (*write_file)(file_t file, const void *data, size_t count),
                     file_t module_file, file_t encoding_file,
                     bool disable_optimizations = false,
                     void (*log)(uint level, const char *fmt, ...) = nullptr);
    virtual ~offline_instru_t();

    trace_type_t
    get_entry_type(byte *buf_ptr) const override;
    size_t
    get_entry_size(byte *buf_ptr) const override;
    int
    get_instr_count(byte *buf_ptr) const override;
    addr_t
    get_entry_addr(void *drcontext, byte *buf_ptr) const override;
    void
    set_entry_addr(byte *buf_ptr, addr_t addr) override;

    uint64_t
    get_modoffs(void *drcontext, app_pc pc, OUT uint *modidx);

    int
    append_pid(byte *buf_ptr, process_id_t pid) override;
    int
    append_tid(byte *buf_ptr, thread_id_t tid) override;
    int
    append_thread_exit(byte *buf_ptr, thread_id_t tid) override;
    int
    append_marker(byte *buf_ptr, trace_marker_type_t type, uintptr_t val) override;
    int
    append_iflush(byte *buf_ptr, addr_t start, size_t size) override;
    int
    append_thread_header(byte *buf_ptr, thread_id_t tid) override;
    virtual int
    append_thread_header(byte *buf_ptr, thread_id_t tid, offline_file_type_t file_type);
    int
    append_unit_header(byte *buf_ptr, thread_id_t tid, ptr_int_t window) override;
    bool
    refresh_unit_header_timestamp(byte *buf_ptr, uint64 min_timestamp) override;

    int
    instrument_memref(void *drcontext, void *bb_field, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, int adjust, instr_t *app, opnd_t ref,
                      int ref_index, bool write, dr_pred_type_t pred,
                      bool memref_needs_full_info) override;
    int
    instrument_instr(void *drcontext, void *tag, void *bb_field, instrlist_t *ilist,
                     instr_t *where, reg_id_t reg_ptr, int adjust, instr_t *app,
                     bool memref_needs_full_info, uintptr_t mode) override;
    int
    instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_ptr, int adjust, instr_t **delay_instrs,
                       int num_delay_instrs) override;
    int
    instrument_instr_encoding(void *drcontext, void *tag, void *bb_field,
                              instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr,
                              int adjust, instr_t *app) override;
    int
    instrument_rseq_entry(void *drcontext, instrlist_t *ilist, instr_t *where,
                          instr_t *rseq_label, reg_id_t reg_ptr, int adjust) override;

    void
    bb_analysis(void *drcontext, void *tag, void **bb_field, instrlist_t *ilist,
                bool repstr_expanded, bool memref_needs_full_info) override;
    void
    bb_analysis_cleanup(void *drcontext, void *bb_field) override;

    static bool
    custom_module_data(void *(*load_cb)(module_data_t *module, int seg_idx),
                       int (*print_cb)(void *data, char *dst, size_t max_len),
                       void (*free_cb)(void *data));

    bool
    opnd_disp_is_elidable(opnd_t memop);
    // "version" is an OFFLINE_FILE_VERSION* constant.
    bool
    opnd_is_elidable(opnd_t memop, OUT reg_id_t &base, int version);
    // Inserts labels marking elidable addresses. label_marks_elidable() identifies them.
    // "version" is an OFFLINE_FILE_VERSION* constant.
    void
    identify_elidable_addresses(void *drcontext, instrlist_t *ilist, int version,
                                bool memref_needs_full_info);
    bool
    label_marks_elidable(instr_t *instr, OUT int *opnd_index, OUT int *memopnd_index,
                         OUT bool *is_write, OUT bool *needs_base);
    static int
    print_module_data_fields(char *dst, size_t max_len, const void *custom_data,
                             size_t custom_size,
                             int (*user_print_cb)(void *data, char *dst, size_t max_len),
                             void *user_cb_data);

private:
    struct custom_module_data_t {
        custom_module_data_t(const char *base_in, size_t size_in, void *user_in)
            : base(base_in)
            , size(size_in)
            , user_data(user_in)
        {
        }
        const char *base;
        size_t size;
        void *user_data;
    };

    struct per_block_t {
        uint64_t id = 0;
        uint instr_count = 0;
    };

    bool
    instr_has_multiple_different_memrefs(instr_t *instr);
    int
    insert_save_entry(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, reg_id_t scratch, int adjust,
                      offline_entry_t *entry);
    int
    insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr,
                   reg_id_t scratch, int adjust, app_pc pc, uint instr_count,
                   per_block_t *per_block);
    int
    insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                     reg_id_t reg_ptr, int adjust, opnd_t ref, bool write);
    int
    insert_save_type_and_size(void *drcontext, instrlist_t *ilist, instr_t *where,
                              reg_id_t reg_ptr, reg_id_t scratch, int adjust,
                              instr_t *app, opnd_t ref, bool write);
    ssize_t (*write_file_func_)(file_t file, const void *data, size_t count);

    void
    opnd_check_elidable(void *drcontext, instrlist_t *ilist, instr_t *instr, opnd_t memop,
                        int op_index, int memop_index, bool write, int version,
                        reg_id_set_t &saw_base);
    void
    record_instr_encodings(void *drcontext, app_pc tag_pc, per_block_t *per_block,
                           instrlist_t *ilist);
    void
    flush_instr_encodings();

    // Custom module fields are global (since drmodtrack's support is global, we don't
    // try to pass void* user data params through).
    static void *(*user_load_)(module_data_t *module, int seg_idx);
    static int (*user_print_)(void *data, char *dst, size_t max_len);
    static void (*user_free_)(void *data);
    static void *
    load_custom_module_data(module_data_t *module, int seg_idx);
    static int
    print_custom_module_data(void *data, char *dst, size_t max_len);
    static void
    free_custom_module_data(void *data);

    // These identify the 4 fields we store in the label data area array.
    static constexpr int LABEL_DATA_ELIDED_INDEX = 0;       // Index among all operands.
    static constexpr int LABEL_DATA_ELIDED_MEMOP_INDEX = 1; // Index among memory ops.
    static constexpr int LABEL_DATA_ELIDED_IS_WRITE = 2;
    static constexpr int LABEL_DATA_ELIDED_NEEDS_BASE = 3;
    ptr_uint_t elide_memref_note_;
    bool standalone_ = false;
    file_t modfile_ = INVALID_FILE;
    void (*log_)(uint level, const char *fmt, ...);

    file_t encoding_file_ = INVALID_FILE;
    int max_block_encoding_size_ = 0;
    void *encoding_lock_ = nullptr;
    byte *encoding_buf_start_ = nullptr;
    size_t encoding_buf_sz_ = 0;
    byte *encoding_buf_ptr_ = nullptr;
    uint64_t encoding_id_ = 0;
    uint64_t encoding_bytes_written_ = 0;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _INSTRU_H_ */
