/* **********************************************************
 * Copyright (c) 2012-2015 Google, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

#include "dr_api.h"
#include "drmgr.h"
#include "client_tools.h"
#include <string.h> /* memset */
#ifdef UNIX
#    ifdef MACOS
#        include <sys/syscall.h>
#    else
#        include <syscall.h>
#        include <linux/sched.h> /* for clone and CLONE_ flags */
#    endif
#endif

static client_id_t client_id = 0;

static int cls_idx;

static bool sent_self;

static process_id_t child_pid;

typedef struct {
    ptr_int_t saved_param;
    process_id_t child_pid;
} per_thread_t;

#define NUDGE_ARG_SELF 101
#define NUDGE_ARG_PRINT 102
#define NUDGE_ARG_TERMINATE 103
#define NUDGE_TIMEOUT_MS 2000
#define NUDGE_TERMINATE_STATUS 42
#define NUDGE_MAX_TRIES 30

#ifdef WINDOWS
static int sysnum_CreateProcess;
static int sysnum_CreateProcessEx;
static int sysnum_CreateUserProcess;
static int sysnum_ResumeThread;

static int
get_sysnum(const char *wrapper)
{
    byte *entry;
    module_data_t *data = dr_lookup_module_by_name("ntdll.dll");
    ASSERT(data != NULL);
    entry = (byte *)dr_get_proc_address(data->handle, wrapper);
    dr_free_module_data(data);
    if (entry == NULL)
        return -1;
    return drmgr_decode_sysnum_from_wrapper(entry);
}
#endif

static void
event_nudge(void *drcontext, uint64 arg)
{
    dr_fprintf(STDERR, "nudge delivered %d\n", (uint)arg);
    if (arg == NUDGE_ARG_SELF)
        dr_fprintf(STDERR, "self\n");
    else if (arg == NUDGE_ARG_PRINT)
        dr_fprintf(STDERR, "printing\n");
    else if (arg == NUDGE_ARG_TERMINATE) {
        dr_fprintf(STDERR, "terminating\n");
        dr_exit_process(NUDGE_TERMINATE_STATUS);
        ASSERT_MSG(false, "should not be reached");
    }
}

static void
nudge_child(process_id_t child_pid)
{
    /* send two nudges to the child process */
    dr_config_status_t res;
    uint count = 0;
    const char *msg;
    res = dr_nudge_client_ex(child_pid, client_id, NUDGE_ARG_PRINT, 0);
    msg = dr_config_status_code_to_string(res);
    if (res != DR_SUCCESS) {
        dr_fprintf(STDERR, "nudge failed: %s\n", msg);
        ASSERT_MSG(strcmp(msg, "success") != 0, "wrong dr_config_status msg");
    } else {
        ASSERT_MSG(strcmp(msg, "success") == 0, "wrong dr_config_status msg");
    }
    /* On Linux, wait for child's signal handler to finish so this
     * nudge won't be blocked (xref i#744).
     * XXX: flaky!
     */
    dr_sleep(200);
    res = dr_nudge_client_ex(child_pid, client_id, NUDGE_ARG_TERMINATE, NUDGE_TIMEOUT_MS);
    msg = dr_config_status_code_to_string(res);
    if (res != DR_SUCCESS) {
        dr_fprintf(STDERR, "nudge failed or timed out: %s\n", msg);
        ASSERT_MSG(strcmp(msg, "success") != 0, "wrong dr_config_status msg");
    } else {
        ASSERT_MSG(strcmp(msg, "success") == 0, "wrong dr_config_status msg");
    }
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true; /* intercept everything */
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
#ifdef LINUX
    if (sysnum == SYS_clone) {
        per_thread_t *data = (per_thread_t *)drmgr_get_cls_field(drcontext, cls_idx);
        data->saved_param = dr_syscall_get_param(drcontext, 0);
    }
#elif defined(WINDOWS)
    if (sysnum == sysnum_CreateProcess || sysnum == sysnum_CreateProcessEx ||
        sysnum == sysnum_CreateUserProcess) {
        per_thread_t *data = (per_thread_t *)drmgr_get_cls_field(drcontext, cls_idx);
        data->saved_param = dr_syscall_get_param(drcontext, 0);
    }
#endif
    return true;
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_cls_field(drcontext, cls_idx);
    /* XXX i#752: should DR provide a child creation event that gives us the pid? */
#ifdef UNIX
    if (sysnum == SYS_fork
#    ifdef LINUX
        || (sysnum == SYS_clone && !TEST(CLONE_VM, data->saved_param))
#    endif
    ) {
        child_pid = dr_syscall_get_result(drcontext);
        /* we nudge once we see notification from parent, via bb pattern (i#953) */
    }
#else
    if (sysnum == sysnum_CreateProcess || sysnum == sysnum_CreateProcessEx ||
        sysnum == sysnum_CreateUserProcess) {
        if (dr_syscall_get_result(drcontext) >= 0) { /* success */
            HANDLE *hproc = (HANDLE)data->saved_param;
            HANDLE h;
            size_t read;
            if (dr_safe_read(hproc, sizeof(h), &h, &read) && read == sizeof(h))
                data->child_pid = dr_convert_handle_to_pid(h);
            /* we can't nudge now b/c the child's initial thread is suspended */
        }
    } else if (sysnum == sysnum_ResumeThread) {
        /* child should be alive now but we nudge in bb event (i#953) */
        child_pid = data->child_pid;
    }
#endif
}

