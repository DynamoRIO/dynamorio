/* **********************************************************
 * Copyright (c) 2013-2016 Google, Inc.  All rights reserved.
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


/* Tests the drx extension without drmgr */

#include "dr_api.h"
#include "drx.h"
#include "client_tools.h"
#include "string.h"

#define CHECK(x, msg) do {               \
    if (!(x)) {                          \
        dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
        dr_abort();                      \
    }                                    \
} while (0);

static client_id_t client_id;

static uint counterA;
static uint counterB;

static void
event_exit(void)
{
    drx_exit();
    CHECK(counterB == 2*counterA, "counter inc messed up");
    dr_fprintf(STDERR, "event_exit\n");
}

static void
event_nudge(void *drcontext, uint64 argument)
{
    static int nudge_term_count;
    /* handle multiple from both NtTerminateProcess and NtTerminateJobObject */
    uint count = dr_atomic_add32_return_sum(&nudge_term_count, 1);
    if (count == 1) {
        dr_fprintf(STDERR, "event_nudge exit code %d\n", (int)argument);
        dr_exit_process((int)argument);
        CHECK(false, "should not reach here");
    }
}

static bool
event_soft_kill(process_id_t pid, int exit_code)
{
    dr_config_status_t res =
        dr_nudge_client_ex(pid, client_id, exit_code, 0);
    CHECK(res == DR_SUCCESS, dr_config_status_code_to_string(res));
    return true; /* skip kill */
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *first = instrlist_first_app(bb);
    /* Exercise drx's adjacent increment aflags spill removal code */
    drx_insert_counter_update(drcontext, bb, first,
                              SPILL_SLOT_1, IF_NOT_X86_(SPILL_SLOT_2)
                              &counterA, 1,
                              /* DRX_COUNTER_LOCK is not yet supported on ARM */
                              IF_X86_ELSE(DRX_COUNTER_LOCK, 0));
    drx_insert_counter_update(drcontext, bb, first,
                              SPILL_SLOT_1, IF_NOT_X86_(SPILL_SLOT_2)
                              &counterB, 2, IF_X86_ELSE(DRX_COUNTER_LOCK, 0));
    return DR_EMIT_DEFAULT;
}

static void
test_unique_files(void)
{
    char cwd[MAXIMUM_PATH];
    char buf[MAXIMUM_PATH];
    bool res;
    file_t f;
    res = dr_get_current_directory(cwd, BUFFER_SIZE_ELEMENTS(cwd));
    CHECK(res, "dr_get_current_directory failed");
#ifdef ANDROID
    /* On Android cwd is / where we have no write privs. */
    const char *cpath = dr_get_client_path(client_id);
    const char *dir = strrchr(cpath, '/');
    dr_snprintf(cwd, BUFFER_SIZE_ELEMENTS(cwd), "%.*s", dir - cpath, cpath);
    NULL_TERMINATE_BUFFER(cwd);
#endif

    f = drx_open_unique_file(cwd, "drx-test", "log",
                             DRX_FILE_SKIP_OPEN,
                             buf, BUFFER_SIZE_ELEMENTS(buf));
    CHECK(f == INVALID_FILE, "drx_open_unique_file should skip file open");
    CHECK(strstr(buf, "drx-test.") != NULL,
          "drx_open_unique_file fail to return path string");
    f = drx_open_unique_file(cwd, "drx-test", "log",
                             0, buf, BUFFER_SIZE_ELEMENTS(buf));
    CHECK(f != INVALID_FILE, "drx_open_unique_file failed");
    CHECK(dr_file_exists(buf), "drx_open_unique_file failed");
    dr_close_file(f);
    res = dr_delete_file(buf);
    CHECK(res, "drx_open_unique_file failed");

    f = drx_open_unique_appid_file(cwd, 1234, "drx-test", "txt",
                                   DRX_FILE_SKIP_OPEN,
                                   buf, BUFFER_SIZE_ELEMENTS(buf));
    CHECK(f == INVALID_FILE, "drx_open_unique_appid_file should skip file open");
    CHECK(strstr(buf,
                 "drx-test.client.drx-test.") != NULL,
          "drx_open_unique_appid_file fail to return path string");
    f = drx_open_unique_appid_file(cwd, dr_get_process_id(), "drx-test", "txt",
                                   0, buf, BUFFER_SIZE_ELEMENTS(buf));
    CHECK(f != INVALID_FILE, "drx_open_unique_appid_file failed");
    CHECK(dr_file_exists(buf), "drx_open_unique_appid_file failed");
    dr_close_file(f);
    res = dr_delete_file(buf);
    CHECK(res, "drx_open_unique_appid_file failed");

    res = drx_open_unique_appid_dir(cwd, dr_get_process_id(), "drx-test", "dir",
                                    buf, BUFFER_SIZE_ELEMENTS(buf));
    CHECK(res, "drx_open_unique_appid_dir failed");
    CHECK(dr_directory_exists(buf), "drx_open_unique_appid_dir failed");
    res = dr_delete_dir(buf);
    CHECK(res, "drx_open_unique_appid_dir failed");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    bool ok = drx_init();
    client_id = id;
    CHECK(ok, "drx_init failed");
    dr_register_exit_event(event_exit);
    drx_register_soft_kills(event_soft_kill);
    dr_register_nudge_event(event_nudge, id);
    dr_register_bb_event(event_basic_block);
    test_unique_files();
}
