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

/* Tests the drwrap extension */

#include "dr_api.h"
#include "client_tools.h"
#include "drwrap.h"
#include "drmgr.h"
#include <string.h> /* memset */

#define DRWRAP_NATIVE_PARAM 0xdeadbeef

static void
event_exit(void);
static void
wrap_pre(void *wrapcxt, OUT void **user_data);
static void
wrap_post(void *wrapcxt, void *user_data);
static void
wrap_post_might_miss(void *wrapcxt, void *user_data);
static void
wrap_unwindtest_pre(void *wrapcxt, OUT void **user_data);
static void
wrap_unwindtest_post(void *wrapcxt, void *user_data);
static void
wrap_unwindtest_seh_pre(void *wrapcxt, OUT void **user_data);
static void
wrap_unwindtest_seh_post(void *wrapcxt, void *user_data);
static int
replacewith(int *x);
static int
replacewith2(int *x);
static int
replace_callsite(int *x);

static uint load_count;
static bool repeated = false;
static ptr_uint_t repeat_xsp;
#ifdef ARM
static ptr_uint_t repeat_link;
#endif
static void *mutex_lock;
static void *rw_lock;
static void *recur_lock;

static int tls_idx;

static app_pc addr_replace;
static app_pc addr_replace2;
static app_pc addr_replace_callsite;

static app_pc addr_skip_flags;
static app_pc addr_level0;
static app_pc addr_level1;
static app_pc addr_level2;
static app_pc addr_tailcall;
static app_pc addr_skipme;
static app_pc addr_repeat;
static app_pc addr_preonly;
static app_pc addr_postonly;
static app_pc addr_runlots;
static app_pc addr_direct1;
static app_pc addr_direct2;

static app_pc addr_long0;
static app_pc addr_long1;
static app_pc addr_long2;
static app_pc addr_long3;
static app_pc addr_longdone;

static app_pc addr_called_indirectly;
static app_pc addr_called_indirectly_subcall;
static app_pc addr_tailcall_test2;
static app_pc addr_tailcall_tail;

static void
wrap_addr(INOUT app_pc *addr, const char *name, const module_data_t *mod,
          void (*pre_cb)(void *, void **), void (*post_cb)(void *, void *), uint flags)
{
    bool ok;
    if (*addr == NULL)
        *addr = (app_pc)dr_get_proc_address(mod->handle, name);
    CHECK(*addr != NULL, "cannot find lib export");
    if (flags == 0)
        ok = drwrap_wrap(*addr, pre_cb, post_cb);
    else {
        ok = drwrap_wrap_ex(*addr, pre_cb, post_cb, NULL, flags);
    }
    CHECK(ok, "wrap failed");
    CHECK(drwrap_is_wrapped(*addr, pre_cb, post_cb), "drwrap_is_wrapped query failed");
}

static void
unwrap_addr(app_pc addr, const char *name, const module_data_t *mod,
            void (*pre_cb)(void *, void **), void (*post_cb)(void *, void *))
{
    bool ok;
    ok = drwrap_unwrap(addr, pre_cb, post_cb);
    CHECK(ok, "unwrap failed");
    CHECK(!drwrap_is_wrapped(addr, pre_cb, post_cb), "drwrap_is_wrapped query failed");
}

static void
wrap_unwindtest_addr(OUT app_pc *addr, const char *name, const module_data_t *mod)
{
    bool ok;
    *addr = (app_pc)dr_get_proc_address(mod->handle, name);
    CHECK(*addr != NULL, "cannot find lib export");
    ok = drwrap_wrap(*addr, wrap_unwindtest_pre, wrap_unwindtest_post);
    CHECK(ok, "wrap unwindtest failed");
    CHECK(drwrap_is_wrapped(*addr, wrap_unwindtest_pre, wrap_unwindtest_post),
          "drwrap_is_wrapped query failed");
}

