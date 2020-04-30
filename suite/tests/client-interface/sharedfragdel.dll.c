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

static app_pc step1_pc = NULL;
static app_pc step2_pc = NULL;

void
delete_fragment(app_pc tag, app_pc pc)
{
    if (step1_pc == NULL) {

        bool succ = dr_unlink_flush_fragment(tag);
        if (succ) {
            dr_mcontext_t mcontext = {
                sizeof(mcontext),
                DR_MC_ALL,
            };
            dr_get_mcontext(dr_get_current_drcontext(), &mcontext);

            mcontext.pc = pc;
            step1_pc = pc;
            dr_redirect_execution(&mcontext);
        }
    } else {
        if (step2_pc != pc) {
            dr_fprintf(STDERR, "error: Tag mismatch - step 2\n");
            DR_ASSERT(false);
        }
        step1_pc = NULL;
        step2_pc = NULL;
    }
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr = instrlist_first(bb);

    app_pc pc = instr_get_app_pc(instr);

    if (step1_pc != NULL) {
        DR_ASSERT(step2_pc == NULL);
        if (step1_pc != pc)
            dr_fprintf(STDERR, "error: Tag mismatch - step 1\n");
        step2_pc = step1_pc;
    }

    dr_insert_clean_call(drcontext, bb, instr, delete_fragment, false, 2,
                         OPND_CREATE_INTPTR(tag), OPND_CREATE_INTPTR(pc));

    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
}
