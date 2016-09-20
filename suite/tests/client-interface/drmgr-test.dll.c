/* **********************************************************
 * Copyright (c) 2011-2015 Google, Inc.  All rights reserved.
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


/* Tests the drmgr extension */

#include "dr_api.h"
#include "drmgr.h"
#include "client_tools.h"
#include <string.h> /* memset */

#define CHECK(x, msg) do {               \
    if (!(x)) {                          \
        dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
        dr_abort();                      \
    }                                    \
} while (0);

/* CLS tests: easiest to assume a single thread (the 2nd thread the
 * app creates, in this case) hitting callbacks and use global data to
 * check preservation
 */
static int tls_idx;
static int cls_idx;
static thread_id_t main_thread;
static int cb_depth;
static bool in_syscall_A;
static bool in_syscall_B;
static bool in_post_syscall_A;
static bool in_post_syscall_B;
static bool in_event_thread_init;
static bool in_event_thread_init_ex;
static bool in_event_thread_exit;
static bool in_event_thread_exit_ex;
static void *syslock;
static void *threadlock;
static uint one_time_exec;

#define MAGIC_NUMBER_FROM_CACHE 0x0eadbeef

static bool checked_tls_from_cache;
static bool checked_cls_from_cache;
static bool checked_tls_write_from_cache;
static bool checked_cls_write_from_cache;

static void event_exit(void);
static void event_thread_init(void *drcontext);
static void event_thread_exit(void *drcontext);
static void event_thread_init_ex(void *drcontext);
static void event_thread_exit_ex(void *drcontext);
static void event_thread_context_init(void *drcontext, bool new_depth);
static void event_thread_context_exit(void *drcontext, bool process_exit);
static bool event_filter_syscall(void *drcontext, int sysnum);
static bool event_pre_sys_A(void *drcontext, int sysnum);
static bool event_pre_sys_B(void *drcontext, int sysnum);
static void event_post_sys_A(void *drcontext, int sysnum);
static void event_post_sys_B(void *drcontext, int sysnum);
static dr_emit_flags_t event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating,
                                         OUT void **user_data);
static dr_emit_flags_t event_bb_insert(void *drcontext, void *tag, instrlist_t *bb,
                                       instr_t *inst, bool for_trace, bool translating,
                                       void *user_data);

static dr_emit_flags_t event_bb4_app2app(void *drcontext, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating,
                                         OUT void **user_data);
static dr_emit_flags_t event_bb4_analysis(void *drcontext, void *tag, instrlist_t *bb,
                                          bool for_trace, bool translating,
                                          void *user_data);
static dr_emit_flags_t event_bb4_insert(void *drcontext, void *tag, instrlist_t *bb,
                                       instr_t *inst, bool for_trace, bool translating,
                                       void *user_data);
static dr_emit_flags_t event_bb4_instru2instru(void *drcontext, void *tag, instrlist_t *bb,
                                               bool for_trace, bool translating,
                                               void *user_data);
static dr_emit_flags_t event_bb4_insert2(void *drcontext, void *tag, instrlist_t *bb,
                                         instr_t *inst, bool for_trace, bool translating,
                                         void *user_data);