static void
unwrap_unwindtest_addr(app_pc addr, const char *name, const module_data_t *mod)
{
    bool ok;
    ok = drwrap_unwrap(addr, wrap_unwindtest_pre, wrap_unwindtest_post);
    CHECK(ok, "unwrap failed");
    CHECK(!drwrap_is_wrapped(addr, wrap_unwindtest_pre, wrap_unwindtest_post),
          "drwrap_is_wrapped query failed");
}

static void
module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    if (strstr(dr_module_preferred_name(mod), "client.drwrap-test.appdll.") != NULL) {
        bool ok;
        instr_t inst;
        app_pc init_pc, pc, next_pc;

        load_count++;
        if (load_count == 2) {
            /* test no-frills */
            drwrap_set_global_flags(DRWRAP_NO_FRILLS);
        }

        addr_replace = (app_pc)dr_get_proc_address(mod->handle, "replaceme");
        CHECK(addr_replace != NULL, "cannot find lib export");
        ok = drwrap_replace(addr_replace, (app_pc)replacewith, false);
        CHECK(ok, "replace failed");

        addr_replace2 = (app_pc)dr_get_proc_address(mod->handle, "replaceme2");
        CHECK(addr_replace2 != NULL, "cannot find lib export");
        ok = drwrap_replace_native(addr_replace2, (app_pc)replacewith2, true /*at entry*/,
                                   0, (void *)(ptr_int_t)DRWRAP_NATIVE_PARAM, false);
        CHECK(ok, "replace_native failed");

        init_pc = (app_pc)dr_get_proc_address(mod->handle, "replace_callsite");
        CHECK(init_pc != NULL, "cannot find lib export");
        /* Find callsite: we assume we'll linearly hit a ret.  We take final call
         * to skip any PIC call.
         */
        instr_init(drcontext, &inst);
        pc = init_pc;
        do {
            instr_reset(drcontext, &inst);
            next_pc = decode(drcontext, pc, &inst);
            if (!instr_valid(&inst))
                break;
            /* if initial jmp, follow it to handle ILT-indirection */
            if (pc == init_pc && instr_is_ubr(&inst))
                next_pc = opnd_get_pc(instr_get_target(&inst));
            else if (instr_is_call(&inst))
                addr_replace_callsite = pc;
            pc = next_pc;
        } while (instr_valid(&inst) && !instr_is_return(&inst));
        CHECK(addr_replace_callsite != NULL, "cannot find lib export");
        ok = drwrap_replace_native(addr_replace_callsite, (app_pc)replace_callsite,
                                   false /*!at entry*/, 0,
                                   (void *)(ptr_int_t)DRWRAP_NATIVE_PARAM, false);
        CHECK(ok, "replace_native failed");
        instr_free(drcontext, &inst);

        wrap_addr(&addr_level0, "level0", mod, wrap_pre, wrap_post, 0);
        wrap_addr(&addr_level1, "level1", mod, wrap_pre, wrap_post, 0);
        wrap_addr(&addr_level2, "level2", mod, wrap_pre, wrap_post, 0);
        wrap_addr(&addr_tailcall, "makes_tailcall", mod, wrap_pre, wrap_post, 0);
        wrap_addr(&addr_skipme, "skipme", mod, wrap_pre, wrap_post, 0);
        wrap_addr(&addr_repeat, "repeatme", mod, wrap_pre, wrap_post, 0);
        wrap_addr(&addr_preonly, "preonly", mod, wrap_pre, NULL, 0);
        wrap_addr(&addr_postonly, "postonly", mod, NULL, wrap_post, 0);
        wrap_addr(&addr_runlots, "runlots", mod, NULL, wrap_post, 0);

        /* test longjmp */
        wrap_unwindtest_addr(&addr_long0, "long0", mod);
        wrap_unwindtest_addr(&addr_long1, "long1", mod);
        wrap_unwindtest_addr(&addr_long2, "long2", mod);
        wrap_unwindtest_addr(&addr_long3, "long3", mod);
        wrap_unwindtest_addr(&addr_longdone, "longdone", mod);
        drmgr_set_tls_field(drcontext, tls_idx, (void *)(ptr_uint_t)0);
#ifdef WINDOWS
        /* test SEH */
        /* we can't do this test for no-frills b/c only one wrap per addr */
        if (load_count == 1) {
            ok = drwrap_wrap_ex(addr_long0, wrap_unwindtest_seh_pre,
                                wrap_unwindtest_seh_post, NULL,
                                DRWRAP_UNWIND_ON_EXCEPTION);
            CHECK(ok, "wrap failed");
            ok = drwrap_wrap_ex(addr_long1, wrap_unwindtest_seh_pre,
                                wrap_unwindtest_seh_post, NULL,
                                DRWRAP_UNWIND_ON_EXCEPTION);
            CHECK(ok, "wrap failed");
            ok = drwrap_wrap_ex(addr_long2, wrap_unwindtest_seh_pre,
                                wrap_unwindtest_seh_post, NULL,
                                DRWRAP_UNWIND_ON_EXCEPTION);
            CHECK(ok, "wrap failed");
            ok = drwrap_wrap_ex(addr_long3, wrap_unwindtest_seh_pre,
                                wrap_unwindtest_seh_post, NULL,
                                DRWRAP_UNWIND_ON_EXCEPTION);
            CHECK(ok, "wrap failed");
            ok = drwrap_wrap_ex(addr_longdone, wrap_unwindtest_seh_pre,
                                wrap_unwindtest_seh_post, NULL,
                                DRWRAP_UNWIND_ON_EXCEPTION);
            CHECK(ok, "wrap failed");
        }
#endif
        /* test leaner wrapping */
        if (load_count == 2)
            drwrap_set_global_flags(DRWRAP_NO_FRILLS | DRWRAP_FAST_CLEANCALLS);
        wrap_addr(&addr_skip_flags, "skip_flags", mod, wrap_pre, NULL, 0);

        generic_func_t func = dr_get_proc_address(mod->handle, "direct_call1_ptr");
        CHECK(func != NULL, " failed to find ptr");
        addr_direct1 = *((app_pc *)func);
        func = dr_get_proc_address(mod->handle, "direct_call2_ptr");
        CHECK(func != NULL, " failed to find ptr");
        addr_direct2 = *((app_pc *)func);
        wrap_addr(&addr_direct1, "direct_call1", mod, wrap_pre, wrap_post_might_miss,
                  DRWRAP_NO_DYNAMIC_RETADDRS);
        wrap_addr(&addr_direct2, "direct_call2", mod, wrap_pre, wrap_post,
                  DRWRAP_NO_DYNAMIC_RETADDRS);

        wrap_addr(&addr_called_indirectly, "called_indirectly", mod, wrap_pre, wrap_post,
                  DRWRAP_REPLACE_RETADDR);
        wrap_addr(&addr_called_indirectly_subcall, "called_indirectly_subcall", mod,
                  wrap_pre, wrap_post, DRWRAP_REPLACE_RETADDR);
        wrap_addr(&addr_tailcall_test2, "tailcall_test2", mod, wrap_pre, wrap_post,
                  DRWRAP_REPLACE_RETADDR);
        wrap_addr(&addr_tailcall_tail, "tailcall_tail", mod, wrap_pre, wrap_post,
                  DRWRAP_REPLACE_RETADDR);
    }
}