static void
event_thread_context_init(void *drcontext, bool new_depth)
{
    /* create an instance of our data structure for this thread context */
    per_thread_t *data;
    if (new_depth) {
        data = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
        drmgr_set_cls_field(drcontext, cls_idx, data);
    } else
        data = (per_thread_t *)drmgr_get_cls_field(drcontext, cls_idx);
    memset(data, 0, sizeof(*data));
    /* test self-nudge to make up for lack of nudge_test on windows (waiting
     * for runall support (i#120)
     */
    if (!sent_self) {
        sent_self = true;
        if (!dr_nudge_client(client_id, NUDGE_ARG_SELF))
            dr_fprintf(STDERR, "self nudge failed");
    }
}

static void
event_thread_context_exit(void *drcontext, bool thread_exit)
{
    if (thread_exit) {
        per_thread_t *data = (per_thread_t *)drmgr_get_cls_field(drcontext, cls_idx);
        dr_thread_free(drcontext, data, sizeof(per_thread_t));
    }
    /* else, nothing to do: we leave the struct for re-use on next context */
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data)
{
    instr_t *instr, *next_instr, *next_next_instr;
    /* Look for 3 nops in parent code to know child is live and avoid flakiness (i#953) */
    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (next_instr != NULL)
            next_next_instr = instr_get_next(next_instr);
        else
            next_next_instr = NULL;
        if (instr_is_nop(instr) && next_instr != NULL && instr_is_nop(next_instr) &&
            next_next_instr != NULL && instr_is_call_direct(next_next_instr)) {
            /* we set child_pid while watching syscalls creating it */
            if (child_pid != INVALID_PROCESS_ID)
                nudge_child(child_pid);
            break;
        }
    }
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    dr_fprintf(STDERR, "client exiting\n");
    drmgr_unregister_cls_field(event_thread_context_init, event_thread_context_exit,
                               cls_idx);
    drmgr_exit();
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    bool ok;
    client_id = id;
    dr_fprintf(STDERR, "thank you for testing the client interface\n");
    drmgr_init();
    cls_idx =
        drmgr_register_cls_field(event_thread_context_init, event_thread_context_exit);
    ASSERT(cls_idx != -1);
    dr_register_nudge_event(event_nudge, id);
    dr_register_filter_syscall_event(event_filter_syscall);
    drmgr_register_pre_syscall_event(event_pre_syscall);
    drmgr_register_post_syscall_event(event_post_syscall);
    dr_register_exit_event(event_exit);

    ok = drmgr_register_bb_instrumentation_event(event_bb_analysis, NULL, NULL);
    ASSERT(ok);

#ifdef WINDOWS
    sysnum_CreateProcess = get_sysnum("NtCreateProcess");
    ASSERT(sysnum_CreateProcess != -1);
    /* not asserting on these since added later */
    sysnum_CreateProcessEx = get_sysnum("NtCreateProcessEx");
    sysnum_CreateUserProcess = get_sysnum("NtCreateUserProcess");
    sysnum_ResumeThread = get_sysnum("NtResumeThread");
    ASSERT(sysnum_ResumeThread != -1);
#endif
}