static dr_emit_flags_t one_time_bb_event(void *drcontext, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating);
DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_priority_t priority = {sizeof(priority), "drmgr-test", NULL, NULL, 0};
    drmgr_priority_t priority4 = {sizeof(priority), "drmgr-test4", NULL, NULL, 0};
    drmgr_priority_t sys_pri_A = {sizeof(priority), "drmgr-test-A",
                                  NULL, NULL, 10};
    drmgr_priority_t sys_pri_B = {sizeof(priority), "drmgr-test-B",
                                  "drmgr-test-A", NULL, 5};
    drmgr_priority_t thread_init_pri = {sizeof(priority), "drmgr-thread-init-test",
                                        NULL, NULL, -1};
    drmgr_priority_t thread_exit_pri = {sizeof(priority), "drmgr-thread-exit-test",
                                        NULL, NULL, 1};
    bool ok;

    drmgr_init();
    dr_register_exit_event(event_exit);
    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);
    drmgr_register_thread_init_event_ex(event_thread_init_ex, &thread_init_pri);
    drmgr_register_thread_exit_event_ex(event_thread_exit_ex, &thread_exit_pri);

    ok = drmgr_register_bb_instrumentation_event(event_bb_analysis,
                                                 event_bb_insert,
                                                 &priority);
    CHECK(ok, "drmgr register bb failed");

    /* check register/unregister instrumentation_ex */
    ok = drmgr_register_bb_instrumentation_ex_event(event_bb4_app2app,
                                                    event_bb4_analysis,
                                                    event_bb4_insert2,
                                                    event_bb4_instru2instru,
                                                    NULL);
    CHECK(ok, "drmgr_register_bb_instrumentation_ex_event failed");
    ok = drmgr_unregister_bb_instrumentation_ex_event(event_bb4_app2app,
                                                      event_bb4_analysis,
                                                      event_bb4_insert2,
                                                      event_bb4_instru2instru);
    CHECK(ok, "drmgr_unregister_bb_instrumentation_ex_event failed");

    /* test data passing among all 4 phases */
    ok = drmgr_register_bb_instrumentation_ex_event(event_bb4_app2app,
                                                    event_bb4_analysis,
                                                    event_bb4_insert,
                                                    event_bb4_instru2instru,
                                                    &priority4);

    tls_idx = drmgr_register_tls_field();
    CHECK(tls_idx != -1, "drmgr_register_tls_field failed");
    cls_idx = drmgr_register_cls_field(event_thread_context_init,
                                       event_thread_context_exit);
    CHECK(cls_idx != -1, "drmgr_register_tls_field failed");

    dr_register_filter_syscall_event(event_filter_syscall);
    ok = drmgr_register_pre_syscall_event_ex(event_pre_sys_A, &sys_pri_A) &&
        drmgr_register_pre_syscall_event_ex(event_pre_sys_B, &sys_pri_B);
    CHECK(ok, "drmgr register sys failed");
    ok = drmgr_register_post_syscall_event_ex(event_post_sys_A, &sys_pri_A) &&
        drmgr_register_post_syscall_event_ex(event_post_sys_B, &sys_pri_B);
    CHECK(ok, "drmgr register sys failed");

    syslock = dr_mutex_create();
    threadlock = dr_mutex_create();

    ok = drmgr_register_bb_app2app_event(one_time_bb_event, NULL);
    CHECK(ok, "drmgr app2app registration failed");
}

static void
event_exit(void)
{
    dr_mutex_destroy(syslock);
    dr_mutex_destroy(threadlock);
    CHECK(checked_tls_from_cache, "failed to hit clean call");
    CHECK(checked_cls_from_cache, "failed to hit clean call");
    CHECK(checked_tls_write_from_cache, "failed to hit clean call");
    CHECK(checked_cls_write_from_cache, "failed to hit clean call");
    CHECK(one_time_exec == 1, "failed to execute one-time event");

    if (!drmgr_unregister_bb_instrumentation_event(event_bb_analysis))
        CHECK(false, "drmgr unregistration failed");

    if (!drmgr_unregister_bb_instrumentation_ex_event(event_bb4_app2app,
                                                      event_bb4_analysis,
                                                      event_bb4_insert,
                                                      event_bb4_instru2instru))
        CHECK(false, "drmgr unregistration failed");

    if (!drmgr_unregister_cls_field(event_thread_context_init,
                                    event_thread_context_exit, cls_idx))
        CHECK(false, "drmgr unregistration failed");

    drmgr_exit();
    dr_fprintf(STDERR, "all done\n");
}

