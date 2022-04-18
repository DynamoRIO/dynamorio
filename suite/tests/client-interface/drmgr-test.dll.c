/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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
#include <stdint.h> /* uintptr_t */

#ifdef UNIX
#    include <signal.h>
#endif

/* CLS tests: easiest to assume a single thread (the 2nd thread the
 * app creates, in this case) hitting callbacks and use global data to
 * check preservation
 */
static int tls_idx;
static int cls_idx;
static thread_id_t main_thread;
static int cb_depth;
static volatile bool in_opcode_A;
static volatile bool in_insert_B;
static volatile bool in_opcode_C;
static volatile bool in_syscall_A;
static volatile bool in_syscall_A_user_data;
static volatile bool in_syscall_B;
static volatile bool in_syscall_B_user_data;
static volatile bool in_post_syscall_A;
static volatile bool in_post_syscall_A_user_data;
static volatile bool in_post_syscall_B;
static volatile bool in_post_syscall_B_user_data;
static volatile bool in_event_thread_init;
static volatile bool in_event_thread_init_ex;
static volatile bool in_event_thread_init_user_data;
static volatile bool in_event_thread_init_null_user_data;
static int thread_exit_events;
static int thread_exit_ex_events;
static int thread_exit_user_data_events;
static int thread_exit_null_user_data_events;
static int mod_load_events;
static int mod_unload_events;
static int meta_instru_events;

static void *opcodelock;
static void *syslock;
static void *threadlock;
static uint one_time_exec;

#define MAGIC_NUMBER_FROM_CACHE 0x0eadbeef

static bool checked_tls_from_cache;
static bool checked_cls_from_cache;
static bool checked_tls_write_from_cache;
static bool checked_cls_write_from_cache;

static void
event_exit(void);
static void
event_thread_init(void *drcontext);
static void
event_thread_exit(void *drcontext);
static void
event_thread_init_ex(void *drcontext);
static void
event_thread_exit_ex(void *drcontext);
static void
event_thread_init_user_data(void *drcontext, void *user_data);
static void
event_thread_exit_user_data(void *drcontext, void *user_data);
static void
event_thread_init_null_user_data(void *drcontext, void *user_data);
static void
event_thread_exit_null_user_data(void *drcontext, void *user_data);
static void
event_thread_context_init(void *drcontext, bool new_depth);
static void
event_thread_context_exit(void *drcontext, bool process_exit);
static void
event_mod_load(void *drcontext, const module_data_t *mod, bool loaded, void *user_data);
static void
event_mod_unload(void *drcontext, const module_data_t *mod, void *user_data);
static bool
event_filter_syscall(void *drcontext, int sysnum);
static bool
event_pre_sys_A(void *drcontext, int sysnum);
static bool
event_pre_sys_A_user_data(void *drcontext, int sysnum, void *user_data);
static bool
event_pre_sys_B(void *drcontext, int sysnum);
static bool
event_pre_sys_B_user_data(void *drcontext, int sysnum, void *user_data);
static void
event_post_sys_A(void *drcontext, int sysnum);
static void
event_post_sys_A_user_data(void *drcontext, int sysnum, void *user_data);
static void
event_post_sys_B(void *drcontext, int sysnum);
static void
event_post_sys_B_user_data(void *drcontext, int sysnum, void *user_data);
static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data);
static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                bool for_trace, bool translating, void *user_data);

static dr_emit_flags_t
event_opcode_add_insert_A(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                          bool for_trace, bool translating, void *user_data);
static dr_emit_flags_t
event_bb_insert_B(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                  bool for_trace, bool translating, void *user_data);
static dr_emit_flags_t
event_opcode_add_insert_C(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                          bool for_trace, bool translating, void *user_data);

