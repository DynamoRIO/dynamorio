/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

/* Tests the deletion of a single fragment. We assume a single threaded app.
 * Upon fragment deletion, execution is redirected to the same code again,
 * which in turn should reconstruct the same fragment which was just flushed.
 */

static app_pc deleted_fragment_start_pc = NULL;
static app_pc bb_event_after_delete_start_pc = NULL;

void
delete_fragment(app_pc tag, app_pc pc)
{
    if (deleted_fragment_start_pc == NULL) {

        bool succ = dr_unlink_flush_fragment(tag);
        if (succ) {
            dr_mcontext_t mcontext;
            mcontext.size = sizeof(mcontext);
            mcontext.flags = DR_MC_ALL;
            dr_get_mcontext(dr_get_current_drcontext(), &mcontext);

            mcontext.pc = pc;
            /* This is the first step of the test. The test sets the PC of
             * the deleted fragment and redirects execution. The next bb event should
             * be for the same code.
             */
            deleted_fragment_start_pc = pc;
            dr_redirect_execution(&mcontext);
        }
    } else {
        /* This is the second step of the test. At this point,
         * bb_event_after_delete_start_pc should be set to the same pc of the
         * fragment that was deleted.
         */
        if (bb_event_after_delete_start_pc != pc) {
            dr_fprintf(STDERR, "error: Tag mismatch - step 2\n");
            DR_ASSERT(false);
        }

        /* Re-initialize globals to test fragment deletion for the next bb. */
        deleted_fragment_start_pc = NULL;
        bb_event_after_delete_start_pc = NULL;
    }
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr = instrlist_first(bb);

    app_pc pc = instr_get_app_pc(instr);

    if (deleted_fragment_start_pc != NULL) {
        if (bb_event_after_delete_start_pc != NULL)
            dr_fprintf(STDERR, "error: should not be set.\n");

        /* If deleted_fragment_start_pc was set, then this bb event should capture the
         * reconstruction of the same bb. */
        if (deleted_fragment_start_pc != pc)
            dr_fprintf(STDERR, "error: Tag mismatch - step 1\n");
        bb_event_after_delete_start_pc = deleted_fragment_start_pc;
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
