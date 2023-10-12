/* ******************************************************************************
 * Copyright (c) 2017-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2023      Arm Limited.  All rights reserved.
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

/* This client is designed to test the behaviour of the scatter/gather expansion state
 * restore function when a fault occurs from an instruction which was inserted by a
 * client rather than drx_expand_scatter_gather().
 * The state restore function should detect that the faulting instruction is not a
 * scatter/gather expansion load/store and pass it on for the client/app to handle.
 * This can happen with clients (such as memval_simple) which use drx_buf to manage
 * their trace buffer because drx_buf uses faulting writes to detect when a buffer is
 * full and needs to be flushed.
 *
 * This client is a stripped down version of memval_simple.c with several changes:
 *
 * - Instead of inserting code to write trace data to a buffer, it inserts store
 *   instructions which always write to read-only memory and trigger a SIGSEGV.
 *
 * - It instruments load instructions as well as stores.
 *
 * - It only instruments instructions which are part of a scatter/gather emulation
 *   sequence.
 */

#include <signal.h>
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drx.h"

/* Cross-instrumentation-phase data. */
typedef struct {
    bool is_scatter_gather;
} instru_data_t;

static client_id_t client_id;

static void *faulting_memory;

static dr_signal_action_t
signal_event(void *drcontext, dr_siginfo_t *info)
{
    if (info->sig == SIGSEGV && info->access_address == faulting_memory) {
        instr_t faulting_instr;
        instr_init(drcontext, &faulting_instr);

        /* Skip over the faulting instruction. */
        info->raw_mcontext->pc =
            decode(drcontext, info->raw_mcontext->pc, &faulting_instr);

        instr_free(drcontext, &faulting_instr);
        return DR_SIGNAL_SUPPRESS;
    }

    return DR_SIGNAL_DELIVER;
}

static dr_emit_flags_t
event_app_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, void *user_data)
{
    return DR_EMIT_DEFAULT;
}

static void
insert_faulting_store(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    reg_id_t reg_ptr;

    if (drreg_reserve_register(drcontext, ilist, where, NULL, &reg_ptr) !=
        DRREG_SUCCESS) {
        DR_ASSERT(false);
    }

    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)faulting_memory,
                                     opnd_create_reg(reg_ptr), ilist, where, NULL, NULL);

    instrlist_meta_preinsert(
        ilist, where,
        INSTR_XL8(XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, 0),
                                     opnd_create_reg(reg_ptr)),
                  instr_get_app_pc(where)));

    if (drreg_unreserve_register(drcontext, ilist, where, reg_ptr) != DRREG_SUCCESS)
        DR_ASSERT(false);
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *where,
                      bool for_trace, bool translating, void *user_data)
{
    int i;
    bool seen_memref = false;
    instru_data_t *data = (instru_data_t *)user_data;

    if (data->is_scatter_gather) {
        instr_t *instr_operands = drmgr_orig_app_instr_for_operands(drcontext);
        if (instr_operands != NULL &&
            (instr_writes_memory(instr_operands) || instr_reads_memory(instr_operands))) {
            DR_ASSERT(instr_is_app(instr_operands));
            for (i = 0; i < instr_num_dsts(instr_operands); ++i) {
                const opnd_t dst = instr_get_dst(instr_operands, i);
                if (opnd_is_memory_reference(dst)) {
                    if (seen_memref) {
                        DR_ASSERT_MSG(false,
                                      "Found inst with multiple memory destinations");
                        break;
                    }
                    insert_faulting_store(drcontext, bb, where);
                    seen_memref = true;
                }
            }
            for (i = 0; i < instr_num_srcs(instr_operands); ++i) {
                const opnd_t src = instr_get_src(instr_operands, i);
                if (opnd_is_memory_reference(src)) {
                    if (seen_memref) {
                        DR_ASSERT_MSG(false,
                                      "Found inst with multiple memory destinations");
                        break;
                    }
                    insert_faulting_store(drcontext, bb, where);
                    seen_memref = true;
                }
            }
        }
    }

    if (drmgr_is_last_instr(drcontext, where))
        dr_thread_free(drcontext, data, sizeof(*data));
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, void **user_data)
{
    instru_data_t *data = (instru_data_t *)dr_thread_alloc(drcontext, sizeof(*data));
    data->is_scatter_gather = false;

    if (!drx_expand_scatter_gather(drcontext, bb, &data->is_scatter_gather)) {
        DR_ASSERT(false);
    }

    *user_data = (void *)data;
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    dr_raw_mem_free(faulting_memory, dr_page_size());

    if (!drmgr_unregister_bb_instrumentation_ex_event(
            event_bb_app2app, event_app_analysis, event_app_instruction, NULL))
        DR_ASSERT(false);
    drmgr_unregister_signal_event(signal_event);

    drreg_exit();
    drmgr_exit();
    drx_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };

    if (!drmgr_init() || !drx_init())
        DR_ASSERT(false);
    if (drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_ex_event(event_bb_app2app, event_app_analysis,
                                                    event_app_instruction, NULL, NULL) ||
        !drmgr_register_signal_event(signal_event))
        DR_ASSERT(false);
    client_id = id;

    faulting_memory = dr_raw_mem_alloc(dr_page_size(), DR_MEMPROT_READ, NULL);
}
