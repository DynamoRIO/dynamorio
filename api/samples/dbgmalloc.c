/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
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

#include <string.h>

#include "dr_api.h"
#include "drmgr.h"
#include "drwrap.h"
#include "drdbg.h"

#ifdef WINDOWS
# define IF_WINDOWS_ELSE(x,y) x
#else
# define IF_WINDOWS_ELSE(x,y) y
#endif

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) buf[(sizeof(buf)/sizeof(buf[0])) - 1] = '\0'

static void event_exit(void);
static void alloc_wrap_pre(void *wrapcxt, OUT void **user_data);
static void free_wrap_pre(void *wrapcxt, OUT void **user_data);

static ssize_t malloc_total;
static ssize_t max_malloc = 0xffffffff;
static void *max_lock; /* to synch writes to max_malloc */

#define FREE_ROUTINE_NAME IF_WINDOWS_ELSE("HeapFree", "free")
#define MALLOC_ROUTINE_NAME IF_WINDOWS_ELSE("HeapAlloc", "malloc")

drdbg_status_t
cmd_handler(char *buf, ssize_t len, char **outbuf, ssize_t *outlen)
{
    #define TOOL_KEY "dbgmalloc"
    if (!strncmp(TOOL_KEY, buf, strlen(TOOL_KEY))) {
        max_malloc = strtoul(buf+strlen(TOOL_KEY)+1, NULL, 10);
        dr_fprintf(STDERR, "Set malloc maximum to %u\n", max_malloc);
        return DRDBG_SUCCESS;
    }
    return DRDBG_ERROR;
}

static
void module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    app_pc allocwrap = (app_pc)
        dr_get_proc_address(mod->handle, MALLOC_ROUTINE_NAME);
    app_pc freewrap = (app_pc)
        dr_get_proc_address(mod->handle, FREE_ROUTINE_NAME);
    if (allocwrap != NULL && freewrap != NULL) {
#ifdef SHOW_RESULTS
       bool ok =
#endif
            drwrap_wrap(allocwrap, alloc_wrap_pre, NULL);
       ok = ok && drwrap_wrap(freewrap, free_wrap_pre, NULL);
#ifdef SHOW_RESULTS
        if (ok) {
            dr_fprintf(STDERR, "<wrapped "MALLOC_ROUTINE_NAME" @"PFX"\n", allocwrap);
            dr_fprintf(STDERR, "<wrapped "FREE_ROUTINE_NAME" @"PFX"\n", freewrap);
        } else {
            /* We expect this w/ forwarded exports (e.g., on win7 both
             * kernel32!HeapAlloc and kernelbase!HeapAlloc forward to
             * the same routine in ntdll.dll)
             */
            dr_fprintf(STDERR, "<FAILED to wrap "MALLOC_ROUTINE_NAME
                       " @"PFX": already wrapped?\n",
                       allocwrap);
            dr_fprintf(STDERR, "<FAILED to wrap "FREE_ROUTINE_NAME
                       " @"PFX": already wrapped?\n",
                       freewrap);
        }
#endif
    }
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'wrap'", "http://dynamorio.org/issues");
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, LOG_ALL, 1, "Client 'wrap' initializing\n");
    /* also give notification to stderr */
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
# ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
# endif
        dr_fprintf(STDERR, "Client wrap is running\n");
    }
#endif
    drmgr_init();
    drwrap_init();
    dr_register_exit_event(event_exit);
    drmgr_register_module_load_event(module_load_event);
    max_lock = dr_mutex_create();

    /* Register command handler with drdbg */
    drdbg_api_register_cmd(cmd_handler);
}

static void
event_exit(void)
{
    dr_mutex_destroy(max_lock);
    drwrap_exit();
    drmgr_exit();
}

static void
alloc_wrap_pre(void *wrapcxt, OUT void **user_data)
{
    /* malloc(size) or HeapAlloc(heap, flags, size) */
    size_t sz = (size_t) drwrap_get_arg(wrapcxt, IF_WINDOWS_ELSE(2,0));
    /* Add malloc size to sum */
    dr_mutex_lock(max_lock);
    malloc_total += sz;
    dr_mutex_unlock(max_lock);
    if (malloc_total > max_malloc) {
        drdbg_api_break(drwrap_get_retaddr(wrapcxt));
    }
    dr_fprintf(STDERR, "Amount: %u\n", malloc_total);
}

static void
free_wrap_pre(void *wrapcxt, OUT void **user_data)
{
    /* malloc(size) or HeapAlloc(heap, flags, size) */
    size_t sz = (size_t) drwrap_get_arg(wrapcxt, IF_WINDOWS_ELSE(2,0));
    /* Subtract free size from malloc total */
    dr_mutex_lock(max_lock);
    if (sz > malloc_total) {
        malloc_total = 0;
    } else {
        malloc_total -= sz;
    }
    dr_mutex_unlock(max_lock);
    dr_fprintf(STDERR, "Amount: %u\n", malloc_total);
}
