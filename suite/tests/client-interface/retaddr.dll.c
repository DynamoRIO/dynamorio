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

uint num_ret = 0;

static void
mbr_instru(app_pc instr_addr, app_pc target_addr)
{
    num_ret++;
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr;
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if (instr_is_return(instr)) {
            dr_clobber_retaddr_after_read(drcontext, bb, instr, 0);
            /* i#2364 : mbr should be tested against different kinds of return. */
            dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc)mbr_instru,
                                          SPILL_SLOT_1);
        }
    }
    return DR_EMIT_DEFAULT;
}

static void
exit_event(void)
{
    /* We assume returns are executed at least twice. */
    if (num_ret > 1) {
        dr_fprintf(STDERR, "instrumentation for return ok\n");
    } else {
        dr_fprintf(STDERR, "FAIL no instrumented returns\n");
    }
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_fprintf(STDERR, "thank you for testing the client interface\n");
    dr_register_bb_event(bb_event);
    dr_register_exit_event(exit_event);
}