static dr_emit_flags_t
event_bb4_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data);
static dr_emit_flags_t
event_bb4_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, void *user_data);
static dr_emit_flags_t
event_bb4_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                 bool for_trace, bool translating, void *user_data);
static dr_emit_flags_t
event_bb4_instru2instru(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating, void *user_data);
static dr_emit_flags_t
event_bb5_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data);
static dr_emit_flags_t
event_bb5_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, void *user_data);
static dr_emit_flags_t
event_bb5_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                 bool for_trace, bool translating, void *user_data);
static dr_emit_flags_t
event_bb5_instru2instru(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating, void *user_data);
static dr_emit_flags_t
event_bb5_meta_instru(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                      bool translating, void *user_data);
static dr_emit_flags_t
event_bb_meta_instru(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                     bool translating);
static dr_emit_flags_t
event_bb4_insert2(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                  bool for_trace, bool translating, void *user_data);

static dr_emit_flags_t
one_time_bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating);
static void
event_kernel_xfer(void *drcontext, const dr_kernel_xfer_info_t *info);

#ifdef UNIX
static dr_signal_action_t
event_signal(void *drcontext, dr_siginfo_t *siginfo, void *user_data);
static dr_signal_action_t
event_null_signal(void *drcontext, dr_siginfo_t *siginfo, void *user_data);
#endif

/* The following test values are arbitrary */

static const uintptr_t thread_user_data_test = 9090;
static const uintptr_t opcode_user_data_test = 3333;
static const uintptr_t syscall_A_user_data_test = 7189;
static const uintptr_t syscall_B_user_data_test = 3218;
static const uintptr_t mod_user_data_test = 1070;

#ifdef UNIX
static const uintptr_t signal_user_data_test = 5115;
#endif

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_priority_t priority = { sizeof(priority), "drmgr-test", NULL, NULL, 0 };
    drmgr_priority_t priority4 = { sizeof(priority), "drmgr-test4", NULL, NULL, 0 };
    drmgr_priority_t priority5 = { sizeof(priority), "drmgr-test5", NULL, NULL, -10 };
    drmgr_priority_t sys_pri_A = { sizeof(priority), "drmgr-test-A", NULL, NULL, 10 };
    drmgr_priority_t sys_pri_A_user_data = { sizeof(priority),
                                             "drmgr-test-A-usr-data-test", "drmgr-test-A",
                                             NULL, 9 };
    drmgr_priority_t sys_pri_B = { sizeof(priority), "drmgr-test-B",
                                   "drmgr-test-A-usr-data-test", NULL, 5 };
    drmgr_priority_t sys_pri_B_user_data = { sizeof(priority),
                                             "drmgr-test-B-usr-data-test", "drmgr-test-B",
                                             NULL, 4 };
    drmgr_priority_t thread_init_null_user_data_pri = { sizeof(priority),
                                                        "drmgr-t-in-null-user-data-test",
                                                        NULL, NULL, -3 };
    drmgr_priority_t thread_init_user_data_pri = { sizeof(priority),
                                                   "drmgr-thread-init-user-data-test",
                                                   NULL, NULL, -2 };
    drmgr_priority_t thread_init_pri = { sizeof(priority), "drmgr-thread-init-test", NULL,
                                         NULL, -1 };
    drmgr_priority_t thread_exit_pri = { sizeof(priority), "drmgr-thread-exit-test", NULL,
                                         NULL, 1 };
    drmgr_priority_t thread_exit_user_data_pri = { sizeof(priority),
                                                   "drmgr-thread-exit-user-data-test",
                                                   NULL, NULL, 2 };
    drmgr_priority_t thread_exit_null_user_data_pri = { sizeof(priority),
                                                        "drmgr-t-exit-null-usr-data-test",
                                                        NULL, NULL, 3 };
    drmgr_priority_t opcode_pri_A = { sizeof(priority), "drmgr-opcode-test-A", NULL, NULL,
                                      5 };
    drmgr_priority_t insert_pri_B = { sizeof(priority), "drmgr-opcode-test-B", NULL, NULL,
                                      6 };
    drmgr_priority_t opcode_pri_C = { sizeof(priority), "drmgr-opcode-test-C", NULL, NULL,
                                      7 };

