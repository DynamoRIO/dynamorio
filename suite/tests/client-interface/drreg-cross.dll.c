/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
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

/* Tests reserving across app instrs with the drreg extension */

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "client_tools.h"

/* This test assumes that DR has a global lock around bb creation,
 * allowing us to use a global var here.
 */
static reg_id_t reg = DR_REG_NULL;

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    /* We test reserving across app instrs by reserving on each store and
     * unreserving on the subsequent instr.
     */
    drvector_t allowed;
    if (reg != DR_REG_NULL) {
        if (drreg_unreserve_register(drcontext, bb, instr, reg) != DRREG_SUCCESS)
            CHECK(false, "failed to unreserve");
        reg = DR_REG_NULL;
    }

    if (!instr_is_app(instr))
        return DR_EMIT_DEFAULT;
    if (!instr_writes_memory(instr))
        return DR_EMIT_DEFAULT;

    if (!drmgr_is_last_instr(drcontext, instr)) {
        drreg_init_and_fill_vector(&allowed, true);
        /* Limit the registers for more of a stress test: */
        drreg_set_vector_entry(&allowed, IF_X86_ELSE(DR_REG_XCX, DR_REG_R0), false);
        drreg_set_vector_entry(&allowed, IF_X86_ELSE(DR_REG_XDX, DR_REG_R1), false);
        if (drreg_reserve_register(drcontext, bb, instr, &allowed, &reg) != DRREG_SUCCESS)
            DR_ASSERT(false);
        drvector_delete(&allowed);
    }
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
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t ops = { sizeof(ops), 1 /*max slots needed*/, false };
    if (!drmgr_init())
        CHECK(false, "drmgr init failed");
    if (drreg_init(&ops) != DRREG_SUCCESS)
        CHECK(false, "drreg_init failed");
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        CHECK(false, "bb reg failed");
}
