/* ****************************************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
 * ***************************************************************************/

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

/*
 * Code Manipulation API Test for Instruction Traversal.
 *
 * The code is to test API functions: instr_get_next_app() and
 * instrlist_first_app(), by comparing with the output of instr_get_next() and
 * instrlist_first()
 *
 * Note: Do not apply this test to multi-threading applications. And do not
 * test it in multi-client experiments.
 */

#include "dr_api.h"

/* we have two global count variables used by two API sets */
static uint64 global_count;
static uint64 global_count_app;

static void
inscount(uint num_instrs)
{
    global_count += num_instrs;
}

static void
inscount_app(uint num_instrs)
{
    global_count_app += num_instrs;
}

static void
event_exit(void);
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating);

DR_EXPORT void
dr_init(client_id_t id)
{
    /* register events */
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);
}

static void
event_exit(void)
{
    if (global_count_app == global_count)
        dr_fprintf(STDERR, "all instructions matched\n");
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating)
{
    instr_t *instr;
    uint num_instrs;

    /* first loop instruction count, using old API */
    for (instr = instrlist_first(bb), num_instrs = 0; instr != NULL;
         instr = instr_get_next(instr)) {
        num_instrs++;
    }

    dr_insert_clean_call(drcontext, bb, instrlist_first(bb), (void *)inscount,
                         false /* save fpstate */, 1, OPND_CREATE_INT32(num_instrs));

    /* second loop instruction count, using new API */
    for (instr = instrlist_first_app(bb), num_instrs = 0; instr != NULL;
         instr = instr_get_next_app(instr)) {
        num_instrs++;
    }

    dr_insert_clean_call(drcontext, bb, instrlist_first_app(bb), (void *)inscount_app,
                         false /* save fpstate */, 1, OPND_CREATE_INT32(num_instrs));

    return DR_EMIT_DEFAULT;
}