#ifdef UNIX
    drmgr_priority_t signal_user_data = { sizeof(priority), "drmgr-signal-usr-data-test",
                                          NULL, NULL, 2 };
    drmgr_priority_t signal_null_user_data = { sizeof(priority),
                                               "drmgr-signal-null-usr-data-test", NULL,
                                               NULL, 3 };
#endif

    bool ok;

    drmgr_init();
    dr_register_exit_event(event_exit);
    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);
    drmgr_register_thread_init_event_ex(event_thread_init_ex, &thread_init_pri);
    drmgr_register_thread_exit_event_ex(event_thread_exit_ex, &thread_exit_pri);
    drmgr_register_thread_init_event_user_data(event_thread_init_user_data,
                                               &thread_init_user_data_pri,
                                               (void *)thread_user_data_test);
    drmgr_register_thread_exit_event_user_data(event_thread_exit_user_data,
                                               &thread_exit_user_data_pri,
                                               (void *)thread_user_data_test);
    drmgr_register_thread_init_event_user_data(event_thread_init_null_user_data,
                                               &thread_init_null_user_data_pri, NULL);
    drmgr_register_thread_exit_event_user_data(event_thread_exit_null_user_data,
                                               &thread_exit_null_user_data_pri, NULL);

#ifdef UNIX
    ok = drmgr_register_signal_event_user_data(event_signal, &signal_user_data,
                                               (void *)signal_user_data_test);
    CHECK(ok, "drmgr_register_signal_event_user_data failed");

    ok = drmgr_register_signal_event_user_data(event_null_signal, &signal_null_user_data,
                                               NULL);
    CHECK(ok, "drmgr_register_signal_event_user_data (null) failed");
