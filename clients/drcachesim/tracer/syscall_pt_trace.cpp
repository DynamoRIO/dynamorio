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

/* syscall_pt_trace.cpp: module for recording kernel PT traces for every syscall. */

#include <cstring>

#include "dr_api.h"
#include "drmgr.h"
#include "drpttracer.h"
#include "../common/utils.h"
#include "syscall_pt_trace.h"

#if !defined(X86_64) && !defined(LINUX)
#    error "This is only for Linux x86_64."
#endif

#define RING_BUFFER_SIZE_SHIFT 8

struct per_thread_t {
    /* The count of recorded syscalls. */
    int recorded_syscall_num;
    /* The recording syscall's sysnum. */
    int recording_sysnum;
    /* Initialize the tracer_handle before each syscall, and free it after each syscall.
     */
    void *tracer_handle;
};

struct drpttracer_output_cleanup_last_t {
public:
    ~drpttracer_output_cleanup_last_t()
    {
        if (output != nullptr) {
            void *drcontext = dr_get_current_drcontext();
            drpttracer_destroy_output(drcontext, output);
            output = nullptr;
        }
    }
    drpttracer_output_t *output;
};

static int syscall_pt_trace_init_count;
static int tls_idx;
static char logsubdir[MAXIMUM_PATH];
static ssize_t (*write_file_func)(file_t file, const void *data, size_t count);
static bool (*post_syscall_trace_func)(void *drcontext, int recorded_syscall_id);

static void
event_thread_init(void *drcontext);

static void
event_thread_exit(void *drcontext);

static bool
event_filter_syscall(void *drcontext, int sysnum);

static bool
event_pre_syscall(void *drcontext, int sysnum);

static void
event_post_syscall(void *drcontext, int sysnum);

bool
syscall_pt_trace_init(char *kernel_logsubdir,
                      ssize_t (*write_file)(file_t file, const void *data, size_t count),
                      bool (*post_syscall_trace)(void *drcontext,
                                                 int recorded_syscall_id));

void
syscall_pt_trace_exit(void);

bool
syscall_pt_trace_init(char *kernel_logsubdir,
                      ssize_t (*write_file)(file_t file, const void *data, size_t count),
                      bool (*post_syscall_trace)(void *drcontext,
                                                 int recorded_syscall_id))
{

    if (dr_atomic_add32_return_sum(&syscall_pt_trace_init_count, 1) > 1)
        return true;
    drmgr_init();
    tls_idx = drmgr_register_tls_field();

    drmgr_priority_t syscall_event_priority = { sizeof(drmgr_priority_t),
                                                DRMGR_PRIORITY_NAME_SYSCALL_PT_TRACE,
                                                NULL, NULL,
                                                DRMGR_PRIORITY_SYSCALL_PT_TRACE };

    dr_register_filter_syscall_event(event_filter_syscall);
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_pre_syscall_event_ex(event_pre_syscall,
                                             &syscall_event_priority) ||
        !drmgr_register_post_syscall_event_ex(event_post_syscall,
                                              &syscall_event_priority)) {
        ASSERT(false, "dr/drmgr_register_*_event failed");
        goto failed;
    }

    if (!drpttracer_init()) {
        ASSERT(false, "drpttracer_init failed");
        ERRMSG("Failed to initialize drpttracer. Please check whether the device support "
               "Intel PT.\n");
        goto failed;
    }

    write_file_func = write_file;
    strcpy(logsubdir, kernel_logsubdir);
    post_syscall_trace_func = post_syscall_trace;

    return true;
failed:
    syscall_pt_trace_exit();
    return false;
}

void
syscall_pt_trace_exit(void)
{
    if (dr_atomic_add32_return_sum(&syscall_pt_trace_init_count, -1) != 0)
        return;
    drpttracer_exit();
    dr_unregister_filter_syscall_event(event_filter_syscall);
    if (!drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        !drmgr_unregister_pre_syscall_event(event_pre_syscall) ||
        !drmgr_unregister_post_syscall_event(event_post_syscall) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        ASSERT(false, "dr/drmgr_unregister_*_event failed");
    }
    drmgr_exit();
}

static bool
syscall_pt_trace_filter(int sysnum)
{
    if (sysnum == 60 || sysnum == 231) {
        return false;
    }
    return true;
}

