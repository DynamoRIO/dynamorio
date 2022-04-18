/* **********************************************************
 * Copyright (c) 2013-2022 Google, Inc.  All rights reserved.
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

static client_id_t client_id;

static uint counterA;
static uint counterB;
#if defined(ARM)
static uint counterC;
static uint counterD;
#endif
#if defined(AARCH64)
static uint64 counterE;
static uint64 counterF;
#endif
#if defined(AARCHXX)
static uint counterG;
#endif

static void
event_exit(void)
{
    drx_exit();
    CHECK(counterB == 2 * counterA, "counter inc messed up");
#if defined(ARM)
    CHECK(counterD == 2 * counterA, "counter inc messed up");
#endif
#if defined(AARCH64)
    CHECK(counterE == 2 * counterA, "64-bit counter inc messed up");
    CHECK(counterF == 2 * counterA, "64-bit counter inc with acq_rel messed up");
#endif
#if defined(AARCHXX)
    CHECK(counterG == 2 * counterA, "32-bit counter inc with acq_rel messed up");
#endif
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
    dr_config_status_t res = dr_nudge_client_ex(pid, client_id, exit_code, 0);
    CHECK(res == DR_SUCCESS, dr_config_status_code_to_string(res));
    return true; /* skip kill */
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating)
{
    instr_t *first = instrlist_first_app(bb);
    instr_t *last;
    /* Exercise drx's adjacent increment aflags spill removal code */
    drx_insert_counter_update(drcontext, bb, first, SPILL_SLOT_1,
                              IF_NOT_X86_(SPILL_SLOT_2) & counterA, 1,
                              /* DRX_COUNTER_LOCK is not yet supported on ARM */
                              IF_X86_ELSE(DRX_COUNTER_LOCK, 0));
    drx_insert_counter_update(drcontext, bb, first, SPILL_SLOT_1,
                              IF_NOT_X86_(SPILL_SLOT_2) & counterB, 3,
                              IF_X86_ELSE(DRX_COUNTER_LOCK, 0));
    /* Ensure subtraction works. */
    drx_insert_counter_update(drcontext, bb, first, SPILL_SLOT_1,
                              IF_NOT_X86_(SPILL_SLOT_2) & counterB, -1,
                              IF_X86_ELSE(DRX_COUNTER_LOCK, 0));
    instrlist_meta_preinsert(bb, first, INSTR_CREATE_label(drcontext));
#if defined(ARM)
    /* Exercise drx's optimization bail-out in the presence of predication */
    /* XXX: For a more thorough test, we should save/restore aflags, and set
     * flags so that next counter update never occurs.
     */
    instrlist_set_auto_predicate(bb, DR_PRED_LS);
    drx_insert_counter_update(drcontext, bb, first, SPILL_SLOT_1,
                              IF_NOT_X86_(SPILL_SLOT_2) & counterC, 1,
                              IF_X86_ELSE(DRX_COUNTER_LOCK, 0));
    instrlist_set_auto_predicate(bb, DR_PRED_NONE);
    drx_insert_counter_update(drcontext, bb, first, SPILL_SLOT_1,
                              IF_NOT_X86_(SPILL_SLOT_2) & counterD, 2,
                              IF_X86_ELSE(DRX_COUNTER_LOCK, 0));
#endif
#if defined(AARCH64)
    drx_insert_counter_update(drcontext, bb, first, SPILL_SLOT_1, SPILL_SLOT_2, &counterE,
                              2, DRX_COUNTER_64BIT);
    drx_insert_counter_update(drcontext, bb, first, SPILL_SLOT_1, SPILL_SLOT_2, &counterF,
                              2, DRX_COUNTER_64BIT | DRX_COUNTER_REL_ACQ);
#endif
#if defined(AARCHXX)
    drx_insert_counter_update(drcontext, bb, first, SPILL_SLOT_1, SPILL_SLOT_2, &counterG,
                              2, DRX_COUNTER_REL_ACQ);
#endif
    /* Exercise drx's basic block termination with a zero-cost label */
    drx_tail_pad_block(drcontext, bb);
    last = instrlist_last(bb);
    CHECK(instr_is_syscall(last) || instr_is_cti(last) || instr_is_label(last),
          "did not correctly pad basic block");
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

    f = drx_open_unique_file(cwd, "drx-test", "log", DRX_FILE_SKIP_OPEN, buf,
                             BUFFER_SIZE_ELEMENTS(buf));
    CHECK(f == INVALID_FILE, "drx_open_unique_file should skip file open");
    CHECK(strstr(buf, "drx-test.") != NULL,
          "drx_open_unique_file fail to return path string");
    f = drx_open_unique_file(cwd, "drx-test", "log", 0, buf, BUFFER_SIZE_ELEMENTS(buf));
    CHECK(f != INVALID_FILE, "drx_open_unique_file failed");
    CHECK(dr_file_exists(buf), "drx_open_unique_file failed");
    dr_close_file(f);
    res = dr_delete_file(buf);
    CHECK(res, "drx_open_unique_file failed");

    f = drx_open_unique_appid_file(cwd, 1234, "drx-test", "txt", DRX_FILE_SKIP_OPEN, buf,
                                   BUFFER_SIZE_ELEMENTS(buf));
    CHECK(f == INVALID_FILE, "drx_open_unique_appid_file should skip file open");
    CHECK(strstr(buf, "drx-test.client.drx-test.") != NULL,
          "drx_open_unique_appid_file fail to return path string");
    f = drx_open_unique_appid_file(cwd, dr_get_process_id(), "drx-test", "txt", 0, buf,
                                   BUFFER_SIZE_ELEMENTS(buf));
    CHECK(f != INVALID_FILE, "drx_open_unique_appid_file failed");
    CHECK(dr_file_exists(buf), "drx_open_unique_appid_file failed");
    dr_close_file(f);
    res = dr_delete_file(buf);
    CHECK(res, "drx_open_unique_appid_file failed");

    res = drx_open_unique_appid_dir(cwd, dr_get_process_id(), "drx-test", "dir", buf,
                                    BUFFER_SIZE_ELEMENTS(buf));
    CHECK(res, "drx_open_unique_appid_dir failed");
    CHECK(dr_directory_exists(buf), "drx_open_unique_appid_dir failed");
    res = dr_delete_dir(buf);
    CHECK(res, "drx_open_unique_appid_dir failed");
}

