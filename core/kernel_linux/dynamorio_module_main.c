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

#include <linux/capability.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/preempt.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "configure.h"
#include "dr_interface.h"
#include "dynamorio_ioctl.h"
#include "globals_shared.h"
#include "kernel_interface.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("DynamoRIO dynamic instrumentation engine");
MODULE_AUTHOR("DynamoRIO developers");

static ulong dr_heap_size = 256 * 1024 * 1024;
module_param(dr_heap_size, ulong, 0444);
MODULE_PARM_DESC(dr_heap_size, "DynamoRIO module heap size in bytes (read-only)");

static DEFINE_MUTEX(session_lock);
static enum { SESSION_NONE, SESSION_INITIALIZED, SESSION_FAILED } session_state;

static long
dynamorio_ioctl_init(const struct dynamorio_init_args __user *uargs)
{
    struct dynamorio_init_args args;
    if (copy_from_user(&args, uargs, sizeof(args))) {
        return -EFAULT;
    }

    char *options = strndup_user((const char __user *)args.options, KERNEL_ENV_VALUE_MAX);
    if (IS_ERR(options)) {
        return PTR_ERR(options);
    }

    long ret;
    mutex_lock(&session_lock);
    if (session_state != SESSION_NONE) {
        pr_err(session_state == SESSION_FAILED
                   ? "A previous initialization failed; reload the module and retry\n"
                   : "Session is already initialized; only one session can be active\n");
        ret = -EBUSY;
        goto out;
    }

    ret = kernel_setenv(DYNAMORIO_VAR_OPTIONS, options);
    if (ret != 0) {
        goto out;
    }

    /* Although session initialization is single-threaded (due to session_lock),
     * dynamorio_app_init() acquires locks (options_lock, heap and vmarea locks, etc.)
     * through the shared DR code.  Those locks record their owner using
     * d_r_get_thread_id(), which returns the CPU ID plus one in kernel mode.
     * Disabling preemption prevents migration from breaking lock ownership checks.
     */
    preempt_disable();
    ret = dynamorio_app_init();
    preempt_enable();
    if (ret != 0) {
        /* DR reports failure as FAILURE (1), but ioctl must return a negative errno. */
        pr_err("dynamorio_app_init() failed: %ld\n", ret);
        session_state = SESSION_FAILED;
        ret = -EINVAL;
        goto out;
    }

    session_state = SESSION_INITIALIZED;
    pr_info("Session initialized\n");

out:
    mutex_unlock(&session_lock);
    kfree(options);
    return ret;
}

static long
dynamorio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (!capable(CAP_SYS_ADMIN)) {
        return -EPERM;
    }

    switch (cmd) {
    case DYNAMORIO_IOCTL_INIT:
        return dynamorio_ioctl_init((const struct dynamorio_init_args __user *)arg);
    default: return -ENOTTY;
    }
}

static const struct file_operations dynamorio_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = dynamorio_ioctl,
};

static struct miscdevice dynamorio_miscdevice = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DYNAMORIO_DEVICE_NAME,
    .fops = &dynamorio_fops,
    .mode = 0600,
};

static int __init
dynamorio_module_init(void)
{
    int ret = kernel_module_init(dr_heap_size);
    if (ret != 0) {
        return ret;
    }

    ret = misc_register(&dynamorio_miscdevice);
    if (ret != 0) {
        kernel_module_exit();
        return ret;
    }

    pr_info("Module started\n");
    return 0;
}

static void __exit
dynamorio_module_exit(void)
{
    /* TODO i#8021: Call dynamorio_app_exit() here when it's ported. */
    misc_deregister(&dynamorio_miscdevice);
    kernel_module_exit();
    pr_info("Module exited\n");
}

module_init(dynamorio_module_init);
module_exit(dynamorio_module_exit);