#endif

    ok = drmgr_register_bb_instrumentation_event(event_bb_analysis, event_bb_insert,
                                                 &priority);
    CHECK(ok, "drmgr register bb failed");

    ok = drmgr_register_opcode_instrumentation_event(event_opcode_add_insert_A, OP_add,
                                                     &opcode_pri_A, NULL);
    CHECK(ok, "drmgr register opcode failed");

    ok = drmgr_register_bb_instrumentation_event(NULL, event_bb_insert_B, &insert_pri_B);
    CHECK(ok, "drmgr register bb failed");

    ok = drmgr_register_opcode_instrumentation_event(
        event_opcode_add_insert_C, OP_add, &opcode_pri_C, (void *)opcode_user_data_test);
    CHECK(ok, "drmgr register opcode failed");

    /* check register/unregister instrumentation_ex */
    ok = drmgr_register_bb_instrumentation_ex_event(event_bb4_app2app, event_bb4_analysis,
                                                    event_bb4_insert2,
                                                    event_bb4_instru2instru, NULL);
    CHECK(ok, "drmgr_register_bb_instrumentation_ex_event failed");
    ok = drmgr_unregister_bb_instrumentation_ex_event(
        event_bb4_app2app, event_bb4_analysis, event_bb4_insert2,
        event_bb4_instru2instru);
    CHECK(ok, "drmgr_unregister_bb_instrumentation_ex_event failed");

    /* check register/unregister instrumentation_all_events */
    drmgr_instru_events_t events = { sizeof(events),          event_bb5_app2app,
                                     event_bb5_analysis,      event_bb5_insert,
                                     event_bb5_instru2instru, event_bb5_meta_instru };
    ok = drmgr_register_bb_instrumentation_all_events(&events, NULL);
    CHECK(ok, "drmgr_register_bb_instrumentation_all_events failed");
    ok = drmgr_unregister_bb_instrumentation_all_events(&events);
    CHECK(ok, "drmgr_unregister_bb_instrumentation_all_events failed");

    /* test data passing among four first phases */
    ok = drmgr_register_bb_instrumentation_ex_event(event_bb4_app2app, event_bb4_analysis,
                                                    event_bb4_insert,
                                                    event_bb4_instru2instru, &priority4);

    /* test data passing among all five phases */
    ok = drmgr_register_bb_instrumentation_all_events(&events, &priority5);
    CHECK(ok, "drmgr_register_bb_instrumentation_all_events failed");

    drmgr_register_module_load_event_user_data(event_mod_load, NULL,
                                               (void *)mod_user_data_test);

    drmgr_register_module_unload_event_user_data(event_mod_unload, NULL,
                                                 (void *)mod_user_data_test);

    tls_idx = drmgr_register_tls_field();
    CHECK(tls_idx != -1, "drmgr_register_tls_field failed");
    cls_idx =
        drmgr_register_cls_field(event_thread_context_init, event_thread_context_exit);
    CHECK(cls_idx != -1, "drmgr_register_tls_field failed");

    dr_register_filter_syscall_event(event_filter_syscall);
    ok = drmgr_register_pre_syscall_event_ex(event_pre_sys_A, &sys_pri_A) &&
        drmgr_register_pre_syscall_event_user_data(event_pre_sys_A_user_data,
                                                   &sys_pri_A_user_data,
                                                   (void *)syscall_A_user_data_test) &&
        drmgr_register_pre_syscall_event_ex(event_pre_sys_B, &sys_pri_B) &&
        drmgr_register_pre_syscall_event_user_data(event_pre_sys_B_user_data,
                                                   &sys_pri_B_user_data,
                                                   (void *)syscall_B_user_data_test);
    CHECK(ok, "drmgr register sys failed");
    ok = drmgr_register_post_syscall_event_ex(event_post_sys_A, &sys_pri_A) &&
        drmgr_register_post_syscall_event_user_data(event_post_sys_A_user_data,
                                                    &sys_pri_A_user_data,
                                                    (void *)syscall_A_user_data_test) &&
        drmgr_register_post_syscall_event_ex(event_post_sys_B, &sys_pri_B) &&
        drmgr_register_post_syscall_event_user_data(event_post_sys_B_user_data,
                                                    &sys_pri_B_user_data,
                                                    (void *)syscall_B_user_data_test);
    CHECK(ok, "drmgr register sys failed");

    opcodelock = dr_mutex_create();
    syslock = dr_mutex_create();
    threadlock = dr_mutex_create();

    ok = drmgr_register_bb_app2app_event(one_time_bb_event, NULL);
    CHECK(ok, "drmgr app2app registration failed");

    ok = drmgr_register_bb_meta_instru_event(event_bb_meta_instru, &priority);
    CHECK(ok, "drmgr meta_instru registration failed");

    ok = drmgr_register_kernel_xfer_event(event_kernel_xfer);
    CHECK(ok, "drmgr_register_kernel_xfer_event failed");
    ok = drmgr_unregister_kernel_xfer_event(event_kernel_xfer);
    CHECK(ok, "drmgr_unregister_kernel_xfer_event failed");
    ok = drmgr_register_kernel_xfer_event_ex(event_kernel_xfer, &priority);
    CHECK(ok, "drmgr_register_kernel_xfer_event_ex failed");
}

