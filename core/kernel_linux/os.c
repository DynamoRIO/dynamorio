/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
 * Copyright (c) 2013 Peter Feiner.  All rights reserved.
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

/* This file provides implementations for the same set of OS-interface
 * functions as core/unix/os.c and core/win32/os.c, but for the Linux
 * kernel-module target.
 */

#include "globals.h"
#include "kernel_interface.h"

/* Kernel code has no standard streams of its own. These synthetic handles use
 * conventional descriptor numbers for compatibility with shared DR code.
 */
DR_API file_t our_stdin = 0;
DR_API file_t our_stdout = 1;
DR_API file_t our_stderr = 2;

app_pc vsyscall_syscall_end_pc = NULL;
app_pc vsyscall_sysenter_return_pc = NULL;

#define ASSERT_NOT_PORTED(x) assert_not_ported(__FILE__, __LINE__, __func__)

static void
assert_not_ported(const char *file, int line, const char *func)
{
    print_file(STDERR, "%s:%d - %s not ported.\n", file, line, func);
#ifdef DEBUG
    ASSERT_NOT_IMPLEMENTED(false);
#else
    os_terminate(NULL, 0);
#endif
}

ushort
os_get_app_tls_base_offset(reg_id_t reg)
{
    ASSERT_NOT_PORTED(false);
    return 0;
}

ushort
os_get_app_tls_reg_offset(reg_id_t reg)
{
    ASSERT_NOT_PORTED(false);
    return 0;
}

thread_id_t
d_r_get_thread_id(void)
{
    /* kernel_get_cpu_id is reentrant and fast
     * (it just reads gs:[&per_cpu_var(cpu_number)])
     */
    return kernel_get_cpu_id();
}

thread_id_t
get_tls_thread_id(void)
{
    return d_r_get_thread_id();
}

thread_id_t
get_sys_thread_id(void)
{
    return kernel_get_cpu_id();
}

bool
is_thread_terminated(dcontext_t *dcontext)
{
    ASSERT_NOT_PORTED(false);
    return true;
}

void
os_wait_thread_terminated(dcontext_t *dcontext)
{
    ASSERT_NOT_PORTED(false);
}

char *
get_application_name(void)
{
    return "Linux kernel";
}

DYNAMORIO_EXPORT const char *
get_application_short_name(void)
{
    return get_application_name();
}

file_t
os_open(const char *fname, int os_open_flags)
{
    return INVALID_FILE;
}

void
os_close(file_t f)
{
    /* No-op in kernel mode. */
}

/* Only supports text as it's backed by printk.
 * Long messages are chunked and each chunk becomes a separate printk record,
 * so they may gain line breaks when displayed and interleave with other output.
 */
ssize_t
os_write(file_t f, const void *buf, size_t count)
{
    if (f != STDOUT && f != STDERR) {
        return -1;
    }
    if (buf == NULL && count != 0) {
        return -1;
    }

    /* Each printk record reservation is capped at 1024 bytes (PRINTKRB_RECORD_MAX).
     * Leave headroom for a possible text prefix and the terminating NUL.
     */
    const size_t chunk_size = 900;
    const char *cursor = (const char *)buf;
    size_t remaining = count;

    while (remaining > 0) {
        int chunk = MIN(remaining, chunk_size);
        kernel_printk("%.*s", chunk, cursor);
        cursor += chunk;
        remaining -= chunk;
    }

    return count;
}

void
os_flush(file_t f)
{
    /* This is a no-op as there is no DR-side buffering for printk output. */
}

uint
query_time_seconds(void)
{
    /* kernel_query_time_seconds() returns seconds since the Unix Epoch
     * (1970-01-01), but DR's cross-platform contract for query_time_seconds()
     * is seconds since 1601-01-01 (see os_shared.h). Add UTC_TO_EPOCH_SECONDS
     * so the kernel build matches the user space build.
     */
    return kernel_query_time_seconds() + UTC_TO_EPOCH_SECONDS;
}

size_t
os_page_size(void)
{
    return kernel_get_page_size();
}
