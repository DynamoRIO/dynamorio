/* *******************************************************************************
 * Copyright (c) 2010-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
 * *******************************************************************************/

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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * ksynch_linux.c - synchronization via the kernel
 *
 * i#96/PR 295561: use futex(2) if available.
 */

#include "../globals.h"
#include "ksynch.h"
#include <linux/futex.h> /* for futex op code */
#include <sys/time.h>
#include "include/syscall.h" /* our own local copy */

#ifndef LINUX
#    error Linux-only
#endif

/* Does the kernel support SYS_futex syscall? Safe to initialize assuming
 * no futex support.
 */
static bool kernel_futex_support = false;

void
ksynch_init(void)
{
    /* Determines whether the kernel supports SYS_futex syscall or not.
     * From man futex(2): initial futex support was merged in 2.5.7, in current six
     * argument format since 2.6.7.
     */
    volatile int futex_for_test = 0;
    ptr_int_t res =
        dynamorio_syscall(SYS_futex, 6, &futex_for_test, FUTEX_WAKE, 1, NULL, NULL, 0);
    kernel_futex_support = (res >= 0);
    ASSERT_CURIOSITY(kernel_futex_support);
}

void
ksynch_exit(void)
{
}

bool
ksynch_kernel_support(void)
{
    return kernel_futex_support;
}

/* We use volatile int rather than bool since these are used as futexes.
 * 0 is unset, 1 is set, and no other value is used.
 */
bool
ksynch_init_var(volatile int *futex)
{
    ASSERT(ALIGNED(futex, sizeof(int)));
    *futex = 0;
    return true;
}

bool
ksynch_var_initialized(volatile int *futex)
{
    return (*futex != -1);
}

bool
ksynch_free_var(volatile int *futex)
{
    /* nothing to be done */
    return true;
}

/* Waits on the futex until woken if the kernel supports SYS_futex syscall
 * and the futex's value has not been changed from mustbe. Does not block
 * if the kernel doesn't support SYS_futex. If timeout_ms is 0, there is no timeout;
 * else returns a negative value on a timeout.  Returns 0 if woken by another thread,
 * and negative value for all other cases.
 */
ptr_int_t
ksynch_wait(volatile int *futex, int mustbe, int timeout_ms)
{
    ptr_int_t res;
    ASSERT(ALIGNED(futex, sizeof(int)));
    if (kernel_futex_support) {
        /* XXX: Having debug timeout like win32 os_wait_event() would be useful */
        struct timespec timeout;
        timeout.tv_sec = (timeout_ms / 1000);
        timeout.tv_nsec = ((int64)timeout_ms % 1000) * 1000000;
        res = dynamorio_syscall(SYS_futex, 6, futex, FUTEX_WAIT, mustbe,
                                timeout_ms > 0 ? &timeout : NULL, NULL, 0);
    } else {
        res = -1;
    }
    return res;
}

/* Wakes up at most one thread waiting on the futex if the kernel supports
 * SYS_futex syscall. Does nothing if the kernel doesn't support SYS_futex.
 */
ptr_int_t
ksynch_wake(volatile int *futex)
{
    ptr_int_t res;
    ASSERT(ALIGNED(futex, sizeof(int)));
    if (kernel_futex_support) {
        res = dynamorio_syscall(SYS_futex, 6, futex, FUTEX_WAKE, 1, NULL, NULL, 0);
    } else {
        res = -1;
    }
    return res;
}

/* Wakes up all the threads waiting on the futex if the kernel supports
 * SYS_futex syscall. Does nothing if the kernel doesn't support SYS_futex.
 */
ptr_int_t
ksynch_wake_all(volatile int *futex)
{
    ptr_int_t res;
    ASSERT(ALIGNED(futex, sizeof(int)));
    if (kernel_futex_support) {
        do {
            res = dynamorio_syscall(SYS_futex, 6, futex, FUTEX_WAKE, INT_MAX, NULL, NULL,
                                    0);
        } while (res == INT_MAX);
        res = 0;
    } else {
        res = -1;
    }
    return res;
}

volatile int *
mutex_get_contended_event(mutex_t *lock)
{
    if (!ksynch_var_initialized(&lock->contended_event)) {
        /* We just don't want to clobber an in-use event */
        atomic_compare_exchange_int((int *)&lock->contended_event, -1, (int)0);
    }
    return &lock->contended_event;
}

void
mutex_free_contended_event(mutex_t *lock)
{
    /* Nothing to do */
}
