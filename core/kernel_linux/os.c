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
#include "module_shared.h"

/* Kernel code has no standard streams of its own. These synthetic handles use
 * conventional descriptor numbers for compatibility with shared DR code.
 */
DR_API file_t our_stdin = 0;
DR_API file_t our_stdout = 1;
DR_API file_t our_stderr = 2;

app_pc vsyscall_syscall_end_pc = NULL;
app_pc vsyscall_sysenter_return_pc = NULL;

static bool heap_already_reserved = false;
static int num_online_processors = 0;
static bool os_state_ready = false;

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

void
d_r_os_init(void)
{
    size_t size, alignment;
    kernel_get_cpu_local_state_layout(&size, &alignment);
    /* Make sure the size and alignment of our cpu local storage meet the requirements of
     * DR's local_state_extended_t.
     */
    if (size < sizeof(local_state_extended_t) ||
        alignment % __alignof__(local_state_extended_t) != 0) {
        print_file(STDERR,
                   "DynamoRIO per-CPU storage has incompatible size or alignment: "
                   "size=" SZFMT ", alignment=" SZFMT "; required size=" SZFMT
                   ", alignment=" SZFMT "\n",
                   size, alignment, sizeof(local_state_extended_t),
                   __alignof__(local_state_extended_t));
        /* TODO i#8141: Make os_terminate() fail module loading cleanly. It currently
         * panics even before takeover.
         */
        os_terminate(NULL, 0);
    }
    os_state_ready = true;
}

void
os_loader_init_prologue(void)
{
    /* Linux has already loaded and relocated this module.
     * No OS-specific setup is needed until kernel client loading is supported.
     */
}

/* This is CPU-local active execution state, not persistent application-thread state.
 * Callers must keep preemption disabled for the entire use of the pointer.
 * Takeover must separately handle persistent thread state and nested execution.
 */
local_state_extended_t *
get_local_state_extended(void)
{
    ASSERT(os_state_ready);
    return (local_state_extended_t *)kernel_get_cpu_local_state();
}

/* Callers must keep preemption disabled for the entire use of the pointer. */
local_state_t *
get_local_state(void)
{
    ASSERT(os_state_ready);
    return (local_state_t *)kernel_get_cpu_local_state();
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
    /* kernel_get_cpu_id is reentrant and fast because it just reads
     * gs:[&per_cpu_var(cpu_number)].
     * Avoid INVALID_THREAD_ID (0) which is used for unowned locks by returning CPU ID
     * plus one.
     *
     * XXX i#8131: It's also possible to avoid the plus one by changing the value of
     * INVALID_THREAD_ID, possibly also switching thread_id_t to signed. Xref comments for
     * INVALID_THREAD_ID in core/globals.h.
     */
    return kernel_get_cpu_id() + 1;
}

thread_id_t
get_tls_thread_id(void)
{
    return d_r_get_thread_id();
}

thread_id_t
get_sys_thread_id(void)
{
    return d_r_get_thread_id();
}

dcontext_t *
get_thread_private_dcontext(void)
{
    if (!os_state_ready)
        return NULL;
    return get_local_state()->spill_space.dcontext;
}

