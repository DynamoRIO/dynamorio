/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

/* This client helpfully instruments every basic block, and flushes
 * each block with 4% probability! It is great at causing synchalls.
 */

#include "dr_api.h"

static void
bb_event(void *pc)
{
    void *drcontext = dr_get_current_drcontext();

    /* Counter to only instrument every 1 in 25 bbs. Racy but don't care */
    static int count = 0;

    /* Avoids executing the hook twice after redirecting execution */
    if (dr_get_tls_field(drcontext) != NULL) {
        dr_set_tls_field(drcontext, (void *)0);
        return;
    }

    if (++count % 25 == 0) {
        dr_flush_region(pc, 1);

        dr_mcontext_t mcontext;
        mcontext.size = sizeof(mcontext);
        mcontext.flags = DR_MC_ALL;
        dr_get_mcontext(drcontext, &mcontext);
        mcontext.pc = (app_pc)pc;

        dr_set_tls_field(drcontext, (void *)1);
        dr_redirect_execution(&mcontext);
    }
}

static dr_emit_flags_t
instrument_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
              bool translating)
{
    instr_t *instr = instrlist_first(bb);
    if (!instr_is_app(instr))
        return DR_EMIT_DEFAULT;

    dr_insert_clean_call(drcontext, bb, instr, (void *)bb_event, true /* save fpstate */,
                         1, OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_register_bb_event(instrument_bb);
}