static void
event_thread_init(void *drcontext)
{
    if (main_thread == 0)
        main_thread = dr_get_thread_id(drcontext);
    drmgr_set_tls_field(drcontext, tls_idx,
                        (void *)(ptr_int_t)dr_get_thread_id(drcontext));
    if (!in_event_thread_init) {
        dr_mutex_lock(threadlock);
        if (!in_event_thread_init) {
            dr_fprintf(STDERR, "in event_thread_init\n");
            in_event_thread_init = true;
        }
        dr_mutex_unlock(threadlock);
    }
}

static void
event_thread_init_ex(void *drcontext)
{
    if (!in_event_thread_init_ex) {
        dr_mutex_lock(threadlock);
        if (!in_event_thread_init_ex) {
            dr_fprintf(STDERR, "in event_thread_init_ex\n");
            in_event_thread_init_ex = true;
        }
        dr_mutex_unlock(threadlock);
    }
}

static void
event_thread_exit(void *drcontext)
{
    CHECK(drmgr_get_tls_field(drcontext, tls_idx) ==
          (void *)(ptr_int_t)dr_get_thread_id(drcontext),
          "tls not preserved");
    if (!in_event_thread_exit) {
        dr_mutex_lock(threadlock);
        if (!in_event_thread_exit) {
            dr_fprintf(STDERR, "in event_thread_exit\n");
            in_event_thread_exit = true;
        }
        dr_mutex_unlock(threadlock);
    }
}

static void
event_thread_exit_ex(void *drcontext)
{
    if (!in_event_thread_exit_ex) {
        dr_mutex_lock(threadlock);
        if (!in_event_thread_exit_ex) {
            dr_fprintf(STDERR, "in event_thread_exit_ex\n");
            in_event_thread_exit_ex = true;
        }
        dr_mutex_unlock(threadlock);
    }
}

static void
event_thread_context_init(void *drcontext, bool new_depth)
{
    if (dr_get_thread_id(drcontext) != main_thread) {
        cb_depth++;
#if VERBOSE
        /* # cbs differs on xp vs win7 so not part of output */
        dr_fprintf(STDERR, "non-main thread entering callback depth=%d\n", cb_depth);
#endif
        CHECK(new_depth || drmgr_get_cls_field(drcontext, cls_idx) ==
              (void *)(ptr_int_t)cb_depth,
              "not re-using prior callback value");
        drmgr_set_cls_field(drcontext, cls_idx, (void *)(ptr_int_t)cb_depth);
        CHECK(drmgr_get_tls_field(drcontext, tls_idx) ==
              (void *)(ptr_int_t)dr_get_thread_id(drcontext), "tls not preserved");
    }
}

static void
event_thread_context_exit(void *drcontext, bool thread_exit)
{
    if (!thread_exit && dr_get_thread_id(drcontext) != main_thread) {
#if VERBOSE
        dr_fprintf(STDERR, "  non-main thread exiting callback depth=%d cls=%d\n",
                   cb_depth, (int)(ptr_int_t) drmgr_get_cls_field(drcontext, cls_idx));
#endif
        CHECK(drmgr_get_cls_field(drcontext, cls_idx) ==
              (void *)(ptr_int_t)cb_depth,
              "cls not preserved");
        cb_depth--;
        CHECK(drmgr_get_tls_field(drcontext, tls_idx) ==
              (void *)(ptr_int_t)dr_get_thread_id(drcontext), "tls not preserved");
    }
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating, OUT void **user_data)
{
    /* point at first instr */
    *user_data = (void *) instrlist_first(bb);
    return DR_EMIT_DEFAULT;
}

static void
check_tls_from_cache(void *tls_val)
{
    CHECK(tls_val == drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx),
          "tls read from cache incorrect");
    checked_tls_from_cache = true;
}

static void
check_cls_from_cache(void *cls_val)
{
    CHECK(cls_val == drmgr_get_cls_field(dr_get_current_drcontext(), cls_idx),
          "cls read from cache incorrect");
    checked_cls_from_cache = true;
}

