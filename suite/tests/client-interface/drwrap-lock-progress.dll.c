/* **********************************************************
 * Copyright (c) Bryan Berns  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drwrap.h"

static void *done;
static app_pc fresh, late;
static int late_calls;
static bool register_late;

static void
late_pre(void *wrapcxt, void **user_data)
{
    ++late_calls;
}

static void
wait_pre(void *wrapcxt, void **user_data)
{
    /* Register an already translated target while inside a wrap callback. */
    register_late = (ptr_uint_t)drwrap_get_arg(wrapcxt, 1) != 0;
    if (register_late)
        ASSERT(drwrap_wrap(late, late_pre, NULL));
    ASSERT(SetEvent((HANDLE)drwrap_get_arg(wrapcxt, 0)));
    /* The worker must translate new application code while this callback is active. */
    ASSERT(dr_event_wait(done));
}

static void
signal_done(void)
{
    ASSERT(dr_event_signal(done));
}

static dr_emit_flags_t
instrument(void *dc, void *tag, instrlist_t *bb, instr_t *inst, bool for_trace,
           bool translating, void *user_data)
{
    if (drmgr_is_first_instr(dc, inst) && dr_fragment_app_pc(tag) == fresh)
        dr_insert_clean_call(dc, bb, inst, signal_done, false, 0);
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    ASSERT(late_calls == (register_late ? 1 : 0));
    dr_event_destroy(done);
    drwrap_exit();
    drmgr_exit();
    dr_fprintf(STDERR, "wrapper progress verified\n");
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    module_data_t *app = dr_get_main_module();
    app_pc wait = (app_pc)dr_get_proc_address(app->handle, "wrap_and_wait");
    fresh = (app_pc)dr_get_proc_address(app->handle, "fresh_target");
    late = (app_pc)dr_get_proc_address(app->handle, "late_target");
    ASSERT(wait != NULL && fresh != NULL && late != NULL);
    dr_free_module_data(app);
    ASSERT(drmgr_init() && drwrap_init());
    done = dr_event_create();
    ASSERT(drwrap_wrap(wait, wait_pre, NULL));
    ASSERT(drmgr_register_bb_instrumentation_event(NULL, instrument, NULL));
    ASSERT(drmgr_register_exit_event(event_exit));
}
