/* **********************************************************
 * Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* DRMF sample: strace.c
 *
 * Uses drsyscall to print the name and result of of every system call executed.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drsyscall.h"

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true; /* we want to see all syscalls */
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    drsys_syscall_t *syscall;
    const char *name = "<unknown>";
    if (drsys_cur_syscall(drcontext, &syscall) == DRMF_SUCCESS)
        drsys_syscall_name(syscall, &name);
    dr_fprintf(STDERR, "[%s ", name, sysnum);
    /* We can also get the # of args and the type of each arg.
     * See the drstrace tool for an example of how to do that.
     */
    return true; /* execute normally */
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
    bool success;
    uint64 result;
    uint error;
    if (drsys_cur_syscall_result(drcontext, &success, &result, &error) == DRMF_SUCCESS) {
        dr_fprintf(STDERR, "=> 0x"HEX64_FORMAT_STRING" ("SZFMT"), error=%d%s]\n",
                   result, (ptr_int_t)result, error, success ? "" : " (failed)");
    }
}

static void
event_exit(void)
{
    if (drsys_exit() != DRMF_SUCCESS)
        DR_ASSERT(false);
    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drsys_options_t ops = { sizeof(ops), 0, };
    drmgr_init();
    dr_register_filter_syscall_event(event_filter_syscall);
    drmgr_register_pre_syscall_event(event_pre_syscall);
    drmgr_register_post_syscall_event(event_post_syscall);
    if (drsys_init(id, &ops) != DRMF_SUCCESS)
        DR_ASSERT(false);
    dr_register_exit_event(event_exit);
#ifdef WINDOWS
    dr_enable_console_printing(); /* ensure output shows up in cmd */
#endif
}