static void
check_tls_write_from_cache(void)
{
    void *drcontext = dr_get_current_drcontext();
    CHECK(drmgr_get_tls_field(drcontext, tls_idx) == (void *) MAGIC_NUMBER_FROM_CACHE,
          "cls write from cache incorrect");
    /* now restore */
    drmgr_set_tls_field(drcontext, tls_idx,
                        (void *)(ptr_int_t)dr_get_thread_id(drcontext));
    checked_tls_write_from_cache = true;
}

static void
check_cls_write_from_cache(void)
{
    CHECK(drmgr_get_cls_field(dr_get_current_drcontext(), cls_idx) ==
          (void *) MAGIC_NUMBER_FROM_CACHE,
          "cls write from cache incorrect");
    /* now restore */
    drmgr_set_cls_field(dr_get_current_drcontext(), cls_idx,
                        (void *)(ptr_int_t)cb_depth);
    checked_cls_write_from_cache = true;
}

static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                bool for_trace, bool translating, void *user_data)
{
    static int freq;
    reg_id_t reg1 = IF_X86_ELSE(DR_REG_XAX, DR_REG_R0);
    reg_id_t reg2 = IF_X86_ELSE(DR_REG_XCX, DR_REG_R1);

    CHECK(drmgr_is_first_instr(drcontext, instrlist_first_app(bb)), "first incorrect");
    CHECK(!drmgr_is_first_instr(drcontext, instrlist_last(bb)) ||
          instrlist_first_app(bb) == instrlist_last(bb), "first incorrect");
    CHECK(drmgr_is_last_instr(drcontext, instrlist_last(bb)), "last incorrect");
    CHECK(!drmgr_is_last_instr(drcontext, instrlist_first_app(bb)) ||
          instrlist_first_app(bb) == instrlist_last(bb), "last incorrect");

    /* hack to instrument every nth bb.  assumes DR serializes bb events. */
    freq++;
    if (freq % 100 == 0 && inst == (instr_t*)user_data/*first instr*/) {
        /* test read from cache */
        dr_save_reg(drcontext, bb, inst, reg1, SPILL_SLOT_1);
        drmgr_insert_read_tls_field(drcontext, tls_idx, bb, inst, reg1);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_tls_from_cache,
                             false, 1, opnd_create_reg(reg1));
        drmgr_insert_read_cls_field(drcontext, cls_idx, bb, inst, reg1);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_cls_from_cache,
                             false, 1, opnd_create_reg(reg1));
        dr_restore_reg(drcontext, bb, inst, reg1, SPILL_SLOT_1);
    }
    if (freq % 300 == 0 && inst == (instr_t*)user_data/*first instr*/) {
        /* test write from cache */
        dr_save_reg(drcontext, bb, inst, reg1, SPILL_SLOT_1);
        dr_save_reg(drcontext, bb, inst, reg2, SPILL_SLOT_2);
        instrlist_insert_mov_immed_ptrsz(drcontext,
                                         (ptr_int_t)MAGIC_NUMBER_FROM_CACHE,
                                         opnd_create_reg(reg1),
                                         bb, inst, NULL, NULL);
        drmgr_insert_write_tls_field(drcontext, tls_idx, bb, inst, reg1, reg2);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_tls_write_from_cache,
                             false, 0);
        drmgr_insert_write_cls_field(drcontext, cls_idx, bb, inst, reg1, reg2);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_cls_write_from_cache,
                             false, 0);
        dr_restore_reg(drcontext, bb, inst, reg2, SPILL_SLOT_2);
        dr_restore_reg(drcontext, bb, inst, reg1, SPILL_SLOT_1);
    }
    return DR_EMIT_DEFAULT;
}