static void
module_unload_event(void *drcontext, const module_data_t *mod)
{
    if (strstr(dr_module_preferred_name(mod), "client.drwrap-test.appdll.") != NULL) {
        bool ok;
        ok = drwrap_replace(addr_replace, NULL, true);
        CHECK(ok, "un-replace failed");
        ok = drwrap_replace_native(addr_replace2, NULL, true, 0, NULL, true);
        CHECK(ok, "un-replace_native failed");
        ok = drwrap_replace_native(addr_replace_callsite, NULL, false, 0, NULL, true);
        CHECK(ok, "un-replace_native failed");

        unwrap_addr(addr_skip_flags, "skip_flags", mod, wrap_pre, NULL);
        unwrap_addr(addr_level0, "level0", mod, wrap_pre, wrap_post);
        unwrap_addr(addr_level1, "level1", mod, wrap_pre, wrap_post);
        unwrap_addr(addr_level2, "level2", mod, wrap_pre, wrap_post);
        unwrap_addr(addr_tailcall, "makes_tailcall", mod, wrap_pre, wrap_post);
        unwrap_addr(addr_preonly, "preonly", mod, wrap_pre, NULL);
        /* skipme, postonly, and runlots were already unwrapped */

        /* test longjmp */
        unwrap_unwindtest_addr(addr_long0, "long0", mod);
        unwrap_unwindtest_addr(addr_long1, "long1", mod);
        unwrap_unwindtest_addr(addr_long2, "long2", mod);
        unwrap_unwindtest_addr(addr_long3, "long3", mod);
        unwrap_unwindtest_addr(addr_longdone, "longdone", mod);
        drmgr_set_tls_field(drcontext, tls_idx, (void *)(ptr_uint_t)0);
#ifdef WINDOWS
        /* test SEH */
        if (load_count == 1) {
            ok = drwrap_unwrap(addr_long0, wrap_unwindtest_seh_pre,
                               wrap_unwindtest_seh_post);
            CHECK(ok, "unwrap failed");
            ok = drwrap_unwrap(addr_long1, wrap_unwindtest_seh_pre,
                               wrap_unwindtest_seh_post);
            CHECK(ok, "unwrap failed");
            ok = drwrap_unwrap(addr_long2, wrap_unwindtest_seh_pre,
                               wrap_unwindtest_seh_post);
            CHECK(ok, "unwrap failed");
            ok = drwrap_unwrap(addr_long3, wrap_unwindtest_seh_pre,
                               wrap_unwindtest_seh_post);
            CHECK(ok, "unwrap failed");
            ok = drwrap_unwrap(addr_longdone, wrap_unwindtest_seh_pre,
                               wrap_unwindtest_seh_post);
        }
        CHECK(ok, "unwrap failed");
#endif
        unwrap_addr(addr_direct1, "direct_call1", mod, wrap_pre, wrap_post_might_miss);
        unwrap_addr(addr_direct2, "direct_call2", mod, wrap_pre, wrap_post);

        unwrap_addr(addr_called_indirectly, "called_indirectly", mod, wrap_pre,
                    wrap_post);
        unwrap_addr(addr_called_indirectly_subcall, "called_indirectly_subcall", mod,
                    wrap_pre, wrap_post);
        unwrap_addr(addr_tailcall_test2, "tailcall_test2", mod, wrap_pre, wrap_post);
        unwrap_addr(addr_tailcall_tail, "tailcall_tail", mod, wrap_pre, wrap_post);
    }
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_init();
    drwrap_init();
    dr_register_exit_event(event_exit);
    drmgr_register_module_load_event(module_load_event);
    drmgr_register_module_unload_event(module_unload_event);
    tls_idx = drmgr_register_tls_field();
    CHECK(tls_idx > -1, "unable to reserve TLS field");
    mutex_lock = dr_mutex_create();
    dr_mutex_mark_as_app(mutex_lock);
    rw_lock = dr_rwlock_create();
    dr_rwlock_mark_as_app(rw_lock);
    recur_lock = dr_recurlock_create();
    dr_recurlock_mark_as_app(recur_lock);
}