static void
syscall_pt_trace_start(void *drcontext, int sysnum)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    ASSERT(pt->recording_sysnum == -1 && pt->tracer_handle == nullptr, "");

    void *tracer_handle = nullptr;
    if (drpttracer_start_tracing(drcontext, DRPTTRACER_TRACING_ONLY_KERNEL,
                                 RING_BUFFER_SIZE_SHIFT, RING_BUFFER_SIZE_SHIFT,
                                 &tracer_handle) != DRPTTRACER_SUCCESS) {
        ASSERT(false, "drpttracer_start_tracing failed");
        ERRMSG("Failed to start drpttracer.\n");
        return;
    }
    pt->tracer_handle = tracer_handle;
    pt->recording_sysnum = sysnum;
}

static void
syscall_pt_trace_end_and_dump(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

    ASSERT(pt->recording_sysnum != -1 && pt->tracer_handle != nullptr, "");

    drpttracer_output_cleanup_last_t pt_output;
    if (drpttracer_end_tracing(drcontext, pt->tracer_handle, &pt_output.output) !=
        DRPTTRACER_SUCCESS) {
        ASSERT(false, "drpttracer_end_tracing failed");
        ERRMSG("Failed to end tracing.\n");
        return;
    }
    pt->recorded_syscall_num++;
    pt->tracer_handle = nullptr;
    pt->recording_sysnum = -1;
    if (post_syscall_trace_func == nullptr ||
        !post_syscall_trace_func(drcontext, pt->recorded_syscall_num))
        return;

    /* Dump the PT file. */
    char pt_filename[MAXIMUM_PATH];
    dr_snprintf(pt_filename, BUFFER_SIZE_ELEMENTS(pt_filename), "%s%s%d.%d.pt", logsubdir,
                DIRSEP, dr_get_thread_id(drcontext), pt->recorded_syscall_num);
    if (pt_output.output == nullptr || pt_output.output->pt_buf == nullptr) {
        ERRMSG("Failed to write PT to %s\n", pt_filename);
        return;
    }
    file_t pt_file = dr_open_file(pt_filename, DR_FILE_WRITE_OVERWRITE);
    write_file_func(pt_file, pt_output.output->pt_buf, pt_output.output->pt_buf_size);
    dr_close_file(pt_file);

    char pt_metadata_filename[MAXIMUM_PATH];
    dr_snprintf(pt_metadata_filename, BUFFER_SIZE_ELEMENTS(pt_metadata_filename),
                "%s.metadata", pt_filename);
    file_t pt_metadata_file = dr_open_file(pt_metadata_filename, DR_FILE_WRITE_OVERWRITE);
    write_file_func(pt_metadata_file, &pt_output.output->metadata, sizeof(pt_metadata_t));
    dr_close_file(pt_metadata_file);
}

static void
syscall_pt_trace_end(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

    ASSERT(pt->recording_sysnum != -1 && pt->tracer_handle != nullptr, "");

    drpttracer_output_cleanup_last_t pt_output;
    if (drpttracer_end_tracing(drcontext, pt->tracer_handle, &pt_output.output) !=
        DRPTTRACER_SUCCESS) {
        ASSERT(false, "drpttracer_end_tracing failed");
        ERRMSG("Failed to end tracing.\n");
        return;
    }
    pt->tracer_handle = nullptr;
    pt->recording_sysnum = -1;
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));
    memset(pt, 0, sizeof(*pt));
    pt->recording_sysnum = -1;
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (pt->recording_sysnum != -1 && pt->tracer_handle != nullptr) {
        ASSERT(false, "Thread exited while recording syscall\n");
        ERRMSG("Failed to stop recording syscall %d before thread %d ends.\n",
               pt->recording_sysnum, dr_get_thread_id(drcontext));
        syscall_pt_trace_end(drcontext);
    }
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true;
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    if (!syscall_pt_trace_filter(sysnum)) {
        return true;
    }

    /* Start trace before syscall. */
    syscall_pt_trace_start(drcontext, sysnum);
    return true;
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
    if (!syscall_pt_trace_filter(sysnum)) {
        return;
    }

    /* End trace after syscall. */
    syscall_pt_trace_end_and_dump(drcontext);
}
