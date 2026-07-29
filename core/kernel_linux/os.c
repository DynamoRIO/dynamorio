/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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

#include "globals.h"
#include "kernel_interface.h"

app_pc vsyscall_syscall_end_pc = NULL;
app_pc vsyscall_sysenter_return_pc = NULL;

static bool in_assert_not_ported = false;

#define ASSERT_NOT_PORTED(x) assert_not_ported(__FILE__, __LINE__, __func__)

static void
assert_not_ported(const char *file, int line, const char *func)
{
    print_file(STDERR, "%s:%d - %s not ported.\n", file, line, func);
    if (!in_assert_not_ported) {
        in_assert_not_ported = true;
#ifdef DEBUG
        ASSERT_NOT_IMPLEMENTED(false);
#else
        os_terminate(NULL, 0);
#endif
    } else {
        print_file(STDERR, "not ported recursion\n");
        os_terminate(NULL, 0);
    }
}

ushort
os_get_app_tls_base_offset(reg_id_t reg)
{
    ASSERT_NOT_REACHED();
    return 0;
}

ushort
os_get_app_tls_reg_offset(reg_id_t reg)
{
    ASSERT_NOT_REACHED();
    return 0;
}

thread_id_t
get_thread_id(void)
{
    /* kernel_get_cpu_id is reentrant and fast
     * (it just reads gs:[&per_cpu_var(cpu_number)])
     */
    return kernel_get_cpu_id();
}

thread_id_t
get_tls_thread_id(void)
{
    return get_thread_id();
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

bool
os_wait_thread_terminated(dcontext_t *dcontext)
{
    ASSERT_NOT_PORTED(false);
    return true;
}

uint
query_time_seconds(void)
{
    return kernel_query_time_seconds();
}
