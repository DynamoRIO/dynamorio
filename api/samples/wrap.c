/* **********************************************************
 * Copyright (c) 2011-2018 Google, Inc.  All rights reserved.
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

/* Illustrates using the drwrap extension.
 *
 * Wraps malloc on Linux, HeapAlloc on Windows.  Finds the maximum
 * malloc size requested, and randomly changes a malloc to return
 * failure to test an application's handling of out-of-memory
 * conditions.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drwrap.h"

#ifdef WINDOWS
#    define IF_WINDOWS_ELSE(x, y) x
#else
#    define IF_WINDOWS_ELSE(x, y) y
#endif

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) (buf)[(sizeof((buf)) / sizeof((buf)[0])) - 1] = '\0'

static void
event_exit(void);
static void
wrap_pre(void *wrapcxt, OUT void **user_data);
static void
wrap_post(void *wrapcxt, void *user_data);

static size_t max_malloc;
#ifdef SHOW_RESULTS
static uint malloc_oom;
#endif
static void *max_lock; /* to synch writes to max_malloc */

#define MALLOC_ROUTINE_NAME IF_WINDOWS_ELSE("HeapAlloc", "malloc")

static void
module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    app_pc towrap = (app_pc)dr_get_proc_address(mod->handle, MALLOC_ROUTINE_NAME);
    if (towrap != NULL) {
#ifdef SHOW_RESULTS
        bool ok =
#endif
            drwrap_wrap(towrap, wrap_pre, wrap_post);
#ifdef SHOW_RESULTS
        if (ok) {
            dr_fprintf(STDERR, "<wrapped " MALLOC_ROUTINE_NAME " @" PFX "\n", towrap);
        } else {
            /* We expect this w/ forwarded exports (e.g., on win7 both
             * kernel32!HeapAlloc and kernelbase!HeapAlloc forward to
             * the same routine in ntdll.dll)
             */
            dr_fprintf(STDERR,
                       "<FAILED to wrap " MALLOC_ROUTINE_NAME " @" PFX
                       ": already wrapped?\n",
                       towrap);
        }
#endif
    }
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'wrap'", "http://dynamorio.org/issues");
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'wrap' initializing\n");
    /* also give notification to stderr */
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client wrap is running\n");
    }
#endif
    drmgr_init();
    drwrap_init();
    dr_register_exit_event(event_exit);
    drmgr_register_module_load_event(module_load_event);
    max_lock = dr_mutex_create();
}

static void
event_exit(void)
{
#ifdef SHOW_RESULTS
    char msg[256];
    int len;
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                      "<Largest " MALLOC_ROUTINE_NAME
                      " request: %d>\n<OOM simulations: %d>\n",
                      max_malloc, malloc_oom);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */

    dr_mutex_destroy(max_lock);
    drwrap_exit();
    drmgr_exit();
}

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    /* malloc(size) or HeapAlloc(heap, flags, size) */
    size_t sz = (size_t)drwrap_get_arg(wrapcxt, IF_WINDOWS_ELSE(2, 0));
    /* find the maximum malloc request */
    if (sz > max_malloc) {
        dr_mutex_lock(max_lock);
        if (sz > max_malloc)
            max_malloc = sz;
        dr_mutex_unlock(max_lock);
    }
    *user_data = (void *)sz;
}

static void
wrap_post(void *wrapcxt, void *user_data)
{
#ifdef SHOW_RESULTS /* we want determinism in our test suite */
    size_t sz = (size_t)user_data;
    /* test out-of-memory by having a random moderately-large alloc fail */
    if (sz > 1024 && dr_get_random_value(1000) < 10) {
        bool ok = drwrap_set_retval(wrapcxt, NULL);
        DR_ASSERT(ok);
        dr_mutex_lock(max_lock);
        malloc_oom++;
        dr_mutex_unlock(max_lock);
    }
#endif
}
