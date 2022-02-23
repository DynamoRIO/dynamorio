/* **********************************************************
 * Copyright (c) 2022 Google, Inc.   All rights reserved.
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

/* Tests the drbbdup extension. */

/* This test checks drbbdup's processing of empty basic blocks. */

/* Assumes that the target program contains nop instructions and that basic blocks
 * constructed by DynamoRIO are size of 1 instruction. The latter is achieved using the
 * max_bb_instrs runtime option.
 *
 * Nops are required as they are the type of instructions removed by this test.
 */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drbbdup.h"

#define TEST_PRIORITY_APP2APP_NAME "TEST_PRIORITY_APP2APP"
#define TEST_PRIORITY_APP2APP 100

#define ORIG_ANALYSIS_VAL (void *)555
#define ANALYSIS_VAL_1 (void *)888

static bool orig_analysis_called = false;
static bool default_analysis_called = false;
static bool case1_analysis_called = false;
static bool instrum_called = false;
static bool encountered_empty = false;
static bool is_cur_empty = false;

/* Assume single threaded. */
static uintptr_t encode_val = 2;

static bool
is_empty_bb(instrlist_t *bb)
{
    return instrlist_first(bb) == NULL;
}

static dr_emit_flags_t
remove_app_instr(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating)
{
    instr_t *first_instr = instrlist_first_app(bb);
    /* Relies on the -max_bb_instrs 0 option. */
    CHECK(instr_get_next(first_instr) == NULL, "must just be 1 instr");

    /* Remove app instruction to create an empty block. */
    if (instr_is_nop(first_instr)) {
        instrlist_remove(bb, first_instr);
        instr_destroy(drcontext, first_instr);
        CHECK(is_empty_bb(bb), "now must be an empty block");
        is_cur_empty = true;
    } else {
        is_cur_empty = false; /* note, current block is not empty. */
    }

    return DR_EMIT_DEFAULT;
}

static uintptr_t
set_up_bb_dups(void *drbbdup_ctx, void *drcontext, void *tag, instrlist_t *bb,
               bool *enable_dups, bool *enable_dynamic_handling, void *user_data)
{
    drbbdup_status_t res;

    if (is_empty_bb(bb))
        encountered_empty = true;

    res = drbbdup_register_case_encoding(drbbdup_ctx, 1);
    CHECK(res == DRBBDUP_SUCCESS, "failed to register case 1");

    *enable_dups = true;              /* always enable dups */
    *enable_dynamic_handling = false; /* disable dynamic handling */

    return 0; /* return default case */
}

static void
orig_analyse_bb(void *drcontext, void *tag, instrlist_t *bb, void *user_data,
                void **orig_analysis_data)
{
    if (is_cur_empty)
        CHECK(is_empty_bb(bb), "should be empty");

    *orig_analysis_data = ORIG_ANALYSIS_VAL;
    orig_analysis_called = true;
}

static dr_emit_flags_t
analyse_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating,
           uintptr_t encoding, void *user_data, void *orig_analysis_data,
           void **analysis_data)
{
    if (is_cur_empty)
        CHECK(is_empty_bb(bb), "should be empty");

    switch (encoding) {
    case 0:
        *analysis_data = NULL;
        default_analysis_called = true;
        break;
    case 1:
        *analysis_data = ANALYSIS_VAL_1;
        case1_analysis_called = true;
        break;
    default: CHECK(false, "invalid encoding");
    }

    return DR_EMIT_DEFAULT;
}

static void
update_encoding()
{
    if (encode_val != 0)
        encode_val--;
}

static void
insert_encode(void *drcontext, void *tag, instrlist_t *bb, instr_t *where,
              void *user_data, void *orig_analysis_data)
{
    if (!is_cur_empty)
        dr_insert_clean_call(drcontext, bb, where, update_encoding, false, 0);
}

static void
print_case(uintptr_t case_val)
{
    dr_fprintf(STDERR, "case %u\n", case_val);
}

static dr_emit_flags_t
instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                 instr_t *where, bool for_trace, bool translating, uintptr_t encoding,
                 void *user_data, void *orig_analysis_data, void *analysis_data)
{
    bool is_first, is_first_nonlabel, is_last;
    drbbdup_status_t res;

    CHECK(!is_cur_empty, "should not be called for empty basic block");

    res = drbbdup_is_first_instr(drcontext, instr, &is_first);
    CHECK(res == DRBBDUP_SUCCESS, "failed to check whether instr is start");
    CHECK(is_first, "must be first");

    res = drbbdup_is_first_nonlabel_instr(drcontext, instr, &is_first_nonlabel);
    CHECK(res == DRBBDUP_SUCCESS, "failed to check whether instr is first non-label");
    CHECK(is_first_nonlabel, "must be first non-label");

    /* Relies on the -max_bb_instrs 0 option. */
    res = drbbdup_is_last_instr(drcontext, instr, &is_last);
    CHECK(res == DRBBDUP_SUCCESS, "failed to check whether instr is last");
    CHECK(is_last, "must be last");

    if (is_first && encoding != 0) {
        instrum_called = true;
        dr_insert_clean_call(drcontext, bb, where, print_case, false, 1,
                             OPND_CREATE_INTPTR(encoding));
    }
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    drbbdup_status_t res;

    res = drbbdup_exit();
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup exit failed");

    CHECK(orig_analysis_called, "orig analysis was not done");
    CHECK(default_analysis_called, "default analysis was not done");
    CHECK(case1_analysis_called, "case 1 analysis was not done");

    CHECK(instrum_called, "instrumentation was not inserted");
    CHECK(encountered_empty, "encountered empty bb");

    bool app2app_res = drmgr_unregister_bb_app2app_event(remove_app_instr);
    CHECK(app2app_res, "failed to unregister app2app event");

    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_init();

    drbbdup_options_t opts = { 0 };
    opts.struct_size = sizeof(drbbdup_options_t);
    opts.set_up_bb_dups = set_up_bb_dups;
    opts.insert_encode = insert_encode;
    opts.analyze_orig = orig_analyse_bb;
    opts.destroy_orig_analysis = NULL;
    opts.analyze_case_ex = analyse_bb;
    opts.destroy_case_analysis = NULL;
    opts.instrument_instr_ex = instrument_instr;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&encode_val, OPSZ_PTR);
    /* Though single-threaded, we sanity-check the atomic load feature. */
    opts.atomic_load_encoding = true;
    opts.user_data = NULL;
    opts.non_default_case_limit = 2;
    opts.is_stat_enabled = false;

    drmgr_priority_t app2app_priority = { sizeof(app2app_priority),
                                          TEST_PRIORITY_APP2APP_NAME, NULL, NULL,
                                          TEST_PRIORITY_APP2APP };

    bool app2app_res =
        drmgr_register_bb_app2app_event(remove_app_instr, &app2app_priority);
    CHECK(app2app_res, "app2app failed");

    drbbdup_status_t res = drbbdup_init(&opts);
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup init failed");
    dr_register_exit_event(event_exit);
}
