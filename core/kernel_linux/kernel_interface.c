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

#include "kernel_interface.h"

#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/smp.h>
#include <linux/string.h>

static unsigned long (*kallsyms_lookup_name_ptr)(const char *name) = NULL;
static void *(*module_alloc_ptr)(unsigned long) = NULL;

typedef struct _kernel_symbol_t {
    unsigned long address;
    bool has_size;
    size_t size;
    const char *name;
} kernel_symbol_t;

static bool
get_symbol_size(kernel_symbol_t *symbol)
{
    unsigned long size = 0;
    unsigned long offset = 0;
    if (kallsyms_lookup_size_offset(symbol->address, &size, &offset)) {
        DR_ASSERT(offset == 0);
        symbol->size = size;
        symbol->has_size = true;
    } else {
        symbol->size = 0;
        symbol->has_size = false;
    }
}

static bool
find_kernel_symbol(kernel_symbol_t *symbol)
{
    symbol->address = kallsyms_lookup_name_ptr(symbol->name);
    if (symbol->address != 0) {
        get_symbol_size(symbol);
        return true;
    }
    pr_err("find_kernel_symbol failed for %s.\n", symbol->name);
    return false;
}

int
kernel_module_init(size_t dr_heap_size)
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
        pr_err("Failed to register kprobe for kallsyms_lookup_name.\n");
        return ret;
    }

    if (kp.addr == NULL) {
        pr_err("kprobe registered for kallsyms_lookup_name but the address is NULL\n");
        unregister_kprobe(&kp);
        return -ENOENT;
    }

    kallsyms_lookup_name_ptr = kp.addr;
    unregister_kprobe(&kp);

    module_alloc_ptr = (void *)kallsyms_lookup_name_ptr("module_alloc");
    if (module_alloc_ptr == NULL) {
        pr_err("dynamorio: Failed to resolve module_alloc\n");
    }

    return 0;
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
