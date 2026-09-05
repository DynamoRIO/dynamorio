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

/* The kernel's print helpers (pr_info(), pr_err(), etc.) expand pr_fmt(): this must be
 * defined at the top before the #include block to have the module name prepended to
 * every message. Since this is a kernel macro, not a DR one, it has to be lower-case.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "kernel_interface.h"

#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "configure.h"
#include "kernel_assert.h"

static void *heap = NULL;
static size_t heap_size;

static unsigned long (*kallsyms_lookup_name_ptr)(const char *name) = NULL;
static int (*kallsyms_lookup_size_offset_ptr)(unsigned long addr,
                                              unsigned long *symbolsize,
                                              unsigned long *offset) = NULL;
static void *(*vmalloc_node_range_ptr)(unsigned long size, unsigned long align,
                                       unsigned long start, unsigned long end,
                                       gfp_t gfp_mask, pgprot_t prot,
                                       unsigned long vm_flags, int node,
                                       const void *caller) = NULL;

static size_t
get_symbol_size(unsigned long address)
{
    KERNEL_ASSERT(kallsyms_lookup_size_offset_ptr != NULL);

    unsigned long size, offset;
    /* kallsyms_lookup_size_offset returns 1 on success,
     * and 0 if the address is not found.
     */
    if (kallsyms_lookup_size_offset_ptr(address, &size, &offset)) {
        return size;
    }
    return 0;
}

void *
kernel_find_symbol(const char *name, size_t *size)
{
    KERNEL_ASSERT(kallsyms_lookup_name_ptr != NULL);
    KERNEL_ASSERT(name != NULL);

    unsigned long addr = kallsyms_lookup_name_ptr(name);
    if (addr == 0) {
        return NULL;
    }

    if (size != NULL) {
        *size = get_symbol_size(addr);
    }
    return (void *)addr;
}

static int
resolve_kallsyms_lookup_name(void)
{
    /* kallsyms_lookup_name is unexported in newer kernels (5.7+), so we use
     * this kprobe trick to resolve its address.
     * NOTE: This requires the kernel to be configured with CONFIG_KPROBES=y.
     * If this becomes an issue, an alternative is to parse the address from
     * /proc/kallsyms in user space and pass it into the kernel module as a
     * module parameter.
     */
    struct kprobe kp = {
        .symbol_name = "kallsyms_lookup_name",
    };

    int ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("Failed to register kprobe for kallsyms_lookup_name\n");
        return ret;
    }

    kallsyms_lookup_name_ptr = (void *)kp.addr;
    unregister_kprobe(&kp);
    return 0;
}

static int
resolve_kernel_symbols(void)
{
    kallsyms_lookup_size_offset_ptr =
        kernel_find_symbol("kallsyms_lookup_size_offset", NULL);
    if (kallsyms_lookup_size_offset_ptr == NULL) {
        pr_err("Failed to resolve kallsyms_lookup_size_offset\n");
        return -ENOENT;
    }

    /* Across 6.6+ LTS kernels, __vmalloc_node_range is the only reliable API to allocate
     * writable memory within the 2GB reach of kernel text (MODULES_VADDR..MODULES_END).
     * Alternatives:
     * - module_alloc(): Removed in 6.10. Returns RW+NX.
     * - execmem_alloc(): Introduced in 6.10. Returns RW+NX in 6.10-6.12, but
     *   read-only-execute (ROX) in 6.13+.
     * - execmem_alloc_rw(): Introduced in 6.14. Returns RW+NX.
     *
     * TODO i#8021: We would need to allocate two separate memory heaps at startup for
     * fine-grained W^X protection:
     *   1) An executable code heap (marked +x).
     *   2) A non-executable data heap (left NX).
     */
    vmalloc_node_range_ptr = kernel_find_symbol("__vmalloc_node_range", NULL);
    if (vmalloc_node_range_ptr == NULL) {
        /* In 6.10+, CONFIG_MEM_ALLOC_PROFILING renames the symbol to
         * __vmalloc_node_range_noprof, so we need to probe for that instead.
         */
        vmalloc_node_range_ptr = kernel_find_symbol("__vmalloc_node_range_noprof", NULL);
    }
    if (vmalloc_node_range_ptr == NULL) {
        pr_err("Failed to resolve __vmalloc_node_range or __vmalloc_node_range_noprof\n");
        return -ENOENT;
    }

    return 0;
}

int
kernel_module_init(size_t dr_heap_size)
{
    int ret = resolve_kallsyms_lookup_name();
    if (ret != 0) {
        return ret;
    }

    ret = resolve_kernel_symbols();
    if (ret != 0) {
        return ret;
    }

    heap_size = dr_heap_size;
    heap = vmalloc_node_range_ptr(heap_size, PAGE_SIZE, MODULES_VADDR, MODULES_END,
                                  GFP_KERNEL, PAGE_KERNEL, VM_FLUSH_RESET_PERMS,
                                  NUMA_NO_NODE, __builtin_return_address(0));
    if (heap == NULL) {
        pr_err("Failed to allocate %zu bytes with __vmalloc_node_range\n", heap_size);
        return -ENOMEM;
    }

    return 0;
}

void
kernel_module_exit(void)
{
    if (heap != NULL) {
        vfree(heap);
        heap = NULL;
    }
}

void *
kernel_allocate_heap(size_t size)
{
    if (heap != NULL && heap_size >= size) {
        return heap;
    }
    return NULL;
}

int
kernel_get_cpu_id(void)
{
    return smp_processor_id();
}

unsigned int
kernel_query_time_seconds(void)
{
    return (unsigned int)ktime_get_real_seconds();
}

#define KERNEL_ENV_MAX 20

typedef struct {
    char name[KERNEL_ENV_NAME_MAX];
    char value[KERNEL_ENV_VALUE_MAX];
} kernel_env_t;

/* The env table is not protected by any lock: it is only written during module
 * initialization (single-threaded) and read afterwards.
 */
static kernel_env_t env_vars[KERNEL_ENV_MAX];
static int env_count = 0;

int
kernel_setenv(const char *name, const char *value)
{
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL || value == NULL) {
        return -EINVAL;
    }
    if (strlen(name) >= KERNEL_ENV_NAME_MAX || strlen(value) >= KERNEL_ENV_VALUE_MAX) {
        return -E2BIG;
    }

    /* If name already exists in env_vars, overwrite the existing value. */
    for (int i = 0; i < env_count; i++) {
        if (strncmp(name, env_vars[i].name, KERNEL_ENV_NAME_MAX) == 0) {
            strscpy(env_vars[i].value, value, KERNEL_ENV_VALUE_MAX);
            return 0;
        }
    }

    /* If name doesn't exist in env_vars, try appending a new entry. */
    if (env_count >= KERNEL_ENV_MAX) {
        return -ENOSPC;
    }
    strscpy(env_vars[env_count].name, name, KERNEL_ENV_NAME_MAX);
    strscpy(env_vars[env_count].value, value, KERNEL_ENV_VALUE_MAX);
    env_count++;
    return 0;
}

const char *
kernel_getenv(const char *name)
{
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
        return NULL;
    }
    for (int i = 0; i < env_count; i++) {
        if (strncmp(name, env_vars[i].name, KERNEL_ENV_NAME_MAX) == 0) {
            return (const char *)env_vars[i].value;
        }
    }
    return NULL;
}
