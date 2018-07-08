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
#include "client_tools.h"

#define MINSERT instrlist_meta_preinsert

static void
thread_init_event(void *drcontext)
{
    dr_set_tls_field(drcontext, NULL);
}

static void
at_taken(app_pc targ_addr)
{
    void *drcontext = dr_get_current_drcontext();
    dr_set_tls_field(drcontext, targ_addr);
}

static void
at_not_taken(app_pc fall_addr)
{
    void *drcontext = dr_get_current_drcontext();
    dr_set_tls_field(drcontext, fall_addr);
}

static void
at_bb(app_pc bb_addr)
{
    void *drcontext = dr_get_current_drcontext();
    app_pc cbr_addr = dr_get_tls_field(drcontext);

    if (cbr_addr != NULL && cbr_addr != bb_addr) {
        dr_fprintf(STDERR,
                   "ERROR: expected branch to " PFX ", but entered BB at " PFX "\n",
                   cbr_addr, bb_addr);
    }

    dr_set_tls_field(drcontext, NULL);
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;

    /*
     * Callback on entry to each basic block.  Dynamically verify that
     * when the last executed BB ends in a cbr, the target (or
     * fallthrough) matches next addr of the next executed BB.
     */
    app_pc bb_addr = dr_fragment_app_pc(tag);
    instr = instrlist_first(bb);

    dr_prepare_for_call(drcontext, bb, instr);

    MINSERT(bb, instr,
            INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((ptr_uint_t)bb_addr)));
    MINSERT(bb, instr, INSTR_CREATE_call(drcontext, opnd_create_pc((void *)at_bb)));

    dr_cleanup_after_call(drcontext, bb, instr, 4);

    for (; instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);

        if (instr_is_cbr(instr)) {
            instr_t *jmp_ft;
            app_pc targ, fall, instr_addr;
            opnd_t opnd;

            /*
             * Build the callback sequence for the not-taken case
             */
            instr_addr = instr_get_app_pc(instr);
            fall = (app_pc)decode_next_pc(drcontext, (byte *)instr_addr);

            dr_prepare_for_call(drcontext, bb, NULL);

            MINSERT(
                bb, NULL,
                INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((ptr_uint_t)fall)));
            MINSERT(bb, NULL,
                    INSTR_CREATE_call(drcontext, opnd_create_pc((void *)at_not_taken)));

            dr_cleanup_after_call(drcontext, bb, NULL, 4);

            /* jump to the original fallthrough block (this should not
             * be a meta-instruction). */
            jmp_ft = INSTR_XL8(INSTR_CREATE_jmp(drcontext, opnd_create_pc(fall)), fall);
            instrlist_preinsert(bb, NULL, jmp_ft);

            /*
             * Build the callback sequence for the taken case
             */
            opnd = instr_get_target(instr);
            ASSERT(opnd_is_pc(opnd));
            targ = opnd_get_pc(opnd);

            dr_prepare_for_call(drcontext, bb, NULL);

            MINSERT(
                bb, NULL,
                INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((ptr_uint_t)targ)));
            MINSERT(bb, NULL,
                    INSTR_CREATE_call(drcontext, opnd_create_pc((void *)at_taken)));

            dr_cleanup_after_call(drcontext, bb, NULL, 4);

            /* jump to the original target block (this should
             * not be a meta-instruction). */
            instrlist_preinsert(
                bb, NULL,
                INSTR_XL8(INSTR_CREATE_jmp(drcontext, opnd_create_pc(targ)), targ));

            /*
             * Redirect the cbr to jump to our 'taken' callback.
             * The fallthrough will hit the 'not-taken' callback.
             */
            instr_set_meta(instr);
            instr_set_translation(instr, NULL);
            /* If this is a short cti, make sure it can reach its new target */
            if (instr_is_cti_short(instr)) {
                /* if jecxz/loop we want to set the target of the long-taken
                 * so set instr to the return value
                 */
                instr = instr_convert_short_meta_jmp_to_long(drcontext, bb, instr);
            }
            instr_set_target(instr, opnd_create_instr(instr_get_next(jmp_ft)));
        }
    }
    /* since our added instrumentation is not constant, we ask to store
     * translations now
     */
    return DR_EMIT_STORE_TRANSLATIONS;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_fprintf(STDERR, "thank you for testing the client interface\n");
    dr_register_bb_event(bb_event);
    dr_register_thread_init_event(thread_init_event);
}
