/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

/* Tests the drreg extension */

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "client_tools.h"
#include <string.h> /* memset */

#define CHECK(x, msg) do {               \
    if (!(x)) {                          \
        dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
        dr_abort();                      \
    }                                    \
} while (0);

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    reg_id_t reg;
    drreg_status_t res;
    drvector_t allowed;

    res = drreg_reserve_register(drcontext, bb, inst, NULL, &reg);
    CHECK(res == DRREG_SUCCESS, "default reserve should always work");
    res = drreg_unreserve_register(drcontext, bb, inst, reg);
    CHECK(res == DRREG_SUCCESS, "default unreserve should always work");

    drvector_init(&allowed, DR_NUM_GPR_REGS, false/*!synch*/, NULL);
    for (reg = 0; reg < DR_NUM_GPR_REGS; reg++)
        drvector_set_entry(&allowed, reg, NULL);
    drvector_set_entry(&allowed, DR_REG_XDI-DR_REG_START_GPR, (void *)(ptr_uint_t)1);
    res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg);
    CHECK(res == DRREG_SUCCESS && reg == DR_REG_XDI, "only 1 choice");
    res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg);
    CHECK(res == DRREG_ERROR_REG_CONFLICT, "still reserved");
    res = drreg_unreserve_register(drcontext, bb, inst, reg);
    CHECK(res == DRREG_SUCCESS, "unreserve should work");
    drvector_delete(&allowed);

    /* XXX i#511: add more tests */

    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    if (!drmgr_unregister_bb_insertion_event(event_app_instruction) ||
        drreg_exit() != DRREG_SUCCESS)
        CHECK(false, "exit failed");

    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drreg_options_t ops = {sizeof(ops), 2 /*max slots needed*/, false};
    if (!drmgr_init() ||
        drreg_init(&ops) != DRREG_SUCCESS)
        CHECK(false, "init failed");

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        CHECK(false, "init failed");
}
