/* **********************************************************
 * Copyright (c) 2016-2022 Google, Inc.  All rights reserved.
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

/* Tests the combination of drreg and drutil, along with other inserted control flow */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include <string.h> /* memcpy */

static bool verbose;

static void
event_exit(void);
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, OUT void **user_data);
static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void *user_data);
static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                bool for_trace, bool translating, void *user_data);

DR_EXPORT void
dr_init(client_id_t id)
{
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
    drmgr_priority_t priority = { sizeof(priority), "drutil-test", NULL, NULL, 0 };
    bool ok;
    drreg_status_t res;

    dr_set_client_name("DynamoRIO Sample Client 'countcalls'",
                       "http://dynamorio.org/issues");

    drmgr_init();
    res = drreg_init(&ops);
    CHECK(res == DRREG_SUCCESS, "drreg init failed");
    drutil_init();
    dr_register_exit_event(event_exit);

    ok = drmgr_register_bb_instrumentation_ex_event(event_bb_app2app, event_bb_analysis,
                                                    event_bb_insert, NULL, &priority);
    CHECK(ok, "drmgr register bb failed");
}

static void
event_exit(void)
{
    bool ok = drmgr_unregister_bb_instrumentation_ex_event(
        event_bb_app2app, event_bb_analysis, event_bb_insert, NULL);
    CHECK(ok, "drmgr un register bb failed");
    drutil_exit();
    drreg_exit();
    drmgr_exit();
    dr_fprintf(STDERR, "all done\n");
}

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, OUT void **user_data)
{
    bool *expanded = (bool *)dr_thread_alloc(drcontext, sizeof(bool));
    *user_data = (void *)expanded;
    if (!drutil_expand_rep_string_ex(drcontext, bb, expanded, NULL)) {
        CHECK(false, "drutil rep expansion failed");
    }
    /* XXX: Unfortunately it's not easy to automate a proper test that this
     * does what we want: for now we just test that it doesn't assert.
     * I tested by manually disabling this and ensuring that 32-bit common.eflags
     * crashes (thus reproducing i#1954).
     */
    if (!*expanded) {
        drreg_status_t res =
            drreg_set_bb_properties(drcontext, DRREG_IGNORE_CONTROL_FLOW);
        CHECK(res == DRREG_SUCCESS, "failed to set properties");
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void *user_data)
{
    if (*(bool *)user_data) {
        drreg_status_t res =
            drreg_set_bb_properties(drcontext, DRREG_CONTAINS_SPANNING_CONTROL_FLOW);
        CHECK(res == DRREG_SUCCESS, "failed to set properties");
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                bool for_trace, bool translating, void *user_data)
{
    int i;
    reg_id_t reg1, reg2;
    drreg_status_t res;
    if (instr_writes_memory(instr)) {
        for (i = 0; i < instr_num_dsts(instr); i++) {
            if (opnd_is_memory_reference(instr_get_dst(instr, i))) {
                res = drreg_reserve_register(drcontext, bb, instr, NULL, &reg1);
                CHECK(res == DRREG_SUCCESS, "failed to reserve");
                res = drreg_reserve_register(drcontext, bb, instr, NULL, &reg2);
                CHECK(res == DRREG_SUCCESS, "failed to reserve");
                drutil_insert_get_mem_addr(drcontext, bb, instr, instr_get_dst(instr, i),
                                           reg1, reg2);
                res = drreg_unreserve_register(drcontext, bb, instr, reg2);
                CHECK(res == DRREG_SUCCESS, "failed to unreserve");
                res = drreg_unreserve_register(drcontext, bb, instr, reg1);
                CHECK(res == DRREG_SUCCESS, "failed to unreserve");
            }
        }
    }
    if (drmgr_is_last_instr(drcontext, instr))
        dr_thread_free(drcontext, user_data, sizeof(bool));
    return DR_EMIT_DEFAULT;
}
