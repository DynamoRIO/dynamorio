/* **********************************************************
 * Copyright (c) 2013-2022 Google, Inc.  All rights reserved.
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

/* Tests the drx extension with drmgr */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drreg.h"
#include "drx.h"

static uint counterA;
static uint counterB;
#if defined(AARCH64)
static uint64 counterC;
static uint64 counterD;
#endif
#if defined(AARCHXX)
static uint counterE;
#endif

static void
event_exit(void)
{
    drx_exit();
    drreg_exit();
    drmgr_exit();
    CHECK(counterB == 3 * counterA, "counter inc messed up");
#if defined(AARCH64)
    CHECK(counterC == 3 * counterA, "64-bit counter inc messed up");
    CHECK(counterD == 3 * counterA, "64-bit counter inc with acq_rel messed up");
#endif
#if defined(AARCHXX)
    CHECK(counterE == 3 * counterA, "32-bit counter inc with acq_rel messed up");
#endif
    dr_fprintf(STDERR, "event_exit\n");
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    if (!drmgr_is_first_instr(drcontext, inst))
        return DR_EMIT_DEFAULT;
    /* Exercise drreg's adjacent increment aflags spill removal code */
    drx_insert_counter_update(drcontext, bb, inst, SPILL_SLOT_MAX + 1,
                              IF_NOT_X86_(SPILL_SLOT_MAX + 1) & counterA, 1, 0);
    drx_insert_counter_update(drcontext, bb, inst, SPILL_SLOT_MAX + 1,
                              IF_NOT_X86_(SPILL_SLOT_MAX + 1) & counterB, 3, 0);
#if defined(AARCH64)
    drx_insert_counter_update(drcontext, bb, inst, SPILL_SLOT_MAX + 1, SPILL_SLOT_MAX + 1,
                              &counterC, 3, DRX_COUNTER_64BIT);
    drx_insert_counter_update(drcontext, bb, inst, SPILL_SLOT_MAX + 1, SPILL_SLOT_MAX + 1,
                              &counterD, 3, DRX_COUNTER_64BIT | DRX_COUNTER_REL_ACQ);
#endif
#if defined(AARCHXX)
    drx_insert_counter_update(drcontext, bb, inst, SPILL_SLOT_MAX + 1, SPILL_SLOT_MAX + 1,
                              &counterE, 3, DRX_COUNTER_REL_ACQ);
#endif
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
    drreg_status_t res;
    bool ok = drmgr_init();
    CHECK(ok, "drmgr_init failed");
    ok = drx_init();
    CHECK(ok, "drx_init failed");
    res = drreg_init(&ops);
    CHECK(res == DRREG_SUCCESS, "drreg_init failed");
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        DR_ASSERT(false);
}
