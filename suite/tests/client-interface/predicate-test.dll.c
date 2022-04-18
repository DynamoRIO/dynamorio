/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
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

/* Tests the auto-predicate functionality built into drmgr */

#include "dr_api.h"
#include "drmgr.h"
#include "client_tools.h"
#include "drutil.h"
#include "drreg.h"
#include <string.h> /* memset */

#define MINSERT instrlist_meta_preinsert

static app_pc app;

static void
dereference_app(app_pc app_addr)
{
    /* store to a global so the load hopefully does not get optimized away */
    app = *(app_pc *)app_addr;
}

static void
instrument_mem(void *drcontext, instrlist_t *bb, instr_t *inst, opnd_t ref)
{
    reg_id_t reg_ptr, reg_tmp;
    bool ok;

    if (drreg_reserve_register(drcontext, bb, inst, NULL, &reg_ptr) != DRREG_SUCCESS ||
        drreg_reserve_register(drcontext, bb, inst, NULL, &reg_tmp) != DRREG_SUCCESS) {
        CHECK(false, "drreg_reserve_register() failed");
    }
    ok = drutil_insert_get_mem_addr(drcontext, bb, inst, ref, reg_ptr, reg_tmp);
    CHECK(ok, "drutil_insert_get_mem_addr() failed");
    /* Test that a clean call is predicated correctly; if this clean call
     * is not predicated correctly then DR will crash.
     */
    dr_insert_clean_call(drcontext, bb, inst, (void *)dereference_app, false, 1,
                         opnd_create_reg(reg_ptr));
    /* Test that regular meta-instrumentation is predicated correctly; if
     * this load is not predicated correctly then DR will crash.
     */
    MINSERT(bb, inst,
            XINST_CREATE_load_1byte(drcontext, opnd_create_reg(reg_tmp),
                                    OPND_CREATE_MEM8(reg_ptr, 0)));

    if (drreg_unreserve_register(drcontext, bb, inst, reg_ptr) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, bb, inst, reg_tmp) != DRREG_SUCCESS)
        CHECK(false, "drreg_unreserve_register() failed");
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    int i;

    if (!instr_reads_memory(inst))
        return DR_EMIT_DEFAULT;
    /* Reading an app value should never crash if the underlying app doesn't
     * crash; we can ensure this because even if the app instruction is
     * predicated, if the load does not occur neither does the clean call
     * due to drmgr's auto-predication guarantees.
     */
    for (i = 0; i < instr_num_srcs(inst); i++) {
        if (opnd_is_memory_reference(instr_get_src(inst, i)))
            instrument_mem(drcontext, bb, inst, instr_get_src(inst, i));
    }
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    if (!drmgr_unregister_bb_insertion_event(event_app_instruction) ||
        drreg_exit() != DRREG_SUCCESS)
        CHECK(false, "exit failed");
    drutil_exit();
    drmgr_exit();
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
    if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS || !drutil_init())
        CHECK(false, "init failed");

    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        CHECK(false, "init failed");
}
