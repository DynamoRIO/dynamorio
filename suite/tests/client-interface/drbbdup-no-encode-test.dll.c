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

/* Tests the drbbdup extension when encoding is not inserted at the start of
 * basic blocks. It relies on drbbdup's guarantee that it does not modify
 * any set encoding of a thread on its own accord.
 */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drbbdup.h"

#define USER_DATA_VAL (void *)222
static uintptr_t case_encoding = 1;
static bool instrum_called = false;

static uintptr_t
set_up_bb_dups(void *drbbdup_ctx, void *drcontext, void *tag, instrlist_t *bb,
               bool *enable_dups, bool *enable_dynamic_handling, void *user_data)
{
    drbbdup_status_t res;

    CHECK(enable_dups != NULL, "should not be NULL");
    CHECK(enable_dynamic_handling != NULL, "should not be NULL");
    CHECK(user_data == USER_DATA_VAL, "user data does not match");

    res = drbbdup_register_case_encoding(drbbdup_ctx, 1);
    CHECK(res == DRBBDUP_SUCCESS, "failed to register case 1");

    *enable_dups = true;
    *enable_dynamic_handling = false; /* disable dynamic handling */
    return 0;                         /* return default case */
}

static void
print_case(uintptr_t case_val)
{
    dr_fprintf(STDERR, "case %u\n", case_val);
}

static void
instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                 instr_t *where, uintptr_t encoding, void *user_data,
                 void *orig_analysis_data, void *analysis_data)
{
    bool is_start;
    drbbdup_status_t res;

    CHECK(user_data == USER_DATA_VAL, "user data does not match");
    CHECK(orig_analysis_data == NULL, "orig analysis data should be NULL");
    CHECK(analysis_data == NULL, "analysis should be NULL");

    res = drbbdup_is_first_instr(drcontext, instr, &is_start);
    CHECK(res == DRBBDUP_SUCCESS, "failed to check whether instr is start");

    if (is_start && encoding != 1) {
        dr_insert_clean_call(drcontext, bb, where, print_case, false, 1,
                             OPND_CREATE_INTPTR(encoding));
    } else {
        instrum_called = true;
    }
}

static void
event_exit(void)
{
    drbbdup_status_t res = drbbdup_exit();
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup exit failed");
    CHECK(case_encoding = 1, "encoding has be 1");
    CHECK(instrum_called, "instrumentation was not inserted");

    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_init();

    drbbdup_options_t opts = { 0 };
    opts.struct_size = sizeof(drbbdup_options_t);
    opts.set_up_bb_dups = set_up_bb_dups;
    opts.instrument_instr = instrument_instr;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&case_encoding, OPSZ_PTR);
    opts.atomic_load_encoding = false;
    /* Test optimizations with case comparisons. */
    opts.max_case_encoding = 1;
    opts.user_data = USER_DATA_VAL;
    opts.non_default_case_limit = 1;

    drbbdup_status_t res = drbbdup_init(&opts);
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup init failed");
    dr_register_exit_event(event_exit);
}
