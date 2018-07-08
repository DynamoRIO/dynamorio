/* *******************************************************************************
 * Copyright (c) 2013-2017 Google, Inc.  All rights reserved.
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

/*
 * ksynch_macos.c - synchronization via the kernel
 *
 * FIXME i#58: NYI (see comments below as well):
 * + Haven't really tested this yet
 * + Longer-term i#1291: use raw syscalls instead of libSystem wrappers.
 *   Some of these are basically just Mach syscall wrappers, but others like
 *   semaphore_create() are slightly more involved.
 */

#include "../globals.h"
#include <errno.h>
/* avoid problems with use of errno as var name in rest of file */
#if !defined(STANDALONE_UNIT_TEST) && !defined(MACOS)
#    undef errno
#endif

#include "ksynch.h"

#include <mach/mach.h>
#include <mach/task.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>

/* Only in kernel headers, not in user headers */
#define SYNC_POLICY_PREPOST 0x4

#ifndef MACOS
#    error Mac-only
#endif

void
ksynch_init(void)
{
}

void
ksynch_exit(void)
{
}

bool
ksynch_kernel_support(void)
{
    return true;
}

bool
ksynch_init_var(mac_synch_t *synch)
{
    /* We use SYNC_POLICY_PREPOST so that a signal sent is not lost if there's
     * no thread waiting at that precise point.
     */
    kern_return_t res =
        semaphore_create(mach_task_self(), &synch->sem, SYNC_POLICY_PREPOST, 0);
    ASSERT(synch->sem != 0); /* we assume 0 is never a legitimate value */
    synch->value = 0;
    LOG(THREAD_GET, LOG_THREADS, 2, "semaphore %d created, status %d\n", synch->sem, res);
    return (res == KERN_SUCCESS);
}

bool
ksynch_var_initialized(mac_synch_t *synch)
{
    /* semaphore_t is a mach_port_t which is a natural_t == unsigned */
    return (synch->sem != 0);
}

bool
ksynch_free_var(mac_synch_t *synch)
{
    kern_return_t res = semaphore_destroy(mach_task_self(), synch->sem);
    synch->sem = 0;
    return (res == KERN_SUCCESS);
}

ptr_int_t
ksynch_get_value(mac_synch_t *synch)
{
    return synch->value;
}

void
ksynch_set_value(mac_synch_t *synch, int new_val)
{
    synch->value = new_val;
}

ptr_int_t
ksynch_wait(mac_synch_t *synch, int mustbe, int timeout_ms)
{
    /* We don't need to bother with "mustbe" b/c of SYNC_POLICY_PREPOST */
    kern_return_t res;
    if (timeout_ms > 0) {
        mach_timespec_t timeout;
        timeout.tv_sec = (timeout_ms / 1000);
        timeout.tv_nsec = ((int64)timeout_ms % 1000) * 1000000;
        res = semaphore_timedwait(synch->sem, timeout);
    } else
        res = semaphore_wait(synch->sem);

    /* Conform to the API specified in ksynch.h */
    switch (res) {
    case KERN_SUCCESS: return 0;
    case KERN_OPERATION_TIMED_OUT: return -ETIMEDOUT;
    default: return -1;
    }
}

ptr_int_t
ksynch_wake(mac_synch_t *synch)
{
    kern_return_t res = semaphore_signal(synch->sem);
    return (res == KERN_SUCCESS ? 0 : -1);
}

ptr_int_t
ksynch_wake_all(mac_synch_t *synch)
{
    kern_return_t res = semaphore_signal_all(synch->sem);
    return (res == KERN_SUCCESS ? 0 : -1);
}

mac_synch_t *
mutex_get_contended_event(mutex_t *lock)
{
    if (!ksynch_var_initialized(&lock->contended_event)) {
        mac_synch_t local;
        bool not_yet_created;
        if (!ksynch_init_var(&local)) {
            ASSERT_NOT_REACHED();
            return NULL;
        }
        not_yet_created = atomic_compare_exchange_int((int *)&lock->contended_event.sem,
                                                      0, (int)local.sem);
        if (not_yet_created) {
            /* we're the ones to initialize it */
        } else {
            /* already created */
            ksynch_free_var(&local);
        }
    }
    return &lock->contended_event;
}

void
mutex_free_contended_event(mutex_t *lock)
{
    ksynch_free_var(&lock->contended_event);
    lock->contended_event.sem = 0;
}
