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

/*
 * ksynch.h - synchronization via the kernel
 */

#ifndef _KSYNCH_H_
#define _KSYNCH_H_ 1

/* These are simply kernel wait queue variables, suitable for use in
 * signal handlers where we can't use event_t.  They take the values 0
 * or 1.  They should only be used in a manner such that replacing
 * ksynch_{wait,wake,wake_all} with nops will still result in correct
 * (if less performant) behavior.  Wrapping a mutex around them is up
 * to the caller.
 */

/* We have to expose KSYNCH_TYPE for inlining into mutex_t, so we declare
 * it in utils.h.
 */

void
ksynch_init(void);

void
ksynch_exit(void);

bool
ksynch_kernel_support(void);

bool
ksynch_init_var(KSYNCH_TYPE *var);

bool
ksynch_free_var(KSYNCH_TYPE *var);

#ifdef LINUX
/* avoid de-ref */
static inline int
ksynch_get_value(volatile int *futex)
{
    return *futex;
}
static inline void
ksynch_set_value(volatile int *futex, int new_val)
{
    *futex = new_val;
}
#else
ptr_int_t
ksynch_get_value(mac_synch_t *synch);

void
ksynch_set_value(mac_synch_t *synch, int new_val);
#endif

KSYNCH_TYPE *
mutex_get_contended_event(mutex_t *lock);

/* These return 0 on success and a negative value on failure. ksynch_wait
 * returns -ETIMEDOUT if there was a timeout condition.
 */

ptr_int_t
ksynch_wait(KSYNCH_TYPE *var, int mustbe, int timeout_ms);

ptr_int_t
ksynch_wake(KSYNCH_TYPE *var);

ptr_int_t
ksynch_wake_all(KSYNCH_TYPE *var);

#endif /* _KSYNCH_H_ */