/* test data passed among all 4 phases */
static dr_emit_flags_t
event_bb4_app2app(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating,
                  OUT void **user_data)
{
    *user_data = (void *) ((ptr_uint_t)tag + 1);
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb4_analysis(void *drcontext, void *tag, instrlist_t *bb,
                   bool for_trace, bool translating,
                   void *user_data)
{
    CHECK(user_data == (void *) ((ptr_uint_t)tag + 1), "user data not preserved");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb4_insert(void *drcontext, void *tag, instrlist_t *bb,
                 instr_t *inst, bool for_trace, bool translating,
                 void *user_data)
{
    CHECK(user_data == (void *) ((ptr_uint_t)tag + 1), "user data not preserved");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb4_insert2(void *drcontext, void *tag, instrlist_t *bb,
                  instr_t *inst, bool for_trace, bool translating,
                  void *user_data)
{
    CHECK(false, "should never be executed");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb4_instru2instru(void *drcontext, void *tag, instrlist_t *bb,
                        bool for_trace, bool translating,
                        void *user_data)
{
    CHECK(user_data == (void *) ((ptr_uint_t)tag + 1), "user data not preserved");
    return DR_EMIT_DEFAULT;
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true;
}

static bool
event_pre_sys_A(void *drcontext, int sysnum)
{
    if (!in_syscall_A) {
        dr_mutex_lock(syslock);
        if (!in_syscall_A) {
            dr_fprintf(STDERR, "in pre_sys_A\n");
            in_syscall_A = true;
        }
        dr_mutex_unlock(syslock);
    }
    return true;
}

static bool
event_pre_sys_B(void *drcontext, int sysnum)
{
    if (!in_syscall_B) {
        dr_mutex_lock(syslock);
        if (!in_syscall_B) {
            dr_fprintf(STDERR, "in pre_sys_B\n");
            in_syscall_B = true;
        }
        dr_mutex_unlock(syslock);
    }
    return true;
}

static void
event_post_sys_A(void *drcontext, int sysnum)
{
    if (!in_post_syscall_A) {
        dr_mutex_lock(syslock);
        if (!in_post_syscall_A) {
            dr_fprintf(STDERR, "in post_sys_A\n");
            in_post_syscall_A = true;
        }
        dr_mutex_unlock(syslock);
    }
}

static void
event_post_sys_B(void *drcontext, int sysnum)
{
    if (!in_post_syscall_B) {
        dr_mutex_lock(syslock);
        if (!in_post_syscall_B) {
            dr_fprintf(STDERR, "in post_sys_B\n");
            in_post_syscall_B = true;
        }
        dr_mutex_unlock(syslock);
    }
}

/* test unregistering from inside an event */
static dr_emit_flags_t
one_time_bb_event(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    int i;
#   define STRESS_REGISTER_ITERS 64
#   define NAME_SZ 32
    char *names[STRESS_REGISTER_ITERS];
    drmgr_priority_t pri = { sizeof(pri), };

    one_time_exec++;
    if (!drmgr_unregister_bb_app2app_event(one_time_bb_event))
        CHECK(false, "drmgr unregistration failed");

    /* stress-test adding and removing */
    for (i = 0; i < STRESS_REGISTER_ITERS; i++) {
        /* force sorted insertion on each add */
        pri.priority = STRESS_REGISTER_ITERS - i;
        names[i] = dr_thread_alloc(drcontext, NAME_SZ);
        dr_snprintf(names[i], NAME_SZ, "%d", pri.priority);
        pri.name = names[i];
        if (!drmgr_register_bb_app2app_event(one_time_bb_event, &pri))
            CHECK(false, "drmgr app2app registration failed");
    }
    /* XXX: drmgr lets us add multiple instances of the same callback
     * so long as they have different priority names (or use default
     * priority) -- but on removal it only asks for callback and
     * removes the first it finds.  Thus we cannot free any memory
     * tied up in a priority until we remove *all* of them.
     * Normally priorities use string literals, so seems ok.
     */
    for (i = 0; i < STRESS_REGISTER_ITERS; i++) {
        if (!drmgr_unregister_bb_app2app_event(one_time_bb_event))
            CHECK(false, "drmgr app2app unregistration failed");
    }
    for (i = 0; i < STRESS_REGISTER_ITERS; i++) {
        dr_thread_free(drcontext, names[i], NAME_SZ);
    }

    return DR_EMIT_DEFAULT;
}
