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
#include <linux/irqflags.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/preempt.h>
#include <linux/slab.h>
#include <linux/smp.h>
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

/* Serializes session state changes, which includes setting the options environment
 * variable, running dynamorio_app_init(), and initializing the remaining CPUs: none of
 * these is safe to run concurrently.
 */
static DEFINE_MUTEX(session_lock);
static enum { SESSION_NONE, SESSION_INITIALIZED, SESSION_FAILED } session_state;

/* SPIKE: runs drk_spike_entry() from the spike test module, which enters DR with
 * dr_app_start(), runs a test function, and leaves DR with dr_app_stop().
 */
static int spike_init_cpu = -1;

/* Leaves IRQs enabled (preemption still disabled) while running under DR. */
static bool spike_irqs_on;
module_param(spike_irqs_on, bool, 0444);
/* Runs a test function that lives inside this module instead of the test module. */
static bool spike_internal;
module_param(spike_internal, bool, 0444);
/* Kernel symbol (e.g., a syscall handler) to pass to the test module's workload 9. */
static char *spike_sym;
module_param(spike_sym, charp, 0444);

static noinline int
spike_internal_fib(int n)
{
    if (n < 2)
        return n;
    return spike_internal_fib(n - 1) + spike_internal_fib(n - 2);
}

static noinline int
spike_internal_entry(void (*start)(void), void (*stop)(void), void *fn)
{
    volatile int n = 10;
    volatile int res;
    start();
    res = spike_internal_fib(n);
    stop();
    return res == 55 ? 1 : 2;
}

static long
dynamorio_ioctl_spike(void)
{
    int (*entry)(void (*)(void), void (*)(void), void *) =
        spike_internal ? spike_internal_entry
                       : kernel_find_symbol("drk_spike_entry", NULL);
    if (entry == NULL) {
        pr_err("spike: drk_spike_entry not found; load the spike test module\n");
        return -ENOENT;
    }

    void *fn = NULL;
    if (spike_sym != NULL && spike_sym[0] != 0) {
        fn = kernel_find_symbol(spike_sym, NULL);
        if (fn == NULL) {
            pr_err("spike: symbol %s not found\n", spike_sym);
            return -ENOENT;
        }
        pr_info("spike: %s=%px\n", spike_sym, fn);
    }

    long ret;
    unsigned long flags;
    mutex_lock(&session_lock);
    if (session_state != SESSION_INITIALIZED) {
        ret = -EINVAL;
        goto out;
    }
    preempt_disable();
    if (!spike_irqs_on)
        local_irq_save(flags);
    if (false /* every CPU has a dcontext now */) {
        if (!spike_irqs_on)
            local_irq_restore(flags);
        preempt_enable();
        pr_err("spike: not on the initialized CPU\n");
        ret = -EXDEV;
        goto out;
    }
    pr_info("spike: entering DR, entry=%px\n", entry);
    ret = entry(dr_app_start, dr_app_stop, fn);
    pr_info("spike: back to native, result=%ld irqs_on=%d internal=%d\n", ret,
            spike_irqs_on, spike_internal);
    if (!spike_irqs_on)
        local_irq_restore(flags);
    preempt_enable();
    spike_report();
out:
    mutex_unlock(&session_lock);
    return ret;
}

static int
dynamorio_cpu_init(void *arg)
{
    int init_cpu = *(int *)arg;
    /* The CPU that ran dynamorio_app_init() already has its dcontext initialized. */
    if (smp_processor_id() == init_cpu) {
        return 0;
    }

    preempt_disable();
    int res = dynamo_thread_init(NULL, NULL, NULL, false);
    preempt_enable();

    return res == 0 ? 0 : -EINVAL;
}

/* XXX i#8115: Define support for per-process options, including how to reuse the
 * existing configuration infrastructure.
 */
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
        if (session_state == SESSION_FAILED) {
            pr_err("A previous initialization failed; reload the module and retry\n");
        } else {
            pr_err("Session is already initialized; only one session can be active\n");
        }
        ret = -EBUSY;
        goto out;
    }

    ret = kernel_setenv(DYNAMORIO_VAR_OPTIONS, options);
    if (ret != 0) {
        goto out;
    }

    /* Although session initialization is serialized by session_lock, dynamorio_app_init()
     * acquires locks (options_lock, heap and vmarea locks, etc.) through the shared DR
     * code.  Those locks record their owner using d_r_get_thread_id(), which returns the
     * CPU ID plus one in kernel mode.  Disabling preemption prevents migration from
     * breaking lock ownership checks.
     */
    preempt_disable();
    spike_init_cpu = smp_processor_id(); /* SPIKE */
    int init_cpu = smp_processor_id();
    ret = dynamorio_app_init();
    preempt_enable();
    if (ret != 0) {
        /* DR reports failure as FAILURE (1), but ioctl must return a negative errno. */
        pr_err("dynamorio_app_init() failed: %ld\n", ret);
        session_state = SESSION_FAILED;
        ret = -EINVAL;
        goto out;
    }

    /* Give every other online CPU a dcontext.
     * XXX i#8021: CPU hotplug is not handled yet: a CPU brought online after this has
     * no dcontext, and one taken offline leaves a stale dcontext and thread record.
     * Either block hotplug for the whole session, starting before dynamorio_app_init(),
     * or handle it with hotplug callbacks.
     */
    ret = kernel_run_on_each_online_cpu(dynamorio_cpu_init, &init_cpu);
    if (ret != 0) {
        pr_err("Per-CPU initialization failed: %ld\n", ret);
        session_state = SESSION_FAILED;
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
    case DYNAMORIO_IOCTL_SPIKE: return dynamorio_ioctl_spike(); /* SPIKE */
    /* TODO i#8021: Implement an exit ioctl that ends the tracing session. */
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
    /* TODO i#8021: Stop any active session and restore the kernel before unloading. */
    misc_deregister(&dynamorio_miscdevice);
    kernel_module_exit();
    pr_info("Module exited\n");
}

module_init(dynamorio_module_init);
module_exit(dynamorio_module_exit);
