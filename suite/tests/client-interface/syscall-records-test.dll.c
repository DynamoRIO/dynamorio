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

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drsyscall.h"
#include "drsyscall_record.h"
#include "drsyscall_record_lib.h"
#include "syscall.h"

#define WRITE_BUFFER_SIZE 8192

static int offset = 0;
static file_t record_file;
static char buffer[WRITE_BUFFER_SIZE];

static inline uint64_t
get_microsecond_timestamp()
{
    static uint64_t fake_timestamp = 10000;
    return ++fake_timestamp;
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

static size_t
write_file(char *buf, size_t size)
{
    size_t byte_written = 0;
    size_t remaining = size;

    while (remaining + offset >= WRITE_BUFFER_SIZE) {
        memcpy(&buffer[offset], buf, WRITE_BUFFER_SIZE - offset);
        write(record_file, buffer, WRITE_BUFFER_SIZE);
        remaining -= WRITE_BUFFER_SIZE - offset;
        buf += WRITE_BUFFER_SIZE - offset;
        byte_written += WRITE_BUFFER_SIZE - offset;
        offset = 0;
    }
    if (remaining > 0) {
        memcpy(&buffer[offset], buf, remaining);
        offset += remaining;
        byte_written += remaining;
    }
    return byte_written;
}

static size_t
flush_file()
{
    if (offset > 0) {
        write(record_file, buffer, offset);
    }
    return offset;
}

static bool
drsys_iter_memarg_cb(drsys_arg_t *arg, void *user_data)
{
    drsyscall_write_memarg_record(write_file, arg);
    return true;
}

static bool
drsys_iter_arg_cb(drsys_arg_t *arg, void *user_data)
{
    drsyscall_write_param_record(write_file, arg);
    return true;
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
    if (drsyscall_write_syscall_number_timestamp_record(
            write_file, sysnum_full, get_microsecond_timestamp()) == 0) {
        dr_fprintf(STDERR, "failed to write syscall number record, sysnum = %d", sysnum);
        return false;
    }

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
    if (drsys_iterate_args(drcontext, drsys_iter_arg_cb, NULL) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_iterate_args failed, sysnum = %d", sysnum);
        return;
    }
    if (drsys_iterate_memargs(drcontext, drsys_iter_memarg_cb, NULL) != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys_iterate_memargs failed, sysnum = %d", sysnum);
        return;
    }
    if (drsyscall_write_syscall_end_timestamp_record(write_file, sysnum_full,
                                                     get_microsecond_timestamp()) == 0) {
        dr_fprintf(STDERR, "failed to write syscall end record, sysnum = %d", sysnum);
        return;
    }
}

static void
exit_event(void)
{
    flush_file();
    dr_close_file(record_file);
    if (drsys_exit() != DRMF_SUCCESS) {
        dr_fprintf(STDERR, "drsys failed to exit");
    }
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    syscall_record_t record = {};
    ASSERT((SYSCALL_RECORD_UNION_SIZE_BYTES + sizeof(record.type)) ==
           sizeof(syscall_record_t));

    char filename[MAXIMUM_PATH];
    sprintf(filename, "syscall_record_file.%d", getpid());
    record_file = dr_open_file(filename, DR_FILE_WRITE_OVERWRITE);
    if (record_file == INVALID_FILE) {
        dr_fprintf(STDERR, "Error opening file %d\n", filename);
        return;
    }

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
