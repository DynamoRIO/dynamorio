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

/* instru: inserts instrumentation to write to a trace stream.
 */

#ifndef _INSTRU_H_
#define _INSTRU_H_ 1

#include <stdint.h>
#include "drvector.h"
#include "../common/trace_entry.h"

#define MINSERT instrlist_meta_preinsert

// Versioning for our drmodtrack custom module fields.
#define CUSTOM_MODULE_VERSION 1

class instru_t {
public:
    // The one dependence we have on the user is that we need to know how
    // to insert code to re-load the current trace buffer pointer into a register.
    // We require that this is passed at construction time:
    instru_t(void (*insert_load_buf)(void *, instrlist_t *, instr_t *, reg_id_t),
             bool memref_needs_info, drvector_t *reg_vector_in, size_t instruction_size)
        : insert_load_buf_ptr(insert_load_buf)
        , memref_needs_full_info(memref_needs_info)
        , reg_vector(reg_vector_in)
        , instr_size(instruction_size)
    {
    }
    virtual ~instru_t()
    {
    }

    instru_t &
    operator=(instru_t &) = delete;

    size_t
    sizeof_entry() const
    {
        return instr_size;
    }

    virtual trace_type_t
    get_entry_type(byte *buf_ptr) const = 0;
    virtual size_t
    get_entry_size(byte *buf_ptr) const = 0;
    virtual addr_t
    get_entry_addr(byte *buf_ptr) const = 0;
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
    append_unit_header(byte *buf_ptr, thread_id_t tid) = 0;

    // These insert inlined code to add an entry into the trace buffer.
    virtual int
    instrument_memref(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, int adjust, instr_t *app, opnd_t ref, bool write,
                      dr_pred_type_t pred) = 0;
    virtual int
    instrument_instr(void *drcontext, void *tag, void **bb_field, instrlist_t *ilist,
                     instr_t *where, reg_id_t reg_ptr, int adjust, instr_t *app) = 0;
    virtual int
    instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_ptr, int adjust, instr_t **delay_instrs,
                       int num_delay_instrs) = 0;

    virtual void
    bb_analysis(void *drcontext, void *tag, void **bb_field, instrlist_t *ilist,
                bool repstr_expanded) = 0;

    // Utilities.
    static unsigned short
    instr_to_prefetch_type(instr_t *instr);
    static unsigned short
    instr_to_instr_type(instr_t *instr, bool repstr_expanded = false);
    static bool
    instr_is_flush(instr_t *instr);
    static int
    get_cpu_id();
    static uint64
    get_timestamp();
    virtual void
    insert_obtain_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_addr, reg_id_t reg_scratch, opnd_t ref,
                       OUT bool *scratch_used = NULL);

protected:
    void (*insert_load_buf_ptr)(void *, instrlist_t *, instr_t *, reg_id_t);
    // Whether each data ref needs its own PC and type entry (i.e.,
    // this info cannot be inferred from surrounding icache entries).
    bool memref_needs_full_info;
    drvector_t *reg_vector;

private:
    instru_t()
        : instr_size(0)
    {
    }

    const size_t instr_size;
};

class online_instru_t : public instru_t {
public:
    online_instru_t(void (*insert_load_buf)(void *, instrlist_t *, instr_t *, reg_id_t),
                    bool memref_needs_info, drvector_t *reg_vector);
    virtual ~online_instru_t();

    virtual trace_type_t
    get_entry_type(byte *buf_ptr) const;
    virtual size_t
    get_entry_size(byte *buf_ptr) const;
    virtual addr_t
    get_entry_addr(byte *buf_ptr) const;
    virtual void
    set_entry_addr(byte *buf_ptr, addr_t addr);

    virtual int
    append_pid(byte *buf_ptr, process_id_t pid);
    virtual int
    append_tid(byte *buf_ptr, thread_id_t tid);
    virtual int
    append_thread_exit(byte *buf_ptr, thread_id_t tid);
    virtual int
    append_marker(byte *buf_ptr, trace_marker_type_t type, uintptr_t val);
    virtual int
    append_iflush(byte *buf_ptr, addr_t start, size_t size);
    virtual int
    append_thread_header(byte *buf_ptr, thread_id_t tid);
    virtual int
    append_unit_header(byte *buf_ptr, thread_id_t tid);

