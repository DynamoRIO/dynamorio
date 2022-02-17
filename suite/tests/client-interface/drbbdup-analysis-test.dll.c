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

/* A test for the drbbdup extension. In particular, the test inserts analysis
 * labels during case analysis and checks that these labels persist during the
 * insertion stage.
 */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drbbdup.h"

#define TEST_NOTE_VAL (void *)767LL

static bool instrum_called = false;
static bool test_label_persisted = false;

/* Assume single threaded. */
static uintptr_t encode_val = 1;

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

    return 0; /* return default case */
}

static void
insert_analysis_labels(void *drcontext, instrlist_t *bb)
{
    instr_t *instr = instrlist_first(bb);
    while (instr != NULL) {
        instr_t *test_label = INSTR_CREATE_label(drcontext);
        instr_set_note(test_label, TEST_NOTE_VAL);
        instrlist_meta_preinsert(bb, instr, test_label);
        instr = instr_get_next_app(instr);
    }
}

static void
analyse_bb(void *drcontext, void *tag, instrlist_t *bb, uintptr_t encoding,
           void *user_data, void *orig_analysis_data, void **analysis_data)
{
    switch (encoding) {
    case 0: break;
    case 1: insert_analysis_labels(drcontext, bb); break;
    default: CHECK(false, "invalid encoding");
    }
}

static bool
is_test_label(instr_t *instr)
{
    return instr_is_label(instr) && instr_get_note(instr) == TEST_NOTE_VAL;
}

static void
instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                 instr_t *where, uintptr_t encoding, void *user_data,
                 void *orig_analysis_data, void *analysis_data)
{
    switch (encoding) {
    case 0:
        if (is_test_label(instr))
            CHECK(false, "no test label should be present in default case");
        break;
    case 1:
        if (is_test_label(instr)) {
            test_label_persisted = true;
        } else if (instr_is_app(instr)) {
            bool is_label = is_test_label(instr_get_prev(where));
            CHECK(is_label, "prev instr should be test label");
        }
        break;
    default: CHECK(false, "invalid encoding");
    }

    instrum_called = true;
}

static void
event_exit(void)
{
    drbbdup_status_t res = drbbdup_exit();
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup exit failed");

    CHECK(instrum_called, "instrumentation was not inserted");
    CHECK(test_label_persisted, "test label should persist to insertion stage");

    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_init();

    drbbdup_options_t opts = { 0 };
    opts.struct_size = sizeof(drbbdup_options_t);
    opts.set_up_bb_dups = set_up_bb_dups;
    opts.insert_encode = NULL;
    opts.analyze_orig = NULL;
    opts.destroy_orig_analysis = NULL;
    opts.analyze_case = analyse_bb;
    opts.destroy_case_analysis = NULL;
    opts.instrument_instr = instrument_instr;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&encode_val, OPSZ_PTR);
    opts.user_data = NULL;
    opts.non_default_case_limit = 1;
    opts.is_stat_enabled = false;

    drbbdup_status_t res = drbbdup_init(&opts);
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup init failed");
    dr_register_exit_event(event_exit);
}
