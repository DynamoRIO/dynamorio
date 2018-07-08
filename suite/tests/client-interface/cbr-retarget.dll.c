/* **********************************************************
 * Copyright (c) 2010-2011 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
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

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr, *next_next_instr, *cbr;
    app_pc target = NULL;

    /* Look for pattern: nop; nop; call direct; */
    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (next_instr != NULL)
            next_next_instr = instr_get_next(next_instr);
        else
            next_next_instr = NULL;

        if (instr_is_nop(instr) && next_instr != NULL && instr_is_nop(next_instr) &&
            instr_is_nop(next_next_instr)) {
            cbr = instrlist_last(bb);
            if (instr_is_cbr(cbr)) {
                target = opnd_get_pc(instr_get_target(cbr));
                /* makes the fall-though the same as the taken target */
                instrlist_set_fall_through_target(bb, target);
            }
            break;
        }
    }

    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
}
