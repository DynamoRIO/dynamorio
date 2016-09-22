/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
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

#include "../common/trace_entry.h"

#define MINSERT instrlist_meta_preinsert

class instru_t
{
 public:
    // The one dependence we have on the user is that we need to know how
    // to insert code to re-load the current trace buffer pointer into a register.
    // We require that this is passed at construction time:
    explicit instru_t(void (*insert_load_buf)(void *, instrlist_t *,
                                              instr_t *, reg_id_t))
        : insert_load_buf_ptr(insert_load_buf) {}
    virtual ~instru_t() {}

    virtual size_t sizeof_entry() const = 0;

    virtual trace_type_t get_entry_type(byte *buf_ptr) const = 0;
    virtual size_t get_entry_size(byte *buf_ptr) const = 0;
    virtual addr_t get_entry_addr(byte *buf_ptr) const = 0;
    virtual void set_entry_addr(byte *buf_ptr, addr_t addr) = 0;

    // All of these return how many bytes to advance the buffer pointer.

    virtual int append_pid(byte *buf_ptr, process_id_t pid) = 0;
    virtual int append_tid(byte *buf_ptr, thread_id_t tid) = 0;
    virtual int append_thread_exit(byte *buf_ptr, thread_id_t tid) = 0;
    virtual int append_iflush(byte *buf_ptr, addr_t start, size_t size) = 0;

    // These insert inlined code to add an entry into the trace buffer. */
    virtual int instrument_memref(void *drcontext, instrlist_t *ilist, instr_t *where,
                                  reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust,
                                  opnd_t ref, bool write, dr_pred_type_t pred) = 0;
    virtual int instrument_instr(void *drcontext, instrlist_t *ilist, instr_t *where,
                                 reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust,
                                 instr_t *app) = 0;
    virtual int instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                                   reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust,
                                   instr_t **delay_instrs, int num_delay_instrs) = 0;
    virtual int instrument_per_bb(void *drcontext, instrlist_t *ilist,
                                  reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust) = 0;

 protected:
    void (*insert_load_buf_ptr)(void *, instrlist_t *, instr_t *, reg_id_t);

 private:
    instru_t() {}
};

class online_instru_t : public instru_t
{
 public:
    explicit online_instru_t(void (*insert_load_buf)(void *, instrlist_t *,
                                                     instr_t *, reg_id_t));
    virtual ~online_instru_t();

    virtual size_t sizeof_entry() const;

    virtual trace_type_t get_entry_type(byte *buf_ptr) const;
    virtual size_t get_entry_size(byte *buf_ptr) const;
    virtual addr_t get_entry_addr(byte *buf_ptr) const;
    virtual void set_entry_addr(byte *buf_ptr, addr_t addr);

    virtual int append_pid(byte *buf_ptr, process_id_t pid);
    virtual int append_tid(byte *buf_ptr, thread_id_t tid);
    virtual int append_thread_exit(byte *buf_ptr, thread_id_t tid);
    virtual int append_iflush(byte *buf_ptr, addr_t start, size_t size);

    virtual int instrument_memref(void *drcontext, instrlist_t *ilist, instr_t *where,
                                  reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust,
                                  opnd_t ref, bool write, dr_pred_type_t pred);
    virtual int instrument_instr(void *drcontext, instrlist_t *ilist, instr_t *where,
                                 reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust,
                                 instr_t *app);
    virtual int instrument_ibundle(void *drcontext, instrlist_t *ilist, instr_t *where,
                                   reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust,
                                   instr_t **delay_instrs, int num_delay_instrs);
    virtual int instrument_per_bb(void *drcontext, instrlist_t *ilist,
                                  reg_id_t reg_ptr, reg_id_t reg_tmp, int adjust);

 private:
    void insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t base, reg_id_t scratch, app_pc pc, int adjust);
    void insert_save_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                          reg_id_t reg_ptr, reg_id_t reg_addr, int adjust, opnd_t ref);
    void insert_save_type_and_size(void *drcontext, instrlist_t *ilist, instr_t *where,
                                   reg_id_t base, reg_id_t scratch, ushort type,
                                   ushort size, int adjust);
};

// FIXME i#1729: this temporarily is identical to online_instru_t.
// We'll split it in a separate CL.
class offline_instru_t : public online_instru_t
{
 public:
    explicit offline_instru_t(void (*insert_load_buf)(void *, instrlist_t *,
                                                      instr_t *, reg_id_t))
        : online_instru_t(insert_load_buf) {}
    virtual ~offline_instru_t() {}
};

#endif /* _INSTRU_H_ */
