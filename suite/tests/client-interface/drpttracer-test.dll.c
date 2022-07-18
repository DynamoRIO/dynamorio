/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Tests the drpttracer extension */

#include "string.h"
#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drpttracer.h"

typedef struct _per_thread_t {
    void *tracer_handle;
} per_thread_t;

static int tls_idx;

static void
event_exit(void);

static void
event_thread_init(void *drcontext);

static void
event_thread_exit(void *drcontext);

static bool
event_pre_syscall(void *drcontext, int sysnum);

static void
event_post_syscall(void *drcontext, int sysnum);

DR_EXPORT void
dr_init(client_id_t id)
{
    bool ok;
    drmgr_init();
    ok = drpttracer_init();
    CHECK(ok, "drpttracer init succeeded");
    dr_register_exit_event(event_exit);
    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);
    drmgr_register_pre_syscall_event(event_pre_syscall);
    drmgr_register_post_syscall_event(event_post_syscall);
    tls_idx = drmgr_register_tls_field();
    CHECK(tls_idx > -1, "unable to reserve TLS field");
}

static void
event_exit(void)
{
    bool ok;
    ok = drpttracer_dump_kcore_and_kallsyms("./");
    CHECK(ok, "drpttracer dump kcore and kallsym succeeded");
    drpttracer_exit();
    CHECK(true, "drpttracer exit succeeded");
    drmgr_unregister_thread_init_event(event_thread_init);
    drmgr_unregister_thread_exit_event(event_thread_exit);
    drmgr_unregister_pre_syscall_event(event_pre_syscall);
    drmgr_unregister_post_syscall_event(event_post_syscall);
    drmgr_unregister_tls_field(tls_idx);
    drmgr_exit();
    dr_fprintf(STDERR, "all done\n");
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));
    memset(pt, 0, sizeof(*pt));
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    bool ok;
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    ok = drpttracer_start_trace(false, true, pt->tracer_handle) == DRPTTRACER_SUCCESS;
    CHECK(ok, "drpttracer start tracing");
    return true;
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
    bool ok;
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    ok = drpttracer_end_trace(pt->tracer_handle, NULL, NULL) == DRPTTRACER_SUCCESS;
    CHECK(ok, "drpttracer stop tracing");
}