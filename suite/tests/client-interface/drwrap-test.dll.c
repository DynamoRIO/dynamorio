/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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
#include "drwrap.h"
#include <string.h> /* memset */

#define CHECK(x, msg) do {               \
    if (!(x)) {                          \
        dr_fprintf(STDERR, "%s\n", msg); \
        dr_abort();                      \
    }                                    \
} while (0);

static void event_exit(void);
static void wrap_pre(void *wrapcxt, OUT void **user_data);
static void wrap_post(void *wrapcxt, void *user_data);
static int replacewith(int *x);

static app_pc addr_level0;
static app_pc addr_level1;
static app_pc addr_level2;
static app_pc addr_tailcall;
static app_pc addr_skipme;
static app_pc addr_preonly;
static app_pc addr_postonly;
static app_pc addr_runlots;

static void
wrap_addr(OUT app_pc *addr, const char *name, const module_data_t *mod,
          bool pre, bool post)
{
    bool ok;
    *addr = (app_pc) dr_get_proc_address(mod->start, name);
    CHECK(*addr != NULL, "cannot find lib export");
    ok = drwrap_wrap(*addr, pre ? wrap_pre : NULL, post ? wrap_post : NULL);
    CHECK(ok, "wrap failed");
    CHECK(drwrap_is_wrapped(*addr, pre ? wrap_pre : NULL, post ? wrap_post : NULL),
          "drwrap_is_wrapped query failed");
}

static
void module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    if (strstr(dr_module_preferred_name(mod),
               "client.drwrap-test.appdll.") != NULL) {
        app_pc toreplace = (app_pc) dr_get_proc_address(mod->start, "replaceme");
        bool ok;
        CHECK(toreplace != NULL, "cannot find lib export");
        ok = drwrap_replace(toreplace, (app_pc) replacewith, false);
        CHECK(ok, "replace failed");

        wrap_addr(&addr_level0, "level0", mod, true, true);
        wrap_addr(&addr_level1, "level1", mod, true, true);
        wrap_addr(&addr_level2, "level2", mod, true, true);
        wrap_addr(&addr_tailcall, "makes_tailcall", mod, true, true);
        wrap_addr(&addr_skipme, "skipme", mod, true, true);
        wrap_addr(&addr_preonly, "preonly", mod, true, false);
        wrap_addr(&addr_postonly, "postonly", mod, false, true);
        wrap_addr(&addr_runlots, "runlots", mod, false, true);
    }
}

DR_EXPORT void 
dr_init(client_id_t id)
{
    drwrap_init();
    dr_register_exit_event(event_exit);
    dr_register_module_load_event(module_load_event);
}

static void 
event_exit(void)
{
    drwrap_exit();
    dr_fprintf(STDERR, "all done\n");
}

static int
replacewith(int *x)
{
    *x = 6;
    return 0;
}

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    bool ok;
    CHECK(wrapcxt != NULL && user_data != NULL, "invalid arg");
    if (drwrap_get_func(wrapcxt) == addr_level0) {
        dr_fprintf(STDERR, "  <pre-level0>\n");
        CHECK(drwrap_get_arg(wrapcxt, 0) == (void *) 37, "get_arg wrong");
        ok = drwrap_set_arg(wrapcxt, 0, (void *) 42);
        CHECK(ok, "set_arg error");
        *user_data = (void *) 99;
    } else if (drwrap_get_func(wrapcxt) == addr_level1) {
        dr_fprintf(STDERR, "  <pre-level1>\n");
        ok = drwrap_set_arg(wrapcxt, 1, (void *) 1111);
        CHECK(ok, "set_arg error");
    } else if (drwrap_get_func(wrapcxt) == addr_tailcall) {
        dr_fprintf(STDERR, "  <pre-makes_tailcall>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_level2) {
        dr_fprintf(STDERR, "  <pre-level2>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_skipme) {
        dr_fprintf(STDERR, "  <pre-skipme>\n");
        drwrap_skip_call(wrapcxt, (void *) 7, 0);
    } else if (drwrap_get_func(wrapcxt) == addr_preonly) {
        dr_fprintf(STDERR, "  <pre-preonly>\n");
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
        CHECK(user_data == (void *)99, "user_data not preserved");
        CHECK(drwrap_get_retval(wrapcxt) == (void *) 42, "get_retval error");
    } else if (drwrap_get_func(wrapcxt) == addr_level1) {
        dr_fprintf(STDERR, "  <post-level1>\n");
        ok = drwrap_set_retval(wrapcxt, (void *) -4);
        CHECK(ok, "set_retval error");
    } else if (drwrap_get_func(wrapcxt) == addr_tailcall) {
        dr_fprintf(STDERR, "  <post-makes_tailcall>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_level2) {
        dr_fprintf(STDERR, "  <post-level2>\n");
    } else if (drwrap_get_func(wrapcxt) == addr_skipme) {
        CHECK(false, "should have skipped!");
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
    } else
        CHECK(false, "invalid wrap");
}