static void
event_exit(void)
{
    dr_mutex_destroy(mutex_lock);
    dr_rwlock_destroy(rw_lock);
    dr_recurlock_destroy(recur_lock);
    drwrap_stats_t stats = {
        sizeof(stats),
    };
    bool ok = drwrap_get_stats(&stats);
    CHECK(ok, "get_stats failed");
    CHECK(stats.flush_count > 0, "force-replaces should result in some flushes");
    drmgr_unregister_tls_field(tls_idx);
    drwrap_exit();
    drmgr_exit();
    dr_fprintf(STDERR, "all done\n");
}

static int
replacewith(int *x)
{
    *x = 6;
    return 0;
}

static int
on_clean_stack(int i, int j, int k, int l, int m, int n, int o, int p)
{
    /* Test lock/unlock of marked-app lock. */
    dr_mutex_lock(mutex_lock);
    dr_mutex_unlock(mutex_lock);
    dr_rwlock_read_lock(rw_lock);
    dr_rwlock_read_unlock(rw_lock);
    dr_rwlock_write_lock(rw_lock);
    dr_rwlock_write_unlock(rw_lock);
    dr_recurlock_lock(recur_lock);
    dr_recurlock_unlock(recur_lock);

    return i + j + k + l + m + n + o + p;
}

