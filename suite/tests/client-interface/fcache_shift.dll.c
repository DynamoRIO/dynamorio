/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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

#include "dr_api.h"

#define PRE instrlist_meta_preinsert

#define SLOWPATH_SIZE 4096

static app_pc slowpath;

static dr_emit_flags_t
event_bb(void *dc, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *where = instrlist_first(bb);
    instr_t *ret_label = INSTR_CREATE_label(dc);
    dr_save_reg(dc, bb, where, DR_REG_XAX, SPILL_SLOT_1);
    PRE(bb, where,
        INSTR_CREATE_mov_imm(dc, opnd_create_reg(DR_REG_XAX),
                             opnd_create_instr(ret_label)));
    PRE(bb, where, INSTR_CREATE_jmp(dc, opnd_create_pc(slowpath)));
    PRE(bb, where, ret_label);
    dr_restore_reg(dc, bb, where, DR_REG_XAX, SPILL_SLOT_1);
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    dr_nonheap_free(slowpath, SLOWPATH_SIZE);
}

DR_EXPORT void
dr_init(client_id_t id)
{
    /* Generate the "slowpath" which just returns to eax. */
    void *dc = dr_get_current_drcontext();
    instrlist_t *ilist = instrlist_create(dc);
    PRE(ilist, NULL, INSTR_CREATE_jmp_ind(dc, opnd_create_reg(DR_REG_XAX)));
    slowpath = dr_nonheap_alloc(SLOWPATH_SIZE,
                                (DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC));
    instrlist_encode(dc, ilist, slowpath, false /*no relative jumps*/);
    instrlist_clear_and_destroy(dc, ilist);

    dr_register_bb_event(event_bb);
    dr_register_exit_event(event_exit);
}
