/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

/* TODO i#2985: Test the future drx_expand_scatter_gather() extension using drmgr. */

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drx.h"

#define CHECK(x, msg)                                                                \
    do {                                                                             \
        if (!(x)) {                                                                  \
            dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
            dr_abort();                                                              \
        }                                                                            \
    } while (0);

static void
event_exit(void)
{
    drx_exit();
    drreg_exit();
    drmgr_exit();
    dr_fprintf(STDERR, "event_exit\n");
}

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating)
{
    instr_t *instr;
    bool expanded = false;
    bool scatter_gather_present = false;

    for (instr = instrlist_first_app(bb); instr != NULL;
         instr = instr_get_next_app(instr)) {
        if (instr_is_gather(instr)) {
            scatter_gather_present = true;
        }
        if (instr_is_scatter(instr)) {
            scatter_gather_present = true;
        }
    }
    bool expansion_ok = drx_expand_scatter_gather(drcontext, bb, &expanded);
    if (!expansion_ok) {
        /* XXX i#2985: The qword versions of scatter/gather are unsupported
         * in 32-bit mode.
         */
        IF_X64(CHECK(false, "drx_expand_scatter_gather() failed"));
    }
    CHECK((scatter_gather_present IF_X64(&&expanded)) || (expansion_ok && !expanded),
          "drx_expand_scatter_gather() bad OUT values");
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_priority_t priority = { sizeof(priority), "drx-scattergather", NULL, NULL, 0 };
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
    drreg_status_t res;
    bool ok = drmgr_init();
    CHECK(ok, "drmgr_init failed");
    ok = drx_init();
    CHECK(ok, "drx_init failed");
    res = drreg_init(&ops);
    CHECK(res == DRREG_SUCCESS, "drreg_init failed");
    dr_register_exit_event(event_exit);

    ok = drmgr_register_bb_app2app_event(event_bb_app2app, &priority);
    CHECK(ok, "drmgr register bb failed");
}
