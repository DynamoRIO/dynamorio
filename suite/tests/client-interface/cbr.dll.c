/* **********************************************************
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"

static void
thread_init_event(void *drcontext)
{
    dr_set_tls_field(drcontext, NULL);
}

static void
at_cbr(app_pc inst_addr, app_pc targ_addr, int taken)
{
    void *drcontext = dr_get_current_drcontext();

    if (taken == 1) {
        dr_set_tls_field(drcontext, targ_addr);
    } else if (taken == 0) {
        app_pc fall_addr = decode_next_pc(drcontext, inst_addr);
        dr_set_tls_field(drcontext, fall_addr);
    } else {
        dr_fprintf(STDERR, "ERROR: expecting 'taken' to be 0 or 1\n");
    }
}

static void
at_bb(void *drcontext, app_pc bb_addr)
{
    app_pc cbr_addr = dr_get_tls_field(drcontext);

    if (cbr_addr != NULL && cbr_addr != bb_addr) {
        dr_fprintf(STDERR, "ERROR: expected jmp to " PFX ", but entered BB at " PFX "\n",
                   cbr_addr, bb_addr);
    }

    dr_set_tls_field(drcontext, NULL);
}

#define MINSERT instrlist_meta_preinsert

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;

    app_pc bb_addr = dr_fragment_app_pc(tag);

    instr = instrlist_first(bb);

    dr_prepare_for_call(drcontext, bb, instr);

    MINSERT(bb, instr,
            INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((ptr_uint_t)bb_addr)));
    MINSERT(bb, instr,
            INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((ptr_uint_t)drcontext)));
    MINSERT(bb, instr, INSTR_CREATE_call(drcontext, opnd_create_pc((void *)at_bb)));

    dr_cleanup_after_call(drcontext, bb, instr, 8);

    for (; instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);

        if (instr_is_cbr(instr))
            dr_insert_cbr_instrumentation(drcontext, bb, instr, (app_pc)at_cbr);
    }

    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_fprintf(STDERR, "thank you for testing the client interface\n");
    dr_register_bb_event(bb_event);
    dr_register_thread_init_event(thread_init_event);
}