    virtual int
    instrument_memref(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, int adjust, instr_t *app, opnd_t ref, bool write,
                      dr_pred_type_t pred);
    virtual int
    instrument_instr(void *drcontext, void *tag, void **bb_field, instrlist_t *ilist,
                     instr_t *where, reg_id_t reg_ptr, int adjust, instr_t *app);
    virtual int
    instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_ptr, int adjust, instr_t **delay_instrs,
                       int num_delay_instrs);

    virtual void
    bb_analysis(void *drcontext, void *tag, void **bb_field, instrlist_t *ilist,
                bool repstr_expanded);

private:
    void
    insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
                   reg_id_t scratch, app_pc pc, int adjust);
    void
    insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                     reg_id_t reg_ptr, reg_id_t reg_addr, int adjust, opnd_t ref);
    void
    insert_save_type_and_size(void *drcontext, instrlist_t *ilist, instr_t *where,
                              reg_id_t base, reg_id_t scratch, ushort type, ushort size,
                              int adjust);
};

class offline_instru_t : public instru_t {
public:
    offline_instru_t(void (*insert_load_buf)(void *, instrlist_t *, instr_t *, reg_id_t),
                     bool memref_needs_info, drvector_t *reg_vector,
                     ssize_t (*write_file)(file_t file, const void *data, size_t count),
                     file_t module_file);
    virtual ~offline_instru_t();

    virtual trace_type_t
    get_entry_type(byte *buf_ptr) const;
    virtual size_t
    get_entry_size(byte *buf_ptr) const;
    virtual addr_t
    get_entry_addr(byte *buf_ptr) const;
    virtual void
    set_entry_addr(byte *buf_ptr, addr_t addr);

    virtual int
    append_pid(byte *buf_ptr, process_id_t pid);
    virtual int
    append_tid(byte *buf_ptr, thread_id_t tid);
    virtual int
    append_thread_exit(byte *buf_ptr, thread_id_t tid);
    virtual int
    append_marker(byte *buf_ptr, trace_marker_type_t type, uintptr_t val);
    virtual int
    append_iflush(byte *buf_ptr, addr_t start, size_t size);
    virtual int
    append_thread_header(byte *buf_ptr, thread_id_t tid);
    virtual int
    append_unit_header(byte *buf_ptr, thread_id_t tid);

    virtual int
    instrument_memref(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, int adjust, instr_t *app, opnd_t ref, bool write,
                      dr_pred_type_t pred);
    virtual int
    instrument_instr(void *drcontext, void *tag, void **bb_field, instrlist_t *ilist,
                     instr_t *where, reg_id_t reg_ptr, int adjust, instr_t *app);
    virtual int
    instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_ptr, int adjust, instr_t **delay_instrs,
                       int num_delay_instrs);

    virtual void
    bb_analysis(void *drcontext, void *tag, void **bb_field, instrlist_t *ilist,
                bool repstr_expanded);

    static bool
    custom_module_data(void *(*load_cb)(module_data_t *module),
                       int (*print_cb)(void *data, char *dst, size_t max_len),
                       void (*free_cb)(void *data));

private:
    struct custom_module_data_t {
        custom_module_data_t(const char *base_, size_t size_, void *user_)
            : base(base_)
            , size(size_)
            , user_data(user_)
        {
        }
        const char *base;
        size_t size;
        void *user_data;
    };

    bool
    instr_has_multiple_different_memrefs(instr_t *instr);
    int
    insert_save_entry(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, reg_id_t scratch, int adjust,
                      offline_entry_t *entry);
    int
    insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr,
                   reg_id_t scratch, int adjust, app_pc pc, uint instr_count);
    int
    insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                     reg_id_t reg_ptr, int adjust, opnd_t ref, bool write);
    int
    insert_save_type_and_size(void *drcontext, instrlist_t *ilist, instr_t *where,
                              reg_id_t reg_ptr, reg_id_t scratch, int adjust,
                              instr_t *app, opnd_t ref, bool write);
    ssize_t (*write_file_func)(file_t file, const void *data, size_t count);
    file_t modfile;

    // Custom module fields are global (since drmodtrack's support is global, we don't
    // try to pass void* user data params through).
    static void *(*user_load)(module_data_t *module);
    static int (*user_print)(void *data, char *dst, size_t max_len);
    static void (*user_free)(void *data);
    static void *
    load_custom_module_data(module_data_t *module);
    static int
    print_custom_module_data(void *data, char *dst, size_t max_len);
    static void
    free_custom_module_data(void *data);
};

#endif /* _INSTRU_H_ */
