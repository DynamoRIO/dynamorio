/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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
 * thread.h - thread synchronization
 */

#ifndef SYNCH_H
#define SYNCH_H

#include "globals.h"

/*for synch */
/* Given permissions */
/* The order is in increasing permissiveness and the values are chosen to
 * match up with equivalent requested states below */
typedef enum {
    THREAD_SYNCH_NONE = 0,

    /* At consistent state, holding no locks, suitable for terminate,
     * suitable for suspending */
    THREAD_SYNCH_NO_LOCKS = 3,

    /* At consistent state, holding no locks, suitable for terminate,
     * suitable for suspending, but cannot be transferred elsewhere:
     * must be resumed at suspend point in order to finish an
     * in-progress task (such as flushing or hot patch updating).
     * Xref case 6821.
     */
    THREAD_SYNCH_NO_LOCKS_NO_XFER = 4,

    /* At consistent state, holding no locks, with valid mcontext
     * (including app_errno), suitable for suspending.  But, not
     * suitable for transferring elsewhere in DR -- ok to transfer
     * if going native, though.
     */
    THREAD_SYNCH_VALID_MCONTEXT_NO_XFER = 5,

    /* At consistent state, holding no locks, with valid mcontext, suitable
     *  for suspending, note that valid mcontext includes app_errno */
    THREAD_SYNCH_VALID_MCONTEXT = 6
} thread_synch_permission_t;

/* Requested states */
/* clean means that the supporting dynamorio data structures are cleaned up */
/* The order is in increasingly strong requests and the values are chosen to
 * match up with equivalent given permissions above */
typedef enum {
    THREAD_SYNCH_SUSPENDED = 1,
    THREAD_SYNCH_SUSPENDED_AND_CLEANED = 2,
    /* xref case 8747, may be best to avoid TERMINATED_AND_CLEANED where possible */
    THREAD_SYNCH_TERMINATED_AND_CLEANED = 3,

    /* A target thread that has THREAD_SYNCH_NO_LOCKS_NO_XFER is acceptable.
     * Xref case 6821.
     */
    THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER = 4,

    THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT = 5
} thread_synch_state_t;

typedef enum {
    THREAD_SYNCH_RESULT_SUCCESS = 1,
    THREAD_SYNCH_RESULT_SUSPEND_FAILURE = 2,
    THREAD_SYNCH_RESULT_NOT_SAFE = 3,
} thread_synch_result_t;

/* option bits that affect both synch_with_all_threads() and synch_with_thread(),
 * though the two routines interpret some options in different ways
 */
enum {
    /* these 3 are mutually exclusive and apply to failures to suspend or
     * to get the context of a target thread.  the first two apply to
     * both synch_with_thread() and all_threads() while the last only
     * applies to all_threads().
     */
    THREAD_SYNCH_SUSPEND_FAILURE_ABORT = 0x00000001,
    THREAD_SYNCH_SUSPEND_FAILURE_IGNORE = 0x00000002,
    /* retry will cause synch_with_all_threads to keep trying until hit
     * the loop max, so may want to combine w/ SMALL_LOOP_MAX.
     * retry does not apply to synch_with_thread(), only to all_threads().
     */
    THREAD_SYNCH_SUSPEND_FAILURE_RETRY = 0x00000004,

    /* specifies a much smaller loop max */
    THREAD_SYNCH_SMALL_LOOP_MAX = 0x00000008,

    /* specifies whether we should terminate client threads */
    THREAD_SYNCH_SKIP_CLIENT_THREAD = 0x00000010,
};

/* convenience macros */
#define THREAD_SYNCH_IS_CLEANED(desired_synch_state)              \
    (desired_synch_state == THREAD_SYNCH_SUSPENDED_AND_CLEANED || \
     desired_synch_state == THREAD_SYNCH_TERMINATED_AND_CLEANED)
#define THREAD_SYNCH_IS_TERMINATED(desired_synch_state) \
    (desired_synch_state == THREAD_SYNCH_TERMINATED_AND_CLEANED)
/* desired_perm can be a thread_synch_state_t.  The enums are designed to be
 * comparable. */
#define THREAD_SYNCH_SAFE(synch_perm, desired_perm) \
    (synch_perm >= (thread_synch_permission_t)desired_perm)

void
synch_init(void);

void
synch_exit(void);

void
synch_thread_init(dcontext_t *dcontext);

void
synch_thread_exit(dcontext_t *dcontext);

bool
thread_synch_state_no_xfer(dcontext_t *dcontext);

/* We support calling this from a signal handler that might have interrupted DR
 * holding arbitrary locks.
 */
bool
thread_synch_check_state(dcontext_t *dcontext, thread_synch_permission_t desired_perm);

/* Only valid while holding all_threads_synch_lock and thread_initexit_lock.  Set to
 * whether synch_with_thread was successful.
 * Cannot be called when THREAD_SYNCH_*_AND_CLEANED was requested as the
 * thread-local memory will be freed on success!
 */