static void
test_instrlist()
{

    void *drcontext;
    instrlist_t *bb;
    size_t size;

    drcontext = dr_get_current_drcontext();

    bb = instrlist_create(drcontext);
    instrlist_init(bb);

    size = drx_instrlist_size(bb);
    CHECK(size == 0, "drmgr_get_bb_size should return 0");
    size = drx_instrlist_app_size(bb);
    CHECK(size == 0, "drmgr_get_bb_app_size should return 0");

#ifdef X86
    instr_t *instr;
    instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_XCX),
                                OPND_CREATE_MEMPTR(DR_REG_XBP, 8));
    instrlist_append(bb, instr);
    instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_XDI),
                                OPND_CREATE_MEMPTR(DR_REG_XBP, 16));
    instrlist_append(bb, instr);
    instr = INSTR_CREATE_add(drcontext, opnd_create_reg(DR_REG_XDI),
                             opnd_create_reg(DR_REG_XCX));
    instrlist_meta_append(bb, instr);

    size = drx_instrlist_size(bb);
    CHECK(size == 3, "drmgr_get_bb_size should return 3");
    size = drx_instrlist_app_size(bb);
    CHECK(size == 2, "drmgr_get_bb_app_size should return 2");
#endif

    instrlist_clear_and_destroy(drcontext, bb);
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
    test_instrlist();
}
