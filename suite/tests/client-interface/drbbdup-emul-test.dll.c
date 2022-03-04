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

/* Tests the drbbdup extension's interactions with emulation. */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drbbdup.h"
#include "drutil.h"

/* Assume single threaded. */
static uintptr_t encode_val = 3;

static dr_emit_flags_t
app2app_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
              bool translating)
{
    /* Test drutil rep string expansion interacting with drbbdup. */
    bool expanded;
    if (!drutil_expand_rep_string_ex(drcontext, bb, &expanded, NULL))
        DR_ASSERT(false);
    if (expanded) {
        /* We can't overlap ours with drutil's so bow out. */
        return DR_EMIT_DEFAULT;
    }
    /* Test handling of DR_EMULATE_REST_OF_BLOCK by inserting it in the middle
     * of a multi-instr block and ensuring drbbdup doesn't let it leak through into
     * the start of subsequent cloned blocks.
     * We want to leave some blocks unchanged though to test drbbdup's own
     * emulation labels.
     *
     * XXX i#5390: We should also test the last instr being emulated: not sure drbbdup
     * will do the right thing there if it's a "special" instr.
     */
    static int bb_count;
    if (++bb_count % 2 == 0)
        return DR_EMIT_DEFAULT;
    instr_t *mid = instrlist_last_app(bb);
    if (mid == NULL || mid == instrlist_first_app(bb))
        return DR_EMIT_DEFAULT;
    mid = instr_get_prev_app(mid);
    if (mid == NULL || mid == instrlist_first_app(bb))
        return DR_EMIT_DEFAULT;
    emulated_instr_t emulated_instr;
    emulated_instr.size = sizeof(emulated_instr);
    emulated_instr.pc = instr_get_app_pc(mid);
    emulated_instr.instr = instr_clone(drcontext, mid);
    emulated_instr.flags = DR_EMULATE_REST_OF_BLOCK;
    drmgr_insert_emulation_start(drcontext, bb, mid, &emulated_instr);

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

static dr_emit_flags_t
instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                 instr_t *where, bool for_trace, bool translating, uintptr_t encoding,
                 void *user_data, void *orig_analysis_data, void *analysis_data)
{
    bool is_first;
    drbbdup_status_t res;

    res = drbbdup_is_first_instr(drcontext, instr, &is_first);
    CHECK(res == DRBBDUP_SUCCESS, "failed to check whether instr is first");
    if (is_first) {
        /* Ensure DR_EMULATE_REST_OF_BLOCK didn't leak through. */
        const emulated_instr_t *emul_info;
        if (drmgr_in_emulation_region(drcontext, &emul_info)) {
            /* It might be the rep string, which sets DR_EMULATE_INSTR_ONLY.
             * XXX: We could try to pass a flag: but drbbdup won't let
             * us pass from app2app; we'd need TLS or a global.
             */
            CHECK(!TEST(DR_EMULATE_REST_OF_BLOCK, emul_info->flags) ||
                      TEST(DR_EMULATE_INSTR_ONLY, emul_info->flags),
                  "DR_EMULATE_REST_OF_BLOCK leaked through!");
        }
    }

    /* Ensure the drmgr emulation API works for a final special instr where "instr" !=
     * "where".  The emulation shows up on the label prior to the "where" for the last
     * instr so we can't compare emul to instr; instead we make sure the emul returned
     * is never a jump to a label.
     */
    instr_t *emul = drmgr_orig_app_instr_for_fetch(drcontext);
    CHECK(emul == NULL || !instr_is_ubr(emul) || !opnd_is_instr(instr_get_target(emul)),
          "app instr should never be jump to label");

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

    dr_fprintf(STDERR, "Success\n");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_init();

    drbbdup_options_t opts = { 0 };
    opts.struct_size = sizeof(drbbdup_options_t);
    opts.set_up_bb_dups = set_up_bb_dups;
    opts.instrument_instr_ex = instrument_instr;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&encode_val, OPSZ_PTR);
    opts.non_default_case_limit = 3;

    drbbdup_status_t res = drbbdup_init(&opts);
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup init failed");
    dr_register_exit_event(event_exit);

    bool app2app_res = drmgr_register_bb_app2app_event(app2app_event, NULL);
    CHECK(app2app_res, "app2app failed");
}