void
set_thread_private_dcontext(dcontext_t *dcontext)
{
    get_local_state()->spill_space.dcontext = dcontext;
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

void
os_terminate(dcontext_t *dcontext, terminate_flags_t flags)
{
    /* All termination requests are system-fatal in kernel mode. Returning, killing
     * the current task, or resuming native execution could leave locks held and
     * shared state inconsistent. Expected failures must use explicit error returns.
     */
    kernel_panic("DynamoRIO kernel termination requested");
}

#define KERNEL_PROCESS_ID 0

process_id_t
get_process_id(void)
{
    return KERNEL_PROCESS_ID;
}

app_pc
get_application_base(void)
{
    return (app_pc)kernel_get_image_start();
}

app_pc
get_application_end(void)
{
    return (app_pc)kernel_get_image_end();
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

/* The kernel port has no OS-specific module-list state, matching core/unix/module.c. */
void
os_modules_init(void)
{
    /* Nothing. */
}

void
os_modules_exit(void)
{
    /* Nothing. */
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

void *
os_heap_reserve_in_region(void *start, void *end, size_t size,
                          heap_error_code_t *error_code, bool executable)
{
    /* The kernel module cannot allocate virtual memory after takeover: the kernel
     * allocators can sleep and may re-enter instrumented code. Instead, a single region
     * is reserved at module load (see `kernel_module_init()`) and handed out here.
     * Therefore, only one reservation can succeed. We verify that the region satisfies
     * DR's requested range rather than making a new allocation within it.
     *
     * `executable` is currently ignored because we only have a single RWX heap.
     * TODO i#8124: Split into a +x code region and an NX data region, then route on
     * `executable`.
     */
    *error_code = HEAP_ERROR_CANT_RESERVE_IN_REGION;

    if (heap_already_reserved) {
        LOG(GLOBAL, LOG_HEAP, 1, "%s: heap already reserved\n", __FUNCTION__);
        return NULL;
    }

    byte *heap = (byte *)kernel_allocate_heap(size);
    if (heap == NULL) {
        LOG(GLOBAL, LOG_HEAP, 1, "%s: cannot satisfy " SZFMT " bytes\n", __FUNCTION__,
            size);
        return NULL;
    }
    if (heap < (byte *)start || POINTER_OVERFLOW_ON_ADD(heap, size) ||
        heap + size > (byte *)end) {
        LOG(GLOBAL, LOG_HEAP, 1, "%s: heap " PFX "-" PFX " outside " PFX "-" PFX "\n",
            __FUNCTION__, heap, heap + size, start, end);
        return NULL;
    }

    heap_already_reserved = true;
    *error_code = HEAP_ERROR_SUCCESS;
    LOG(GLOBAL, LOG_HEAP, 2, "%s: reserved " SZFMT " bytes @ " PFX "\n", __FUNCTION__,
        size, heap);
    return heap;
}

bool
os_heap_commit(void *p, size_t size, uint prot, heap_error_code_t *error_code)
{
    /* The kernel heap is allocated at module load via __vmalloc_node_range(), which
     * allocates and maps every page upfront with PAGE_KERNEL_EXEC (RWX). So there is
     * nothing to commit here.
     */
    *error_code = HEAP_ERROR_SUCCESS;
    return true;
}

void
os_heap_decommit(void *p, size_t size, heap_error_code_t *error_code)
{
    *error_code = HEAP_ERROR_SUCCESS;
}

bool
os_heap_get_commit_limit(size_t *commit_used, size_t *commit_limit)
{
    /* DR only uses this to trigger a reset when the system is low on memory, and
     * resets are currently disabled (see os_check_option_compatibility()).
     */
    return false;
}

/* Unlike user space, there is no all_memory_areas cache and no maps file to parse.  For
 * now, readability is determined by a fault-safe trial read (see
 * kernel_is_readable_without_fault()), which acquires no locks and cannot block, so the
 * plain, "query_os" and "noblock" variants are all the same check.
 * TODO i#8021: Query memory attributes from the kernel and derive these from that query
 * instead of a trial read.
 */
bool
is_readable_without_exception(const byte *pc, size_t size)
{
    return kernel_is_readable_without_fault(pc, size);
}

bool
is_readable_without_exception_query_os(byte *pc, size_t size)
{
    return kernel_is_readable_without_fault(pc, size);
}

bool
is_readable_without_exception_query_os_noblock(byte *pc, size_t size)
{
    return kernel_is_readable_without_fault(pc, size);
}

void
all_memory_areas_lock(void)
{
    /* No-op in kernel mode. */
}

void
all_memory_areas_unlock(void)
{
    /* No-op in kernel mode. */
}

void
update_all_memory_areas(app_pc start, app_pc end, uint prot, int type)
{
    /* No-op in kernel mode. */
}

bool
remove_from_all_memory_areas(app_pc start, app_pc end)
{
    /* No-op in kernel mode. */
    return true;
}

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

    /* The module heap is within rel32 reach of kernel text, so a single vmcode unit
     * serves every allocation. DR's separate vmheap cannot be reserved after load.
     */
    FORCE_OPTION_VALUE(reachable_heap, true);
#endif

    /* Place vmcode via the near-app path in vmm_place_vmcode(), which reserves
     * the module heap within rel32 reach of kernel text.
     */
    FORCE_OPTION_VALUE(vm_base_near_app, true);

    /* Blocks must be at least page-sized for memory protection and a power of two
     * for the alignment helpers. Preserve larger valid sizes for experiments.
     */
    if (DYNAMO_OPTION(vmm_block_size) < PAGE_SIZE ||
        !IS_POWER_OF_2(DYNAMO_OPTION(vmm_block_size))) {
        SYSLOG_INTERNAL_WARNING("vmm_block_size must be a power of two and at least "
                                "PAGE_SIZE; resetting to PAGE_SIZE");
        dynamo_options.vmm_block_size = PAGE_SIZE;
        changed_options = true;
    }

    /* Larger blocks need an extra block for alignment and at least one complete
     * bitmap group in the module heap.
     */
    if (DYNAMO_OPTION(vmm_block_size) > PAGE_SIZE &&
        DYNAMO_OPTION(vmm_block_size) > kernel_get_heap_size() / (BITMAP_DENSITY + 1)) {
        SYSLOG_INTERNAL_WARNING("vmm_block_size leaves no complete bitmap group in the "
                                "module heap; resetting to PAGE_SIZE");
        dynamo_options.vmm_block_size = PAGE_SIZE;
        changed_options = true;
    }

    /* For blocks larger than a page, vmm_place_vmcode() reserves an extra block for
     * alignment. Subtract that allowance before rounding down to BITMAP_DENSITY
     * blocks: DR's VMM leaves a partial group untracked.
     */
    size_t max_vm_size = kernel_get_heap_size();
    if (DYNAMO_OPTION(vmm_block_size) > PAGE_SIZE) {
        max_vm_size -= DYNAMO_OPTION(vmm_block_size);
    }
    max_vm_size =
        ALIGN_BACKWARD(max_vm_size, DYNAMO_OPTION(vmm_block_size) * BITMAP_DENSITY);
    if (DYNAMO_OPTION(vm_size) > max_vm_size) {
        dynamo_options.vm_size = max_vm_size;
        changed_options = true;
    }

    /* Currently the kernel module has no filesystem logging support.
     * Only logging to printk is supported.
     */
    FORCE_OPTION_VALUE(log_to_stderr, true);

    /* Reserve all DR-managed virtual memory before takeover. Falling back to
     * the kernel allocator afterward could re-enter instrumented allocation paths.
     */
    FORCE_OPTION_VALUE(switch_to_os_at_vmm_reset_limit, false);
    FORCE_OPTION_VALUE(vm_reserve, true);

    /* A reset rebuilds the code cache after suspending every thread at a safe point,
     * which we cannot do yet.  DISABLE_RESET() also clears the -reset_at_* sub-options:
     * leaving those set makes check_option_compatibility_helper() turn -enable_reset
     * back on.
     * XXX i#8021: Revisit once CPUs can be synched.
     */
    if (DYNAMO_OPTION(enable_reset)) {
        DISABLE_RESET(&dynamo_options);
        changed_options = true;
    }

    /* SMP takeover requires CPUs to initialize and enter DR concurrently; the
     * global single-thread-in-DR mode would serialize them.
     */
    FORCE_OPTION_VALUE(single_thread_in_DR, false);

    /* The kernel interrupt path only supports CPU-private fragments. Shared-fragment
     * unlinking and state-reconstruction races have not been addressed.
     * XXX i#8116: This is a temporary restriction. We want CPU-shared fragments in the
     * future, which requires revisiting interrupt patching, unlink/relink coordination
     * across CPUs, and TLS bootstrapping at kernel entry points.
     */
    FORCE_OPTION_VALUE(shared_bbs, false);

    /* Coarse units require shared BBs and would otherwise re-enable them during
     * recursive compatibility checking.
     */
    FORCE_OPTION_VALUE(coarse_units, false);

    /* Shared traces have the same unsupported interrupt-handling races. */
    FORCE_OPTION_VALUE(shared_traces, false);

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

char *
our_getenv(const char *name)
{
    return (char *)kernel_getenv(name);
}

/* Diagnostics are not supported in kernel mode, as is also the case in
 * core/unix/diagnost.c.
 */
void
report_diagnostics(DR_PARAM_IN const char *message, DR_PARAM_IN const char *name,
                   security_violation_t violation_type)
{
    /* No-op in kernel mode. */
}

void
diagnost_exit(void)
{
    /* No-op in kernel mode. */
}