bool
thread_synch_successful(thread_record_t *tr);

/* returns a thread_synch_result_t value
 * id - the thread you want to synch with
 * block - whether or not should spin until synch is successful
 * hold_initexit_lock - whether or not the caller holds the thread_initexit_lock
 * caller_state - a given permission define from above that describes the
 *                current state of the caller (note that holding the initexit
 *                lock is ok with respect to NO_LOCK
 * desired_state - a requested state define from above that describes the
 *                 desired synchronization
 * flags - options from THREAD_SYNCH_ bitmask values
 * NOTE - if you hold the initexit_lock and block with greater than NONE for
 * caller state, then initexit_lock may be released and reaquired
 * NOTE - if any of the nt_ routines fails, it is assumed the thread no longer
 * exists and returns true
 * NOTE - if called directly (i.e. not through synch_with_all_threads)
 * requires THREAD_SYNCH_IS_SAFE(caller_state, desired_state) to avoid deadlock
 * NOTE - Requires the caller is !could_be_linking (i.e. not in an
 * enter_couldbelinking state)
 * NOTE - you can't call this with a thread that you've already suspended
 */
thread_synch_result_t
synch_with_thread(thread_id_t id, bool block, bool hold_initexit_lock,
                  thread_synch_permission_t cur_state, thread_synch_state_t desired_state,
                  uint flags);

/* held on return from synch_with_all_threads together with thread_initexit_lock*/
extern mutex_t all_threads_synch_lock;

/* desired_synch_state - a requested state define from above that describes
 *                        the synchronization required
 * threads, num_threads - must not be NULL, if !THREAD_SYNCH_IS_CLEANED(desired
 *                        synch_state) then will hold a list and num of threads
 * cur_state - a given permission from above that describes the state of the
 *             caller
 * flags - options from THREAD_SYNCH_ bitmask values
 * NOTE - Requires that the caller doesn't hold the thread_initexit_lock, on
 * return caller will hold the thread_initexit_lock
 * NOTE - Requires the caller is !could_be_linking (i.e. not in an
 * enter_couldbelinking state)
 * NOTE - To avoid deadlock this routine should really only be called with
 * cur_state giving maximum permissions, (currently app_exit and detach could
 * conflict, except our routes to app_exit go through different synch point
 * (TermThread or TermProcess) first
 * Caller must call end_synch_with_all_threads to clean up, unless they pass
 * THREAD_SYNCH_SUSPEND_FAILURE_ABORT and the request fails.
 * Note - if the caller doesn't intend to resume all
 * threads (say detach or process exit) then it should first call
 * instrument_client_thread_termination() to inform the client that client-owned threads
 * will be killed/permanently stopped.
 */
bool
synch_with_all_threads(thread_synch_state_t desired_synch_state,
                       thread_record_t ***threads, int *num_threads,
                       thread_synch_permission_t cur_state, uint flags);
void
end_synch_with_all_threads(thread_record_t **threads, uint num_threads, bool resume);
void
resume_all_threads(thread_record_t **threads, const uint num_threads);

/* a fast way to tell a thread if it should call check_wait_at_safe_spot
 * if translating context would be expensive */
bool
should_wait_at_safe_spot(dcontext_t *dcontext);

/* checks to see if any threads are waiting to synch with this one and waits
 * if they are
 * cur_state - a given permission define from above that describes the current
 *             state of the caller
 * NOTE - Requires the caller is !could_be_linking (i.e. not in an
 * enter_couldbelinking state)
 */
void
check_wait_at_safe_spot(dcontext_t *dcontext, thread_synch_permission_t cur_state);

/* use with care!  normally check_wait_at_safe_spot() should be called instead */
void
set_synch_state(dcontext_t *dcontext, thread_synch_permission_t state);

bool
at_safe_spot(thread_record_t *trec, priv_mcontext_t *mc,
             thread_synch_state_t desired_state);

bool
set_synched_thread_context(thread_record_t *trec,
                           /* pass either mc or both cxt and cxt_size */
                           priv_mcontext_t *mc, void *cxt, size_t cxt_size,
                           thread_synch_state_t desired_state _IF_X64(byte *cxt_alloc)
                               _IF_WINDOWS(NTSTATUS *status /*OUT*/));

bool
translate_mcontext(thread_record_t *trec, priv_mcontext_t *mc, bool restore_memory,
                   fragment_t *f);

/* resets a thread to start interpreting anew */
void
translate_from_synchall_to_dispatch(thread_record_t *tr,
                                    thread_synch_state_t synch_state);

void
send_all_other_threads_native(void);

void
detach_on_permanent_stack(bool internal, bool do_cleanup, dr_stats_t *drstats);

/*** exported for detach only ***/

bool
is_at_do_syscall(dcontext_t *dcontext, app_pc pc, byte *esp);

/* adjusts the pending synch count */
void
adjust_wait_at_safe_spot(dcontext_t *dcontext, int amt);

#endif /* SYNCH_H */