static int
replacewith2(int *x)
{
    ptr_int_t param =
        dr_read_saved_reg(dr_get_current_drcontext(), DRWRAP_REPLACE_NATIVE_DATA_SLOT);
    CHECK(param == DRWRAP_NATIVE_PARAM, "native param wrong");
    /* Test dr_call_on_clean_stack() */
    *x = (int)(ptr_uint_t)dr_call_on_clean_stack(
        dr_get_current_drcontext(), (void *(*)(void))on_clean_stack,
        (void *)(ptr_uint_t)500, (void *)(ptr_uint_t)400, (void *)(ptr_uint_t)50,
        (void *)(ptr_uint_t)40, (void *)(ptr_uint_t)4, (void *)(ptr_uint_t)3,
        (void *)(ptr_uint_t)1, (void *)(ptr_uint_t)1);
    /* We must call this prior to returning, to avoid going native.
     * This also serves as a test of dr_redirect_native_target()
     * as drwrap's continuation relies on that.
     * Because drwrap performs a bunch of flushes, it tests
     * the unlink/relink of the client ibl xfer gencode.
     *
     * XXX: could also verify that retaddr is app's, and that
     * traces continue on the other side.  The latter, certainly,
     * is very difficult to write a simple test for.
     */
    drwrap_replace_native_fini(dr_get_current_drcontext());
    return 1;
}