static void
event_exit(void)
{
    dr_mutex_destroy(opcodelock);
    dr_mutex_destroy(syslock);
    dr_mutex_destroy(threadlock);
    CHECK(checked_tls_from_cache, "failed to hit clean call");
    CHECK(checked_cls_from_cache, "failed to hit clean call");
    CHECK(checked_tls_write_from_cache, "failed to hit clean call");
    CHECK(checked_cls_write_from_cache, "failed to hit clean call");
    CHECK(one_time_exec == 1, "failed to execute one-time event");

    if (thread_exit_events > 0)
        dr_fprintf(STDERR, "saw event_thread_exit\n");
    if (thread_exit_ex_events > 0)
        dr_fprintf(STDERR, "saw event_thread_exit_ex\n");
    if (thread_exit_user_data_events > 0)
        dr_fprintf(STDERR, "saw event_thread_exit_user_data\n");
    if (thread_exit_null_user_data_events > 0)
        dr_fprintf(STDERR, "saw event_thread_exit_null_user_data\n");

    if (mod_load_events > 0)
        dr_fprintf(STDERR, "saw event_mod_load\n");
    if (mod_unload_events > 0)
        dr_fprintf(STDERR, "saw event_mod_unload\n");

    if (meta_instru_events > 0)
        dr_fprintf(STDERR, "saw event_meta_instru\n");

    if (!drmgr_unregister_bb_instrumentation_event(event_bb_analysis))
        CHECK(false, "drmgr unregistration failed");

#ifdef UNIX
    if (!drmgr_unregister_signal_event_user_data(event_signal))
        CHECK(false, "drmgr unregister signal event user_data failed");
    if (!drmgr_unregister_signal_event_user_data(event_null_signal))
        CHECK(false, "drmgr unregister null signal event user_data failed");
#endif

    if (!drmgr_unregister_bb_instrumentation_ex_event(
            event_bb4_app2app, event_bb4_analysis, event_bb4_insert,
            event_bb4_instru2instru))
        CHECK(false, "drmgr unregistration failed");

    drmgr_instru_events_t events = { sizeof(events),          event_bb5_app2app,
                                     event_bb5_analysis,      event_bb5_insert,
                                     event_bb5_instru2instru, event_bb5_meta_instru };
    if (!drmgr_unregister_bb_instrumentation_all_events(&events))
        CHECK(false, "drmgr_unregister_bb_instrumentation_all_events failed");

    if (!drmgr_unregister_opcode_instrumentation_event(event_opcode_add_insert_A, OP_add))
        CHECK(false, "drmgr opcode unregistration failed");

    if (!drmgr_unregister_bb_insertion_event(event_bb_insert_B))
        CHECK(false, "drmgr opcode unregistration failed");

    if (!drmgr_unregister_opcode_instrumentation_event(event_opcode_add_insert_C, OP_add))
        CHECK(false, "drmgr opcode unregistration failed");

    if (!drmgr_unregister_module_load_event_user_data(event_mod_load))
        CHECK(false, "drmgr mod load unregistration failed");

    if (!drmgr_unregister_module_unload_event_user_data(event_mod_unload))
        CHECK(false, "drmgr mod load unregistration failed");

    if (!drmgr_unregister_cls_field(event_thread_context_init, event_thread_context_exit,
                                    cls_idx))
        CHECK(false, "drmgr unregistration failed");
    if (!drmgr_unregister_kernel_xfer_event(event_kernel_xfer))
        CHECK(false, "drmgr_unregister_kernel_xfer_event failed");

    if (!drmgr_unregister_bb_meta_instru_event(event_bb_meta_instru))
        CHECK(false, "drmgr meta_instru unregistration failed");

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
event_thread_init_user_data(void *drcontext, void *user_data)
{
    if (!in_event_thread_init_user_data) {
        dr_mutex_lock(threadlock);
        if (!in_event_thread_init_user_data) {
            dr_fprintf(STDERR, "in event_thread_init_user_data\n");
            in_event_thread_init_user_data = true;

            CHECK(user_data == (void *)thread_user_data_test,
                  "incorrect user data passed");
        }
        dr_mutex_unlock(threadlock);
    }
}

static void
event_thread_init_null_user_data(void *drcontext, void *user_data)
{
    if (!in_event_thread_init_null_user_data) {
        dr_mutex_lock(threadlock);
        if (!in_event_thread_init_null_user_data) {
            dr_fprintf(STDERR, "in event_thread_init_null_user_data\n");
            in_event_thread_init_null_user_data = true;

            CHECK(user_data == NULL, "incorrect user data passed");
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
    /* We do not print as on Win10 there are extra threads messing up the order. */
    dr_mutex_lock(threadlock);
    thread_exit_events++;
    dr_mutex_unlock(threadlock);
}

static void
event_thread_exit_ex(void *drcontext)
{
    /* We do not print as on Win10 there are extra threads messing up the order. */
    dr_mutex_lock(threadlock);
    thread_exit_ex_events++;
    dr_mutex_unlock(threadlock);
}

static void
event_thread_exit_user_data(void *drcontext, void *user_data)
{
    /* We do not print as on Win10 there are extra threads messing up the order. */
    dr_mutex_lock(threadlock);
    CHECK(user_data == (void *)thread_user_data_test, "incorrect user data passed");
    thread_exit_user_data_events++;
    dr_mutex_unlock(threadlock);
}

static void
event_thread_exit_null_user_data(void *drcontext, void *user_data)
{
    /* We do not print as on Win10 there are extra threads messing up the order. */
    dr_mutex_lock(threadlock);
    CHECK(user_data == NULL, "incorrect user data passed");
    thread_exit_null_user_data_events++;
    dr_mutex_unlock(threadlock);
}

static void
event_mod_load(void *drcontext, const module_data_t *mod, bool loaded, void *user_data)
{
    mod_load_events++;
    CHECK(user_data == (void *)mod_user_data_test, "incorrect user data for mod load");
}

static void
event_mod_unload(void *drcontext, const module_data_t *mod, void *user_data)
{
    mod_unload_events++;
    CHECK(user_data == (void *)mod_user_data_test, "incorrect user data for mod unload");
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
        CHECK(new_depth ||
                  drmgr_get_cls_field(drcontext, cls_idx) == (void *)(ptr_int_t)cb_depth,
              "not re-using prior callback value");
        drmgr_set_cls_field(drcontext, cls_idx, (void *)(ptr_int_t)cb_depth);
        CHECK(drmgr_get_tls_field(drcontext, tls_idx) ==
                  (void *)(ptr_int_t)dr_get_thread_id(drcontext),
              "tls not preserved");
    }
}

static void
event_thread_context_exit(void *drcontext, bool thread_exit)
{
    if (!thread_exit && dr_get_thread_id(drcontext) != main_thread) {
#if VERBOSE
        dr_fprintf(STDERR, "  non-main thread exiting callback depth=%d cls=%d\n",
                   cb_depth, (int)(ptr_int_t)drmgr_get_cls_field(drcontext, cls_idx));
#endif
        CHECK(drmgr_get_cls_field(drcontext, cls_idx) == (void *)(ptr_int_t)cb_depth,
              "cls not preserved");
        cb_depth--;
        CHECK(drmgr_get_tls_field(drcontext, tls_idx) ==
                  (void *)(ptr_int_t)dr_get_thread_id(drcontext),
              "tls not preserved");
    }
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data)
{
    /* point at first non-label instr */
    *user_data = (void *)instrlist_first_nonlabel(bb);
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
    CHECK(drmgr_get_tls_field(drcontext, tls_idx) == (void *)MAGIC_NUMBER_FROM_CACHE,
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
              (void *)MAGIC_NUMBER_FROM_CACHE,
          "cls write from cache incorrect");
    /* now restore */
    drmgr_set_cls_field(dr_get_current_drcontext(), cls_idx, (void *)(ptr_int_t)cb_depth);
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
              instrlist_first_app(bb) == instrlist_last(bb),
          "first incorrect");
    CHECK(drmgr_is_last_instr(drcontext, instrlist_last(bb)), "last incorrect");
    CHECK(!drmgr_is_last_instr(drcontext, instrlist_first_app(bb)) ||
              instrlist_first_app(bb) == instrlist_last(bb),
          "last incorrect");
    if (inst == (instr_t *)user_data) {
        CHECK(drmgr_is_first_nonlabel_instr(drcontext, inst),
              "first non-label incorrect");
    } else {
        CHECK(!drmgr_is_first_nonlabel_instr(drcontext, inst),
              "first non-label incorrect");
    }

    /* hack to instrument every nth bb.  assumes DR serializes bb events. */
    freq++;
    if (freq % 100 == 0 && drmgr_is_first_instr(drcontext, inst)) {
        /* test read from cache */
        dr_save_reg(drcontext, bb, inst, reg1, SPILL_SLOT_1);
        drmgr_insert_read_tls_field(drcontext, tls_idx, bb, inst, reg1);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_tls_from_cache, false, 1,
                             opnd_create_reg(reg1));
        drmgr_insert_read_cls_field(drcontext, cls_idx, bb, inst, reg1);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_cls_from_cache, false, 1,
                             opnd_create_reg(reg1));
        dr_restore_reg(drcontext, bb, inst, reg1, SPILL_SLOT_1);
    }
    if (freq % 300 == 0 && drmgr_is_first_instr(drcontext, inst)) {
        /* test write from cache */
        dr_save_reg(drcontext, bb, inst, reg1, SPILL_SLOT_1);
        dr_save_reg(drcontext, bb, inst, reg2, SPILL_SLOT_2);
        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)MAGIC_NUMBER_FROM_CACHE,
                                         opnd_create_reg(reg1), bb, inst, NULL, NULL);
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

