/* **********************************************************
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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
#include <string.h>

#define DEFAULT_BB_TRUNCATION_LENGTH 2

static uint bb_truncation_length;

/* PR 306971: test bb truncation */
static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    uint app_instruction_count = 0;
    instr_t *next, *instr = instrlist_first(bb);

    while (instr != NULL) {
        next = instr_get_next(instr);
        if (!instr_is_meta(instr)) {
            if (app_instruction_count == bb_truncation_length) {
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
            } else {
                app_instruction_count++;
            }
        }
        instr = next;
    }

    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    const char *options = dr_get_options(id);
    if (strlen(options) == 0) {
        bb_truncation_length = DEFAULT_BB_TRUNCATION_LENGTH;
    } else {
        ASSERT(strlen(options) == 1); /* supports bb truncation at 1-9 instrs */
        bb_truncation_length = (options[0] - '0');
        ASSERT(bb_truncation_length < 10 && bb_truncation_length > 0);
    }

    dr_fprintf(STDERR, "thank you for testing the client interface\n");
    dr_register_bb_event(bb_event);
}
