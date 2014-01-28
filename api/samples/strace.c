/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * strace.c
 *
 * Monitors system calls.  As an example, we modify SYS_write/NtWriteFile.
 * On Windows we have to take extra steps to find system call numbers
 * and handle emulation parameters for WOW64 (Windows-On-Windows: 32-bit
 * applications on 64-bit Windows).
 *
 * Uses the drmgr extension for thread-context-local data that is preserved
 * properly across Windows callbacks.
 */

#include "dr_api.h"
#include "drmgr.h"
#include <string.h> /* memset */

#ifdef UNIX
# ifdef MACOS
#  include <sys/syscall.h>
# else
#  include <syscall.h>
# endif
#endif

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
# define ATOMIC_INC(var) _InterlockedIncrement((volatile LONG *)(&(var)))
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
# define ATOMIC_INC(var) __asm__ __volatile__("lock incl %0" : "=m" (var) : : "memory")
#endif

/* Some syscalls have more args, but this is the max we need for SYS_write/NtWriteFile */
#ifdef WINDOWS
# define SYS_MAX_ARGS 9
#else
# define SYS_MAX_ARGS 3
#endif

/* Thread-context-local data structure for storing system call
 * parameters.  Since this state spans application system call
 * execution, thread-local data is not sufficient on Windows: we need
 * thread-context-local, or "callback-local", provided by the drmgr
 * extension.
 */
typedef struct {
    reg_t param[SYS_MAX_ARGS];
#ifdef WINDOWS
    reg_t xcx; /* emulation parameter for WOW64 */
#endif
    bool repeat;
} per_thread_t;

/* Thread-context-local storage index from drmgr */
static int tcls_idx;

/* The system call number of SYS_write/NtWriteFile */
static int write_sysnum;

static int num_syscalls;

static int get_write_sysnum(void);
static void event_exit(void);
static void event_thread_context_init(void *drcontext, bool new_depth);
static void event_thread_context_exit(void *drcontext, bool process_exit);
static bool event_filter_syscall(void *drcontext, int sysnum);
static bool event_pre_syscall(void *drcontext, int sysnum);
static void event_post_syscall(void *drcontext, int sysnum);

DR_EXPORT void 
dr_init(client_id_t id)
{
    drmgr_init();
    write_sysnum = get_write_sysnum();
    dr_register_filter_syscall_event(event_filter_syscall);
    drmgr_register_pre_syscall_event(event_pre_syscall);
    drmgr_register_post_syscall_event(event_post_syscall);
    dr_register_exit_event(event_exit);
    tcls_idx = drmgr_register_cls_field(event_thread_context_init,
                                        event_thread_context_exit);
    DR_ASSERT(tcls_idx != -1);
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
# ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called in dr_init(). */
        dr_enable_console_printing();
# endif
        dr_fprintf(STDERR, "Client strace is running\n");
    }
#endif
}

static void 
show_results(void)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    /* Note that using %f with dr_printf or dr_fprintf on Windows will print
     * garbage as they use ntdll._vsnprintf, so we must use dr_snprintf.
     */
    len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                      "<Number of system calls seen: %d>", num_syscalls);
    DR_ASSERT(len > 0);
    msg[sizeof(msg)/sizeof(msg[0])-1] = '\0';
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
}

static void 
event_exit(void)
{
    show_results();
    drmgr_unregister_cls_field(event_thread_context_init,
                               event_thread_context_exit,
                               tcls_idx);
    drmgr_exit();
}

static void
event_thread_context_init(void *drcontext, bool new_depth)
{
    /* create an instance of our data structure for this thread context */
    per_thread_t *data;
#ifdef SHOW_RESULTS
    dr_fprintf(STDERR, "new thread context id="TIDFMT"%s\n", dr_get_thread_id(drcontext),
               new_depth ? " new depth" : "");
#endif
    if (new_depth) {
        data = (per_thread_t *) dr_thread_alloc(drcontext, sizeof(per_thread_t));
        drmgr_set_cls_field(drcontext, tcls_idx, data);
    } else
        data = (per_thread_t *) drmgr_get_cls_field(drcontext, tcls_idx);
    memset(data, 0, sizeof(*data));
}

