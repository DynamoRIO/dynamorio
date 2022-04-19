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

/* Tests the drbbdup extension when combined with drrwap but using drwrap
 * in only a subset of the cases.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drbbdup.h"
#include "drwrap.h"
#include "client_tools.h"
#include <string.h>

/* We assume the app is single-threaded for this test. */
static uintptr_t case_encoding = 0;

static uintptr_t
set_up_bb_dups(void *drbbdup_ctx, void *drcontext, void *tag, instrlist_t *bb,
               bool *enable_dups, bool *enable_dynamic_handling, void *user_data)
{
    drbbdup_status_t res;

    CHECK(enable_dups != NULL, "should not be NULL");
    CHECK(enable_dynamic_handling != NULL, "should not be NULL");

    res = drbbdup_register_case_encoding(drbbdup_ctx, 1);
    CHECK(res == DRBBDUP_SUCCESS, "failed to register case 1");

    *enable_dups = true;
    *enable_dynamic_handling = false; /* disable dynamic handling */
    return 0;                         /* return default case */
}

static void
switch_modes(void)
{
    case_encoding = (case_encoding == 0) ? 1 : 0;
    dr_fprintf(STDERR, "switching to instrumentation mode %d\n", case_encoding);
}

static dr_emit_flags_t
event_analyse_case(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, uintptr_t encoding, void *user_data,
                   void *orig_analysis_data, void **case_analysis_data)
{
    int consec_nop_count = 0;
    for (instr_t *inst = instrlist_first_app(bb); inst != NULL;
         inst = instr_get_next_app(inst)) {
        if (instr_get_opcode(inst) == OP_nop)
            ++consec_nop_count;
        else {
            if (consec_nop_count >= 4) {
                dr_insert_clean_call(drcontext, bb, inst, switch_modes, false, 0);
            }
            consec_nop_count = 0;
        }
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                       instr_t *where, bool for_trace, bool translating,
                       uintptr_t encoding, void *user_data, void *orig_analysis_data,
                       void *case_analysis_data)
{
    if (encoding == 0) {
        return drwrap_invoke_insert(drcontext, tag, bb, instr, where, for_trace,
                                    translating, case_analysis_data);
    }
    return drwrap_invoke_insert_cleanup_only(drcontext, tag, bb, instr, where, for_trace,
                                             translating, case_analysis_data);
}

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    dr_fprintf(STDERR, "in wrap_pre\n");
    CHECK(wrapcxt != NULL && user_data != NULL, "invalid arg");
    if (drwrap_get_arg(wrapcxt, 0) == (void *)2) {
        bool ok = drwrap_set_arg(wrapcxt, 0, (void *)42);
        CHECK(ok, "set_arg error");
    }
}

static void
wrap_post(void *wrapcxt, void *user_data)
{
    dr_fprintf(STDERR, "in wrap_post\n");
}

static void
event_module_load(void *drcontext, const module_data_t *mod, bool loaded)
{
    if (strstr(dr_module_preferred_name(mod), "client.drbbdup-drwrap-test.appdll.") ==
        NULL)
        return;
    app_pc target = (app_pc)dr_get_proc_address(mod->handle, "wrapme");
    CHECK(target != NULL, "cannot find lib export");
    int flags = 0;
    /* We test the hard-to-handle retaddr-replacing scheme. */
    flags |= DRWRAP_REPLACE_RETADDR;
    bool res = drwrap_wrap_ex(target, wrap_pre, wrap_post, NULL, flags);
    CHECK(res, "wrap failed");
}

static void
event_exit(void)
{
    bool res = drmgr_unregister_module_load_event(event_module_load);
    CHECK(res, "drmgr_unregister_event_module_load failed");
    drwrap_exit();
    drbbdup_status_t status = drbbdup_exit();
    CHECK(status == DRBBDUP_SUCCESS, "drbbdup exit failed");
    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_init();

    drbbdup_options_t opts = { 0 };
    opts.struct_size = sizeof(drbbdup_options_t);
    opts.set_up_bb_dups = set_up_bb_dups;
    opts.analyze_case_ex = event_analyse_case;
    opts.instrument_instr_ex = event_instrument_instr;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&case_encoding, OPSZ_PTR);
    opts.atomic_load_encoding = false;
    opts.max_case_encoding = 1;
    opts.non_default_case_limit = 1;

    drbbdup_status_t status = drbbdup_init(&opts);
    CHECK(status == DRBBDUP_SUCCESS, "drbbdup init failed");

    dr_register_exit_event(event_exit);

    /* Make sure requesting inversion fails *after* drwrap_init().
     * This also stresses drwrap re-attach via init;exit;init.
     */
    bool res = drwrap_init();
    CHECK(res, "drwrap_init failed");
    res = drwrap_set_global_flags(DRWRAP_INVERT_CONTROL);
    CHECK(!res, "DRWRAP_INVERT_CONTROL after drwrap_init should fail");
    drwrap_exit();

    /* Test drwrap re-attach for flags. */
    res = drwrap_init();
    CHECK(res, "drwrap_init failed");
    res = drwrap_set_global_flags(DRWRAP_SAFE_READ_RETADDR);
    CHECK(res, "setting flag should succeed");
    drwrap_exit();
    res = drwrap_init();
    CHECK(res, "drwrap_init failed");
    res = drwrap_set_global_flags(DRWRAP_SAFE_READ_RETADDR);
    CHECK(res, "setting flag 2nd time should succeed");
    drwrap_exit();

    /* Now set up for this test. */
    res = drwrap_set_global_flags(DRWRAP_INVERT_CONTROL);
    CHECK(res, "DRWRAP_INVERT_CONTROL failed");
    res = drwrap_init();
    CHECK(res, "drwrap_init failed");

    res = drmgr_register_module_load_event(event_module_load);
    CHECK(res, "drmgr_register_event_module_load failed");
}
