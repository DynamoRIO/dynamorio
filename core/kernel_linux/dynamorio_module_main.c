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

/* The kernel's print helpers (pr_info(), pr_err(), etc.) expand pr_fmt(): this must be
 * defined at the top before the #include block to have the module name prepended to
 * every message. Since this is a kernel macro, not a DR one, it has to be lower-case.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/preempt.h>

#include "configure.h"
#include "dr_interface.h"
#include "globals_shared.h"
#include "kernel_interface.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("DynamoRIO dynamic instrumentation engine");
MODULE_AUTHOR("DynamoRIO developers");

static ulong dr_heap_size = 256 * 1024 * 1024;
module_param(dr_heap_size, ulong, 0444);
MODULE_PARM_DESC(dr_heap_size, "DynamoRIO module heap size in bytes (read-only)");

/* Initial support accepts global options at module load time only.
 * XXX i#8115: Define support for dynamic updates and per-process options, including
 * how to reuse the existing configuration infrastructure.
 */
static char options[KERNEL_ENV_VALUE_MAX];
module_param_string(options, options, sizeof(options), 0444);
MODULE_PARM_DESC(
    options,
    "DynamoRIO runtime options string (read-only), e.g., \"-loglevel 2 -log_to_stderr\"");

static int __init
dynamorio_module_init(void)
{
    int ret = kernel_module_init(dr_heap_size);
    if (ret != 0) {
        return ret;
    }

    ret = kernel_setenv(DYNAMORIO_VAR_OPTIONS, options);
    if (ret != 0) {
        goto fail;
    }

    /* Although module initialization is single-threaded, dynamorio_app_init() acquires
     * locks (options_lock, heap and vmarea locks, etc.) through the shared DR code. Those
     * locks record their owner using d_r_get_thread_id(), which returns the CPU ID plus
     * one in kernel mode. Disabling preemption prevents migration from breaking lock
     * ownership checks.
     */
    preempt_disable();
    ret = dynamorio_app_init();
    preempt_enable();
    if (ret != 0) {
        /* DR reports failure as FAILURE (1), but the kernel only treats a negative return
         * value as error: a positive return value would leave the module loaded.
         */
        pr_err("dynamorio_app_init() failed: %d\n", ret);
        ret = -EINVAL;
        goto fail;
    }

    pr_info("Module started\n");
    return 0;

fail:
    kernel_module_exit();
    return ret;
}

static void __exit
dynamorio_module_exit(void)
{
    /* TODO i#8021: Call dynamorio_app_exit() here when it's ported. */
    kernel_module_exit();
    pr_info("Module exited\n");
}

module_init(dynamorio_module_init);
module_exit(dynamorio_module_exit);
