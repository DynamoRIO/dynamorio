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
#include "kernel_interface.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("DynamoRIO dynamic instrumentation engine");
MODULE_AUTHOR("DynamoRIO developers");

static ulong dr_heap_size = 257 * 1024 * 1024;
module_param(dr_heap_size, ulong, 0444);
MODULE_PARM_DESC(dr_heap_size, "DynamoRIO module heap size in bytes (read-only)");

static int __init
dynamorio_module_init(void)
{
    int ret = kernel_module_init(dr_heap_size);
    if (ret != 0) {
        return ret;
    }
    pr_info("Module started\n");
    return 0;
}

static void __exit
dynamorio_module_exit(void)
{
    kernel_module_exit();
    pr_info("Module exited\n");
}

module_init(dynamorio_module_init);
module_exit(dynamorio_module_exit);
