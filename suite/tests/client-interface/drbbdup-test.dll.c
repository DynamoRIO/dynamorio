/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "drmgr.h"
#include "drbbdup.h"

#define CHECK(x, msg)                                                                \
    do {                                                                             \
        if (!(x)) {                                                                  \
            dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
            dr_abort();                                                              \
        }                                                                            \
    } while (0);

#define USER_DATA_VAL (void *)222
#define ORIG_ANALYSIS_VAL (void *)555
#define ANALYSIS_VAL_1 (void *)888
#define ANALYSIS_VAL_2 (void *)999

static bool orig_analysis_called = false;
static bool orig_analysis_destroy_called = false;
static bool default_analysis_called = false;
static bool case1_analysis_called = false;
static bool case1_analysis_destroy_called = false;
static bool case2_analysis_called = false;
static bool case2_analysis_destroy_called = false;
static bool instrum_called = false;

/* Assume single threaded. */
static uintptr_t encode_val = 1;
static bool enable_dups_flag = false;
/* Counters to test statistics provided by drbbdup. */
static unsigned long no_dup_count = 0;
static unsigned long no_dynamic_handling_count = 0;

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
    res = drbbdup_register_case_encoding(drbbdup_ctx, 2);
    CHECK(res == DRBBDUP_SUCCESS, "failed to register case 2");

    if (!enable_dups_flag)
        no_dup_count++;
    no_dynamic_handling_count++;

    *enable_dups = enable_dups_flag;
    enable_dups_flag = !enable_dups_flag; /* alternate flag */
    *enable_dynamic_handling = false;     /* disable dynamic handling */

    return 0; /* return default case */
}

static void
orig_analyse_bb(void *drcontext, void *tag, instrlist_t *bb, void *user_data,
                void **orig_analysis_data)
{
    CHECK(user_data == USER_DATA_VAL, "user data does not match");
    *orig_analysis_data = ORIG_ANALYSIS_VAL;
    orig_analysis_called = true;
}

static void
destroy_orig_analysis(void *drcontext, void *user_data, void *orig_analysis_data)
{
    CHECK(user_data == USER_DATA_VAL, "user data does not match");
    CHECK(orig_analysis_data == ORIG_ANALYSIS_VAL, "orig analysis data does not match");
    orig_analysis_destroy_called = true;
}

static void
analyse_bb(void *drcontext, void *tag, instrlist_t *bb, uintptr_t encoding,
           void *user_data, void *orig_analysis_data, void **analysis_data)
{
    CHECK(user_data == USER_DATA_VAL, "user data does not match");
    CHECK(orig_analysis_data == ORIG_ANALYSIS_VAL, "orig analysis data does not match");

    switch (encoding) {
    case 0:
        *analysis_data = NULL;
        default_analysis_called = true;
        break;
    case 1:
        *analysis_data = ANALYSIS_VAL_1;
        case1_analysis_called = true;
        break;
    case 2:
        *analysis_data = ANALYSIS_VAL_2;
        case2_analysis_called = true;
        break;
    default: CHECK(false, "invalid encoding");
    }
}

static void
destroy_analysis(void *drcontext, uintptr_t encoding, void *user_data,
                 void *orig_analysis_data, void *analysis_data)
{
    CHECK(user_data == USER_DATA_VAL, "user data does not match");
    CHECK(orig_analysis_data == ORIG_ANALYSIS_VAL, "orig analysis data does not match");

    switch (encoding) {
    case 0: CHECK(false, "should not be called because analysis data is NULL"); break;
    case 1:
        CHECK(analysis_data == ANALYSIS_VAL_1, "invalid encoding for case 1");
        case1_analysis_destroy_called = true;
        break;
    case 2:
        CHECK(analysis_data == ANALYSIS_VAL_2, "invalid encoding for case 2");
        case2_analysis_destroy_called = true;
        break;
    default: CHECK(false, "invalid encoding");
    }
}

static void
encode()
{
    drbbdup_set_encoding(encode_val);

    switch (encode_val) {
    case 1: encode_val++; break;
    default: encode_val = 0;
    }
}

static void
insert_encode(void *drcontext, void *tag, instrlist_t *bb, instr_t *where,
              void *user_data, void *orig_analysis_data)
{
    CHECK(user_data == USER_DATA_VAL, "user data does not match");
    CHECK(orig_analysis_data == ORIG_ANALYSIS_VAL, "orig analysis data does not match");

    dr_insert_clean_call(drcontext, bb, where, encode, false, 0);
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
    CHECK(orig_analysis_data == ORIG_ANALYSIS_VAL, "orig analysis data does not match");

    switch (encoding) {
    case 0:
        CHECK(analysis_data == NULL, "case analysis does not match for default case");
        break;
    case 1:
        CHECK(analysis_data == ANALYSIS_VAL_1, "case analysis does not match for case 1");
        break;
    case 2:
        CHECK(analysis_data == ANALYSIS_VAL_2, "case analysis does not match for case 2");
        break;
    default: CHECK(false, "invalid encoding");
    }

    res = drbbdup_is_first_instr(drcontext, instr, &is_start);
    CHECK(res == DRBBDUP_SUCCESS, "failed to check whether instr is start");

    if (is_start && encoding != 0) {
        instrum_called = true;
        dr_insert_clean_call(drcontext, bb, where, print_case, false, 1,
                             OPND_CREATE_INTPTR(encoding));
    }
}

static void
event_exit(void)
{
    drbbdup_status_t res;

    drbbdup_stats_t stats = { sizeof(drbbdup_stats_t) };
    res = drbbdup_get_stats(&stats);
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup statistics gathering failed");

    CHECK(stats.no_dup_count == no_dup_count, "no dup count should match");
    CHECK(stats.no_dynamic_handling_count == no_dynamic_handling_count,
          "no dynamic handling count should match");
    CHECK(stats.bail_count == 0, "should be 0 since dynamic case gen is turned off");
    CHECK(stats.gen_count == 0, "should be 0 since dynamic case gen is turned off");

    res = drbbdup_exit();
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup exit failed");

    CHECK(orig_analysis_called, "orig analysis was not done");
    CHECK(default_analysis_called, "default analysis was not done");
    CHECK(case1_analysis_called, "case 1 analysis was not done");
    CHECK(case2_analysis_called, "case 2 analysis was not done");

    CHECK(orig_analysis_destroy_called, "orig analysis was not destroyed");
    CHECK(case1_analysis_destroy_called, "case 1 analysis was not destroyed");
    CHECK(case2_analysis_destroy_called, "case 2 analysis was not destroyed");

    CHECK(instrum_called, "instrumentation was not inserted");

    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drbbdup_status_t res;

    drmgr_init();

    drbbdup_options_t opts = { sizeof(drbbdup_options_t),
                               set_up_bb_dups,
                               insert_encode,
                               orig_analyse_bb,
                               destroy_orig_analysis,
                               analyse_bb,
                               destroy_analysis,
                               instrument_instr,
                               NULL,
                               USER_DATA_VAL,
                               2 /* num of cases */,
                               0 /* threshold */,
                               true /* enable stats */ };

    res = drbbdup_init(&opts);
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup init failed");
    dr_register_exit_event(event_exit);
}
