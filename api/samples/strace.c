/* **********************************************************
 * Copyright (c) 2009 VMware, Inc.  All rights reserved.
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
 */

#include "dr_api.h"
#include <string.h> /* memset */

#ifdef LINUX
# include <syscall.h>
#endif

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
# define ATOMIC_INC(var) _InterlockedIncrement(&var)
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

/* Thread-local data structure for storing system call parameters */
typedef struct {
    reg_t param[SYS_MAX_ARGS];
#ifdef WINDOWS
    reg_t xcx; /* emulation parameter for WOW64 */
#endif
    bool repeat;
} per_thread_t;

/* The system call number of SYS_write/NtWriteFile */
static int write_sysnum;

static int num_syscalls;

static int get_write_sysnum(void);
static void event_exit(void);
static void event_thread_init(void *drcontext);
static void event_thread_exit(void *drcontext);
static bool event_filter_syscall(void *drcontext, int sysnum);
static bool event_pre_syscall(void *drcontext, int sysnum);
static void event_post_syscall(void *drcontext, int sysnum);

DR_EXPORT void 
dr_init(client_id_t id)
{
    write_sysnum = get_write_sysnum();
    dr_register_thread_init_event(event_thread_init);
    dr_register_thread_exit_event(event_thread_exit);
    dr_register_filter_syscall_event(event_filter_syscall);
    dr_register_pre_syscall_event(event_pre_syscall);
    dr_register_post_syscall_event(event_post_syscall);
    dr_register_exit_event(event_exit);
#ifdef SHOW_RESULTS
    if (dr_is_notify_on())
	dr_fprintf(STDERR, "Client strace is running\n");
#endif
}

static void 
show_results(void)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    /* Note that using %f with dr_printf or dr_fprintf on Windows will print
     * garbage as they use ntdll._vsnprintf, so we must use snprintf.
     */
    len = snprintf(msg, sizeof(msg)/sizeof(msg[0]),
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
}

static void
event_thread_init(void *drcontext)
{
    /* create an instance of our data structure for this thread */
    per_thread_t *data = (per_thread_t *)
        dr_thread_alloc(drcontext, sizeof(per_thread_t));
    memset(data, 0, sizeof(*data));
    /* store it in the slot provided in the drcontext */
    dr_set_tls_field(drcontext, data);
}

static void 
event_thread_exit(void *drcontext)
{
    per_thread_t *data = (per_thread_t *) dr_get_tls_field(drcontext);
    /* clean up memory */
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true; /* intercept everything */
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    ATOMIC_INC(num_syscalls);
#ifdef LINUX
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
#endif
    if (sysnum == write_sysnum) {
        /* store params for access post-syscall */
        per_thread_t *data = (per_thread_t *) dr_get_tls_field(drcontext);
        int i;
#ifdef WINDOWS
        /* stderr and stdout are identical in our cygwin rxvt shell so for
         * our example we suppress output starting with 'H' instead
         */
        byte *output = (byte *) dr_syscall_get_param(drcontext, 5);
        size_t len = dr_syscall_get_param(drcontext, 6);
        byte first;
        size_t read;
        bool ok = dr_safe_read(output, 1, &first, &read);
        if (!ok || read != 1)
            return true; /* data unreadable: execute normally */
        if (dr_is_wow64()) {
            /* store the xcx emulation parameter for wow64 */
            dr_mcontext_t mc;
            dr_get_mcontext(drcontext, &mc, NULL);
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
#ifdef LINUX
            /* return the #bytes == 3rd param */
            dr_syscall_set_result(drcontext, dr_syscall_get_param(drcontext, 2));
#else
            /* we should also set the IO_STATUS_BLOCK.Information field */
            dr_syscall_set_result(drcontext, 0);
#endif
#ifdef SHOW_RESULTS
            dr_fprintf(STDERR, "  [%d] => skipped\n", sysnum);
#endif
            return false; /* skip syscall */
        } else if (dr_syscall_get_param(drcontext, 0) == (reg_t) STDOUT) {
            if (!data->repeat) {
                /* redirect stdout to stderr (unless it's our repeat) */
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
    dr_fprintf(STDERR, "  [%d] => "PFX" ("SZFMT")\n",
               sysnum, 
               dr_syscall_get_result(drcontext),
               (ptr_int_t)dr_syscall_get_result(drcontext));
#endif
    if (sysnum == write_sysnum) {
        per_thread_t *data = (per_thread_t *) dr_get_tls_field(drcontext);
        /* we repeat a write originally to stdout that we redirected to
         * stderr: on the repeat we use stdout
         */
        if (data->repeat) {
            /* repeat syscall with stdout */
            int i;
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
                dr_mcontext_t mc;
                dr_get_mcontext(drcontext, &mc, NULL);
                mc.xcx = data->xcx;
                dr_set_mcontext(drcontext, &mc, NULL);
            }
#endif
            dr_syscall_invoke_another(drcontext);
        }
    }
}

static int
get_write_sysnum(void)
{
#ifdef LINUX
    return SYS_write;
#else
    byte *entry;
    module_data_t *data = dr_lookup_module_by_name("ntdll.dll");
    DR_ASSERT(data != NULL);
    entry = (byte *) dr_get_proc_address(data->start, "NtWriteFile");
    DR_ASSERT(entry != NULL);
    dr_free_module_data(data);
    return decode_syscall_num(entry);
#endif
}

#ifdef WINDOWS
/* takes in an Nt syscall wrapper entry point */
static int
decode_syscall_num(byte *entry)
{
    void *drcontext = dr_get_current_drcontext();
    int num = -1;
    byte *pc = entry;
    uint opc;
    instr_t instr;
    instr_init(drcontext, &instr);
    do {
        instr_reset(drcontext, &instr);
        pc = decode(drcontext, pc, &instr);
        DR_ASSERT(instr_valid(&instr));
        opc = instr_get_opcode(&instr);
        /* safety check: should only get 11 or 12 bytes in */
        if (pc - entry > 20) {
            DR_ASSERT(false);
            instr_free(drcontext, &instr);
            return -1;
        }
        /* Note that we'll failed if somebody has hooked the wrapper */
        if (opc == OP_mov_imm && opnd_is_reg(instr_get_dst(&instr, 0)) &&
            opnd_get_reg(instr_get_dst(&instr, 0)) == REG_EAX) {
            DR_ASSERT(opnd_is_immed_int(instr_get_src(&instr, 0)));
            num = opnd_get_immed_int(instr_get_src(&instr, 0));
            break;
        }
        /* stop at call to vsyscall or at int itself */
    } while (opc != OP_call_ind && opc != OP_int);
    instr_free(drcontext, &instr);
    DR_ASSERT(num > -1);
    return num;
}
#endif /* WINDOWS */