static dr_emit_flags_t
event_opcode_add_insert_A(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                          bool for_trace, bool translating, void *user_data)
{
    CHECK(instr_get_opcode(inst) == OP_add, "incorrect opcode");
    CHECK(user_data == NULL, "incorrect user data");

    if (!in_opcode_A) {
        dr_mutex_lock(opcodelock);
        if (!in_opcode_A) {
            dr_fprintf(STDERR, "in insert A\n");
            in_opcode_A = true;
        }
        dr_mutex_unlock(opcodelock);
    }

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_insert_B(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                  bool for_trace, bool translating, void *user_data)
{
    if (!in_insert_B && instr_get_opcode(inst) == OP_add) {
        dr_mutex_lock(opcodelock);
        if (!in_insert_B) {
            dr_fprintf(STDERR, "in insert B\n");
            in_insert_B = true;
        }
        dr_mutex_unlock(opcodelock);
    }

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_opcode_add_insert_C(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                          bool for_trace, bool translating, void *user_data)
{
    CHECK(instr_get_opcode(inst) == OP_add, "incorrect opcode");
    CHECK(user_data == (void *)opcode_user_data_test, "incorrect user data");

    if (!in_opcode_C) {
        dr_mutex_lock(opcodelock);
        if (!in_opcode_C) {
            dr_fprintf(STDERR, "in insert C\n");
            in_opcode_C = true;
        }
        dr_mutex_unlock(opcodelock);
    }

    return DR_EMIT_DEFAULT;
}

/* test data passed among four first phases */
static dr_emit_flags_t
event_bb4_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data)
{
    *user_data = (void *)((ptr_uint_t)tag + 1);
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb4_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, void *user_data)
{
    CHECK(user_data == (void *)((ptr_uint_t)tag + 1), "user data not preserved");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb4_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                 bool for_trace, bool translating, void *user_data)
{
    CHECK(user_data == (void *)((ptr_uint_t)tag + 1), "user data not preserved");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb4_insert2(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                  bool for_trace, bool translating, void *user_data)
{
    CHECK(false, "should never be executed");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb4_instru2instru(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating, void *user_data)
{
    CHECK(user_data == (void *)((ptr_uint_t)tag + 1), "user data not preserved");
    return DR_EMIT_DEFAULT;
}

/* test data passed among all five phases */
static dr_emit_flags_t
event_bb5_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data)
{
    int *phase_cnt = (int *)dr_thread_alloc(drcontext, sizeof(int));
    *phase_cnt = 1;
    *user_data = (void *)phase_cnt;
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb5_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, void *user_data)
{
    int *phase_cnt = (int *)user_data;
    (*phase_cnt)++;
    CHECK(*phase_cnt == 2, "user data not preserved");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb5_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                 bool for_trace, bool translating, void *user_data)
{
    /* Increment the count once per bb. */
    if (drmgr_is_first_instr(drcontext, inst)) {
        int *phase_cnt = (int *)user_data;
        (*phase_cnt)++;
        CHECK(*phase_cnt == 3, "user data not preserved");
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb5_instru2instru(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating, void *user_data)
{
    int *phase_cnt = (int *)user_data;
    (*phase_cnt)++;
    CHECK(*phase_cnt == 4, "user data not preserved");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb5_meta_instru(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                      bool translating, void *user_data)
{
    meta_instru_events++;
    int *phase_cnt = (int *)user_data;
    (*phase_cnt)++;
    CHECK(*phase_cnt == 5, "user data not preserved");
    dr_thread_free(drcontext, user_data, sizeof(int));
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_meta_instru(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                     bool translating)
{
    return DR_EMIT_DEFAULT;
}

#ifdef UNIX
static dr_signal_action_t
event_signal(void *drcontext, dr_siginfo_t *siginfo, void *user_data)
{
    CHECK(siginfo->sig == SIGUSR1, "signal not correct");
    CHECK(user_data == (void *)signal_user_data_test, "user data of signal not valid");
    dr_fprintf(STDERR, "in signal_A_user_data\n");

    return DR_SIGNAL_DELIVER;
}

static dr_signal_action_t
event_null_signal(void *drcontext, dr_siginfo_t *siginfo, void *user_data)
{
    CHECK(siginfo->sig == SIGUSR1, "signal not correct");
    CHECK(user_data == NULL, "user data of signal not valid");
    dr_fprintf(STDERR, "in signal_B_user_data\n");

    return DR_SIGNAL_SUPPRESS;
}
#endif

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
event_pre_sys_A_user_data(void *drcontext, int sysnum, void *user_data)
{
    if (!in_syscall_A_user_data) {
        dr_mutex_lock(syslock);
        if (!in_syscall_A_user_data) {
            dr_fprintf(STDERR, "in pre_sys_A_user_data\n");
            in_syscall_A_user_data = true;
            CHECK(user_data == (void *)syscall_A_user_data_test,
                  "incorrect user data pre-syscall A");
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

static bool
event_pre_sys_B_user_data(void *drcontext, int sysnum, void *user_data)
{
    if (!in_syscall_B_user_data) {
        dr_mutex_lock(syslock);
        if (!in_syscall_B_user_data) {
            dr_fprintf(STDERR, "in pre_sys_B_user_data\n");
            in_syscall_B_user_data = true;
            CHECK(user_data == (void *)syscall_B_user_data_test,
                  "incorrect user data pre-syscall B");
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
event_post_sys_A_user_data(void *drcontext, int sysnum, void *user_data)
{
    if (!in_post_syscall_A_user_data) {
        dr_mutex_lock(syslock);
        if (!in_post_syscall_A_user_data) {
            dr_fprintf(STDERR, "in post_sys_A_user_data\n");
            in_post_syscall_A_user_data = true;
            CHECK(user_data == (void *)syscall_A_user_data_test,
                  "incorrect user data post-syscall A");
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

static void
event_post_sys_B_user_data(void *drcontext, int sysnum, void *user_data)
{
    if (!in_post_syscall_B_user_data) {
        dr_mutex_lock(syslock);
        if (!in_post_syscall_B_user_data) {
            dr_fprintf(STDERR, "in post_sys_B_user_data\n");
            in_post_syscall_B_user_data = true;
        }
        dr_mutex_unlock(syslock);
    }
}

/* test unregistering from inside an event */
static dr_emit_flags_t
one_time_bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating)
{
    int i;
#define STRESS_REGISTER_ITERS 64
#define NAME_SZ 32
    char *names[STRESS_REGISTER_ITERS];
    drmgr_priority_t pri = {
        sizeof(pri),
    };

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

/* test kernel xfer event callback */
static void
event_kernel_xfer(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    /* We rely on other tests for the details here.  Mostly we're just testing
     * the register/unregister logic.
     */
    CHECK(drcontext == dr_get_current_drcontext(), "sanity check");
}
