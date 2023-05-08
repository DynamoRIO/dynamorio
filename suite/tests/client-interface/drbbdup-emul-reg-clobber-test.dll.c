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

/* Test i#5906: verify that drbbdup does not clobber app values when expanding
 * reps. */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drbbdup.h"
#include "drutil.h"
#include "drreg.h"
#include <string.h>

/* Assume single threaded. */
static uintptr_t encode_val = 1;

static dr_emit_flags_t
app2app_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
              bool translating)
{
#if VERBOSE
    /* Useful for debugging. */
    instrlist_disassemble(drcontext, tag, bb, STDERR);
#endif

    /* Test drutil rep string expansion interacting with drbbdup. */
    bool expanded;
    if (!drutil_expand_rep_string_ex(drcontext, bb, &expanded, NULL))
        DR_ASSERT(false);

    return DR_EMIT_DEFAULT;
}

static uintptr_t
set_up_bb_dups(void *drbbdup_ctx, void *drcontext, void *tag, instrlist_t *bb,
               bool *enable_dups, bool *enable_dynamic_handling, void *user_data)
{
    drbbdup_status_t res;

    CHECK(enable_dups != NULL, "should not be NULL");
    CHECK(enable_dynamic_handling != NULL, "should not be NULL");

    res = drbbdup_register_case_encoding(drbbdup_ctx, 1);
    CHECK(res == DRBBDUP_SUCCESS, "failed to register case 1");
    res = drbbdup_register_case_encoding(drbbdup_ctx, 2);
    CHECK(res == DRBBDUP_SUCCESS, "failed to register case 2");

    *enable_dups = true;
    *enable_dynamic_handling = false; /* disable dynamic handling */

    return 0; /* return default case */
}

static void
analyze_case(void *drcontext, void *tag, instrlist_t *bb, uintptr_t encoding,
             void *user_data, void *orig_analysis_data, void **analysis_data)
{
    *analysis_data = NULL;
}

static void
destroy_case_analysis(void *drcontext, uintptr_t encoding, void *user_data,
                      void *orig_analysis_data, void *analysis_data)
{
}

static void
write_aflags(void *drcontext, instrlist_t *bb, instr_t *inst)
{
    instrlist_meta_preinsert(bb, inst,
                             XINST_CREATE_cmp(drcontext, opnd_create_reg(DR_REG_START_32),
                                              OPND_CREATE_INT32(0)));
}

static dr_emit_flags_t
instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                 instr_t *where, bool for_trace, bool translating, uintptr_t encoding,
                 void *user_data, void *orig_analysis_data, void *analysis_data)
{
#ifdef X86
    if (instr_get_opcode(instr) == OP_movs) {
        /* Check that modifying flags does not clobber app values (i#5906). */
        if (drreg_reserve_aflags(drcontext, bb, instr) != DRREG_SUCCESS)
            CHECK(false, "cannot reserve aflags");
        write_aflags(drcontext, bb, instr);
        if (drreg_unreserve_aflags(drcontext, bb, instr) != DRREG_SUCCESS)
            CHECK(false, "cannot unreserve aflags");
    }
#endif

    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    drbbdup_status_t res = drbbdup_exit();
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup exit failed");

    bool app2app_res = drmgr_unregister_bb_app2app_event(app2app_event);
    CHECK(app2app_res, "failed to unregister app2app event");

    drmgr_exit();
    drutil_exit();
    if (drreg_exit() != DRREG_SUCCESS)
        CHECK(false, "drreg_exit failed");

    dr_fprintf(STDERR, "Success\n");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drreg_options_t ops = { sizeof(ops), 1, false };
    if (!drmgr_init() || !drutil_init() || drreg_init(&ops) != DRREG_SUCCESS)
        CHECK(false, "library init failed");

    drbbdup_options_t opts = { 0 };
    opts.struct_size = sizeof(drbbdup_options_t);
    opts.set_up_bb_dups = set_up_bb_dups;
    opts.analyze_case = analyze_case;
    opts.destroy_case_analysis = destroy_case_analysis;
    opts.instrument_instr_ex = instrument_instr;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&encode_val, OPSZ_PTR);
    opts.non_default_case_limit = 3;

    drbbdup_status_t res = drbbdup_init(&opts);
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup init failed");
    dr_register_exit_event(event_exit);

    bool app2app_res = drmgr_register_bb_app2app_event(app2app_event, NULL);
    CHECK(app2app_res, "app2app failed");
}
