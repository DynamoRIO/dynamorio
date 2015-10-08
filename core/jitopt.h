/* ******************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * ******************************************************/

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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

#ifndef _JITOPT_H_
#define _JITOPT_H_ 1

#include "monitor.h"

#define DGC_MAPPING_TABLE_SHIFT 0xc    /* extract page id */
#define DGC_MAPPING_TABLE_MASK 0x3ff   /* mask per size */
#define DGC_MAPPING_TABLE_SIZE 0x400  /* arbitrary size (maskable) */

#define DGC_SHADOW_PAGE_ID(pc) (((ptr_uint_t)(pc)) >> DGC_MAPPING_TABLE_SHIFT)
#define DGC_SHADOW_KEY(page_id) ((page_id) & DGC_MAPPING_TABLE_MASK)

#define DGC_OVERLAP_BUCKET_BIT_SIZE 6

typedef enum _emulation_operation_t {
    EMUL_MOV,
    EMUL_OR,
    EMUL_XOR,
    EMUL_AND,
    EMUL_ADD,
    EMUL_SUB,
} emulation_operation_t;

typedef struct _emulation_plan_t {
    instr_t writer;
    emulation_operation_t op;
    bool src_in_reg;
    union {
        uint mcontext_reg_offset;
        reg_t immediate;
    } src;
    opnd_t dst;
    uint dst_size;
    app_pc writer_pc;
    app_pc resume_pc;
    bool is_jit_self_write;
} emulation_plan_t;

typedef struct dgc_writer_mapping_t dgc_writer_mapping_t;
struct dgc_writer_mapping_t {
    ptr_uint_t page_id;
    ptr_int_t offset;
    dgc_writer_mapping_t *next;
};

void
jitopt_init();

void
jitopt_exit();

void
jitopt_thread_init(dcontext_t *dcontext);

void
annotation_manage_code_area(app_pc start, size_t len);

void
annotation_unmanage_code_area(app_pc start, size_t len);

void
flush_jit_fragments(app_pc start, size_t len);

#ifdef JITOPT
ptr_int_t
lookup_dgc_writer_offset(app_pc addr);

void
notify_readonly_for_cache_consistency(app_pc start, size_t size, bool now_readonly);

void
manage_code_area(app_pc start, size_t len);

void
locate_and_manage_code_area(app_pc pc);

void
notify_exec_invalidation(app_pc start, size_t size);

void
setup_double_mapping(dcontext_t *dcontext, app_pc start, uint len, uint prot);

bool
shrink_double_mapping(app_pc old_start, app_pc new_start, size_t new_size);

bool
clear_double_mapping(app_pc start);

app_pc
instrument_dgc_writer(dcontext_t *dcontext, priv_mcontext_t *mc, fragment_t *f, app_pc instr_app_pc,
                      app_pc write_target, size_t write_size, uint prot, bool is_jit_self_write);

bool
apply_dgc_emulation_plan(dcontext_t *dcontext, OUT app_pc *pc, OUT instr_t **instr);

void
add_patchable_bb(app_pc start, app_pc end, bool is_trace_head);

bool
add_patchable_trace(monitor_data_t *md);

void
patchable_bb_linked(dcontext_t *dcontext, fragment_t *f);

uint
remove_patchable_fragments(dcontext_t *dcontext, app_pc patch_start, app_pc patch_end);

void
dgc_table_dereference_bb(app_pc tag);

void
dgc_notify_region_cleared(app_pc start, app_pc end);

void
dgc_cache_reset();

static inline bool
is_dgc_optimization_label(instr_t * instr)
{
    if (instr != NULL && instr_is_label(instr))
        return (ptr_uint_t) instr_get_note(instr) == DR_NOTE_DGC_OPTIMIZATION;

    return false;
}

#endif /* JITOPT */

#endif
