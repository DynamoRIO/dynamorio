/* **********************************************************
 * Copyright (c) 2013-2020 Google, Inc.  All rights reserved.
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

/* strace: test of the Dr. Syscall Extension.
 *
 * Currently this doesn't do that much testing beyond drsyscall_client.c but
 * the original idea was to turn this into a sample strace client.
 * Now we have the separate drstrace but we keep this for its extra tests.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drsyscall.h"
#include <string.h>
#ifdef WINDOWS
# include <windows.h>
# define IF_WINDOWS_ELSE(x,y) x
#else
# define IF_WINDOWS_ELSE(x,y) y
#endif

#define TEST(mask, var) (((mask) & (var)) != 0)

#undef ASSERT /* we don't want msgbox */
#define ASSERT(cond, msg) \
    ((void)((!(cond)) ? \
     (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s (%s)", \
                 __FILE__,  __LINE__, #cond, msg), \
      dr_abort(), 0) : 0))

#define BUFFER_SIZE_BYTES(buf)      sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf)   (BUFFER_SIZE_BYTES(buf) / sizeof((buf)[0]))
#define BUFFER_LAST_ELEMENT(buf)    (buf)[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf)  BUFFER_LAST_ELEMENT(buf) = 0

#ifdef WINDOWS
/* TODO i#2279: Make it easier for clients to auto-generate! */
# define SYSNUM_FILE IF_X64_ELSE("syscalls_x64.txt", "syscalls_x86.txt")
# define SYSNUM_FILE_WOW64 "syscalls_wow64.txt"
#endif

static bool verbose = true;

#ifdef WINDOWS
static dr_os_version_info_t os_version = {sizeof(os_version),};
#endif

static bool
drsys_iter_memarg_cb(drsys_arg_t *arg, void *user_data)
{
    ASSERT(arg->valid, "no args should be invalid in this app");
    ASSERT(arg->mc != NULL, "mc check");
    ASSERT(arg->drcontext == dr_get_current_drcontext(), "dc check");

    if (verbose) {
        dr_fprintf(STDERR, "\tmemarg %d: name=%s, type=%d %s, start="PFX", size="PIFX"\n",
                   arg->ordinal, arg->arg_name == NULL ? "\"\"" : arg->arg_name,
                   arg->type, arg->type_name == NULL ? "\"\"" : arg->type_name,
                   arg->start_addr, arg->size);
    }

    return true; /* keep going */
}

static uint64
truncate_int_to_size(uint64 val, uint size)
{
    if (size == 1)
        val &= 0xff;
    else if (size == 2)
        val &= 0xffff;
    else if (size == 4)
        val &= 0xffffffff;
    return val;
}

static bool
drsys_iter_arg_cb(drsys_arg_t *arg, void *user_data)
{
    ptr_uint_t val;
    uint64 val64;

    ASSERT(arg->valid, "no args should be invalid in this app");
    ASSERT(arg->mc != NULL, "mc check");
    ASSERT(arg->drcontext == dr_get_current_drcontext(), "dc check");

    if (arg->reg == DR_REG_NULL && !TEST(DRSYS_PARAM_RETVAL, arg->mode)) {
        ASSERT((byte *)arg->start_addr >= (byte *)arg->mc->xsp &&
               (byte *)arg->start_addr < (byte *)arg->mc->xsp + PAGE_SIZE,
               "mem args should be on stack");
    }

    if (verbose) {
        dr_fprintf(STDERR, "\targ %d: name=%s, type=%d %s, value=0x"
                   HEX64_FORMAT_STRING", size="PIFX"\n",
                   arg->ordinal, arg->arg_name == NULL ? "\"\"" : arg->arg_name,
                   arg->type, arg->type_name == NULL ? "\"\"" : arg->type_name,
                   arg->value64, arg->size);
    }

    if (TEST(DRSYS_PARAM_RETVAL, arg->mode)) {
        ASSERT(arg->pre ||
               arg->value == dr_syscall_get_result(dr_get_current_drcontext()),
               "return val wrong");
        if (!arg->pre &&
            drsys_cur_syscall_result(dr_get_current_drcontext(), NULL, &val64, NULL)
            == DRMF_SUCCESS)
            ASSERT(arg->value64 == val64, "return val wrong");
    } else {
        if (drsys_pre_syscall_arg(arg->drcontext, arg->ordinal, &val) != DRMF_SUCCESS)
            ASSERT(false, "drsys_pre_syscall_arg failed");
        if (drsys_pre_syscall_arg64(arg->drcontext, arg->ordinal, &val64) != DRMF_SUCCESS)
            ASSERT(false, "drsys_pre_syscall_arg64 failed");
        if (arg->size < sizeof(val)) {
            val = truncate_int_to_size(val, arg->size);
            val64 = truncate_int_to_size(val64, arg->size);
        }
        ASSERT(val == arg->value, "values do not match");
        ASSERT(val64 == arg->value64, "values do not match");
    }

    /* We could test drsys_handle_is_current_process() but we'd have to
     * locate syscalls operating on processes.  Currently drsyscall.c
     * already tests this call.
     */

    return true; /* keep going */
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    drsys_syscall_t *syscall;
    drsys_sysnum_t sysnum_full;
    bool known;
    drsys_param_type_t ret_type;
    const char *name;

    if (drsys_cur_syscall(drcontext, &syscall) != DRMF_SUCCESS)
        ASSERT(false, "drsys_cur_syscall failed");
    if (drsys_syscall_number(syscall, &sysnum_full) != DRMF_SUCCESS)
        ASSERT(false, "drsys_get_sysnum failed");
    ASSERT(sysnum == sysnum_full.number, "primary should match DR's num");
    if (drsys_syscall_name(syscall, &name) != DRMF_SUCCESS)
        ASSERT(false, "drsys_syscall_name failed");

    if (verbose) {
        dr_fprintf(STDERR, "syscall %d.%d = %s\n", sysnum_full.number,
                   sysnum_full.secondary, name);
    }

    if (drsys_syscall_return_type(syscall, &ret_type) != DRMF_SUCCESS ||
        ret_type == DRSYS_TYPE_INVALID || ret_type == DRSYS_TYPE_UNKNOWN)
        ASSERT(false, "failed to get syscall return type");
    if (verbose)
        dr_fprintf(STDERR, "\treturn type: %d\n", ret_type);

    if (drsys_syscall_is_known(syscall, &known) != DRMF_SUCCESS || !known)
        ASSERT(IF_WINDOWS_ELSE(os_version.version >= DR_WINDOWS_VERSION_10_1607, false),
               "no syscalls in this app should be unknown");

    if (drsys_iterate_args(drcontext, drsys_iter_arg_cb, NULL) != DRMF_SUCCESS)
        ASSERT(false, "drsys_iterate_args failed");
    if (drsys_iterate_memargs(drcontext, drsys_iter_memarg_cb, NULL) != DRMF_SUCCESS)
        ASSERT(false, "drsys_iterate_memargs failed");

    return true;
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
    drsys_syscall_t *syscall;
    drsys_sysnum_t sysnum_full;
    bool success = false;

    if (drsys_cur_syscall(drcontext, &syscall) != DRMF_SUCCESS)
        ASSERT(false, "drsys_cur_syscall failed");
    if (drsys_syscall_number(syscall, &sysnum_full) != DRMF_SUCCESS)
        ASSERT(false, "drsys_get_sysnum failed");
    ASSERT(sysnum == sysnum_full.number, "primary should match DR's num");

    if (drsys_iterate_args(drcontext, drsys_iter_arg_cb, NULL) != DRMF_SUCCESS)
        ASSERT(false, "drsys_iterate_args failed");

    if (verbose) {
        dr_fprintf(STDERR, "\tsyscall returned "PFX"\n",
                   dr_syscall_get_result(drcontext));
    }
    if (drsys_cur_syscall_result(drcontext, &success, NULL, NULL) != DRMF_SUCCESS ||
        !success) {
        if (verbose)
            dr_fprintf(STDERR, "\tsyscall failed\n");
    } else {
        if (drsys_iterate_memargs(drcontext, drsys_iter_memarg_cb, NULL) != DRMF_SUCCESS)
            ASSERT(false, "drsys_iterate_memargs failed");
    }
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true; /* intercept everything */
}

static void
exit_event(void)
{
    if (drsys_exit() != DRMF_SUCCESS)
        ASSERT(false, "drsys failed to exit");
    dr_fprintf(STDERR, "TEST PASSED\n");
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drsys_options_t ops = { sizeof(ops), 0, };
#ifdef WINDOWS
    if (argc > 1) {
        /* Takes an optional argument pointing at the base dir for a sysnum file. */
        char sysnum_path[MAXIMUM_PATH];
        dr_snprintf(sysnum_path, BUFFER_SIZE_ELEMENTS(sysnum_path),
                    "%s\\%s", argv[1], SYSNUM_FILE);
        NULL_TERMINATE_BUFFER(sysnum_path);
        ops.sysnum_file = sysnum_path;
    }
    dr_get_os_version(&os_version);
#endif
    drmgr_init();
    if (drsys_init(id, &ops) != DRMF_SUCCESS)
        ASSERT(false, "drsys failed to init");
    dr_register_exit_event(exit_event);

    dr_register_filter_syscall_event(event_filter_syscall);
    drmgr_register_pre_syscall_event(event_pre_syscall);
    drmgr_register_post_syscall_event(event_post_syscall);
    if (drsys_filter_all_syscalls() != DRMF_SUCCESS)
        ASSERT(false, "drsys_filter_all_syscalls should never fail");
}
