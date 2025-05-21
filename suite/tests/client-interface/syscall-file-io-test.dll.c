/* **********************************************************
 * Copyright (c) 2025 Google, Inc.  All rights reserved.
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
#include "client_tools.h"
#include "drmgr.h"
#include "drsyscall.h"
#include "syscall.h"

static void
write_hexdump(char *hex_buf, char *addr, size_t size)
{
    for (int i = 0; i < size; ++i, ++addr) {
        dr_snprintf(hex_buf, 2, "%02x", *addr);
        hex_buf += 2;
    }
    *hex_buf = '\0';
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    switch (sysnum) {
    case SYS_close:
    case SYS_openat:
    case SYS_read:
    case SYS_write: return true;
    default: return false;
    }
}

static bool
drsys_iter_memarg_cb(drsys_arg_t *arg, void *user_data)
{
    dr_fprintf(STDERR, "%s-syscall, ordinal=%d, mode=0x%x", (arg->pre ? "pre" : "post"),
               arg->ordinal, arg->mode);
    if (arg->valid) {
        dr_fprintf(STDERR, ", start_addr=" PFX ", size=" PIFX, arg->start_addr,
                   arg->size);
        if ((arg->pre && arg->mode & DRSYS_PARAM_IN) ||
            (!arg->pre && arg->mode & DRSYS_PARAM_OUT)) {
            const size_t buf_size = 2 * arg->size + 1;
            char *hex_buf = (char *)dr_global_alloc(buf_size);
            write_hexdump(hex_buf, arg->start_addr, arg->size);
            dr_fprintf(STDERR, "\nmemory hex dump: %s", hex_buf);
            dr_global_free(hex_buf, buf_size);
        }
    }
    dr_fprintf(STDERR, "\n");

    return true;
}

static bool
drsys_iter_arg_cb(drsys_arg_t *arg, void *user_data)
{
    if (!arg->valid) {
        return true; /* keep going */
    }

    // ordinal is set to -1 for return value.
    if (arg->ordinal == -1) {
        if (!arg->pre) {
            dr_fprintf(STDERR,
                       "post-syscall, return value=0x" HEX64_FORMAT_STRING ", size=" PIFX
                       "\n",
                       arg->value64, arg->size);
        }
        return true;
    }
    dr_fprintf(STDERR,
               "%s-syscall, ordinal=%d, mode=0x%x, value=0x" HEX64_FORMAT_STRING
               ", size=" PIFX "\n",
               (arg->pre ? "pre" : "post"), arg->ordinal, arg->mode, arg->value64,
               arg->size);
    return true; /* keep going */
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    if (!event_filter_syscall(drcontext, sysnum)) {
        return true;
    }
    drsys_syscall_t *syscall;
    if (drsys_cur_syscall(drcontext, &syscall) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_cur_syscall failed, sysnum = %d", sysnum);
        return false;
    }
    drsys_sysnum_t sysnum_full;
    if (drsys_syscall_number(syscall, &sysnum_full) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_syscall_number failed, sysnum = %d", sysnum);
        return false;
    }
    if (sysnum != sysnum_full.number) {
        dr_fprintf(STDERR, "primary (%d) should match DR's num %d", sysnum,
                   sysnum_full.number);
        return false;
    }
    const char *name;
    if (drsys_syscall_name(syscall, &name) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_syscall_name failed, sysnum = %d", sysnum);
        return false;
    }
    drsys_param_type_t ret_type = DRSYS_TYPE_INVALID;
    if (drsys_syscall_return_type(syscall, &ret_type) != DRMF_SUCCESS ||
        ret_type == DRSYS_TYPE_INVALID || ret_type == DRSYS_TYPE_UNKNOWN) {
        dr_fprintf(STDERR, "failed to get syscall return type, sysnum = %d", sysnum);
        return false;
    }
    bool known = false;
    if (drsys_syscall_is_known(syscall, &known) != DRMF_SUCCESS || !known) {
        dr_fprintf(STDERR, "syscall %d is unknown", sysnum);
        return false;
    }
    dr_fprintf(STDERR, "syscall %d(%s) start\n", sysnum, name);
    if (drsys_iterate_args(drcontext, drsys_iter_arg_cb, NULL) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_iterate_args failed, sysnum = %d", sysnum);
        return false;
    }
    if (drsys_iterate_memargs(drcontext, drsys_iter_memarg_cb, NULL) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_iterate_memargs failed, sysnum = %d", sysnum);
        return false;
    }
    return true;
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
    if (!event_filter_syscall(drcontext, sysnum)) {
        return;
    }
    drsys_syscall_t *syscall;
    if (drsys_cur_syscall(drcontext, &syscall) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_cur_syscall failed, sysnum = %d", sysnum);
        return;
    }
    drsys_sysnum_t sysnum_full;
    if (drsys_syscall_number(syscall, &sysnum_full) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_syscall_number failed, sysnum = %d", sysnum);
        return;
    }
    if (sysnum != sysnum_full.number) {
        dr_fprintf(STDERR, "primary (%d) should match DR's num %d", sysnum,
                   sysnum_full.number);
        return;
    }
    const char *name;
    if (drsys_syscall_name(syscall, &name) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_syscall_name failed, sysnum = %d", sysnum);
        return;
    }
    if (drsys_iterate_args(drcontext, drsys_iter_arg_cb, NULL) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_iterate_args failed, sysnum = %d", sysnum);
        return;
    }
    if (drsys_iterate_memargs(drcontext, drsys_iter_memarg_cb, NULL) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_iterate_memargs failed, sysnum = %d", sysnum);
        return;
    }
    dr_fprintf(STDERR, "syscall %d end\n", sysnum);
}

static void
exit_event(void)
{
    if (drsys_exit() != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys failed to exit");
    }
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drsys_options_t ops = {
        sizeof(ops),
        0,
    };
    drmgr_init();
    if (drsys_init(id, &ops) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys failed to init");
        return;
    }
    dr_register_exit_event(exit_event);

    dr_register_filter_syscall_event(event_filter_syscall);
    drmgr_register_pre_syscall_event(event_pre_syscall);
    drmgr_register_post_syscall_event(event_post_syscall);
    if (drsys_filter_all_syscalls() != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_filter_all_syscalls should never fail");
    }
}
