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
static void *(*module_alloc_ptr)(unsigned long) = NULL;

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
        pr_err("kernel_find_symbol failed for %s\n", name);
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
        return -ENOENT;
    }

    module_alloc_ptr = kernel_find_symbol("module_alloc", NULL);
    if (module_alloc_ptr == NULL) {
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
    heap = module_alloc_ptr(heap_size);
    if (heap == NULL) {
        pr_err("Failed to allocate %zu bytes with module_alloc\n", heap_size);
        return -ENOMEM;
    }

    return 0;
}

void
kernel_module_exit(void)
{
    if (heap != NULL) {
        /* module_alloc() uses vmalloc internally, so vfree() is the right way to free it.
         * We can't use module_free() instead: it needs a struct module's own layout info,
         * and we don't have that since this memory wasn't allocated through the normal
         * module-loading path.
         */
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

static kernel_env_t env_vars[KERNEL_ENV_MAX];
static int env_count = 0;

const char *
kernel_getenv(const char *name)
{
    for (int i = 0; i < env_count; i++) {
        if (strncmp(name, env_vars[i].name, KERNEL_ENV_NAME_MAX) == 0) {
            return (const char *)env_vars[i].value;
        }
    }
    return NULL;
}
