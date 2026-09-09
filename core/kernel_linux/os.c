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

static bool os_initialized = false;

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

#define KERNEL_PROCESS_ID 0

process_id_t
get_process_id(void)
{
    return KERNEL_PROCESS_ID;
}

char *
get_application_pid(void)
{
    return STRINGIFY(KERNEL_PROCESS_ID);
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

void
os_file_init(void)
{
    /* No-op in kernel mode: there are no process fds to steal or limits to adjust. */
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

size_t
os_page_size(void)
{
    return kernel_get_page_size();
}

static int num_online_processors = 0;

int
get_num_processors(void)
{
    /* Assume that this is called at init time, so synchronization isn't necessary. */
    if (num_online_processors == 0) {
        num_online_processors = kernel_get_online_processor_count();
    }
    return num_online_processors;
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

uint
os_random_seed(void)
{
    uint64 cycles;
    RDTSC_LL(cycles);
    return (uint)cycles;
}

bool
os_check_option_compatibility(void)
{
#define FORCE_OPTION_VALUE(opt, value)       \
    do {                                     \
        if (DYNAMO_OPTION(opt) != (value)) { \
            dynamo_options.opt = (value);    \
            changed_options = true;          \
        }                                    \
    } while (0)
    bool changed_options = false;

#ifdef X64
    /* Kernel and module virtual addresses are above 4 GB, so the user-space
     * heap_in_lower_4GB placement constraint cannot be satisfied.
     */
    FORCE_OPTION_VALUE(heap_in_lower_4GB, false);
#endif

    /* Currently the kernel module has no filesystem logging support.
     * Only logging to printk is supported.
     */
    FORCE_OPTION_VALUE(log_to_stderr, true);

    /* Reserve all DR-managed virtual memory before takeover. Falling back to
     * the kernel allocator afterward could re-enter instrumented allocation paths.
     */
    FORCE_OPTION_VALUE(switch_to_os_at_vmm_reset_limit, false);
    FORCE_OPTION_VALUE(vm_reserve, true);

    /* SMP takeover requires CPUs to initialize and enter DR concurrently; the
     * global single-thread-in-DR mode would serialize them.
     */
    FORCE_OPTION_VALUE(single_thread_in_DR, false);

    /* The kernel interrupt path only supports CPU-private fragments. Shared-fragment
     * unlinking and state-reconstruction races have not been addressed.
     */
    FORCE_OPTION_VALUE(shared_bbs, false);

    /* Coarse units require shared BBs and would otherwise re-enable them during
     * recursive compatibility checking.
     */
    FORCE_OPTION_VALUE(coarse_units, false);

    /* Shared traces have the same unsupported interrupt-handling races. */
    FORCE_OPTION_VALUE(shared_traces, false);

    /* Do not request a shared trace IBL routine. On x86-64, the unconditional
     * shared-gencode path currently overrides this option and must be addressed as
     * part of takeover support.
     */
    FORCE_OPTION_VALUE(shared_trace_ibl_routine, false);

    /* Full state reconstruction from a PC in a separate direct-exit stub remains
     * unsupported, so keep exit stubs with their owning fragments.
     */
    FORCE_OPTION_VALUE(separate_private_stubs, false);
    FORCE_OPTION_VALUE(separate_shared_stubs, false);

    /* Independent stub freeing requires separate stubs; shared-stub freeing also
     * lacks the required linking atomicity.
     */
    FORCE_OPTION_VALUE(free_private_stubs, false);
    FORCE_OPTION_VALUE(unsafe_free_shared_stubs, false);

    /* Asynchronous interrupts require exact reconstruction of application EFLAGS.
     * The unsafe flag-preservation elisions are unsupported in the kernel.
     */
    FORCE_OPTION_VALUE(unsafe_ignore_overflow, false);
    FORCE_OPTION_VALUE(unsafe_ignore_eflags, false);
    FORCE_OPTION_VALUE(unsafe_ignore_eflags_trace, false);
    FORCE_OPTION_VALUE(unsafe_ignore_eflags_prefix, false);
    FORCE_OPTION_VALUE(unsafe_ignore_eflags_ibl, false);

#undef FORCE_OPTION_VALUE

    return changed_options;
}

dcontext_t *
get_thread_private_dcontext(void)
{
    /* TODO i#8021: Return per-CPU dcontext after CPU takeover. */
    ASSERT(!os_initialized);
    return NULL;
}

char *
our_getenv(const char *name)
{
    return (char *)kernel_getenv(name);
}