static void 
event_thread_context_exit(void *drcontext, bool thread_exit)
{
#ifdef SHOW_RESULTS
    dr_fprintf(STDERR, "resuming prior thread context id="TIDFMT"\n",
               dr_get_thread_id(drcontext));
#endif
    if (thread_exit) {
        per_thread_t *data = (per_thread_t *) drmgr_get_cls_field(drcontext, tcls_idx);
        dr_thread_free(drcontext, data, sizeof(per_thread_t));
    }
    /* else, nothing to do: we leave the struct for re-use on next context */
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true; /* intercept everything */
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    bool modify_write = (sysnum == write_sysnum);
    ATOMIC_INC(num_syscalls);
#ifdef UNIX
    if (sysnum == SYS_execve) {
        /* our stats will be re-set post-execve so display now */
        show_results();
# ifdef SHOW_RESULTS
        dr_fprintf(STDERR, "<---- execve ---->\n");
# endif
    }
#endif
#ifdef SHOW_RESULTS
    dr_fprintf(STDERR, "[%d] "PFX" "PFX" "PFX"\n",
               sysnum, 
               dr_syscall_get_param(drcontext, 0),
               dr_syscall_get_param(drcontext, 1),
               dr_syscall_get_param(drcontext, 2));
#else
    /* for sanity tests that don't show results we don't change the app's output */
    modify_write = false;
#endif
    if (modify_write) {
        /* store params for access post-syscall */
        int i;
        per_thread_t *data = (per_thread_t *) drmgr_get_cls_field(drcontext, tcls_idx);
#ifdef WINDOWS
        /* stderr and stdout are identical in our cygwin rxvt shell so for
         * our example we suppress output starting with 'H' instead
         */
        byte *output = (byte *) dr_syscall_get_param(drcontext, 5);
        byte first;
        size_t read;
        bool ok = dr_safe_read(output, 1, &first, &read);
        if (!ok || read != 1)
            return true; /* data unreadable: execute normally */
        if (dr_is_wow64()) {
            /* store the xcx emulation parameter for wow64 */
            dr_mcontext_t mc = {sizeof(mc),DR_MC_INTEGER/*only need xcx*/};
            dr_get_mcontext(drcontext, &mc);
            data->xcx = mc.xcx;
        }
#endif
        for (i = 0; i < SYS_MAX_ARGS; i++)
            data->param[i] = dr_syscall_get_param(drcontext, i);
        /* suppress stderr */
        if (dr_syscall_get_param(drcontext, 0) == (reg_t) STDERR
#ifdef WINDOWS
            && first == 'H'
#endif
            ) {
            /* pretend it succeeded */
#ifdef UNIX
            /* return the #bytes == 3rd param */
            dr_syscall_result_info_t info = { sizeof(info), };
            info.succeeded = true;
            info.value = dr_syscall_get_param(drcontext, 2);
            dr_syscall_set_result_ex(drcontext, &info);
#else
            /* XXX: we should also set the IO_STATUS_BLOCK.Information field */
            dr_syscall_set_result(drcontext, 0);
#endif
#ifdef SHOW_RESULTS
            dr_fprintf(STDERR, "  [%d] => skipped\n", sysnum);
#endif
            return false; /* skip syscall */
        } else if (dr_syscall_get_param(drcontext, 0) == (reg_t) STDOUT) {
            if (!data->repeat) {
                /* redirect stdout to stderr (unless it's our repeat) */
#ifdef SHOW_RESULTS
                dr_fprintf(STDERR, "  [%d] STDOUT => STDERR\n", sysnum);
#endif
                dr_syscall_set_param(drcontext, 0, (reg_t) STDERR);
            }
            /* we're going to repeat this syscall once */
            data->repeat = !data->repeat;
        }
    }
    return true; /* execute normally */
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
#ifdef SHOW_RESULTS
    dr_syscall_result_info_t info = { sizeof(info), };
    dr_syscall_get_result_ex(drcontext, &info);
    dr_fprintf(STDERR, "  [%d] => "PFX" ("SZFMT")%s\n",
               sysnum, info.value, (ptr_int_t)info.value,
               info.succeeded ? "" : " (failed)");
#endif
    if (sysnum == write_sysnum) {
        per_thread_t *data = (per_thread_t *) drmgr_get_cls_field(drcontext, tcls_idx);
        /* we repeat a write originally to stdout that we redirected to
         * stderr: on the repeat we use stdout
         */
        if (data->repeat) {
            /* repeat syscall with stdout */
            int i;
#ifdef SHOW_RESULTS
            dr_fprintf(STDERR, "  [%d] => repeating\n", sysnum);
#endif
            dr_syscall_set_sysnum(drcontext, write_sysnum);
            dr_syscall_set_param(drcontext, 0, (reg_t) STDOUT);
            for (i = 1; i < SYS_MAX_ARGS; i++) 
                dr_syscall_set_param(drcontext, i, data->param[i]);
#ifdef WINDOWS
            if (dr_is_wow64()) {
                /* Set the xcx emulation parameter for wow64: since
                 * we're executing the same system call again we can
                 * use that same parameter.  For new system calls we'd
                 * need to determine the parameter from the ntdll
                 * wrapper.
                 */
                dr_mcontext_t mc = {sizeof(mc),DR_MC_INTEGER/*only need xcx*/};
                dr_get_mcontext(drcontext, &mc);
                mc.xcx = data->xcx;
                dr_set_mcontext(drcontext, &mc);
            }
#endif
            dr_syscall_invoke_another(drcontext);
        }
    }
}

static int
get_write_sysnum(void)
{
#ifdef UNIX
    return SYS_write;
#else
    byte *entry;
    module_data_t *data = dr_lookup_module_by_name("ntdll.dll");
    DR_ASSERT(data != NULL);
    entry = (byte *) dr_get_proc_address(data->handle, "NtWriteFile");
    DR_ASSERT(entry != NULL);
    dr_free_module_data(data);
    return drmgr_decode_sysnum_from_wrapper(entry);
#endif
}