static int
replace_callsite(int *x)
{
    ptr_int_t param =
        dr_read_saved_reg(dr_get_current_drcontext(), DRWRAP_REPLACE_NATIVE_DATA_SLOT);
    CHECK(param == DRWRAP_NATIVE_PARAM, "native param wrong");
    *x = 777;
    drwrap_replace_native_fini(dr_get_current_drcontext());
    return 2;
}

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    bool ok;
    CHECK(wrapcxt != NULL && user_data != NULL, "invalid arg");
    if (drwrap_get_func(wrapcxt) == addr_skip_flags) {
        CHECK(drwrap_get_arg(wrapcxt, 0) == (void *)1, "get_arg wrong");
        CHECK(drwrap_get_arg(wrapcxt, 1) == (void *)2, "get_arg wrong");
    } else if (drwrap_get_func(wrapcxt) == addr_level0) {
        dr_fprintf(STDERR, "  <pre-level0>\n");
        CHECK(drwrap_get_arg(wrapcxt, 0) == (void *)37, "get_arg wrong");
        ok = drwrap_set_arg(wrapcxt, 0, (void *)42);
        CHECK(ok, "set_arg error");
        *user_data = (void *)99;
    } else if (drwrap_get_func(wrapcxt) == addr_level1) {
        dr_fprintf(STDERR, "  <pre-level1>\n");
        ok = drwrap_set_arg(wrapcxt, 1, (void *)1111);
        CHECK(ok, "set_arg error");
    } else if (drwrap_get_func(wrapcxt) == addr_tailcall) {
        dr_fprintf(STDERR, "  <pre-makes_tailcall>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_level2) {
        dr_fprintf(STDERR, "  <pre-level2>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_skipme) {
        dr_fprintf(STDERR, "  <pre-skipme>\n");
        drwrap_skip_call(wrapcxt, (void *)7, 0);
    } else if (drwrap_get_func(wrapcxt) == addr_repeat) {
        dr_mcontext_t *mc = drwrap_get_mcontext(wrapcxt);
        dr_fprintf(STDERR, "  <pre-repeat#%d>\n", repeated ? 2 : 1);
        repeat_xsp = mc->xsp;
#ifdef ARM
        repeat_link = mc->lr;
#endif
        if (repeated) /* test changing the arg value on the second pass */
            drwrap_set_arg(wrapcxt, 0, (void *)2);
        CHECK(drwrap_redirect_execution(NULL) != DREXT_SUCCESS,
              "allowed redirect with NULL wrapcxt");
        CHECK(drwrap_redirect_execution(wrapcxt) != DREXT_SUCCESS,
              "allowed redirect in pre-wrap");
    } else if (drwrap_get_func(wrapcxt) == addr_preonly) {
        dr_fprintf(STDERR, "  <pre-preonly>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_direct1) {
        dr_fprintf(STDERR, "  <pre-direct1>\n");
        CHECK(drwrap_get_arg(wrapcxt, 0) == (void *)42, "get_arg wrong");
        CHECK(drwrap_get_arg(wrapcxt, 1) == (void *)17, "get_arg wrong");
        *user_data = (void *)13;
    } else if (drwrap_get_func(wrapcxt) == addr_direct2) {
        dr_fprintf(STDERR, "  <pre-direct2>\n");
        CHECK(drwrap_get_arg(wrapcxt, 0) == (void *)17, "get_arg wrong");
        CHECK(drwrap_get_arg(wrapcxt, 1) == (void *)42, "get_arg wrong");
    } else if (drwrap_get_func(wrapcxt) == addr_called_indirectly) {
        dr_fprintf(STDERR, "  <pre-called_indirectly>\n");
        CHECK(drwrap_get_arg(wrapcxt, 0) == (void *)42, "get_arg wrong");
    } else if (drwrap_get_func(wrapcxt) == addr_called_indirectly_subcall) {
        dr_fprintf(STDERR, "  <pre-called_indirectly_subcall>\n");
        CHECK(drwrap_get_arg(wrapcxt, 0) == (void *)43, "get_arg wrong");
    } else if (drwrap_get_func(wrapcxt) == addr_tailcall_test2) {
        dr_fprintf(STDERR, "  <pre-tailcall_test2>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_tailcall_tail) {
        dr_fprintf(STDERR, "  <pre-tailcall_tail>\n");
    } else
        CHECK(false, "invalid wrap");
}

static void
wrap_post(void *wrapcxt, void *user_data)
{
    bool ok;
    CHECK(wrapcxt != NULL, "invalid arg");
    if (drwrap_get_func(wrapcxt) == addr_level0) {
        dr_fprintf(STDERR, "  <post-level0>\n");
        /* not preserved for no-frills */
        CHECK(load_count == 2 || user_data == (void *)99, "user_data not preserved");
        CHECK(drwrap_get_retval(wrapcxt) == (void *)42, "get_retval error");
    } else if (drwrap_get_func(wrapcxt) == addr_level1) {
        dr_fprintf(STDERR, "  <post-level1>\n");
        ok = drwrap_set_retval(wrapcxt, (void *)-4);
        CHECK(ok, "set_retval error");
    } else if (drwrap_get_func(wrapcxt) == addr_tailcall) {
        dr_fprintf(STDERR, "  <post-makes_tailcall>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_level2) {
        dr_fprintf(STDERR, "  <post-level2>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_skipme) {
        CHECK(false, "should have skipped!");
    } else if (drwrap_get_func(wrapcxt) == addr_repeat) {
        dr_fprintf(STDERR, "  <post-repeat#%d>\n", repeated ? 2 : 1);
        if (!repeated) {
            dr_mcontext_t *mc = drwrap_get_mcontext(wrapcxt);
            mc->pc = addr_repeat;
            mc->xsp = repeat_xsp;
#ifdef ARM
            mc->lr = repeat_link;
#endif
            CHECK(drwrap_redirect_execution(wrapcxt) == DREXT_SUCCESS,
                  "redirect rejected");
            CHECK(drwrap_redirect_execution(wrapcxt) != DREXT_SUCCESS,
                  "allowed duplicate redirect");
        }
        repeated = !repeated;
    } else if (drwrap_get_func(wrapcxt) == addr_postonly) {
        dr_fprintf(STDERR, "  <post-postonly>\n");
        drwrap_unwrap(addr_skipme, wrap_pre, wrap_post);
        CHECK(!drwrap_is_wrapped(addr_skipme, wrap_pre, wrap_post),
              "drwrap_is_wrapped query failed");
        drwrap_unwrap(addr_postonly, NULL, wrap_post);
        CHECK(!drwrap_is_wrapped(addr_postonly, NULL, wrap_post),
              "drwrap_is_wrapped query failed");
        drwrap_unwrap(addr_runlots, NULL, wrap_post);
        CHECK(!drwrap_is_wrapped(addr_runlots, NULL, wrap_post),
              "drwrap_is_wrapped query failed");
    } else if (drwrap_get_func(wrapcxt) == addr_runlots) {
        dr_fprintf(STDERR, "  <post-runlots>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_direct2) {
        dr_fprintf(STDERR, "  <post-direct2>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_called_indirectly) {
        dr_fprintf(STDERR, "  <post-called_indirectly>\n");
        CHECK(drwrap_get_retval(wrapcxt) == (void *)44, "get_retval wrong");
    } else if (drwrap_get_func(wrapcxt) == addr_called_indirectly_subcall) {
        dr_fprintf(STDERR, "  <post-called_indirectly_subcall>\n");
        CHECK(drwrap_get_retval(wrapcxt) == (void *)44, "get_arg wrong");
    } else if (drwrap_get_func(wrapcxt) == addr_tailcall_test2) {
        dr_fprintf(STDERR, "  <post-tailcall_test2>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_tailcall_tail) {
        dr_fprintf(STDERR, "  <post-tailcall_tail>\n");
    } else
        CHECK(false, "invalid wrap");
}

static void
wrap_post_might_miss(void *wrapcxt, void *user_data)
{
    /* A post-call that was missed has a NULL wrapcxt. */
    if (wrapcxt == NULL) {
        CHECK(user_data == (void *)13, "user_data not preserved");
        return;
    }
    if (drwrap_get_func(wrapcxt) == addr_direct1) {
        dr_fprintf(STDERR, "  <post-direct1>\n");
        CHECK(user_data == (void *)13, "user_data not preserved");
        CHECK(drwrap_get_retval(wrapcxt) == (void *)59, "get_retval error");
    } else
        CHECK(false, "invalid wrap");
}

static void
wrap_unwindtest_pre(void *wrapcxt, OUT void **user_data)
{
    if (drwrap_get_func(wrapcxt) != addr_longdone) {
        void *drcontext = dr_get_current_drcontext();
        ptr_uint_t val = (ptr_uint_t)drmgr_get_tls_field(drcontext, tls_idx);
        dr_fprintf(STDERR, "  <pre-long%d>\n", val);
        /* increment per level of regular calls on way up */
        val++;
        drmgr_set_tls_field(drcontext, tls_idx, (void *)val);
    }
}

static void
wrap_unwindtest_post(void *wrapcxt, void *user_data)
{
    void *drcontext = dr_get_current_drcontext();
    ptr_uint_t val = (ptr_uint_t)drmgr_get_tls_field(drcontext, tls_idx);
    if (drwrap_get_func(wrapcxt) == addr_longdone) {
        /* ensure our post-calls were all called and we got back to 0 */
        CHECK(val == 0, "post-calls were bypassed");
    } else {
        /* decrement on way down */
        val--;
        dr_fprintf(STDERR, "  <post-long%d%s>\n", val,
                   wrapcxt == NULL ? " abnormal" : "");
        drmgr_set_tls_field(drcontext, tls_idx, (void *)val);
    }
}

static void
wrap_unwindtest_seh_pre(void *wrapcxt, OUT void **user_data)
{
    wrap_unwindtest_pre(wrapcxt, user_data);
}

static void
wrap_unwindtest_seh_post(void *wrapcxt, void *user_data)
{
    wrap_unwindtest_post(wrapcxt, user_data);
}
