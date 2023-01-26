/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2017 ARM Limited. All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * utils.c - miscellaneous utilities
 */

#include "globals.h"
#include "configure_defines.h"
#include "utils.h"
#include "module_shared.h"
#include <math.h>

#ifdef PROCESS_CONTROL
#    include "moduledb.h" /* for process control macros */
#endif

#ifdef UNIX
#    include <sys/types.h>
#    include <sys/stat.h>
#    include <fcntl.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <unistd.h>
#    include <errno.h>
#else
#    include <errno.h>
/* FIXME : remove when syslog macros fixed */
#    include "events.h"
#endif

#ifdef SHARING_STUDY
#    include "fragment.h" /* print_shared_stats */
#endif
#ifdef DEBUG
#    include "fcache.h"
#    include "synch.h" /* all_threads_synch_lock */
#endif

#include <stdarg.h> /* for varargs */
#include <stddef.h> /* for offsetof */

try_except_t global_try_except;
#ifdef UNIX
thread_id_t global_try_tid = INVALID_THREAD_ID;
#endif

int do_once_generation = 1;

#ifdef SIDELINE
extern void
sideline_exit(void);
#endif

/* use for soft errors that can handle some cleanup: assertions and apichecks
 * performs some cleanup and then calls os_terminate
 */
static void
soft_terminate()
{
#ifdef SIDELINE
    /* kill child threads */
    if (dynamo_options.sideline) {
        sideline_stop();
        sideline_exit();
    }
#endif
    /* set exited status for shared memory watchers, other threads */
    DOSTATS({
        if (d_r_stats != NULL)
            GLOBAL_STAT(exited) = true;
    });

    /* do not try to clean up */
    os_terminate(NULL, TERMINATE_PROCESS);
}

#if defined(INTERNAL) || defined(DEBUG)
/* checks whether an assert statement should be ignored,
 * produces a warning if so and returns true,
 * otherwise returns false
 */
bool
ignore_assert(const char *assert_stmt, const char *expr)
{
    bool ignore = false;
    if (!IS_STRING_OPTION_EMPTY(ignore_assert_list)) {
        string_option_read_lock();
        ignore = check_filter(DYNAMO_OPTION(ignore_assert_list), assert_stmt);
        string_option_read_unlock();
    }
    if (IS_LISTSTRING_OPTION_FORALL(ignore_assert_list)) {
        ignore = true;
    }
    if (ignore) {
        /* FIXME: could have passed message around */
        SYSLOG_INTERNAL_WARNING("Ignoring assert %s %s", assert_stmt, expr);
    }
    return ignore;
}

/* Hand-made DO_ONCE used in d_r_internal_error b/c ifdefs in
 * report_dynamorio_problem prevent DO_ONCE itself
 */
DECLARE_FREQPROT_VAR(static bool do_once_internal_error, false);

/* abort on internal dynamo error */
void
d_r_internal_error(const char *file, int line, const char *expr)
{
    /* note that we no longer obfuscate filenames in non-internal builds
     * xref PR 303817 */

    /* need to produce a SYSLOG Ignore Error here and return right away */
    /* to avoid adding another ?: in the ASSERT messsage
     * we'll reconstruct file and line # here
     */
    if (!IS_STRING_OPTION_EMPTY(ignore_assert_list)) {
        char assert_stmt[MAXIMUM_PATH]; /* max unique identifier */
        /* note the ignore checks are done with a possible recursive
         * infinte loop if any asserts fail.  Not very safe to set and
         * unset a static bool either since we'll be noisy.
         */

        /* Assert identifiers should be an exact match of message
         * after Internal Error.  Most common look like 'arch/arch.c:142',
         * but could also look like 'Not implemented @arch/arch.c:142'
         * or 'Bug #4809 @arch/arch.c:145;Ignore message
         * @arch/arch.c:146'
         */
        snprintf(assert_stmt, BUFFER_SIZE_ELEMENTS(assert_stmt), "%s:%d", file, line);
        NULL_TERMINATE_BUFFER(assert_stmt);
        ASSERT_CURIOSITY((strlen(assert_stmt) + 1) != BUFFER_SIZE_ELEMENTS(assert_stmt));
        if (ignore_assert(assert_stmt, expr))
            return;
        /* we can ignore multiple asserts without triggering the do_once */
    }

    if (do_once_internal_error) /* recursing, bail out */
        return;
    else
        do_once_internal_error = true;

    report_dynamorio_problem(NULL, DUMPCORE_ASSERTION, NULL, NULL,
                             PRODUCT_NAME " debug check failure: %s:%d %s"
#    if defined(DEBUG) && defined(INTERNAL)
                                          "\n(Error occurred @%d frags in tid " TIDFMT ")"
#    endif
                             ,
                             file, line, expr
#    if defined(DEBUG) && defined(INTERNAL)
                             ,
                             d_r_stats == NULL ? -1 : GLOBAL_STAT(num_fragments),
                             IF_UNIX_ELSE(get_sys_thread_id(), d_r_get_thread_id())
#    endif
    );

    soft_terminate();
}
#endif /* defined(INTERNAL) || defined(DEBUG) */

/* abort on external application created error, i.e. apicheck */
void
external_error(const char *file, int line, const char *msg)
{
    DO_ONCE({
        /* this syslog is before any core dump, unlike our other reports, but
         * not worth fixing
         */
        SYSLOG(SYSLOG_ERROR, EXTERNAL_ERROR, 4, get_application_name(),
               get_application_pid(), PRODUCT_NAME, msg);
        report_dynamorio_problem(NULL, DUMPCORE_FATAL_USAGE_ERROR, NULL, NULL,
                                 "Usage error: %s (%s, line %d)", msg, file, line);
    });
    soft_terminate();
}

/****************************************************************************/
/* SYNCHRONIZATION */

#ifdef DEADLOCK_AVOIDANCE
/* Keeps the head of a linked list of all mutexes currently
   held by a thread.  We also require a LIFO lock unlock order
   to keep things simpler (and stricter).
*/
struct _thread_locks_t {
    mutex_t *last_lock;
};

/* These two locks are never deleted, although innermost_lock is grabbed */
DECLARE_CXTSWPROT_VAR(mutex_t outermost_lock, INIT_LOCK_FREE(outermost_lock));
DECLARE_CXTSWPROT_VAR(mutex_t innermost_lock, INIT_LOCK_FREE(innermost_lock));

/* Case 8075: For selfprot we have no way to put a local-scope mutex into
 * .cspdata, and {add,remove}_process_lock need to write to the
 * do_threshold_mutex when managing adjacent entries in the lock list, so
 * we use a global lock instead.  Could be a contention issue but it's only
 * DEADLOCK_AVOIDANCE builds and there are few uses of DO_THRESHOLD_SAFE.
 */
DECLARE_CXTSWPROT_VAR(mutex_t do_threshold_mutex, INIT_LOCK_FREE(do_threshold_mutex));

/* structure field dumper for both name and value, format with %*s%d */
#    define DUMP_NONZERO(v, field) \
        strlen(#field) + 1, (v->field ? #field "=" : ""), v->field
#    ifdef MACOS
#        define DUMP_CONTENDED(v, field)                                               \
            strlen(#field) + 1, (ksynch_var_initialized(&v->field) ? #field "=" : ""), \
                v->field.sem
#    else
#        define DUMP_CONTENDED DUMP_NONZERO
#    endif

/* common format string used for different log files and loglevels */
#    define DUMP_LOCK_INFO_ARGS(depth, cur_lock, prev)                                  \
        "%d lock " PFX ": name=%s\nrank=%d owner=" TIDFMT " owning_dc=" PFX " "         \
        "%*s" PIFX " prev=" PFX "\n"                                                    \
        "lock %*s%8d %*s%8d %*s%8d %*s%8d %*s%8d+2 %s\n",                               \
            depth, cur_lock, cur_lock->name, cur_lock->rank, cur_lock->owner,           \
            cur_lock->owning_dcontext, DUMP_CONTENDED(cur_lock, contended_event), prev, \
            DUMP_NONZERO(cur_lock, count_times_acquired),                               \
            DUMP_NONZERO(cur_lock, count_times_contended),                              \
            DUMP_NONZERO(cur_lock, count_times_spin_pause),                             \
            DUMP_NONZERO(cur_lock, count_times_spin_only),                              \
            DUMP_NONZERO(cur_lock, max_contended_requests), cur_lock->name

#    ifdef INTERNAL
static void
dump_mutex_callstack(mutex_t *lock)
{
    /* from windbg proper command is
     *  0:001> dds @@(&lock->callstack) L4
     */
#        ifdef MUTEX_CALLSTACK
    uint i;
    if (INTERNAL_OPTION(mutex_callstack) == 0)
        return;
    LOG(GLOBAL, LOG_THREADS, 1, "dump_mutex_callstack %s\n", lock->name);
    for (i = 0; i < INTERNAL_OPTION(mutex_callstack); i++) {
        /* some macro's call this function, so it is easier to ifdef
         * only references to callstack */
        LOG(GLOBAL, LOG_THREADS, 1, "  " PFX "\n", lock->callstack[i]);
    }
#        endif /* MUTEX_CALLSTACK */
}
#    endif

void
dump_owned_locks(dcontext_t *dcontext)
{ /* LIFO order even though order in releasing doesn't matter */
    mutex_t *cur_lock;
    uint depth = 0;
    cur_lock = dcontext->thread_owned_locks->last_lock;
    LOG(THREAD, LOG_THREADS, 1, "Owned locks for thread " TIDFMT " dcontext=" PFX "\n",
        dcontext->owning_thread, dcontext);
    while (cur_lock != &outermost_lock) {
        depth++;
        LOG(THREAD, LOG_THREADS, 1,
            DUMP_LOCK_INFO_ARGS(depth, cur_lock, cur_lock->prev_owned_lock));
        ASSERT(cur_lock->owner == dcontext->owning_thread);
        cur_lock = cur_lock->prev_owned_lock;
    }
}

bool
thread_owns_no_locks(dcontext_t *dcontext)
{
    ASSERT(dcontext != NULL);
    if (!INTERNAL_OPTION(deadlock_avoidance))
        return true; /* can't verify since we aren't keeping track of owned locks */
    return (dcontext->thread_owned_locks->last_lock == &outermost_lock);
}

bool
thread_owns_one_lock(dcontext_t *dcontext, mutex_t *lock)
{
    mutex_t *cur_lock;
    ASSERT(dcontext != NULL);
    if (!INTERNAL_OPTION(deadlock_avoidance))
        return true; /* can't verify since we aren't keeping track of owned locks */
    cur_lock = dcontext->thread_owned_locks->last_lock;
    return (cur_lock == lock && cur_lock->prev_owned_lock == &outermost_lock);
}

/* Returns true if dcontext thread owns lock1 and lock2 and no other locks */
bool
thread_owns_two_locks(dcontext_t *dcontext, mutex_t *lock1, mutex_t *lock2)
{
    mutex_t *cur_lock;
    ASSERT(dcontext != NULL);
    if (!INTERNAL_OPTION(deadlock_avoidance))
        return true; /* can't verify since we aren't keeping track of owned locks */
    cur_lock = dcontext->thread_owned_locks->last_lock;
    return (cur_lock == lock1 && cur_lock->prev_owned_lock == lock2 &&
            lock2->prev_owned_lock == &outermost_lock);
}

/* Returns true if dcontext thread owns lock1 and optionally lock2
 * (acquired before lock1) and no other locks. */
bool
thread_owns_first_or_both_locks_only(dcontext_t *dcontext, mutex_t *lock1, mutex_t *lock2)
{
    mutex_t *cur_lock;
    ASSERT(dcontext != NULL);
    if (!INTERNAL_OPTION(deadlock_avoidance))
        return true; /* can't verify since we aren't keeping track of owned locks */
    cur_lock = dcontext->thread_owned_locks->last_lock;
    return (cur_lock == lock1 &&
            (cur_lock->prev_owned_lock == &outermost_lock ||
             (cur_lock->prev_owned_lock == lock2 &&
              lock2->prev_owned_lock == &outermost_lock)));
}

/* dump process locks that have been acquired at least once */
/* FIXME: since most mutexes are global we don't have thread private lock lists */
void
dump_process_locks()
{
    mutex_t *cur_lock;
    uint depth = 0;
    uint total_acquired = 0;
    uint total_contended = 0;

    LOG(GLOBAL, LOG_STATS, 2, "Currently live process locks:\n");
    /* global list access needs to be synchronized */
    d_r_mutex_lock(&innermost_lock);
    cur_lock = &innermost_lock;
    do {
        depth++;
        LOG(GLOBAL, LOG_STATS,
            (cur_lock->count_times_contended ? 1U : 2U), /* elevate contended ones */
            DUMP_LOCK_INFO_ARGS(depth, cur_lock, cur_lock->next_process_lock));
        DOLOG((cur_lock->count_times_contended ? 2U : 3U), /* elevate contended ones */
              LOG_THREADS, {
                  /* last recorded callstack, not necessarily the most contended path */
                  dump_mutex_callstack(cur_lock);
              });
        cur_lock = cur_lock->next_process_lock;
        total_acquired += cur_lock->count_times_acquired;
        total_contended += cur_lock->count_times_contended;
        ASSERT(cur_lock);
        ASSERT(cur_lock->next_process_lock->prev_process_lock == cur_lock);
        ASSERT(cur_lock->prev_process_lock->next_process_lock == cur_lock);
        ASSERT(cur_lock->prev_process_lock != cur_lock || cur_lock == &innermost_lock);
        ASSERT(cur_lock->next_process_lock != cur_lock || cur_lock == &innermost_lock);
    } while (cur_lock != &innermost_lock);
    d_r_mutex_unlock(&innermost_lock);
    LOG(GLOBAL, LOG_STATS, 1,
        "Currently live process locks: %d, acquired %d, contended %d (current only)\n",
        depth, total_acquired, total_contended);
}

uint
locks_not_closed()
{
    mutex_t *cur_lock;
    uint forgotten = 0;
    uint ignored = 0;
    /* Case 8075: we use a global do_threshold_mutex for DEADLOCK_AVOIDANCE.
     * Leaving the code for a local var here via this bool in case we run
     * this routine in release build while somehow avoiding the global lock.
     */
    static const bool allow_do_threshold_leaks = false;

    /* we assume that we would have removed them from the process list in mutex_close */

    /* locks assigned with do_threshold_mutex are 'leaked' because it
     * is too much hassle to find a good place to DELETE them -- though we
     * now use a global mutex for DEADLOCK_AVOIDANCE so that's not the case.
     */
    d_r_mutex_lock(&innermost_lock);
    /* innermost will stay */
    cur_lock = innermost_lock.next_process_lock;
    while (cur_lock != &innermost_lock) {
        if (allow_do_threshold_leaks && cur_lock->rank == LOCK_RANK(do_threshold_mutex)) {
            ignored++;
        } else if (cur_lock->deleted &&
                   (IF_WINDOWS(cur_lock->rank == LOCK_RANK(debugbox_lock) ||
                               cur_lock->rank == LOCK_RANK(dump_core_lock) ||)
                            cur_lock->rank == LOCK_RANK(report_buf_lock) ||
                    cur_lock->rank == LOCK_RANK(datasec_selfprot_lock) ||
                    cur_lock->rank == LOCK_RANK(logdir_mutex) ||
                    cur_lock->rank ==
                        LOCK_RANK(options_lock)
                        /* This lock can be used parallel to detach cleanup. */
                        IF_UNIX(|| cur_lock->rank == LOCK_RANK(detached_sigact_lock)))) {
            /* i#1058: curiosities during exit re-acquire these locks. */
            ignored++;
        } else {
            LOG(GLOBAL, LOG_STATS, 1, "missing DELETE_LOCK on lock " PFX " %s\n",
                cur_lock, cur_lock->name);
            forgotten++;
        }
        cur_lock = cur_lock->next_process_lock;
    }
    d_r_mutex_unlock(&innermost_lock);
    LOG(GLOBAL, LOG_STATS, 3, "locks_not_closed= %d remaining, %d ignored\n", forgotten,
        ignored);
    return forgotten;
}

void
locks_thread_init(dcontext_t *dcontext)
{
    thread_locks_t *new_thread_locks;
    new_thread_locks = (thread_locks_t *)UNPROTECTED_GLOBAL_ALLOC(
        sizeof(thread_locks_t) HEAPACCT(ACCT_OTHER));
    LOG(THREAD, LOG_STATS, 2, "thread_locks=" PFX " size=%d\n", new_thread_locks,
        sizeof(thread_locks_t));
    /* initialize any thread bookkeeping fields before assigning to dcontext */
    new_thread_locks->last_lock = &outermost_lock;
    dcontext->thread_owned_locks = new_thread_locks;
}

void
locks_thread_exit(dcontext_t *dcontext)
{
    /* using global heap and always have to clean up */
    if (dcontext->thread_owned_locks) {
        thread_locks_t *old_thread_locks = dcontext->thread_owned_locks;
        /* when exiting, another thread may be holding the lock instead of the current,
           CHECK: is this true for detaching */
        ASSERT(dcontext->thread_owned_locks->last_lock == &thread_initexit_lock ||
               dcontext->thread_owned_locks->last_lock == &outermost_lock
               /* PR 546016: sideline client thread might hold client lock */
               || dcontext->thread_owned_locks->last_lock->rank == dr_client_mutex_rank);
        /* disable thread lock checks before freeing memory */
        dcontext->thread_owned_locks = NULL;
        UNPROTECTED_GLOBAL_FREE(old_thread_locks,
                                sizeof(thread_locks_t) HEAPACCT(ACCT_OTHER));
    }
}

static void
add_process_lock(mutex_t *lock)
{
    /* add to global locks circular double linked list */
    d_r_mutex_lock(&innermost_lock);
    if (lock->prev_process_lock != NULL) {
        /* race: someone already added (can only happen for read locks) */
        LOG(THREAD_GET, LOG_THREADS, 2, "\talready added\n");
        ASSERT(lock->next_process_lock != NULL);
        d_r_mutex_unlock(&innermost_lock);
        return;
    }
    LOG(THREAD_GET, LOG_THREADS, 2,
        "add_process_lock: " DUMP_LOCK_INFO_ARGS(0, lock, lock->prev_process_lock));
    ASSERT(lock->next_process_lock == NULL || lock == &innermost_lock);
    ASSERT(lock->prev_process_lock == NULL || lock == &innermost_lock);
    if (innermost_lock.prev_process_lock == NULL) {
        innermost_lock.next_process_lock = &innermost_lock;
        innermost_lock.prev_process_lock = &innermost_lock;
    }
    lock->next_process_lock = &innermost_lock;
    innermost_lock.prev_process_lock->next_process_lock = lock;
    lock->prev_process_lock = innermost_lock.prev_process_lock;
    innermost_lock.prev_process_lock = lock;
    ASSERT(lock->next_process_lock->prev_process_lock == lock);
    ASSERT(lock->prev_process_lock->next_process_lock == lock);
    ASSERT(lock->prev_process_lock != lock || lock == &innermost_lock);
    ASSERT(lock->next_process_lock != lock || lock == &innermost_lock);
    d_r_mutex_unlock(&innermost_lock);
}

static void
remove_process_lock(mutex_t *lock)
{
    LOG(THREAD_GET, LOG_THREADS, 3,
        "remove_process_lock " DUMP_LOCK_INFO_ARGS(0, lock, lock->prev_process_lock));
    STATS_ADD(total_acquired, lock->count_times_acquired);
    STATS_ADD(total_contended, lock->count_times_contended);
    if (lock->count_times_acquired == 0) {
        ASSERT(lock->prev_process_lock == NULL);
        LOG(THREAD_GET, LOG_THREADS, 3, "\tnever acquired\n");
        return;
    }
    ASSERT(lock->prev_process_lock && "if ever acquired should be on the list");
    ASSERT(lock != &innermost_lock && "innermost will be 'leaked'");
    /* remove from global locks list */
    d_r_mutex_lock(&innermost_lock);
    /* innermost should always have both fields set here */
    lock->next_process_lock->prev_process_lock = lock->prev_process_lock;
    lock->prev_process_lock->next_process_lock = lock->next_process_lock;
    lock->next_process_lock = NULL;
    lock->prev_process_lock = NULL; /* so we catch uses after closing */
    d_r_mutex_unlock(&innermost_lock);
}

#    ifdef MUTEX_CALLSTACK
/* FIXME: generalize and merge w/ CALL_PROFILE? */
static void
mutex_collect_callstack(mutex_t *lock)
{
    uint max_depth = INTERNAL_OPTION(mutex_callstack);
    uint depth = 0;
    uint skip = 2; /* ignore calls from deadlock_avoidance() and d_r_mutex_lock() */
    /* FIXME: write_lock could ignore one level further */
    byte *fp;
    dcontext_t *dcontext = get_thread_private_dcontext();

    GET_FRAME_PTR(fp);

    /* only interested in DR addresses which should all be readable */
    while (depth < max_depth && (is_on_initstack(fp) || is_on_dstack(dcontext, fp)) &&
           /* is_on_initstack() and is_on_dstack() do include the guard pages
            * yet we cannot afford to call is_readable_without_exception()
            */
           !is_stack_overflow(dcontext, fp)) {
        app_pc our_ret = *((app_pc *)fp + 1);
        fp = *(byte **)fp;
        if (skip) {
            skip--;
            continue;
        }
        lock->callstack[depth] = our_ret;
        depth++;
    }
}
#    endif /* MUTEX_CALLSTACK */

enum { LOCK_NOT_OWNABLE, LOCK_OWNABLE };

/* if not acquired only update statistics,
   if not ownable (i.e. read lock) only check against previous locks,
   but don't add to thread owned list
 */
static void
deadlock_avoidance_lock(mutex_t *lock, bool acquired, bool ownable)
{
    if (acquired) {
        lock->count_times_acquired++;
        /* CHECK: everything here works without mutex_trylock's */
        LOG(GLOBAL, LOG_THREADS, 6,
            "acquired lock " PFX " %s rank=%d, %s dcontext, tid:%d, %d time\n", lock,
            lock->name, lock->rank, get_thread_private_dcontext() ? "valid" : "not valid",
            d_r_get_thread_id(), lock->count_times_acquired);
        LOG(THREAD_GET, LOG_THREADS, 6, "acquired lock " PFX " %s rank=%d\n", lock,
            lock->name, lock->rank);
        ASSERT(lock->rank > 0 && "initialize with INIT_LOCK_FREE");
        if (ownable) {
            ASSERT(!lock->owner);
            lock->owner = d_r_get_thread_id();
            lock->owning_dcontext = get_thread_private_dcontext();
        }
        /* add to global list */
        if (lock->prev_process_lock == NULL && lock != &innermost_lock) {
            add_process_lock(lock);
        }

        /* cannot hold thread_initexit_lock while couldbelinking, else will
         * deadlock with flushers
         */
        ASSERT(lock != &thread_initexit_lock || !is_self_couldbelinking());

        if (INTERNAL_OPTION(deadlock_avoidance) &&
            get_thread_private_dcontext() != NULL &&
            get_thread_private_dcontext() != GLOBAL_DCONTEXT) {
            dcontext_t *dcontext = get_thread_private_dcontext();
            if (dcontext->thread_owned_locks != NULL) {
                /* PR 198871: same label used for all client locks so allow same rank.
                 * For now we ignore rank order when client lock is 1st, as well,
                 * to support decode_trace() for 0.9.6 release PR 198871 covers safer
                 * long-term fix.
                 */
                bool first_client = (dcontext->thread_owned_locks->last_lock->rank ==
                                     dr_client_mutex_rank);
                bool both_client = (first_client && lock->rank == dr_client_mutex_rank);
                if (dcontext->thread_owned_locks->last_lock->rank >= lock->rank &&
                    !first_client /*FIXME PR 198871: remove */ && !both_client) {
                    /* report rank order violation */
                    SYSLOG_INTERNAL_NO_OPTION_SYNCH(
                        SYSLOG_CRITICAL,
                        "rank order violation %s acquired after %s in tid:%x", lock->name,
                        dcontext->thread_owned_locks->last_lock->name,
                        d_r_get_thread_id());
                    dump_owned_locks(dcontext);
                    /* like syslog don't synchronize options for dumpcore_mask */
                    if (TEST(DUMPCORE_DEADLOCK, DYNAMO_OPTION(dumpcore_mask)))
                        os_dump_core("rank order violation");
                }
                ASSERT((dcontext->thread_owned_locks->last_lock->rank < lock->rank ||
                        first_client /*FIXME PR 198871: remove */
                        || both_client) &&
                       "rank order violation");
                if (ownable) {
                    lock->prev_owned_lock = dcontext->thread_owned_locks->last_lock;
                    dcontext->thread_owned_locks->last_lock = lock;
                }
                DOLOG(6, LOG_THREADS, { dump_owned_locks(dcontext); });
            }
        }
        if (INTERNAL_OPTION(mutex_callstack) != 0 && ownable &&
            get_thread_private_dcontext() != NULL) {
#    ifdef MUTEX_CALLSTACK
            mutex_collect_callstack(lock);
#    endif
        }
    } else {
        /* NOTE check_wait_at_safe_spot makes the assumption that no system
         * calls are made on the non acquired path here */
        ASSERT(lock->rank > 0 && "initialize with INIT_LOCK_FREE");
        if (INTERNAL_OPTION(deadlock_avoidance) && ownable) {
            ASSERT(lock->owner != d_r_get_thread_id() &&
                   "deadlock on recursive mutex_lock");
        }
        lock->count_times_contended++;
    }
}

/* FIXME: exported only for the linux hack -- make static once that's fixed */
void
deadlock_avoidance_unlock(mutex_t *lock, bool ownable)
{
    if (INTERNAL_OPTION(simulate_contention)) {
        /* with higher chances another thread will have to wait */
        os_thread_yield();
    }

    LOG(GLOBAL, LOG_THREADS, 6,
        "released lock " PFX " %s rank=%d, %s dcontext, tid:%d \n", lock, lock->name,
        lock->rank, get_thread_private_dcontext() ? "valid" : "not valid",
        d_r_get_thread_id());
    LOG(THREAD_GET, LOG_THREADS, 6, "released lock " PFX " %s rank=%d\n", lock,
        lock->name, lock->rank);
    if (!ownable)
        return;

    ASSERT(lock->owner == d_r_get_thread_id());
    if (INTERNAL_OPTION(deadlock_avoidance) && lock->owning_dcontext != NULL &&
        lock->owning_dcontext != GLOBAL_DCONTEXT) {
        dcontext_t *dcontext = get_thread_private_dcontext();
        if (dcontext == NULL) {
#    ifdef DEBUG
            /* thread_initexit_lock and all_threads_synch_lock
             * are unlocked after tearing down thread structures
             */
#        if defined(UNIX) && !defined(HAVE_TLS)
            extern mutex_t tls_lock;
#        endif
            bool null_ok =
                (lock == &thread_initexit_lock || lock == &all_threads_synch_lock
#        if defined(UNIX) && !defined(HAVE_TLS)
                 || lock == &tls_lock
#        endif
                );
            ASSERT(null_ok);
#    endif
        } else {
            ASSERT(lock->owning_dcontext == dcontext);
            if (dcontext->thread_owned_locks != NULL) {
                DOLOG(6, LOG_THREADS, { dump_owned_locks(dcontext); });
                /* LIFO order even though order in releasing doesn't matter */
                ASSERT(dcontext->thread_owned_locks->last_lock == lock);
                dcontext->thread_owned_locks->last_lock = lock->prev_owned_lock;
                lock->prev_owned_lock = NULL;
            }
        }
    }
    lock->owner = INVALID_THREAD_ID;
    lock->owning_dcontext = NULL;
}
#    define DEADLOCK_AVOIDANCE_LOCK(lock, acquired, ownable) \
        deadlock_avoidance_lock(lock, acquired, ownable)
#    define DEADLOCK_AVOIDANCE_UNLOCK(lock, ownable) \
        deadlock_avoidance_unlock(lock, ownable)
#else
#    define DEADLOCK_AVOIDANCE_LOCK(lock, acquired, ownable) /* do nothing */
#    define DEADLOCK_AVOIDANCE_UNLOCK(lock, ownable)         /* do nothing */
#endif                                                       /* DEADLOCK_AVOIDANCE */

#ifdef UNIX
void
d_r_mutex_fork_reset(mutex_t *mutex)
{
    /* i#239/PR 498752: need to free locks held by other threads at fork time.
     * We can't call ASSIGN_INIT_LOCK_FREE as that clobbers any contention event
     * (=> leak) and the debug-build lock lists (=> asserts like PR 504594).
     * If the synch before fork succeeded, this is unecessary.  If we encounter
     * more deadlocks after fork because of synch failure, we can add more calls
     * to reset locks on a case by case basis.
     */
    mutex->lock_requests = LOCK_FREE_STATE;
#    ifdef DEADLOCK_AVOIDANCE
    mutex->owner = INVALID_THREAD_ID;
    mutex->owning_dcontext = NULL;
#    endif
}
#endif

static uint spinlock_count = 0; /* initialized in utils_init, but 0 is always safe */
DECLARE_FREQPROT_VAR(static uint random_seed, 1234); /* initialized in utils_init */
DEBUG_DECLARE(static uint initial_random_seed;)

void
utils_init()
{
    /* FIXME: We need to find a formula (or a better constant) based on real experiments
       also see comment on spinlock_count_on_SMP in optionsx.h */
    /* we want to make sure it is 0 on UP, the rest is speculation */
    spinlock_count = (get_num_processors() - 1) * DYNAMO_OPTION(spinlock_count_on_SMP);

    /* allow reproducing PRNG sequence
     * (of course, thread scheduling may still affect requests) */
    random_seed =
        (DYNAMO_OPTION(prng_seed) == 0) ? os_random_seed() : DYNAMO_OPTION(prng_seed);
    /* logged only at end, preserved so can be looked up in a dump */
    DODEBUG(initial_random_seed = random_seed;);

    /* sanity check since we cast back and forth */
    ASSERT(sizeof(spin_mutex_t) == sizeof(mutex_t));

    ASSERT(sizeof(uint64) == 8);
    ASSERT(sizeof(uint32) == 4);
    ASSERT(sizeof(uint) == 4);
    ASSERT(sizeof(reg_t) == sizeof(void *));

#ifdef UNIX /* after options_init(), before we open logfile or call instrument_init() */
    os_file_init();
#endif

    set_exception_strings(NULL, NULL); /* use defaults */
}

/* NOTE since used by spinmutex_lock_no_yield, can make no system calls before
 * the lock is grabbed (required by check_wait_at_safe_spot). */
bool
spinmutex_trylock(spin_mutex_t *spin_lock)
{
    mutex_t *lock = &spin_lock->lock;
    int mutexval;
    mutexval = atomic_swap(&lock->lock_requests, LOCK_SET_STATE);
    ASSERT(mutexval == LOCK_FREE_STATE || mutexval == LOCK_SET_STATE);
    DEADLOCK_AVOIDANCE_LOCK(lock, mutexval == LOCK_FREE_STATE, LOCK_OWNABLE);
    return (mutexval == LOCK_FREE_STATE);
}

void
spinmutex_lock(spin_mutex_t *spin_lock)
{
    /* busy-wait until mutex is locked */
    while (!spinmutex_trylock(spin_lock)) {
        os_thread_yield();
    }
    return;
}

/* special version of spinmutex_lock that makes no system calls (i.e. no yield)
 * as required by check_wait_at_safe_spot */
void
spinmutex_lock_no_yield(spin_mutex_t *spin_lock)
{
    /* busy-wait until mutex is locked */
    while (!spinmutex_trylock(spin_lock)) {
#ifdef DEADLOCK_AVOIDANCE
        mutex_t *lock = &spin_lock->lock;
        /* Trylock inc'ed count_times_contended, but since we are prob. going
         * to spin a lot, we'd rather attribute that to a separate counter
         * count_times_spin_pause to keep the counts meaningful. */
        lock->count_times_contended--;
        lock->count_times_spin_pause++;
#endif
        SPINLOCK_PAUSE();
    }
    return;
}

void
spinmutex_unlock(spin_mutex_t *spin_lock)
{
    mutex_t *lock = &spin_lock->lock;
    /* if this fails, it means you don't already own the lock. */
    ASSERT(lock->lock_requests > LOCK_FREE_STATE && "lock not owned");
    ASSERT(lock->lock_requests == LOCK_SET_STATE);
    DEADLOCK_AVOIDANCE_UNLOCK(lock, LOCK_OWNABLE);
    lock->lock_requests = LOCK_FREE_STATE;
    /* NOTE - check_wait_at_safe_spot requires that no system calls be made
     * after we release the lock */
    return;
}

void
spinmutex_delete(spin_mutex_t *spin_lock)
{
    ASSERT(!ksynch_var_initialized(&spin_lock->lock.contended_event));
    d_r_mutex_delete(&spin_lock->lock);
}

#ifdef DEADLOCK_AVOIDANCE
static bool
mutex_ownable(mutex_t *lock)
{
    bool ownable = LOCK_OWNABLE;
    /* i#779: support DR locks used as app locks */
    if (lock->app_lock) {
        ASSERT(lock->rank == dr_client_mutex_rank);
        ownable = LOCK_NOT_OWNABLE;
    }
    return ownable;
}
#endif

void
d_r_mutex_lock_app(mutex_t *lock, priv_mcontext_t *mc)
{
    bool acquired;
#ifdef DEADLOCK_AVOIDANCE
    bool ownable = mutex_ownable(lock);
#endif

    /* we may want to first spin the lock for a while if we are on a multiprocessor
     * machine
     */
    /* option is external only so that we can set it to 0 on a uniprocessor */
    if (spinlock_count) {
        uint i;
        /* in the common case we'll just get it */
        if (d_r_mutex_trylock(lock))
            return;

        /* otherwise contended, we should spin for some time */
        i = spinlock_count;
        /* while spinning we are PAUSEing and reading without LOCKing the bus in the
         * spin loop
         */
        do {
            /* hint we are spinning */
            SPINLOCK_PAUSE();

            /* We spin only while lock_requests == 0 which means that exactly one thread
               holds the lock, while the current one (and possibly a few others) are
               contending on who will grab it next.  It doesn't make much sense to spin
               when the lock->lock_requests > 0 (which means that at least one thread is
               already blocked).  And of course, we also break if it is LOCK_FREE_STATE.
            */
            if (atomic_aligned_read_int(&lock->lock_requests) != LOCK_SET_STATE) {
#ifdef DEADLOCK_AVOIDANCE
                lock->count_times_spin_only++;
#endif
                break;
            }
            i--;
        } while (i > 0);
    }

    /* we have strong intentions to grab this lock, increment requests */
    acquired = atomic_inc_and_test(&lock->lock_requests);
    DEADLOCK_AVOIDANCE_LOCK(lock, acquired, ownable);

    if (!acquired) {
        mutex_wait_contended_lock(lock, mc);
#ifdef DEADLOCK_AVOIDANCE
        DEADLOCK_AVOIDANCE_LOCK(lock, true, ownable); /* now we got it  */
        /* this and previous owner are not included in lock_requests */
        if (lock->max_contended_requests < (uint)lock->lock_requests)
            lock->max_contended_requests = (uint)lock->lock_requests;
#endif
    }
}

void
d_r_mutex_lock(mutex_t *lock)
{
    d_r_mutex_lock_app(lock, NULL);
}

/* try once to grab the lock, return whether or not successful */
bool
d_r_mutex_trylock(mutex_t *lock)
{
    bool acquired;
#ifdef DEADLOCK_AVOIDANCE
    bool ownable = mutex_ownable(lock);
#endif

    /* preserve old value in case not LOCK_FREE_STATE */
    acquired =
        atomic_compare_exchange(&lock->lock_requests, LOCK_FREE_STATE, LOCK_SET_STATE);
    /* if old value was free, that means we just obtained lock
       old value may be >=0 when several threads are trying to acquire lock,
       so we should return false
     */
    DEADLOCK_AVOIDANCE_LOCK(lock, acquired, ownable);
    return acquired;
}

/* free the lock */
void
d_r_mutex_unlock(mutex_t *lock)
{
#ifdef DEADLOCK_AVOIDANCE
    bool ownable = mutex_ownable(lock);
#endif

    ASSERT(lock->lock_requests > LOCK_FREE_STATE && "lock not owned");
    DEADLOCK_AVOIDANCE_UNLOCK(lock, ownable);

    if (atomic_dec_and_test(&lock->lock_requests))
        return;
    /* if we were not the last one to hold the lock,
       (i.e. final value is not LOCK_FREE_STATE)
       we need to notify another waiting thread */
    mutex_notify_released_lock(lock);
}

/* releases any associated kernel objects */
void
d_r_mutex_delete(mutex_t *lock)
{
    LOG(GLOBAL, LOG_THREADS, 3, "mutex_delete lock " PFX "\n", lock);
    /* When doing detach, application locks may be held on threads which have
     * already been interrupted and will never execute lock code again. Just
     * ignore the assert on the lock_requests field below in those cases.
     */
    DEBUG_DECLARE(bool skip_lock_request_assert = false;)
#ifdef DEADLOCK_AVOIDANCE
    LOG(THREAD_GET, LOG_THREADS, 3,
        "mutex_delete " DUMP_LOCK_INFO_ARGS(0, lock, lock->prev_process_lock));
    remove_process_lock(lock);
    lock->deleted = true;
    if (doing_detach) {
        /* For possible re-attach we clear the acquired count.  We leave
         * deleted==true as it is not used much and we don't have a simple method for
         * clearing it: we'd have to keep a special list of all locks used (appending
         * as they're deleted) and then walk it from dynamo_exit_post_detach().
         */
        lock->count_times_acquired = 0;
#    ifdef DEBUG
        skip_lock_request_assert = lock->app_lock;
#    endif
    }
#else
#    ifdef DEBUG
    /* We don't support !DEADLOCK_AVOIDANCE && DEBUG: we need to know whether to
     * skip the assert lock_requests but don't know if this lock is an app lock
     */
#        error DEBUG mode not supported without DEADLOCK_AVOIDANCE
#    endif /* DEBUG */
#endif     /* DEADLOCK_AVOIDANCE */
    ASSERT(skip_lock_request_assert || lock->lock_requests == LOCK_FREE_STATE);

    if (ksynch_var_initialized(&lock->contended_event)) {
        mutex_free_contended_event(lock);
    }
}

void
d_r_mutex_mark_as_app(mutex_t *lock)
{
#ifdef DEADLOCK_AVOIDANCE
    lock->app_lock = true;
#endif
}

static inline void
own_recursive_lock(recursive_lock_t *lock)
{
#ifdef DEADLOCK_AVOIDANCE
    ASSERT(!mutex_ownable(&lock->lock) || OWN_MUTEX(&lock->lock));
#endif
    ASSERT(lock->owner == INVALID_THREAD_ID);
    ASSERT(lock->count == 0);
    lock->owner = d_r_get_thread_id();
    ASSERT(lock->owner != INVALID_THREAD_ID);
    lock->count = 1;
}

void
acquire_recursive_app_lock(recursive_lock_t *lock, priv_mcontext_t *mc)
{
    /* we no longer use the pattern of implementing acquire_lock as a
       busy try_lock
    */

    if (ATOMIC_READ_THREAD_ID(&lock->owner) == d_r_get_thread_id()) {
        lock->count++;
    } else {
        d_r_mutex_lock_app(&lock->lock, mc);
        own_recursive_lock(lock);
    }
}

/* FIXME: rename recursive routines to parallel mutex_ routines */
void
acquire_recursive_lock(recursive_lock_t *lock)
{
    acquire_recursive_app_lock(lock, NULL);
}

bool
try_recursive_lock(recursive_lock_t *lock)
{
    if (ATOMIC_READ_THREAD_ID(&lock->owner) == d_r_get_thread_id()) {
        lock->count++;
    } else {
        if (!d_r_mutex_trylock(&lock->lock))
            return false;
        own_recursive_lock(lock);
    }
    return true;
}

void
release_recursive_lock(recursive_lock_t *lock)
{
#ifdef DEADLOCK_AVOIDANCE
    ASSERT(!mutex_ownable(&lock->lock) || OWN_MUTEX(&lock->lock));
#endif
    ASSERT(lock->owner == d_r_get_thread_id());
    ASSERT(lock->count > 0);
    lock->count--;
    if (lock->count == 0) {
        lock->owner = INVALID_THREAD_ID;
        d_r_mutex_unlock(&lock->lock);
    }
}

bool
self_owns_recursive_lock(recursive_lock_t *lock)
{
    return (ATOMIC_READ_THREAD_ID(&lock->owner) == d_r_get_thread_id());
}

/* Read write locks */
/* A read write lock allows multiple readers or alternatively a single writer */

/* We're keeping here an older implementation under
   INTERNAL_OPTION(spin_yield_rwlock) that spins on the contention
   path.  In the Attic we also have the initial naive implementation
   wrapping mutex_t'es !INTERNAL_OPTION(fast_rwlock).
*/

/*
   FIXME: Since we are using multiple words to contain the state,
   we still have to keep looping on contention events.

   We need to switch to using a single variable for this but for now
   let's first put all pieces of the kernel objects support together.

   PLAN:  All state should be in one 32bit word.
   Then we need one atomic operation that decrements readers and tells us:
   1) whether there was a writer (e.g. MSB set)
   2) whether this was the last reader (e.g. 0 in all other bits)
   Only when 1) & 2) are true (e.g. 0x80000000) we need to notify the writer.
   Think about using XADD: atomic_add_exchange(state, -1)
*/
/* FIXME: See /usr/src/linux-2.4/include/asm-i386/rwlock.h,
   spinlock.h and /usr/src/linux-2.4/arch/i386/kernel/semaphore.c
   for the Linux kernel implementation on x86.
 */

/* Currently we are using kernel objects to block on contention paths.
   Writers are blocked from each other at the mutex_t, and are notified
   by previous readers by an auto event.  Readers, of course, can have
   the lock simultaneously, but block on a previous writer - note also
   on an auto event.  Broadcasting to all readers is done by
   explicitly waking up each by the previous one, while the writer
   continues execution.  There is no fairness to the readers that are
   blocked vs any new readers that will grab the lock immediately, and
   for that matter vs any new writers.

   FIXME: Keep in mind that a successful wait on the kernel events in read
   locks should not be used as a guarantee that the current thread can
   proceed with a granted request.  We should rather keep looping to
   verify that we are back on the fast path.

   Due to the two reasons above we still have unbound loops in the
   rwlock primitives. It also lets the Linux implementation just yield.
*/

void
d_r_read_lock(read_write_lock_t *rw)
{
    /* wait for writer here if lock is held
     * FIXME: generalize DEADLOCK_AVOIDANCE to both detect
     * order violations and gather contention stats for
     * this mutex-less synch
     */
    if (INTERNAL_OPTION(spin_yield_rwlock)) {
        do {
            while (mutex_testlock(&rw->lock)) {
                /* contended read */
                /* am I the writer?
                 * ASSUMPTION: reading field is atomic
                 * For linux d_r_get_thread_id() is expensive -- we
                 * should either address that through special handling
                 * of native and new thread cases, or switch this
                 * routine to pass in dcontext and use that.
                 * Update: linux d_r_get_thread_id() now calls get_tls_thread_id()
                 * and avoids the syscall (xref PR 473640).
                 * FIXME: we could also reorganize this check so that it is done only once
                 * instead of in the loop body but it doesn't seem wortwhile
                 */
                /* We have the lock so we do not need a load-acquire. */
                if (rw->writer == d_r_get_thread_id()) {
                    /* we would share the code below but we do not want
                     * the deadlock avoidance to consider this an acquire
                     */
                    ATOMIC_INC(int, rw->num_readers);
                    return;
                }
                DEADLOCK_AVOIDANCE_LOCK(&rw->lock, false, LOCK_NOT_OWNABLE);
                /* FIXME: last places where we yield instead of wait */
                os_thread_yield();
            }
            ATOMIC_INC(int, rw->num_readers);
            if (!mutex_testlock(&rw->lock))
                break;
            /* else, race with writer, must try again */
            ATOMIC_DEC(int, rw->num_readers);
        } while (true);
        DEADLOCK_AVOIDANCE_LOCK(&rw->lock, true, LOCK_NOT_OWNABLE);
        return;
    }

    /* event based notification, yet still need to loop */
    do {
        while (mutex_testlock(&rw->lock)) {
            /* contended read */
            /* am I the writer?
             * ASSUMPTION: reading field is atomic
             * For linux d_r_get_thread_id() is expensive -- we
             * should either address that through special handling
             * of native and new thread cases, or switch this
             * routine to pass in dcontext and use that.
             * Update: linux d_r_get_thread_id() now calls get_tls_thread_id()
             * and avoids the syscall (xref PR 473640).
             */
            /* We have the lock so we do not need a load-acquire. */
            if (rw->writer == d_r_get_thread_id()) {
                /* we would share the code below but we do not want
                 * the deadlock avoidance to consider this an acquire
                 */
                /* we also have to do this check on the read_unlock path  */
                ATOMIC_INC(int, rw->num_readers);
                return;
            }
            DEADLOCK_AVOIDANCE_LOCK(&rw->lock, false, LOCK_NOT_OWNABLE);

            ATOMIC_INC(int, rw->num_pending_readers);
            /* if we get interrupted before we have incremented this counter?
               Then no signal will be send our way, so we shouldn't be waiting then
            */
            if (mutex_testlock(&rw->lock)) {
                /* still holding up */
                rwlock_wait_contended_reader(rw);
            } else {
                /* otherwise race with writer */
                /* after the write lock is released pending readers
                   should no longer wait since no one will wake them up */
                /* no need to pause */
            }
            /* Even if we didn't wait another reader may be waiting for notification */
            if (!atomic_dec_becomes_zero(&rw->num_pending_readers)) {
                /* If we were not the last pending reader,
                   we need to notify another waiting one so that
                   it can get out of the contention path.
                */
                rwlock_notify_readers(rw);
                /* Keep in mind that here we don't guarantee that after blocking
                   we have an automatic right to claim the lock.
                */
            }
        }
        /* fast path */
        ATOMIC_INC(int, rw->num_readers);
        if (!mutex_testlock(&rw->lock))
            break;
        /* else, race with writer, must try again */
        /* FIXME: need to get num_readers and the mutex in one place,
           or otherwise add a mutex grabbed by readers for the above
           test.
        */
        ATOMIC_DEC(int, rw->num_readers);
        /* What if a writer thought that this reader has
         already taken turn - and will then wait thinking this guy has
         grabbed the read lock first?  For now we'll have to wake up
         the writer to retry even if it spuriously wakes up the next writer.
        */
        // FIXME: we need to do only when num_readers has become zero,
        // but it is OK for now as this won't usually happen
        rwlock_notify_writer(rw); /* --ok since writers still have to loop */
        /* hint we are spinning */
        SPINLOCK_PAUSE();
    } while (true);

    DEADLOCK_AVOIDANCE_LOCK(&rw->lock, true, LOCK_NOT_OWNABLE);
}

void
d_r_write_lock(read_write_lock_t *rw)
{
    /* we do not follow the pattern of having lock call trylock in
     * a loop because that would be unfair to writers -- first guy
     * in this implementation gets to write
     */
    if (INTERNAL_OPTION(spin_yield_rwlock)) {
        d_r_mutex_lock(&rw->lock);
        /* We have the lock so we do not need a load-acquire. */
        while (rw->num_readers > 0) {
            /* contended write */
            DEADLOCK_AVOIDANCE_LOCK(&rw->lock, false, LOCK_NOT_OWNABLE);
            /* FIXME: last places where we yield instead of wait */
            os_thread_yield();
        }
        rw->writer = d_r_get_thread_id();
        return;
    }

    d_r_mutex_lock(&rw->lock);
    /* We still do this in a loop, since the event signal doesn't guarantee
       that num_readers is 0 when unblocked.
     */
    while (rw->num_readers > 0) {
        /* contended write */
        DEADLOCK_AVOIDANCE_LOCK(&rw->lock, false, LOCK_NOT_OWNABLE);
        rwlock_wait_contended_writer(rw);
    }
    rw->writer = d_r_get_thread_id();
}

bool
d_r_write_trylock(read_write_lock_t *rw)
{
    if (d_r_mutex_trylock(&rw->lock)) {
        ASSERT_NOT_TESTED();
        /* We have the lock so we do not need a load-acquire. */
        if (rw->num_readers == 0) {
            rw->writer = d_r_get_thread_id();
            return true;
        } else {
            /* We need to duplicate the bottom of d_r_write_unlock() */
            /* since if a new reader has appeared after we have acquired the lock
               that one may already be waiting on the broadcast event */
            d_r_mutex_unlock(&rw->lock);
            /* check whether any reader is currently waiting */
            if (atomic_aligned_read_int(&rw->num_pending_readers) > 0) {
                /* after we've released the write lock, pending
                 * readers will no longer wait
                 */
                rwlock_notify_readers(rw);
            }
        }
    }
    return false;
}

void
d_r_read_unlock(read_write_lock_t *rw)
{
    if (INTERNAL_OPTION(spin_yield_rwlock)) {
        ATOMIC_DEC(int, rw->num_readers);
        DEADLOCK_AVOIDANCE_UNLOCK(&rw->lock, LOCK_NOT_OWNABLE);
        return;
    }

    /* if we were the last reader to hold the lock, (i.e. final value is 0)
       we may need to notify a waiting writer */

    /* unfortunately even on the hot path (of a single reader) we have
       to check if the writer is in fact waiting.  Even though this is
       not atomic we don't need to loop here - d_r_write_lock() will loop.
    */
    if (atomic_dec_becomes_zero(&rw->num_readers)) {
        /* if the writer is waiting it definitely needs to hold the mutex */
        if (mutex_testlock(&rw->lock)) {
            /* test that it was not this thread owning both write and read lock */
            /* We have the lock so we do not need a load-acquire. */
            if (rw->writer != d_r_get_thread_id()) {
                /* we're assuming the writer has been forced to wait,
                   but since we can't tell whether it did indeed wait this
                   notify may leave signaled the event for the next turn

                   If the writer has grabbed the mutex and checked
                   when num_readers==0 and has gone assuming to be the
                   rwlock owner.  In that case the above
                   rwlock_notify_writer will give the wrong signal to
                   the next writer.
                   --ok since writers still have to loop
                */
                rwlock_notify_writer(rw);
            }
        }
    }

    DEADLOCK_AVOIDANCE_UNLOCK(&rw->lock, LOCK_NOT_OWNABLE);
}

void
d_r_write_unlock(read_write_lock_t *rw)
{
#ifdef DEADLOCK_AVOIDANCE
    ASSERT(!mutex_ownable(&rw->lock) || rw->writer == rw->lock.owner);
#endif
    rw->writer = INVALID_THREAD_ID;
    if (INTERNAL_OPTION(spin_yield_rwlock)) {
        d_r_mutex_unlock(&rw->lock);
        return;
    }
    /* we need to signal all waiting readers (if any) that they can now go
       ahead.  No writer should be allowed to lock until all currently
       waiting readers are unblocked.
     */
    /* We first unlock so that any blocked readers can start making
       progress as soon as they are notified.  Further field
       accesses however have to be assumed unprotected.
    */
    d_r_mutex_unlock(&rw->lock);
    /* check whether any reader is currently waiting */
    if (atomic_aligned_read_int(&rw->num_pending_readers) > 0) {
        /* after we've released the write lock, pending readers will no longer wait */
        rwlock_notify_readers(rw);
    }
}

bool
self_owns_write_lock(read_write_lock_t *rw)
{
    return (ATOMIC_READ_THREAD_ID(&rw->writer) == d_r_get_thread_id());
}

/****************************************************************************/
/* HASHING */

ptr_uint_t
hash_value(ptr_uint_t val, hash_function_t func, ptr_uint_t mask, uint bits)
{
    if (func == HASH_FUNCTION_NONE)
        return val;
    switch (func) {
    case HASH_FUNCTION_MULTIPLY_PHI: {
        /* case 8457: keep in sync w/ HASH_VALUE_FOR_TABLE() */
        return ((val * HASH_PHI) >> (HASH_TAG_BITS - bits));
    }
#ifdef INTERNAL
    case HASH_FUNCTION_LOWER_BSWAP: {
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        return (((val & 0xFFFF0000)) | ((val & 0x000000FF) << 8) |
                ((val & 0x0000FF00) >> 8));
    }
    case HASH_FUNCTION_BSWAP_XOR: {
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        return (val ^
                (((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) |
                 ((val & 0x00FF0000) >> 8) | ((val & 0xFF000000) >> 24)));
    }
    case HASH_FUNCTION_SWAP_12TO15: {
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        return (((val & 0xFFFF0FF0)) | ((val & 0x0000F000) >> 12) |
                ((val & 0x0000000F) << 12));
    }
    case HASH_FUNCTION_SWAP_12TO15_AND_NONE: {
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        return (mask <= 0xFFF ? val
                              : (((val & 0xFFFF0FF0)) | ((val & 0x0000F000) >> 12) |
                                 ((val & 0x0000000F) << 12)));
    }
    case HASH_FUNCTION_SHIFT_XOR: {
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        return val ^ (val >> 12) ^ (val << 12);
    }
#endif
    case HASH_FUNCTION_STRING:
    case HASH_FUNCTION_STRING_NOCASE: {
        const char *s = (const char *)val;
        char c;
        ptr_uint_t hash = 0;
        uint i, shift;
        uint max_shift = ALIGN_FORWARD(bits, 8);
        /* Simple hash function that combines unbiased via xor and
         * shifts to get input chars to cover the full range.  We clamp
         * the shift to avoid useful bits being truncated.  An
         * alternative is to combine blocks of 4 chars at a time but
         * that's more complex.
         */
        for (i = 0; s[i] != '\0'; i++) {
            c = s[i];
            if (func == HASH_FUNCTION_STRING_NOCASE)
                c = (char)tolower(c);
            shift = (i % 4) * 8;
            hash ^= (c << MIN(shift, max_shift));
        }
        return hash;
    }
    default: {
        ASSERT_NOT_REACHED();
        return 0;
    }
    }
}

uint
hashtable_num_bits(uint size)
{
    uint bits = 0;
    uint sz = size;
    while (sz > 0) {
        sz = sz >> 1;
        bits++;
    }
    ASSERT(HASHTABLE_SIZE(bits) > size && HASHTABLE_SIZE(bits) <= size * 2);
    return bits;
}

/****************************************************************************/
/* BITMAP */

/* Since there is no ffs() on windows we use the one from
 *  /usr/src/linux-2.4/include/linux/bitops.h.  TODO: An easier
 *  x86-specific way using BSF is
 *  /usr/src/linux-2.4/include/asm/bitops.h
 */

/* Returns the position of the first set bit - betwen 0 and 31 */
static inline uint
bitmap_find_first_set_bit(bitmap_element_t x)
{
    int r = 0;

    ASSERT(x);
    if (!(x & 0xffff)) {
        x >>= 16;
        r += 16;
    }
    if (!(x & 0xff)) {
        x >>= 8;
        r += 8;
    }
    if (!(x & 0xf)) {
        x >>= 4;
        r += 4;
    }
    if (!(x & 3)) {
        x >>= 2;
        r += 2;
    }
    if (!(x & 1)) {
        x >>= 1;
        r += 1;
    }
    return r;
}

/* A block is marked free with a set bit.
   Returns -1 if no block is found!
*/
static inline uint
bitmap_find_set_block(bitmap_t b, uint bitmap_size)
{
    uint i = 0;
    uint last_index = BITMAP_INDEX(bitmap_size);

    while (b[i] == 0 && i < last_index)
        i++;
    if (i == last_index)
        return BITMAP_NOT_FOUND;
    return i * BITMAP_DENSITY + bitmap_find_first_set_bit(b[i]);
}

/* Looks for a sequence of free blocks
 * Returns -1 if no such sequence is found!

 * Considering the fact that the majority of our allocations will be
 * for a single block this operation is not terribly efficient.
 */
static uint
bitmap_find_set_block_sequence(bitmap_t b, uint bitmap_size, uint requested)
{
    uint last_bit = bitmap_size - requested + 1;
    /* find quickly at least a single block */
    uint first = bitmap_find_set_block(b, bitmap_size);
    if (first == BITMAP_NOT_FOUND)
        return BITMAP_NOT_FOUND;

    do {
        /* now check if there is room for the requested number of bits */
        uint hole_size = 1;
        while (hole_size < requested && bitmap_test(b, first + hole_size)) {
            hole_size++;
        }
        if (hole_size == requested)
            return first;
        /* otherwise first + hole_size is not set, so we should skip that */
        first += hole_size + 1;
        while (first < last_bit && !bitmap_test(b, first))
            first++;
    } while (first < last_bit);

    return BITMAP_NOT_FOUND;
}

void
bitmap_initialize_free(bitmap_t b, uint bitmap_size)
{
    memset(b, 0xff, BITMAP_INDEX(bitmap_size) * sizeof(bitmap_element_t));
}

uint
bitmap_allocate_blocks(bitmap_t b, uint bitmap_size, uint request_blocks,
                       uint start_block)
{
    uint i, res;
    if (start_block != UINT_MAX) {
        if (start_block + request_blocks > bitmap_size)
            return BITMAP_NOT_FOUND;
        uint hole_size = 0;
        while (hole_size < request_blocks && bitmap_test(b, start_block + hole_size)) {
            hole_size++;
        }
        if (hole_size == request_blocks)
            res = start_block;
        else
            return BITMAP_NOT_FOUND;
    } else if (request_blocks == 1) {
        res = bitmap_find_set_block(b, bitmap_size);
    } else {
        res = bitmap_find_set_block_sequence(b, bitmap_size, request_blocks);
    }
    if (res == BITMAP_NOT_FOUND)
        return BITMAP_NOT_FOUND;
    i = res;
    do {
        bitmap_clear(b, i++);
    } while (--request_blocks);
    return res;
}

void
bitmap_free_blocks(bitmap_t b, uint bitmap_size, uint first_block, uint num_free)
{
    ASSERT(first_block + num_free <= bitmap_size);
    do {
        ASSERT(!bitmap_test(b, first_block));
        bitmap_set(b, first_block++);
    } while (--num_free);
}

#ifdef DEBUG
/* used only for ASSERTs */
bool
bitmap_are_reserved_blocks(bitmap_t b, uint bitmap_size, uint first_block,
                           uint num_blocks)
{
    ASSERT(first_block + num_blocks <= bitmap_size);
    do {
        if (bitmap_test(b, first_block))
            return false;
        first_block++;
    } while (--num_blocks);
    return true;
}

static inline uint
bitmap_count_set_bits(bitmap_element_t x)
{
    int r = 0;

    /* count set bits in each element */
    while (x) {
        r++;
        x &= x - 1;
    }

    return r;
}

bool
bitmap_check_consistency(bitmap_t b, uint bitmap_size, uint expect_free)
{
    uint last_index = BITMAP_INDEX(bitmap_size);
    uint i;
    uint current = 0;
    for (i = 0; i < last_index; i++) {
        current += bitmap_count_set_bits(b[i]);
    }

    LOG(GLOBAL, LOG_HEAP, 3,
        "bitmap_check_consistency(b=" PFX ", bitmap_size=%d)"
        " expected=%d current=%d\n",
        b, bitmap_size, expect_free, current);
    return expect_free == current;
}
#endif /* DEBUG */

/****************************************************************************/
/* LOGGING */

file_t
get_thread_private_logfile()
{
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
    if (dcontext == NULL)
        dcontext = GLOBAL_DCONTEXT;
    return THREAD;
#else
    return INVALID_FILE;
#endif
}

#ifdef DEBUG
DECLARE_FREQPROT_VAR(static bool do_once_do_file_write, false);
#endif

/*  FIXME: add buffering? */
ssize_t
do_file_write(file_t f, const char *fmt, va_list ap)
{
    ssize_t size, written;
    char logbuf[MAX_LOG_LENGTH];

    if (f == INVALID_FILE)
        return -1;
    size = vsnprintf(logbuf, BUFFER_SIZE_ELEMENTS(logbuf), fmt, ap);
    NULL_TERMINATE_BUFFER(logbuf); /* always NULL terminate */
    DOCHECK(1, {
        /* we have our own do-once to avoid infinite recursion w/ protect_data_section */
        if (size < 0 || size >= BUFFER_SIZE_ELEMENTS(logbuf)) {
            if (!do_once_do_file_write) {
                do_once_do_file_write = true;
                ASSERT_CURIOSITY(size >= 0 && size < sizeof(logbuf));
            }
        }
    });
    /* handle failure values */
    if (size >= BUFFER_SIZE_ELEMENTS(logbuf) || size < 0)
        size = strlen(logbuf);
    written = os_write(f, logbuf, size);
    if (written < 0)
        return -1;
    return written;
}

/* a little utiliy for printing a float that is formed by dividing 2 uints,
 * gives back high and low parts for printing, also supports percentages
 * FIXME : we might need to handle signed numbers at some point (but not yet),
 * also could be smarter about overflow conditions (i.e. for calculating
 * bottom or top) but we never call with numbers that big, also truncates
 * instead of rounding
 * Usage : given a, b (uint[64]); d_int()==divide_uint64_print();
 *         uint c, d tmp; parameterized on precision p and width w
 * note that %f is eqv. to %.6f, 3rd ex. is a percentage ex.
 * "%.pf", a/(float)b => d_int(a, b, false, p, &c, &d);  "%u.%.pu", c,d
 * "%w.pf", a/(float)b => d_int(a, b, false, p, &c, &d); "%(w-p-1)u.%.pu", c,d
 * "%.pf%%", 100*(a/(float)b) => d_int(a, b, true, p, &c, &d); "%u.%.pu%%", c,d
 */
void
divide_uint64_print(uint64 numerator, uint64 denominator, bool percentage, uint precision,
                    uint *top, uint *bottom)
{
    uint i, precision_multiple, multiple = percentage ? 100 : 1;
#ifdef HOT_PATCHING_INTERFACE
    /* case 6657: hotp_only does not have many of the DR stats, so
     * numerator and/or denominator may be 0 */
    ASSERT(denominator != 0 || DYNAMO_OPTION(hotp_only));
#else
    ASSERT(denominator != 0);
#endif
    ASSERT(top != NULL && bottom != NULL);
    if (denominator == 0)
        return;
    ASSERT_TRUNCATE(*top, uint, ((multiple * numerator) / denominator));
    *top = (uint)((multiple * numerator) / denominator);
    for (i = 0, precision_multiple = 1; i < precision; i++)
        precision_multiple *= 10;
    ASSERT_TRUNCATE(*bottom, uint,
                    (((precision_multiple * multiple * numerator) / denominator) -
                     (precision_multiple * *top)));
    /* FUNNY: if I forget the above ) I crash the preprocessor: cc1
     * internal compiler error couldn't reproduce in a smaller set to
     * file a bug against gcc version 3.3.3 (cygwin special)
     */
    *bottom = (uint)(((precision_multiple * multiple * numerator) / denominator) -
                     (precision_multiple * *top));
}

/* When building with /QIfist casting rounds instead of truncating (i#763)
 * so we use these routines from io.c.
 */
extern long
double2int_trunc(double d);

/* For printing a float.
 * NOTE: You must preserve x87 floating point state to call this function, unless
 * you can prove the compiler will never use x87 state for float operations.
 * XXX: Truncates instead of rounding; also, negative with width looks funny;
 *      finally, width can be one off if negative
 * Usage : given double/float a; uint c, d and char *s tmp; dp==double_print
 *         parameterized on precision p width w
 * Note that %f is eqv. to %.6f.
 * "%.pf", a => dp(a, p, &c, &d, &s) "%s%u.%.pu", s, c, d
 * "%w.pf", a => dp(a, p, &c, &d, &s) "%s%(w-p-1)u.%.pu", s, c, d
 */
void
double_print(double val, uint precision, uint *top, uint *bottom, const char **sign)
{
    uint i, precision_multiple;
    ASSERT(top != NULL && bottom != NULL && sign != NULL);
    if (val < 0.0) {
        val = -val;
        *sign = "-";
    } else {
        *sign = "";
    }
    for (i = 0, precision_multiple = 1; i < precision; i++)
        precision_multiple *= 10;
    /* when building with /QIfist casting rounds instead of truncating (i#763) */
    *top = double2int_trunc(val);
    *bottom = double2int_trunc((val - *top) * precision_multiple);
}

#ifdef WINDOWS
/* for pre_inject, injector, and core shared files, is just wrapper for syslog
 * internal */
void
display_error(char *msg)
{
    SYSLOG_INTERNAL_ERROR("%s", msg);
}
#endif

#ifdef DEBUG
#    ifdef WINDOWS
/* print_symbolic_address is in module.c */
#    else
/* prints a symbolic name, or best guess of it into a caller provided buffer */
void
print_symbolic_address(app_pc tag, char *buf, int max_chars, bool exact_only)
{
    buf[0] = '\0';
}
#    endif
#endif /* DEBUG */

void
print_file(file_t f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    do_file_write(f, fmt, ap);
    va_end(ap);
}

/* For repeated appending to a buffer.  The "sofar" var should be set
 * to 0 by the caller before the first call to print_to_buffer.
 * Returns false if there was not room for the string plus a null,
 * but still prints the maximum that will fit plus a null.
 */
/* XXX: This is duplicated in ir/decodlib.c's print_to_buffer.
 * Could we move this into io.c to share it with decodelib?
 */
static bool
vprint_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, const char *fmt,
                 va_list ap)
{
    /* in io.c */
    extern int d_r_vsnprintf(char *s, size_t max, const char *fmt, va_list ap);
    ssize_t len;
    bool ok;
    /* we use d_r_vsnprintf for consistent return value and to handle floats */
    len = d_r_vsnprintf(buf + *sofar, bufsz - *sofar, fmt, ap);
    /* we support appending an empty string (len==0) */
    ok = (len >= 0 && len < (ssize_t)(bufsz - *sofar));
    /* If the written chars filled up the max size exactly, that max size is returned
     * without a final null by d_r_vsnprintf().  Since we guarantee a null, we have
     * to clobber a char, just like for -1 being returned when the written chars do
     * not all fit.
     */
    *sofar += (len == -1 || len == (ssize_t)(bufsz - *sofar) ? (bufsz - *sofar - 1)
                                                             : (len < 0 ? 0 : len));
    /* be paranoid: though usually many calls in a row and could delay until end */
    buf[bufsz - 1] = '\0';
    return ok;
}

/* For repeated appending to a buffer.  The "sofar" var should be set
 * to 0 by the caller before the first call to print_to_buffer.
 * Returns false if there was not room for the string plus a null,
 * but still prints the maximum that will fit plus a null.
 */
bool
print_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, const char *fmt, ...)
{
    va_list ap;
    bool ok;
    va_start(ap, fmt);
    ok = vprint_to_buffer(buf, bufsz, sofar, fmt, ap);
    va_end(ap);
    return ok;
}

/* N.B.: this routine is duplicated in instrument.c's dr_log!
 * Maybe export this routine itself, or make another layer of
 * calls that passes the va_list (-> less efficient)?
 * For now I'm assuming this routine changes little.
 */
void
d_r_print_log(file_t logfile, uint mask, uint level, const char *fmt, ...)
{
    va_list ap;

#ifdef DEBUG
    /* FIXME: now the LOG macro checks these, remove here? */
    if (logfile == INVALID_FILE ||
        (d_r_stats != NULL &&
         ((d_r_stats->logmask & mask) == 0 || d_r_stats->loglevel < level)))
        return;
#else
    return;
#endif

    KSTART(logging);
    va_start(ap, fmt);
    do_file_write(logfile, fmt, ap);
    va_end(ap);
    KSTOP_NOT_PROPAGATED(logging);
}

#ifdef WINDOWS
static void
do_syslog(syslog_event_type_t priority, uint message_id, uint substitutions_num, ...)
{
    va_list ap;
    va_start(ap, substitutions_num);
    os_syslog(priority, message_id, substitutions_num, ap);
    va_end(ap);
}
#endif

/* notify present a notification message to one or more destinations,
 * depending on the runtime parameters and the priority:
 *   -syslog_mask controls sending to the system log
 *   -stderr_mask controls sending to stderr
 *   -msgbox_mask controls sending to an interactive pop-up window, or
 *      a wait for a keypress on linux
 */
void
d_r_notify(syslog_event_type_t priority, bool internal, bool synch,
           IF_WINDOWS_(uint message_id) uint substitution_num, const char *prefix,
           const char *fmt, ...)
{
    char msgbuf[MAX_LOG_LENGTH];
    va_list ap;
    va_start(ap, fmt);
    /* XXX: the vsnprintf call is not needed in the most common case where
     * we are going to just os_syslog, but it gets pretty ugly to do that
     */
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    NULL_TERMINATE_BUFFER(msgbuf); /* always NULL terminate */
    /* not a good idea to assert here since we'll just die and lose original message,
     * so we don't check size return value and just go ahead and truncate
     */
    va_end(ap);

    LOG(GLOBAL, LOG_ALL, 1, "%s: %s\n", prefix, msgbuf);
    /* so can skip synchronizing when failure is in option parsing to avoid
     * infinite recursion, still could be issue with exception, but separate
     * recursive bailout will at least kill us then, FIXME, note will use
     * default masks below if original parse */
    if (synch)
        synchronize_dynamic_options(); /* TODO: dynamic THREAD mask */
    LOG(THREAD_GET, LOG_ALL, 1, "%s: %s\n", prefix, msgbuf);

#ifdef WINDOWS
    if (TEST(priority, dynamo_options.syslog_mask)) {
        if (internal) {
            if (TEST(priority, INTERNAL_OPTION(syslog_internal_mask))) {
                do_syslog(priority, message_id, 3, get_application_name(),
                          get_application_pid(), msgbuf);
            }
        } else {
            va_start(ap, fmt);
            os_syslog(priority, message_id, substitution_num, ap);
            va_end(ap);
        }
    }
#else
    /* syslog not yet implemented on linux, FIXME */
#endif

    if (TEST(priority, dynamo_options.stderr_mask))
        print_file(STDERR, "<%s>\n", msgbuf);

    if (TEST(priority, dynamo_options.msgbox_mask)) {
#ifdef WINDOWS
        /* XXX: could use os_countdown_msgbox (if ever implemented) here to
         * do a timed out messagebox, could then also replace the os_timeout in
         * vmareas.c
         */
        debugbox(msgbuf);
#else
        /* i#116/PR 394985: this won't work for apps that are
         * themselves reading from stdin, but this is a simple way to
         * pause and continue, allowing gdb to attach
         */
        if (DYNAMO_OPTION(pause_via_loop)) {
            while (DYNAMO_OPTION(pause_via_loop)) {
                /* infinite loop */
                os_thread_yield();
            }
        } else {
            char keypress;
            print_file(STDERR, "<press enter to continue>\n");
            os_read(STDIN, &keypress, sizeof(keypress));
        }
#endif
    }
}

/****************************************************************************
 * REPORTING DYNAMORIO PROBLEMS
 * Including assertions, curiosity asserts, API usage errors,
 * deadlock timeouts, internal exceptions, and the app modifying our memory.
 *
 * The following constants are for the pieces of a buffer we will send
 * to the event log, to diagnostics, and to stderr/msgbox/logfile.
 * It's static to avoid adding 500+ bytes to the stack on a critical
 * path, and so needs synchronization, but the risk of a problem with
 * the lock is worth getting a clear message on the first exception.
 *
 * Here's a sample of a report.  First four lines here are the
 * passed-in custom string fmt, subsequent are the options and call
 * stack, which are always appended:
 *   Platform exception at PC 0x15003075
 *   0xc0000005 0x00000000 0x15003075 0x15003075 0x00000001 0x00000037
 *   Registers: eax 0x00000000 ebx 0x00000000 ecx 0x177c9040 edx 0x177c9040
 *           esi 0x00000b56 edi 0x0000015f esp 0x177e3eb0 eflags 0x00010246
 *   Base: 0x15000000
 *   internal version, custom build
 *   -loglevel 2 -msgbox_mask 12 -stderr_mask 12
 *   0x00342ee8 0x150100f2
 *   0x00342f64 0x15010576
 *   0x00342f84 0x1503d77b
 *   0x00342fb0 0x1503f12c
 *   0x00342ff4 0x150470e9
 *   0x77f82b95 0x565308ec
 */
/* The magic number 271 comes from MAXIMUM_PATH (on WINDOWS = 260) + 11 for PR 204171
 * (in other words historical reasons). Xref PR 226547 we use a constant value here
 * instead of MAXIMUM_PATH since it has different length on Linux and makes this buffer
 * too long. */
#ifdef X64
#    define REPORT_MSG_MAX (271 + 17 * 8 + 8 * 23 + 2) /* wider, + more regs */
#elif defined(ARM)
#    define REPORT_MSG_MAX (271 + 17 * 8) /* more regs */
#else
#    define REPORT_MSG_MAX (271)
#endif
#define REPORT_LEN_VERSION 96
/* example: "\ninternal version, build 94201\n"
 * For custom builds, the build # is generated as follows
 * (cut-and-paste from Makefile):
 * # custom builds: 9XYYZ
 * # X = developer, YY = tree, Z = diffnum
 * # YY defaults to 1st 2 letters of CUR_TREE, unless CASENUM is defined,
 * # in which case it is the last 2 letters of CASENUM (all % 10 of course)
 */
#define REPORT_LEN_OPTIONS 324
/* still not long enough for ALL non-default options but I'll wager money we'll never
 * see this option string truncated, at least for non-internal builds
 * (famous last words?) => yes!  For clients this can get quite long.
 * List options from staging mode could be problematic though.
 */
#define REPORT_NUM_STACK 15
#ifdef X64
#    define REPORT_LEN_STACK_EACH (22 + 2 * 8)
#else
#    define REPORT_LEN_STACK_EACH 22
#endif
/* just frame ptr, ret addr: "0x0342fc7c 0x77f8c6dd\n" == 22 chars per line */
#define REPORT_LEN_STACK (REPORT_LEN_STACK_EACH) * (REPORT_NUM_STACK)
/* We have to stay under MAX_LOG_LENGTH so we limit to ~10 basenames */
#define REPORT_LEN_PRIVLIBS (45 * 10)
/* Not persistent across code cache execution, so not protected */
DECLARE_NEVERPROT_VAR(
    static char reportbuf[REPORT_MSG_MAX + REPORT_LEN_VERSION + REPORT_LEN_OPTIONS +
                          REPORT_LEN_STACK + REPORT_LEN_PRIVLIBS + 1],
    {
        0,
    });
DECLARE_CXTSWPROT_VAR(static mutex_t report_buf_lock, INIT_LOCK_FREE(report_buf_lock));
/* Avoid deadlock w/ nested reports */
DECLARE_CXTSWPROT_VAR(static thread_id_t report_buf_lock_owner, 0);

#define ASSERT_ROOM(reportbuf, curbuf, maxlen) \
    ASSERT(curbuf + maxlen < reportbuf + sizeof(reportbuf))

/* random number generator */
DECLARE_CXTSWPROT_VAR(static mutex_t prng_lock, INIT_LOCK_FREE(prng_lock));

#ifdef DEBUG
/* callers should play it safe - no memory allocations, no grabbing locks */
bool
under_internal_exception()
{
#    ifdef DEADLOCK_AVOIDANCE
    /* ASSUMPTION: reading owner field is atomic */
    return (report_buf_lock.owner == d_r_get_thread_id());
#    else
    /* mutexes normally don't have an owner, stay safe no matter who owns */
    return mutex_testlock(&report_buf_lock);
#    endif /* DEADLOCK_AVOIDANCE */
}
#endif /* DEBUG */

/* Defaults, overridable by the client (i#1470) */
const char *exception_label_core = PRODUCT_NAME;
static const char *exception_report_url = BUG_REPORT_URL;
const char *exception_label_client = "Client";

/* We allow clients to display their version instead of DR's */
static char display_version[REPORT_LEN_VERSION];

/* HACK: to avoid duplicating the prefix of the event log message, we
 * skip it for the SYSLOG, but not the other notifications
 */
static char exception_prefix[MAXIMUM_PATH];

static inline size_t
report_exception_skip_prefix(void)
{
    return strlen(exception_prefix);
}

static char client_exception_prefix[MAXIMUM_PATH];

static inline size_t
report_client_exception_skip_prefix(void)
{
    return strlen(client_exception_prefix);
}

void
set_exception_strings(const char *override_label, const char *override_url)
{
    if (dynamo_initialized)
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    if (override_url != NULL)
        exception_report_url = override_url;
    if (override_label != NULL)
        exception_label_core = override_label;
    ASSERT(strlen(CRASH_NAME) == strlen(STACK_OVERFLOW_NAME));
    snprintf(exception_prefix, BUFFER_SIZE_ELEMENTS(exception_prefix), "%s %s at PC " PFX,
             exception_label_core, CRASH_NAME, 0);
    NULL_TERMINATE_BUFFER(exception_prefix);
    if (override_label != NULL)
        exception_label_client = override_label;
    snprintf(client_exception_prefix, BUFFER_SIZE_ELEMENTS(client_exception_prefix),
             "%s %s at PC " PFX, exception_label_client, CRASH_NAME, 0);
    NULL_TERMINATE_BUFFER(client_exception_prefix);
#ifdef WINDOWS
    debugbox_setup_title();
#endif
    if (dynamo_initialized)
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
}

void
set_display_version(const char *ver)
{
    if (dynamo_initialized)
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    snprintf(display_version, BUFFER_SIZE_ELEMENTS(display_version), "%s", ver);
    NULL_TERMINATE_BUFFER(display_version);
    if (dynamo_initialized)
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
}

/* Fine to pass NULL for dcontext, will obtain it for you.
 * If TEST(DUMPCORE_INTERNAL_EXCEPTION, dumpcore_flag), does a full SYSLOG;
 * else, does a SYSLOG_INTERNAL_ERROR.
 * Fine to pass NULL for report_ebp: will use current ebp for you.
 */
void
report_dynamorio_problem(dcontext_t *dcontext, uint dumpcore_flag, app_pc exception_addr,
                         app_pc report_ebp, const char *fmt, ...)
{
    /* WARNING: this routine is called for fatal errors, and
     * a fault in DR means that potentially anything could be
     * inconsistent or corrupted!  Do not grab locks or traverse
     * data structures or read memory if you can avoid it!
     */
    char *curbuf;
    ptr_uint_t *pc;
    uint num;
    int len;
    va_list ap;

    /* synchronize dynamic options */
    synchronize_dynamic_options();

    ASSERT(sizeof(reportbuf) < MAX_LOG_LENGTH);
    if (dcontext == NULL)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL)
        dcontext = GLOBAL_DCONTEXT;

    if (report_buf_lock_owner == d_r_get_thread_id()) {
        /* nested report: can't do much except bail on inner */
        return;
    }
    d_r_mutex_lock(&report_buf_lock);
    report_buf_lock_owner = d_r_get_thread_id();
    /* we assume the caller does a DO_ONCE to prevent hanging on a
     * fault in this routine, if called to report a fatal error.
     */

    /* now build up the report */
    curbuf = reportbuf;

    ASSERT_ROOM(reportbuf, curbuf, REPORT_MSG_MAX);
    va_start(ap, fmt);
    len = vsnprintf(curbuf, REPORT_MSG_MAX, fmt, ap);
    curbuf += (len == -1 ? REPORT_MSG_MAX : (len < 0 ? 0 : len));
    va_end(ap);

    if (display_version[0] != '\0') {
        len = snprintf(curbuf, REPORT_LEN_VERSION, "\n%s\n", display_version);
        curbuf += (len == -1 ? REPORT_LEN_VERSION : (len < 0 ? 0 : len));
    } else {
        /* don't use dynamorio_version_string, we don't need copyright notice */
        ASSERT_ROOM(reportbuf, curbuf, REPORT_LEN_VERSION);
        len = snprintf(curbuf, REPORT_LEN_VERSION, "\n%s, %s\n", VERSION_NUMBER_STRING,
                       BUILD_NUMBER_STRING);
        curbuf += (len == -1 ? REPORT_LEN_VERSION : (len < 0 ? 0 : len));
    }

    ASSERT_ROOM(reportbuf, curbuf, REPORT_LEN_OPTIONS);
    /* leave room for newline */
    get_dynamo_options_string(&dynamo_options, curbuf, REPORT_LEN_OPTIONS - 1, true);
    /* get_dynamo_options_string will null-terminate even if truncates */
    curbuf += strlen(curbuf);
    *(curbuf++) = '\n';

    /* print just frame ptr and ret addr for top of call stack */
    ASSERT_ROOM(reportbuf, curbuf, REPORT_LEN_STACK);
    if (report_ebp == NULL) {
        GET_FRAME_PTR(report_ebp);
    }
    for (num = 0, pc = (ptr_uint_t *)report_ebp; num < REPORT_NUM_STACK && pc != NULL &&
         is_readable_without_exception_query_os_noblock((app_pc)pc, 2 * sizeof(reg_t));
         num++, pc = (ptr_uint_t *)*pc) {
        len = snprintf(curbuf, REPORT_LEN_STACK_EACH, PFX " " PFX "\n", pc, *(pc + 1));
        curbuf += (len == -1 ? REPORT_LEN_STACK_EACH : (len < 0 ? 0 : len));
    }
    /* Only walk the module list if we think the data structs are safe */
    if (!TEST(DUMPCORE_INTERNAL_EXCEPTION, dumpcore_flag)) {
        size_t sofar = 0;
        /* We decided it's better to include the paths even if it means we may
         * not fit all the modules (i#968).  We plan to add the modules to the
         * forensics file to have complete info (i#972).
         */
        privload_print_modules(true /*include path*/, false /*no lock*/, curbuf,
                               REPORT_LEN_PRIVLIBS, &sofar);
        curbuf += sofar;
    }

    /* SYSLOG_INTERNAL and diagnostics expect no trailing newline */
    if (*(curbuf - 1) == '\n') /* won't be if we truncated something */
        curbuf--;
    /* now we for sure have room for \0 */
    *curbuf = '\0';
    /* now done with reportbuf */

    if (TEST(dumpcore_flag, DYNAMO_OPTION(dumpcore_mask)) && DYNAMO_OPTION(live_dump)) {
        /* non-fatal coredumps attempted before printing further diagnostics */
        os_dump_core(reportbuf);
    }

    /* we already synchronized the options at the top of this function and we
     * might be stack critical so use _NO_OPTION_SYNCH */
    if (TEST(DUMPCORE_INTERNAL_EXCEPTION, dumpcore_flag) ||
        TEST(DUMPCORE_CLIENT_EXCEPTION, dumpcore_flag)) {
        char saddr[IF_X64_ELSE(19, 11)];
        snprintf(saddr, BUFFER_SIZE_ELEMENTS(saddr), PFX, exception_addr);
        NULL_TERMINATE_BUFFER(saddr);
        if (TEST(DUMPCORE_INTERNAL_EXCEPTION, dumpcore_flag)) {
            SYSLOG_NO_OPTION_SYNCH(
                SYSLOG_CRITICAL, EXCEPTION, 7 /*#args*/, get_application_name(),
                get_application_pid(), exception_label_core,
                TEST(DUMPCORE_STACK_OVERFLOW, dumpcore_flag) ? STACK_OVERFLOW_NAME
                                                             : CRASH_NAME,
                saddr, exception_report_url,
                /* skip the prefix since the event log string
                 * already has it */
                reportbuf + report_exception_skip_prefix());
        } else {
            SYSLOG_NO_OPTION_SYNCH(
                SYSLOG_CRITICAL, CLIENT_EXCEPTION, 7 /*#args*/, get_application_name(),
                get_application_pid(), exception_label_client,
                TEST(DUMPCORE_STACK_OVERFLOW, dumpcore_flag) ? STACK_OVERFLOW_NAME
                                                             : CRASH_NAME,
                saddr, exception_report_url,
                reportbuf + report_client_exception_skip_prefix());
        }
    } else if (TEST(DUMPCORE_ASSERTION, dumpcore_flag)) {
        /* We need to report ASSERTS in DEBUG=1 INTERNAL=0 builds since we're still
         * going to kill the process. Xref PR 232783. d_r_internal_error() already
         * obfuscated the which file info. */
        SYSLOG_NO_OPTION_SYNCH(SYSLOG_ERROR, INTERNAL_SYSLOG_ERROR, 3,
                               get_application_name(), get_application_pid(), reportbuf);
    } else if (TEST(DUMPCORE_CURIOSITY, dumpcore_flag)) {
        SYSLOG_INTERNAL_NO_OPTION_SYNCH(SYSLOG_WARNING, "%s", reportbuf);
    } else {
        SYSLOG_INTERNAL_NO_OPTION_SYNCH(SYSLOG_ERROR, "%s", reportbuf);
    }

    /* no forensics files for usage error */
    if (dumpcore_flag != DUMPCORE_FATAL_USAGE_ERROR) {
        /* NULL for the threat id
         * We always assume BAD state, even for curiosity asserts, etc., since
         * diagnostics grabs memory when ok and we can't have that at arbitrary points!
         */
        report_diagnostics(reportbuf, NULL, NO_VIOLATION_BAD_INTERNAL_STATE);
    }

    /* Print out pretty call stack to logfile where we have plenty of room.
     * This avoids grabbing a lock b/c print_symbolic_address() checks
     * under_internal_exception().  However we cannot include module info b/c
     * that grabs locks: hence the fancier callstack in the main report
     * for client and app crashes but not DR crashes.
     */
    DOLOG(1, LOG_ALL, {
        if (TEST(DUMPCORE_INTERNAL_EXCEPTION, dumpcore_flag))
            dump_callstack(exception_addr, report_ebp, THREAD, DUMP_NOT_XML);
        else
            dump_dr_callstack(THREAD);
    });

    report_buf_lock_owner = 0;
    d_r_mutex_unlock(&report_buf_lock);

    if (dumpcore_flag != DUMPCORE_CURIOSITY) {
        /* print out stats, can't be done inside the report_buf_lock
         * because of non-trivial lock rank order violation on the
         * snapshot_lock */
        DOLOG(1, LOG_ALL, {
            dump_global_stats(false);
            if (dcontext != GLOBAL_DCONTEXT)
                dump_thread_stats(dcontext, false);
        });
    }

    if (TEST(dumpcore_flag, DYNAMO_OPTION(dumpcore_mask)) && !DYNAMO_OPTION(live_dump)) {
        /* fatal coredump goes last */
        os_dump_core(reportbuf);
    }
}

void
report_app_problem(dcontext_t *dcontext, uint appfault_flag, app_pc pc, app_pc report_ebp,
                   const char *fmt, ...)
{
    char buf[MAX_LOG_LENGTH];
    size_t sofar = 0;
    va_list ap;
    char excpt_addr[IF_X64_ELSE(20, 12)];

    if (!TEST(appfault_flag, DYNAMO_OPTION(appfault_mask)))
        return;

    snprintf(excpt_addr, BUFFER_SIZE_ELEMENTS(excpt_addr), PFX, pc);
    NULL_TERMINATE_BUFFER(excpt_addr);

    va_start(ap, fmt);
    vprint_to_buffer(buf, BUFFER_SIZE_ELEMENTS(buf), &sofar, fmt, ap);
    va_end(ap);

    print_to_buffer(buf, BUFFER_SIZE_ELEMENTS(buf), &sofar, "Callstack:\n");
    if (report_ebp == NULL)
        GET_FRAME_PTR(report_ebp);
    /* We decided it's better to include the paths even if it means we may
     * not fit all the modules (i#968).  A forensics file can be requested
     * to get full info.
     */
    dump_callstack_to_buffer(buf, BUFFER_SIZE_ELEMENTS(buf), &sofar, pc, report_ebp,
                             CALLSTACK_MODULE_INFO | CALLSTACK_MODULE_PATH);

    SYSLOG(SYSLOG_WARNING, APP_EXCEPTION, 4, get_application_name(),
           get_application_pid(), excpt_addr, buf);

    report_diagnostics(buf, NULL, NO_VIOLATION_OK_INTERNAL_STATE);

    if (TEST(DUMPCORE_APP_EXCEPTION, DYNAMO_OPTION(dumpcore_mask)))
        os_dump_core("application fault");
}

bool
is_readable_without_exception_try(byte *pc, size_t size)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    /* note we need a dcontext for a TRY block */
    if (dcontext == NULL) {
        /* FIXME: should rename the current
         * is_readable_without_exception() to
         * is_readable_without_exception_os_read(). On each platform
         * we should pick the fastest implementation for the
         * non-faulting common case as the default version of
         * is_readable_without_exception().  Some callers may still call a
         * specific version if the fast path is not as common.
         */
        return is_readable_without_exception(pc, size);
    }

    TRY_EXCEPT(
        dcontext,
        {
            byte *check_pc = (byte *)ALIGN_BACKWARD(pc, PAGE_SIZE);
            if (size > (size_t)((byte *)POINTER_MAX - pc)) {
                ASSERT_NOT_TESTED();
                size = (byte *)POINTER_MAX - pc;
            }
            do {
                PROBE_READ_PC(check_pc);
                /* note the minor perf benefit - we check the whole loop
                 * in a single TRY/EXCEPT, and no system calls xref
                 * is_readable_without_exception() [based on safe_read]
                 * and is_readable_without_exception_query_os() [based on
                 * query_virtual_memory].
                 */

                check_pc += PAGE_SIZE;
            } while (check_pc != 0 /*overflow*/ && check_pc < pc + size);
            /* TRY usage note: can't return here */
        },
        { /* EXCEPT */
          /* no state to preserve */
          return false;
        });

    return true;
}

bool
is_string_readable_without_exception(char *str, size_t *str_length /* OPTIONAL OUT */)
{
    size_t length = 0;
    dcontext_t *dcontext = get_thread_private_dcontext();

    if (str == NULL)
        return false;

    if (dcontext != NULL) {
        TRY_EXCEPT(
            dcontext, /* try */
            {
                length = strlen(str);
                if (str_length != NULL)
                    *str_length = length;
                /* NOTE - can't return here (try usage restriction) */
            },
            /* except */ { return false; });
        return true;
    } else {
        /* ok have to do this the hard way... */
        char *cur_page = (char *)ALIGN_BACKWARD(str, PAGE_SIZE);
        char *cur_str = str;
        do {
            if (!is_readable_without_exception((byte *)cur_str,
                                               (cur_page + PAGE_SIZE) - cur_str)) {
                return false;
            }
            while (cur_str < cur_page + PAGE_SIZE) {
                if (*cur_str == '\0') {
                    if (str_length != NULL)
                        *str_length = length;
                    return true;
                }
                cur_str++;
                length++;
            }
            cur_page += PAGE_SIZE;
            ASSERT(cur_page == cur_str && ALIGNED(cur_page, PAGE_SIZE));
        } while (true);
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool
safe_write_try_except(void *base, size_t size, const void *in_buf, size_t *bytes_written)
{
    uint prot;
    byte *region_base;
    size_t region_size;
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool res = false;
    if (bytes_written != NULL)
        *bytes_written = 0;

    if (dcontext != NULL) {
        TRY_EXCEPT(dcontext,
                   {
                       /* We abort on the 1st fault, just like safe_read */
                       memcpy(base, in_buf, size);
                       res = true;
                   },
                   {
                       /* EXCEPT */
                       /* nothing: res is already false */
                   });
    } else {
        /* this is subject to races, but should only happen at init/attach when
         * there should only be one live thread.
         */
        /* on x86 must be readable to be writable so start with that */
        if (is_readable_without_exception(base, size) &&
            IF_UNIX_ELSE(get_memory_info_from_os, get_memory_info)(base, &region_base,
                                                                   &region_size, &prot) &&
            TEST(MEMPROT_WRITE, prot)) {
            size_t bytes_checked = region_size - ((byte *)base - region_base);
            while (bytes_checked < size) {
                if (!IF_UNIX_ELSE(get_memory_info_from_os, get_memory_info)(
                        region_base + region_size, &region_base, &region_size, &prot) ||
                    !TEST(MEMPROT_WRITE, prot))
                    return false;
                bytes_checked += region_size;
            }
        } else {
            return false;
        }
        /* ok, checks passed do the copy, FIXME - because of races this isn't safe! */
        memcpy(base, in_buf, size);
        res = true;
    }

    if (res) {
        if (bytes_written != NULL)
            *bytes_written = size;
    }
    return res;
}

const char *
memprot_string(uint prot)
{
    switch (prot) {
    case (MEMPROT_READ | MEMPROT_WRITE | MEMPROT_EXEC): return "rwx";
    case (MEMPROT_READ | MEMPROT_WRITE): return "rw-";
    case (MEMPROT_READ | MEMPROT_EXEC): return "r-x";
    case (MEMPROT_READ): return "r--";
    case (MEMPROT_WRITE | MEMPROT_EXEC): return "-wx";
    case (MEMPROT_WRITE): return "-w-";
    case (MEMPROT_EXEC): return "--x";
    case (0): return "---";
    }
    return "<error>";
}

/* returns true if every byte in the region addr to addr+size is set to val */
bool
is_region_memset_to_char(byte *addr, size_t size, byte val)
{
    /* FIXME : we could make this much faster with arch specific implementation
     * (for x86 repe scasd w/proper alignment handling) */
    size_t i;
    for (i = 0; i < size; i++) {
        if (*addr++ != val)
            return false;
    }
    return true;
}

/* returns pointer to first char of string that matches either c1 or c2
 * or NULL if can't find */
char *
double_strchr(char *string, char c1, char c2)
{
    while (*string != '\0') {
        if (*string == c1 || *string == c2) {
            return string;
        }
        string++;
    }
    return NULL;
}

#ifndef WINDOWS
/* returns pointer to last char of string that matches either c1 or c2
 * or NULL if can't find */
const char *
double_strrchr(const char *string, char c1, char c2)
{
    const char *ret = NULL;
    while (*string != '\0') {
        if (*string == c1 || *string == c2) {
            ret = string;
        }
        string++;
    }
    return ret;
}
#else
/* in inject_shared.c, FIXME : move both copies to a common location */
#endif

#ifdef WINDOWS
/* Just like wcslen, but if the string is >= MAX characters long returns MAX
 * whithout interrogating past str+MAX.  NOTE - this matches most library
 * implementations, but does NOT work the same way as the strnlen etc.
 * functions in the hotpatch2 module (they return MAX+1 for strings > MAX).
 * The hotpatch2 module implementation is scheduled to be changed. FIXME -
 * eventually would be nice to share the various string routines used both by
 * the core and the hotpatch2 module. */
size_t
our_wcsnlen(const wchar_t *str, size_t max)
{
    const wchar_t *s = str;
    size_t i = 0;

    while (i < max && *s != L'\0') {
        i++;
        s++;
    }

    return i;
}
#endif

static int
strcasecmp_with_wildcards(const char *regexp, const char *consider)
{
    char cr, cc;
    while (true) {
        if (*regexp == '\0') {
            if (*consider == '\0')
                return 0;
            return -1;
        } else if (*consider == '\0')
            return 1;
        ASSERT((sbyte)*regexp != EOF && (sbyte)*consider != EOF);
        cr = (char)tolower(*regexp);
        cc = (char)tolower(*consider);
        if (cr != '?' && cr != cc) {
            if (cr < cc)
                return -1;
            else
                return 1;
        }
        regexp++;
        consider++;
    }
}

bool
str_case_prefix(const char *str, const char *pfx)
{
    while (true) {
        if (*pfx == '\0')
            return true;
        if (*str == '\0')
            return false;
        if (tolower(*str) != tolower(*pfx))
            return false;
        str++;
        pfx++;
    }
    return false;
}

static bool
check_filter_common(const char *filter, const char *short_name, bool wildcards)
{
    const char *next, *prev;
    /* FIXME: can we shrink this?  not using full paths here */
    char consider[MAXIMUM_PATH];
    bool done = false;

    ASSERT(short_name != NULL && filter != NULL);
    /* FIXME: consider replacing most of this with
       strtok_r(copy_filter, ";", &pos) */
    prev = filter;
    do {
        next = strchr(prev, ';');
        if (next == NULL) {
            next = prev + strlen(prev);
            if (next == prev)
                break;
            done = true;
        }
        strncpy(consider, prev, MIN(BUFFER_SIZE_ELEMENTS(consider), (next - prev)));
        consider[next - prev] = '\0'; /* if max no null */
        LOG(THREAD_GET, LOG_ALL, 3, "considering \"%s\" == \"%s\"\n", consider,
            short_name);
        if (wildcards && strcasecmp_with_wildcards(consider, short_name) == 0)
            return true;
        else if (strcasecmp(consider, short_name) == 0)
            return true;
        prev = next + 1;
    } while (!done);
    return false;
}

bool
check_filter(const char *filter, const char *short_name)
{
    return check_filter_common(filter, short_name, false /*no wildcards*/);
}

bool
check_filter_with_wildcards(const char *filter, const char *short_name)
{
    return check_filter_common(filter, short_name, true /*allow wildcards*/);
}

static char logdir[MAXIMUM_PATH];
static bool logdir_initialized = false;
static char basedir[MAXIMUM_PATH];
static bool basedir_initialized = false;
/* below used in the create_log_dir function to avoid having it on the stack
 * on what is a critical path for stack depth (diagnostics->create_log_dir->
 * get_parameter */
static char old_basedir[MAXIMUM_PATH];
/* this lock is recursive because current implementation recurses to create the
 * basedir when called to create the logdir before the basedir is created, is
 * also useful in case we receive an exception in the create_log_dir function
 * since it is called in the diagnostics path, we should relook this though
 * as is probably not the best way to avoid the diagnostics problem FIXME */
DECLARE_CXTSWPROT_VAR(static recursive_lock_t logdir_mutex,
                      INIT_RECURSIVE_LOCK(logdir_mutex));

/* enable creating a new base logdir (for a fork, e.g.) */
void
enable_new_log_dir()
{
    logdir_initialized = false;
}

void
create_log_dir(int dir_type)
{
#ifdef UNIX
    char *pre_execve = getenv(DYNAMORIO_VAR_EXECVE_LOGDIR);
    DEBUG_DECLARE(bool sharing_logdir = false;)
#endif
    /* synchronize */
    acquire_recursive_lock(&logdir_mutex);
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
#ifdef UNIX
    if (dir_type == PROCESS_DIR && pre_execve != NULL) {
        /* if this app has a logdir option or config, that should trump sharing
         * the pre-execve logdir.  a logdir env var should not.
         */
        bool is_env;
        if (IS_STRING_OPTION_EMPTY(logdir) &&
            (get_config_val_ex(DYNAMORIO_VAR_LOGDIR, NULL, &is_env) == NULL || is_env)) {
            /* use same dir as pre-execve! */
            DODEBUG(sharing_logdir = true;);
            strncpy(logdir, pre_execve, BUFFER_SIZE_ELEMENTS(logdir));
            NULL_TERMINATE_BUFFER(logdir); /* if max no null */
            logdir_initialized = true;
        }
        /* important to remove it, don't want to propagate to forked children */
        /* i#909: unsetenv is unsafe as it messes up auxv access, so we disable */
        disable_env(DYNAMORIO_VAR_EXECVE_LOGDIR);
        /* check that it's gone: we've had problems with unsetenv */
        ASSERT(getenv(DYNAMORIO_VAR_EXECVE_LOGDIR) == NULL);
    }
#endif
    /* used to be an else: leaving indentation though */
    if (dir_type == BASE_DIR) {
        int retval;
        ASSERT(sizeof(basedir) == sizeof(old_basedir));
        strncpy(old_basedir, basedir, sizeof(old_basedir));
        /* option takes precedence over config var */
        if (IS_STRING_OPTION_EMPTY(logdir)) {
            retval = d_r_get_parameter(PARAM_STR(DYNAMORIO_VAR_LOGDIR), basedir,
                                       BUFFER_SIZE_ELEMENTS(basedir));
            if (IS_GET_PARAMETER_FAILURE(retval))
                basedir[0] = '\0';
        } else {
            string_option_read_lock();
            strncpy(basedir, DYNAMO_OPTION(logdir), BUFFER_SIZE_ELEMENTS(basedir));
            string_option_read_unlock();
        }
        basedir[sizeof(basedir) - 1] = '\0';
        if (!basedir_initialized || strncmp(old_basedir, basedir, sizeof(basedir))) {
            /* need to create basedir, is changed or not yet created */
            basedir_initialized = true;
            /* skip creating dir basedir if is empty */
            if (basedir[0] == '\0') {
#ifndef STATIC_LIBRARY
                SYSLOG(SYSLOG_WARNING, WARNING_EMPTY_OR_NONEXISTENT_LOGDIR_KEY, 2,
                       get_application_name(), get_application_pid());
#endif
            } else {
                if (!os_create_dir(basedir, CREATE_DIR_ALLOW_EXISTING)) {
                    /* try to create full path */
                    char swap;
                    char *end = double_strchr(basedir, DIRSEP, ALT_DIRSEP);
                    bool res;
#ifdef WINDOWS
                    /* skip the drive */
                    if (end != NULL && end > basedir && *(end - 1) == ':')
                        end = double_strchr(++end, DIRSEP, ALT_DIRSEP);
#endif
                    while (end) {
                        swap = *end;
                        *end = '\0';
                        res = os_create_dir(basedir, CREATE_DIR_ALLOW_EXISTING);
                        *end = swap;
                        end = double_strchr(++end, DIRSEP, ALT_DIRSEP);
                    }
                    res = os_create_dir(basedir, CREATE_DIR_ALLOW_EXISTING);
                    /* check for success */
                    if (!res) {
                        SYSLOG(SYSLOG_ERROR, ERROR_UNABLE_TO_CREATE_BASEDIR, 3,
                               get_application_name(), get_application_pid(), basedir);
                        /* everything should work out fine, individual log
                         * dirs will also fail to open and just won't be
                         * logged to */
                    }
                }
            }
        }
    }
    /* only create one logging directory (i.e. not dynamic) */
    else if (dir_type == PROCESS_DIR && !logdir_initialized) {
        char *base = basedir;
        if (!basedir_initialized) {
            create_log_dir(BASE_DIR);
        }
        ASSERT(basedir_initialized);
        logdir_initialized = true;
        /* skip creating if basedir is empty */
        if (*base != '\0') {
            if (!get_unique_logfile("", logdir, sizeof(logdir), true, NULL)) {
                SYSLOG_INTERNAL_WARNING("Unable to create log directory %s", logdir);
            }
        }
    }

    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    release_recursive_lock(&logdir_mutex);

#ifdef DEBUG
    if (d_r_stats != NULL) {
        /* if null, we're trying to report an error (probably via a core dump),
         * so who cares if we lose logdir name */
        strncpy(d_r_stats->logdir, logdir, sizeof(d_r_stats->logdir));
        d_r_stats->logdir[sizeof(d_r_stats->logdir) - 1] = '\0'; /* if max no null */
    }
    if (dir_type == PROCESS_DIR
#    ifdef UNIX
        && !sharing_logdir
#    endif
    )
        SYSLOG_INTERNAL_INFO("log dir=%s", logdir);
#endif /* DEBUG */
}

/* Copies the name of the specified directory into buffer, returns true if
 * the specified buffer has been initialized (if it hasn't then no copying is
 * done).
 * Will not copy more than *buffer_length bytes and does not ensure null
 * termination.
 * buffer can be NULL
 * buffer_length can be NULL, but only if buffer is NULL
 * on return (if it is not NULL) *buffer_length will hold the length of the
 * specified directory's name (including the terminating NULL, i.e. the number
 * of chars written to the buffer assuming the buffer was not NULL and large
 * enough)
 */
bool
get_log_dir(log_dir_t dir_type, char *buffer, uint *buffer_length)
{
    bool target_initialized = false;
    char *target_dir = NULL;
    ASSERT(buffer == NULL || buffer_length != NULL);
    acquire_recursive_lock(&logdir_mutex);
    if (dir_type == BASE_DIR) {
        target_dir = basedir;
        target_initialized = basedir_initialized;
    } else if (dir_type == PROCESS_DIR) {
        target_dir = logdir;
        target_initialized = logdir_initialized;
    } else {
        /* should never get here */
        ASSERT(false);
    }
    if (buffer != NULL && target_initialized) {
        strncpy(buffer, target_dir, *buffer_length);
    }
    if (buffer_length != NULL && target_initialized) {
        ASSERT_TRUNCATE(*buffer_length, uint, strlen(target_dir) + 1);
        *buffer_length = (uint)strlen(target_dir) + 1;
    }
    release_recursive_lock(&logdir_mutex);
    return target_initialized;
}

/*#ifdef UNIX
 *  N.B.: if you create a log file, you'll probably want to create a new one
 *  upon a fork.  Should we require a callback passed in to this routine?
 *  For clients we have a dynamorio_fork_init routine.  For internal modules,
 *  for now make your own fork_init routine.
 *  For closing on fork, since could have many threads with their own files
 *  open, we use our fd_table to close.
 *#endif
 */
file_t
open_log_file(const char *basename, char *finalname_with_path, uint maxlen)
{
    file_t file;
    char name[MAXIMUM_PATH];
    uint name_size = BUFFER_SIZE_ELEMENTS(name);
    /* all logfiles are auto-closed on fork; we then make new ones */
    uint flags = OS_OPEN_WRITE | OS_OPEN_ALLOW_LARGE | OS_OPEN_CLOSE_ON_FORK;
    name[0] = '\0';

    if (DYNAMO_OPTION(log_to_stderr))
        return STDERR;

    if (!get_log_dir(PROCESS_DIR, name, &name_size)) {
        create_log_dir(PROCESS_DIR);
        if (!get_log_dir(PROCESS_DIR, name, &name_size)) {
            ASSERT_NOT_REACHED();
        }
    }
    NULL_TERMINATE_BUFFER(name);
    /* skip if logdir empty */
    if (name[0] == '\0')
        return INVALID_FILE;
    snprintf(&name[strlen(name)], BUFFER_SIZE_ELEMENTS(name) - strlen(name),
             "%c%s.%d." TIDFMT ".html", DIRSEP, basename,
             get_thread_num(d_r_get_thread_id()), d_r_get_thread_id());
    NULL_TERMINATE_BUFFER(name);
#ifdef UNIX
    if (post_execve) /* reuse same log file */
        file = os_open_protected(name, flags | OS_OPEN_APPEND);
    else
#endif
        file = os_open_protected(name, flags | OS_OPEN_REQUIRE_NEW);
    if (file == INVALID_FILE) {
        SYSLOG_INTERNAL_WARNING_ONCE("Cannot create log file %s", name);
        /* everything should work out fine, log statements will just fail to
         * write since invalid handle */
    }
    /* full path is often too long, so just print final dir and file name */
#ifdef UNIX
    if (!post_execve) {
#endif
        /* Note that we won't receive a message for the global logfile
         * since the caller won't have set it yet.  However, we will get
         * all thread log files logged here. */
        LOG(GLOBAL, LOG_THREADS, 1, "created log file %d=%s\n", file,
            double_strrchr(name, DIRSEP, ALT_DIRSEP) + 1);
#ifdef UNIX
    }
#endif
    if (finalname_with_path != NULL) {
        strncpy(finalname_with_path, name, maxlen);
        finalname_with_path[maxlen - 1] = '\0'; /* if max no null */
    }
    return file;
}

void
close_log_file(file_t f)
{
    if (f == STDERR)
        return;
    os_close_protected(f);
}

/* Generalize further as needed
 * Creates a unique file or directory of the form
 * BASEDIR/[app_name].[pid].<unique num of up to 8 digits>[file_type]
 * If the filename_buffer is not NULL, the filename of the obtained file
 * is copied there. For creating a directory the file argument is expected
 * to be null.  Return true if the requested file or directory was created
 * and, in the case of a file, returns a handle to file in the file argument.
 */
bool
get_unique_logfile(const char *file_type, char *filename_buffer, uint maxlen,
                   bool open_directory, file_t *file)
{
    char buf[MAXIMUM_PATH];
    uint size = BUFFER_SIZE_ELEMENTS(buf), counter = 0, base_offset;
    bool success = false;
    ASSERT((open_directory && file == NULL) || (!open_directory && file != NULL));
    if (!open_directory)
        *file = INVALID_FILE;
    create_log_dir(BASE_DIR);
    if (get_log_dir(BASE_DIR, buf, &size)) {
        NULL_TERMINATE_BUFFER(buf);
        ASSERT_TRUNCATE(base_offset, uint, strlen(buf));
        base_offset = (uint)strlen(buf);
        buf[base_offset++] = DIRSEP;
        size = BUFFER_SIZE_ELEMENTS(buf) - base_offset;
        do {
            snprintf(&(buf[base_offset]), size, "%s.%s.%.8d%s", get_app_name_for_path(),
                     get_application_pid(), counter, file_type);
            NULL_TERMINATE_BUFFER(buf);
            if (open_directory) {
                success = os_create_dir(buf, CREATE_DIR_REQUIRE_NEW);
            } else {
                *file = os_open(buf, OS_OPEN_REQUIRE_NEW | OS_OPEN_WRITE);
                success = (*file != INVALID_FILE);
            }
        } while (!success && counter++ < 99999999 && os_file_exists(buf, open_directory));
        DOLOG(1, LOG_ALL, {
            if (!success)
                LOG(GLOBAL, LOG_ALL, 1, "Failed to create unique logfile %s\n", buf);
            else
                LOG(GLOBAL, LOG_ALL, 1, "Created unique logfile %s\n", buf);
        });
    }

    /* copy the filename over if we have a valid buffer */
    if (NULL != filename_buffer) {
        strncpy(filename_buffer, buf, maxlen);
        filename_buffer[maxlen - 1] = '\0'; /* NULL terminate */
    }

    return success;
}

const char *
get_app_name_for_path()
{
    return get_short_name(get_application_name());
}

const char *
get_short_name(const char *exename)
{
    const char *exe;
    exe = double_strrchr(exename, DIRSEP, ALT_DIRSEP);
    if (exe == NULL)
        exe = exename;
    else
        exe++; /* skip (back)slash */
    return exe;
}

/****************************************************************************/

#ifdef DEBUG
#    ifdef FRAGMENT_SIZES_STUDY /* to isolate sqrt dependence */
/* given an array of size size of integers, computes and prints the
 * min, max, mean, and stddev
 */
void
print_statistics(int *data, int size)
{
    int i;
    int min, max;
    double mean, stddev, sum;
    uint top, bottom;
    const char *sign;
    /* our context switch does not save & restore floating point state,
     * so we have to do it here!
     */
    PRESERVE_FLOATING_POINT_STATE_START();

    sum = 0.;
    min = max = data[0];
    for (i = 0; i < size; i++) {
        if (data[i] < min)
            min = data[i];
        if (data[i] > max)
            max = data[i];
        sum += data[i];
    }
    mean = sum / (double)size;

    stddev = 0.;
    for (i = 0; i < size; i++) {
        double diff = ((double)data[i]) - mean;
        stddev += diff * diff;
    }
    stddev /= (double)size;
    /* FIXME i#46: We need a private sqrt impl.  libc's sqrt can actually
     * clobber errno, too!
     */
    ASSERT(!DYNAMO_OPTION(early_inject) &&
           "FRAGMENT_SIZES_STUDY incompatible with early injection");
    stddev = sqrt(stddev);

    LOG(GLOBAL, LOG_ALL, 0, "\t#      = %9d\n", size);
    LOG(GLOBAL, LOG_ALL, 0, "\tmin    = %9d\n", min);
    LOG(GLOBAL, LOG_ALL, 0, "\tmax    = %9d\n", max);
    double_print(mean, 1, &top, &bottom, &sign);
    LOG(GLOBAL, LOG_ALL, 0, "\tmean   =   %s%7u.%.1u\n", sign, top, bottom);
    double_print(stddev, 1, &top, &bottom, &sign);
    LOG(GLOBAL, LOG_ALL, 0, "\tstddev =   %s%7u.%.1u\n", sign, top, bottom);

    PRESERVE_FLOATING_POINT_STATE_END();
}
#    endif

/* FIXME: these should be under ifdef STATS, not necessarily ifdef DEBUG */
void
stats_thread_init(dcontext_t *dcontext)
{
    thread_local_statistics_t *new_thread_stats;
    if (!INTERNAL_OPTION(thread_stats))
        return; /* dcontext->thread_stats stays NULL */

    new_thread_stats =
        HEAP_TYPE_ALLOC(dcontext, thread_local_statistics_t, ACCT_STATS, UNPROTECTED);
    LOG(THREAD, LOG_STATS, 2, "thread_stats=" PFX " size=%d\n", new_thread_stats,
        sizeof(thread_local_statistics_t));
    /* initialize any thread stats bookkeeping fields before assigning to dcontext */
    memset(new_thread_stats, 0x0, sizeof(thread_local_statistics_t));
    new_thread_stats->thread_id = d_r_get_thread_id();
    ASSIGN_INIT_LOCK_FREE(new_thread_stats->thread_stats_lock, thread_stats_lock);
    dcontext->thread_stats = new_thread_stats;
}

void
stats_thread_exit(dcontext_t *dcontext)
{
    /* We need to free even in non-debug b/c unprot local heap is global. */
    if (dcontext->thread_stats) {
        thread_local_statistics_t *old_thread_stats = dcontext->thread_stats;
#    ifdef DEBUG
        DELETE_LOCK(old_thread_stats->thread_stats_lock);
#    endif
        dcontext->thread_stats = NULL; /* disable thread stats before freeing memory */
        HEAP_TYPE_FREE(dcontext, old_thread_stats, thread_local_statistics_t, ACCT_STATS,
                       UNPROTECTED);
    }
}

DISABLE_NULL_SANITIZER
void
dump_thread_stats(dcontext_t *dcontext, bool raw)
{
    /* Note that this routine may be called by another thread, but
       we want the LOGs and the STATs to be for the thread getting dumped
       Make sure we use the passed dcontext everywhere here.
       Avoid implicit use of get_thread_private_dcontext (e.g. THREAD_GET).
    */
    /* Each use of THREAD causes cl to make two implicit local variables
     * (without optimizations on) (culprit is the ? : syntax).  Since the
     * number of locals sums over scopes, this leads to stack usage
     * of ~3kb for this function due to the many LOGs.
     * Instead we use THREAD once here and use a local below, cutting stack
     * usage to 12 bytes, ref bug 2203 */
    file_t logfile = THREAD;
    if (!THREAD_STATS_ON(dcontext))
        return;

    /* FIXME: for now we'll have code duplication with dump_global_stats()
     * with the only difference being THREAD vs GLOBAL, e.g. LOG(GLOBAL and GLOBAL_STAT
     * Keep in sync or make a template statistics dump macro for both cases.
     */
    LOG(logfile, LOG_STATS, 1,
        "(Begin) Thread statistics @%d global, %d thread fragments ",
        GLOBAL_STAT(num_fragments), THREAD_STAT(dcontext, num_fragments));
    DOLOG(1, LOG_STATS, { d_r_print_timestamp(logfile); });
    /* give up right away if thread stats lock already held, will dump next time
       most likely thread interrupted while dumping state
     */
    if (!d_r_mutex_trylock(&dcontext->thread_stats->thread_stats_lock)) {
        LOG(logfile, LOG_STATS, 1, " WARNING: skipped! Another dump in progress.\n");
        return;
    }
    LOG(logfile, LOG_STATS, 1, ":\n");

#    define STATS_DEF(desc, stat)                                                     \
        if (THREAD_STAT(dcontext, stat)) {                                            \
            if (raw) {                                                                \
                LOG(logfile, LOG_STATS, 1, "\t%s\t= " SSZFMT "\n", #stat,             \
                    THREAD_STAT(dcontext, stat));                                     \
            } else {                                                                  \
                LOG(logfile, LOG_STATS, 1,                                            \
                    "%50s %s:" IF_X64_ELSE("%18", "%9") SSZFC "\n", desc, "(thread)", \
                    THREAD_STAT(dcontext, stat));                                     \
            }                                                                         \
        }
#    include "statsx.h"
#    undef STATS_DEF

    LOG(logfile, LOG_STATS, 1, "(End) Thread statistics\n");
    d_r_mutex_unlock(&dcontext->thread_stats->thread_stats_lock);
    /* TODO: update thread statistics, using the thread stats delta when implemented */

#    ifdef KSTATS
    dump_thread_kstats(dcontext);
#    endif
}

DISABLE_NULL_SANITIZER
void
dump_global_stats(bool raw)
{
    DOLOG(1, LOG_MEMSTATS, {
        if (!dynamo_exited_and_cleaned)
            mem_stats_snapshot();
    });
    if (!dynamo_exited_and_cleaned)
        print_vmm_heap_data(GLOBAL);
    if (GLOBAL_STATS_ON()) {
        LOG(GLOBAL, LOG_STATS, 1, "(Begin) All statistics @%d ",
            GLOBAL_STAT(num_fragments));
        DOLOG(1, LOG_STATS, { d_r_print_timestamp(GLOBAL); });
        LOG(GLOBAL, LOG_STATS, 1, ":\n");
#    define STATS_DEF(desc, stat)                                                       \
        if (GLOBAL_STAT(stat)) {                                                        \
            if (raw) {                                                                  \
                LOG(GLOBAL, LOG_STATS, 1, "\t%s\t= " SSZFMT "\n", #stat,                \
                    GLOBAL_STAT(stat));                                                 \
            } else {                                                                    \
                LOG(GLOBAL, LOG_STATS, 1, "%50s :" IF_X64_ELSE("%18", "%9") SSZFC "\n", \
                    desc, GLOBAL_STAT(stat));                                           \
            }                                                                           \
        }
#    include "statsx.h"
#    undef STATS_DEF
        LOG(GLOBAL, LOG_STATS, 1, "(End) All statistics\n");
    }
#    ifdef HEAP_ACCOUNTING
    DOLOG(1, LOG_HEAP | LOG_STATS, { print_heap_statistics(); });
#    endif
    DOLOG(1, LOG_CACHE, {
        /* shared cache stats */
        fcache_stats_exit();
    });
#    ifdef SHARING_STUDY
    DOLOG(1, LOG_ALL, {
        if (INTERNAL_OPTION(fragment_sharing_study) && !dynamo_exited)
            print_shared_stats();
    });
#    endif
#    ifdef DEADLOCK_AVOIDANCE
    dump_process_locks();
#    endif
}

uint
print_timestamp_to_buffer(char *buffer, size_t len)
{
    uint min, sec, msec;
    size_t print_len = MIN(len, PRINT_TIMESTAMP_MAX_LENGTH);
    static uint64 initial_time = 0ULL; /* in milliseconds */
    uint64 current_time;

    if (initial_time == 0ULL)
        initial_time = query_time_millis();
    current_time = query_time_millis();
    if (current_time == 0ULL) /* call failed */
        return 0;
    current_time -= initial_time; /* elapsed */
    sec = (uint)(current_time / 1000);
    msec = (uint)(current_time % 1000);
    min = sec / 60;
    sec = sec % 60;
    return d_r_snprintf(buffer, print_len, "(%ld:%02ld.%03ld)", min, sec, msec);
}

/* prints elapsed time since program startup to the given logfile
 * TODO: should also print absolute timestamp
 * TODO: and relative time from thread start
 */
uint
d_r_print_timestamp(file_t logfile)
{
    char buffer[PRINT_TIMESTAMP_MAX_LENGTH];
    uint len = print_timestamp_to_buffer(buffer, PRINT_TIMESTAMP_MAX_LENGTH);

    if (len > 0)
        print_file(logfile, buffer);
    return len;
}
#endif /* DEBUG */

void
dump_global_rstats_to_stderr(void)
{
    if (GLOBAL_STATS_ON()) {
        print_file(STDERR, "%s statistics:\n", PRODUCT_NAME);
#undef RSTATS_DEF
        /* It doesn't make sense to print Current stats.  We assume no rstat starts
         * with "Cu".
         */
#define RSTATS_DEF(desc, stat)                                                 \
    if (GLOBAL_STAT(stat) && (desc[0] != 'C' || desc[1] != 'u')) {             \
        print_file(STDERR, "%50s :" IF_X64_ELSE("%18", "%9") SSZFC "\n", desc, \
                   GLOBAL_STAT(stat));                                         \
    }
#define RSTATS_ONLY
#include "statsx.h"
#undef RSTATS_ONLY
#undef RSTATS_DEF
    }
}

static void
dump_buffer_as_ascii(file_t logfile, char *buffer, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        print_file(logfile, "%c", isprint_fast(buffer[i]) ? buffer[i] : '.');
    }
}

void
dump_buffer_as_bytes(file_t logfile, void *buffer, size_t len, int flags)
{
    bool octal = TEST(DUMP_OCTAL, flags);
    bool raw = TEST(DUMP_RAW, flags);
    bool usechars = !raw && !TEST(DUMP_NO_CHARS, flags);
    bool replayable = usechars && !TEST(DUMP_NO_QUOTING, flags);
    bool dword = TEST(DUMP_DWORD, flags);
    bool prepend_address = TEST(DUMP_ADDRESS, flags);
    bool append_ascii = TEST(DUMP_APPEND_ASCII, flags);

    unsigned char *buf = (unsigned char *)buffer;

    int per_line =
        (flags & DUMP_PER_LINE) ? (flags & DUMP_PER_LINE) : DUMP_PER_LINE_DEFAULT;
    size_t i;
    int nonprint = 0;
    size_t line_start = 0;

    if (!raw)
        print_file(logfile, "%s", "\"");

    for (i = 0; i + (dword ? 4 : 1) <= len; i += (dword ? 4 : 1)) {
        if (i > 0 && 0 == i % per_line) {
            if (append_ascii) {
                print_file(logfile, "%s", " ");
                /* append current line as ASCII */
                ASSERT(line_start == (i - per_line));
                dump_buffer_as_ascii(logfile, (char *)buf + line_start, per_line);
                line_start = i;
            }
            /* new line */
            print_file(logfile, "%s", raw ? "\n" : "\"\n\"");
        }
        if (prepend_address && 0 == i % per_line) // prepend address on new line
            print_file(logfile, PFX " ", buf + i);

        if (replayable) {
            if (isdigit_fast(buf[i]) && nonprint) {
                print_file(logfile, "%s", "\"\""); // to make \01 into \0""1
            }
            if (buf[i] == '"') {
                print_file(logfile, "%s", "\\\"");
                continue;
            }
            if (buf[i] == '\\')
                print_file(logfile, "%s", "\\");
        }

        if (usechars && isprint_fast(buf[i])) {
            print_file(logfile, "%c", buf[i]);
            nonprint = 0;
        } else {
            if (!raw) {
                print_file(logfile, "%s", octal ? "\\" : "\\x");
            }
            if (dword)
                print_file(logfile, "%08x", *((uint *)(buf + i)));
            else
                print_file(logfile, octal ? "%03o" : "%02x", buf[i]);
            nonprint = 1;
            if (raw) {
                print_file(logfile, "%s", " ");
            }
        }
    }

    if (append_ascii) {
        /* append last line as ASCII */
        /* pad to align columns */
        size_t empty = ALIGN_FORWARD(buf + len, per_line);
        uint size = (dword ? 4 : 1);
        /* In general we expect dword requests to be dword aligned but
         * we don't enforce it.  Note that we don't print DWORDs that
         * may extend beyond valid len, but we'll print as ASCII any
         * bytes included in valid len even if not printed in hex.
         */
        for (i = ALIGN_BACKWARD(buf + len, size); i < empty; i += size) {
            if (dword) {
                print_file(logfile, "%8c ", ' ');
            } else {
                print_file(logfile, octal ? "%3c " : "%2c ", ' ');
            }
        }

        print_file(logfile, "%s", " ");
        dump_buffer_as_ascii(logfile, (char *)buf + line_start, len - line_start);
    }

    if (!raw)
        print_file(logfile, "%s", "\";\n");
}

/******************************************************************************/
/* xml escaping routines, presumes iso-8859-1 encoding */

bool
is_valid_xml_char(char c)
{
    /* FIXME - wld.exe xml parsing complains about any C0 control character other
     * then \t \r and \n.   However, in this encoding (to my understanding) all values
     * should be valid and IE doesn't complain opening an xml file in this encoding
     * with these characters.  Not sure where the wld.exe problem lies, but since it is
     * our primary consumer we work around here. */
    if ((uchar)c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
        return false;
    }
    return true;
}

static bool
is_valid_xml_string(const char *str)
{
    while (*str != '\0') {
        if (!is_valid_xml_char(*str))
            return false;
        str++;
    }
    return true;
}

/* NOTE - string should not include the <![CDATA[   ]]> markup as one thing
 * this routine checks for is an inadvertant ending sequence ]]>. Caller is
 * responsible for correct markup. */
static bool
is_valid_xml_cdata_string(const char *str)
{
    /* check for end CDATA tag */
    /* FIXME - optimization,combine the two walks of the string into a
     * single walk.*/
    return (strstr(str, "]]>") == NULL && is_valid_xml_string(str));
}

#if 0 /* Not yet used */
static bool
is_valid_xml_body_string(const char *str)
{
    /* check for & < > */
    /* FIXME - optimization, combine into a single walk of the string. */
    return (strchr(str, '>') == NULL && strchr(str, '<') == NULL &&
            strchr(str, '&') == NULL && is_valid_xml_string(str));
}

static bool
is_valid_xml_attribute_string(const char *str)
{
    /* check for & < > ' " */
    /* FIXME - optimization, combine into a single walk of the string. */
    return (strchr(str, '\'') == NULL && strchr(str, '\"') == NULL &&
            is_valid_xml_body_string(str));
}
#endif

/* NOTE - string should not include the <![CDATA[   ]]> markup as one thing
 * this routine checks for is an inadvertant ending sequence ]]> (in which
 * case the first ] will be escaped). We escape using \%03d, note that since
 * we don't escape \ , '\003' and "\003" will be indistinguishable (FIXME),
 * but given that these should really be normal ascii strings we'll live with
 * that. */
void
print_xml_cdata(file_t f, const char *str)
{
    if (is_valid_xml_cdata_string(str)) {
        print_file(f, "%s", str);
    } else {
        while (*str != '\0') {
            if (!is_valid_xml_char(*str) ||
                (*str == ']' && *(str + 1) == ']' && *(str + 2) == '>')) {
                print_file(f, "\\%03d", (int)*(uchar *)str);
            } else {
                /* FIXME : could batch up printing normal chars for perf.
                 * but we usually expect to have valid strings anyways. */
                print_file(f, "%c", *str);
            }
            str++;
        }
    }
}

/* TODO - NYI print_xml_body_string, print_xml_attribute_string */

void
print_version_and_app_info(file_t file)
{
    print_file(file, "%s\n", dynamorio_version_string);
    /* print qualified name (not d_r_stats->process_name) to get cmdline */
    print_file(file, "Running: %s\n", get_application_name());
#ifdef WINDOWS
    /* FIXME: also get linux cmdline -- separate since wide on win32 */
    print_file(file, "App cmdline: %S\n", get_application_cmdline());
#endif
    print_file(file, PRODUCT_NAME " built with: %s\n", DYNAMORIO_DEFINES);
    print_file(file, PRODUCT_NAME " built on: %s\n", dynamorio_buildmark);
#ifndef _WIN32_WCE
    print_file(file, DYNAMORIO_VAR_OPTIONS ": %s\n", d_r_option_string);
#endif
}

void
utils_exit()
{
    LOG(GLOBAL, LOG_STATS, 1, "-prng_seed " PFX " for reproducing random sequence\n",
        initial_random_seed);
    if (doing_detach)
        enable_new_log_dir(); /* For potential re-attach. */

    DELETE_LOCK(report_buf_lock);
    DELETE_RECURSIVE_LOCK(logdir_mutex);
    DELETE_LOCK(prng_lock);
#ifdef DEADLOCK_AVOIDANCE
    DELETE_LOCK(do_threshold_mutex);
#endif

    spinlock_count = 0;
}

/* returns a pseudo random number in [0, max_offset) */

/* FIXME: [minor security] while the first user may get more
 *   randomness from the lower bits of the seed, I am not sure the
 *   following users should strongly prefer higher or lower bits.
 */
size_t
get_random_offset(size_t max_offset)
{
    /* These linear congruential constants taken from
     * http://remus.rutgers.edu/~rhoads/Code/random.c
     * FIXME: Look up Knuth's recommendations in vol. 2
     */
    enum { LCM_A = 279470273, LCM_Q = 15, LCM_R = 102913196 };
    /* I prefer not risking any of the randomness in our seed to go
     * through the fishy LCM and value generation - it doesn't buy us
     * anything for the first instance which is all we currently use.
     * Calculating value based on the previous seed also removes
     * dependencies from the critical path and will be faster if we
     * use this on a critical path..
     */
    size_t value;

    /* Avoids div-by-zero if offset is 0; see case 8602 for implications. */
    if (max_offset == 0)
        return 0;

    /* process-shared random sequence */
    d_r_mutex_lock(&prng_lock);
    /* FIXME: this is not getting the best randomness
     * see srand() comments why taking higher order bits is usually better
     * j=1+(int) (10.0*rand()/(RAND_MAX+1.0));
     * but I want to do it without floating point
     */
    value = random_seed % max_offset;

    random_seed = LCM_A * (random_seed % LCM_Q) - LCM_R * (random_seed / LCM_Q);
    d_r_mutex_unlock(&prng_lock);
    LOG(GLOBAL, LOG_ALL, 2, "get_random_offset: value=%d (mod %d), new rs=%d\n", value,
        max_offset, random_seed);
    return value;
}

void
d_r_set_random_seed(uint seed)
{
    random_seed = seed;
}

uint
d_r_get_random_seed(void)
{
    return random_seed;
}

/* millis is the number of milliseconds since Jan 1, 1601 (this is
 * the current UTC time).
 */
void
convert_millis_to_date(uint64 millis, dr_time_t *dr_time OUT)
{
    uint64 time = millis;
    uint days, year, month, q;

#define DAYS_IN_400_YEARS (400 * 365 + 97)

    dr_time->milliseconds = (uint)(time % 1000);
    time /= 1000;
    dr_time->second = (uint)(time % 60);
    time /= 60;
    dr_time->minute = (uint)(time % 60);
    time /= 60;
    dr_time->hour = (uint)(time % 24);
    time /= 24;
    /* Time is now number of days since 1 Jan 1601 (Gregorian).
     * We rebase to 1 Mar 1600 (Gregorian) as this day immediately
     * follows the irregular element in each cycle: 12-month cycle,
     * 4-year cycle, 100-year cycle, 400-year cycle.
     * Noon, 1 Jan 1601 is the start of Julian Day 2305814.
     * Noon, 1 Mar 1600 is the start of Julian Day 2305508.
     */
    time += 2305814 - 2305508;

    /* Reduce modulo 400 years, which gets us into the range of a uint. */
    year = 1600 + (uint)(time / DAYS_IN_400_YEARS) * 400;
    days = (uint)(time % DAYS_IN_400_YEARS);

    /* The constants used in the rest of this function are difficult to
     * get right, but the number of days in 400 years is small enough for
     * us to test this code exhaustively, which we will, of course, do.
     */

    /* Sunday is 0, Monday is 1, ... 1 Mar 1600 was Wednesday. */
    dr_time->day_of_week = (days + 3) % 7;

    /* Determine century: divide by average number of days in a century,
     * 36524.25 = 146097 / 4, rounding up because the long century comes last.
     */
    q = (days * 4 + 3) / 146097;
    year += q * 100;
    days -= q * 146097 / 4;

    /* Determine year: divide by average number of days in a year,
     * 365.25 = 1461 / 4, rounding up because the long year comes last.
     * The average is 365.25 because we ignore the irregular year at the
     * end of the century, which we can safely do because it is shorter.
     */
    q = (days * 4 + 3) / 1461;
    year += q;
    days -= q * 1461 / 4;

    /* Determine month: divide by (31 + 30 + 31 + 30 + 31) / 5, with carefully tuned
     * rounding to get the consecutive 31-day months in the right place. This works
     * because the number of days in a month follows a cycle, starting in March:
     * 31, 30, 31, 30, 31; 31, 30, 31, 30, 31; 31, ... This is followed by Februrary,
     * of course, but that is not a problem because it is shorter rather than longer
     * than expected. The values "2" and "2" are easiest to determine experimentally.
     */
    month = (days * 5 + 2) / 153;
    days -= (month * 153 + 2) / 5;

    /* Adjust for calendar year starting in January rather than March. */
    dr_time->day = days + 1;
    dr_time->month = month < 10 ? month + 3 : month - 9;
    dr_time->year = month < 10 ? year : year + 1;
}

/* millis is the number of milliseconds since Jan 1, 1601 (this is
 * the current UTC time).
 */
void
convert_date_to_millis(const dr_time_t *dr_time, uint64 *millis OUT)
{
    /* Formula adapted from http://en.wikipedia.org/wiki/Julian_day.
     * We rebase the input year from -4800 to +1600, and we rebase
     * the output day from from 1 Mar -4800 (Gregorian) to 1 Jan 1601.
     * Noon, 1 Mar 1600 (Gregorian) is the start of Julian Day 2305508.
     * Noon, 1 Mar -4800 (Gregorian) is the start of Julian Day -32044.
     * Noon, 1 Jan 1601 (Gregorian) is the start of Julian Day 2305814.
     */
    uint a = dr_time->month < 3 ? 1 : 0;
    uint y = dr_time->year - a - 1600;
    uint m = dr_time->month + 12 * a - 3;
    uint64 days = ((dr_time->day + (153 * m + 2) / 5 + y / 4 - y / 100 + y / 400) +
                   365 * (uint64)y - 32045 + 2305508 + 32044 - 2305814);
    *millis =
        ((((days * 24 + dr_time->hour) * 60 + dr_time->minute) * 60 + dr_time->second) *
             1000 +
         dr_time->milliseconds);
}

static const uint crctab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535,
    0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd,
    0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d,
    0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
    0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac,
    0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
    0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
    0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb,
    0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea,
    0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce,
    0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
    0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409,
    0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739,
    0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268,
    0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0,
    0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8,
    0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
    0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703,
    0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
    0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae,
    0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6,
    0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d,
    0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5,
    0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
    0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/* This function implements the Ethernet AUTODIN II CRC32 algorithm.  */
uint
d_r_crc32(const char *buf, const uint len)
{
    uint i;
    uint crc = 0xFFFFFFFF;

    for (i = 0; i < len; i++)
        crc = (crc >> 8) ^ crctab[(crc ^ buf[i]) & 0xFF];

    return crc;
}

/* MD5 is used for persistent and process-shared caches, process_control, and
 * ASLR persistent sharing.  The definition of MD5 below has a public
 * license; source: http://stuff.mit.edu/afs/sipb/user/kenta/lj/clive/clive-0.4.5/
 */
/*----------------------------------------------------------------------------*/
/* This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest. This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to d_r_md5_init, call d_r_md5_update as
 * needed on buffers full of bytes, and then call d_r_md5_final, which
 * will fill a supplied 16-byte array with the digest.
 */
static void
MD5Transform(uint32 state[4], const unsigned char block[MD5_BLOCK_LENGTH]);

#define PUT_64BIT_LE(cp, value)                   \
    do {                                          \
        (cp)[7] = (unsigned char)((value) >> 56); \
        (cp)[6] = (unsigned char)((value) >> 48); \
        (cp)[5] = (unsigned char)((value) >> 40); \
        (cp)[4] = (unsigned char)((value) >> 32); \
        (cp)[3] = (unsigned char)((value) >> 24); \
        (cp)[2] = (unsigned char)((value) >> 16); \
        (cp)[1] = (unsigned char)((value) >> 8);  \
        (cp)[0] = (unsigned char)(value);         \
    } while (0)

#define PUT_32BIT_LE(cp, value)                   \
    do {                                          \
        (cp)[3] = (unsigned char)((value) >> 24); \
        (cp)[2] = (unsigned char)((value) >> 16); \
        (cp)[1] = (unsigned char)((value) >> 8);  \
        (cp)[0] = (unsigned char)(value);         \
    } while (0)

static unsigned char PADDING[MD5_BLOCK_LENGTH] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void
d_r_md5_init(struct MD5Context *ctx)
{
    ctx->count = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void
d_r_md5_update(struct MD5Context *ctx, const unsigned char *input, size_t len)
{
    size_t have, need;

    /* Check how many bytes we already have and how many more we need. */
    have = (size_t)((ctx->count >> 3) & (MD5_BLOCK_LENGTH - 1));
    need = MD5_BLOCK_LENGTH - have;

    /* Update bitcount */
    ctx->count += (uint64)len << 3;

    if (len >= need) {
        if (have != 0) {
            memcpy(ctx->buffer + have, input, need);
            MD5Transform(ctx->state, ctx->buffer);
            input += need;
            len -= need;
            have = 0;
        }

        /* Process data in MD5_BLOCK_LENGTH-byte chunks. */
        while (len >= MD5_BLOCK_LENGTH) {
            MD5Transform(ctx->state, input);
            input += MD5_BLOCK_LENGTH;
            len -= MD5_BLOCK_LENGTH;
        }
    }

    /* Handle any remaining bytes of data. */
    if (len != 0)
        memcpy(ctx->buffer + have, input, len);
}

/*
 * Pad pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void
MD5Pad(struct MD5Context *ctx)
{
    unsigned char count[8];
    size_t padlen;

    /* Convert count to 8 bytes in little endian order. */
    PUT_64BIT_LE(count, ctx->count);

    /* Pad out to 56 mod 64. */
    padlen = (size_t)(MD5_BLOCK_LENGTH - ((ctx->count >> 3) & (MD5_BLOCK_LENGTH - 1)));
    if (padlen < 1 + 8)
        padlen += MD5_BLOCK_LENGTH;
    d_r_md5_update(ctx, PADDING, padlen - 8); /* padlen - 8 <= 64 */
    d_r_md5_update(ctx, count, 8);
}

/*
 * Final wrapup--call MD5Pad, fill in digest and zero out ctx.
 */
void
d_r_md5_final(unsigned char digest[MD5_RAW_BYTES], struct MD5Context *ctx)
{
    int i;

    MD5Pad(ctx);
    if (digest != NULL) {
        for (i = 0; i < 4; i++)
            PUT_32BIT_LE(digest + i * 4, ctx->state[i]);
    }
    memset(ctx, 0, sizeof(*ctx)); /* in case it's sensitive */
}

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
    (w += f(x, y, z) + data, w = w << s | w >> (32 - s), w += x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  d_r_md5_update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void
MD5Transform(uint32 state[4], const unsigned char block[MD5_BLOCK_LENGTH])
{
    uint32 a, b, c, d, in[MD5_BLOCK_LENGTH / 4];

#if BYTE_ORDER == LITTLE_ENDIAN
    memcpy(in, block, sizeof(in));
#else
    for (a = 0; a < MD5_BLOCK_LENGTH / 4; a++) {
        in[a] =
            (uint32)((uint32)(block[a * 4 + 0]) | (uint32)(block[a * 4 + 1]) << 8 |
                     (uint32)(block[a * 4 + 2]) << 16 | (uint32)(block[a * 4 + 3]) << 24);
    }
#endif

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}
#undef F1
#undef F2
#undef F3
#undef F4
#undef MD5STEP
/*----------------------------------------------------------------------------*/

bool
module_digests_equal(const module_digest_t *calculated_digest,
                     const module_digest_t *matching_digest, bool check_short,
                     bool check_full)
{
    bool match = true;
    if (check_short) {
        match = match &&
            md5_digests_equal(calculated_digest->short_MD5, matching_digest->short_MD5);
    }
    if (check_full) {
        match = match &&
            md5_digests_equal(calculated_digest->full_MD5, matching_digest->full_MD5);
    }
    return match;
}

/* Reads the full file, returns it in a buffer and sets buf_len to the size
 * of the buffer allocated on the specified heap, if successful.  Returns NULL
 * and sets buf_len to 0 on failure.  Defined when fixing case 8187.
 */
char *
read_entire_file(const char *file, size_t *buf_len /* OUT */ HEAPACCT(which_heap_t heap))
{
    ssize_t bytes_read;
    file_t fd = INVALID_FILE;
    char *buf = NULL;
    uint64 buf_len64 = 0;

    /* No point in reading the file if the length can't be returned - the caller
     * won't be able to free the buffer without the size.
     */
    if (file == NULL || buf_len == NULL)
        return NULL;

    *buf_len = 0;

    fd = os_open((char *)file, OS_OPEN_READ);
    if (fd == INVALID_FILE)
        return NULL;

    if (!os_get_file_size(file, &buf_len64)) {
        os_close(fd);
        return NULL;
    }
    ASSERT_TRUNCATE(*buf_len, uint, buf_len64);

    /* Though only 1 byte is needed for the \0 at the end of the buffer,
     * 4 may be allocated to work around case 8048.  FIXME: remove
     * alignment after case is resolved.
     */
    *buf_len = (uint)ALIGN_FORWARD((buf_len64 + 1), 4);
    buf = (char *)heap_alloc(GLOBAL_DCONTEXT, *buf_len HEAPACCT(heap));
    bytes_read = os_read(fd, buf, *buf_len);
    if (bytes_read <= 0) {
        heap_free(GLOBAL_DCONTEXT, buf, *buf_len HEAPACCT(heap));
        os_close(fd);
        return NULL;
    }
    ASSERT(CHECK_TRUNCATE_TYPE_uint(bytes_read));
    ASSERT((uint)bytes_read != *buf_len && "buffer too small");
    ASSERT((uint)bytes_read < *buf_len); /* use MIN below just to be safe */
    buf[MIN((uint)bytes_read, *buf_len - 1)] = 0;

    os_close(fd);
    return buf;
}

/* returns false if we are too low on disk to create a file of desired size */
bool
check_low_disk_threshold(file_t f, uint64 new_file_size)
{
    /* FIXME: we only use UserAvailable to find the minimum expressed
     * as absolute bytes to leave available.  In addition we could
     * also have percentage limits, where minimum available should be
     * based on TotalQuotaBytes, and maximum cache size on
     * TotalVolumeBytes.
     */
    uint64 user_available_bytes;
    /* FIXME: does this work for compressed volumes? */
    bool ok = os_get_disk_free_space(f, &user_available_bytes, NULL, NULL);
    if (ok) {
        LOG(THREAD_GET, LOG_SYSCALLS | LOG_THREADS, 2,
            "available disk space quota %dMB\n", user_available_bytes / 1024 / 1024);

        /* note that the actual needed bytes are in fact aligned to a
         * cluster, so this is somewhat imprecise */
        ok = (user_available_bytes > new_file_size) &&
            (user_available_bytes - new_file_size) > DYNAMO_OPTION(min_free_disk);
        if (!ok) {
            /* FIXME: notify the customer that they are low on disk space? */
            SYSLOG_INTERNAL_WARNING_ONCE(
                "reached minimal free disk space limit,"
                " available " UINT64_FORMAT_STRING "MB, limit %dMB, "
                "asking for " UINT64_FORMAT_STRING "KB",
                user_available_bytes / 1024 / 1024,
                DYNAMO_OPTION(min_free_disk) / 1024 / 1024, new_file_size / 1024);
            /* ONCE, even though later we may succeed and start failing again */
        }
    } else {
        /* impersonated thread may not have rights to even query */
        /* or we have an invalid path */
        /* do nothing */
        LOG(THREAD_GET, LOG_SYSCALLS | LOG_THREADS, 2,
            "unable to retrieve available disk space\n");
    }
    return ok;
}

#ifdef PROCESS_CONTROL /* currently used for only for this; need not be so */
/* Note: Reading the full file in one shot will cause committed memory and
 * wss to shoot up.  Even if this memory is freed, only wss will come down.
 * While this may be ok for fcache mode, it is probably not for hotp_only mode,
 * and definitely not for thin_client mode.  The main use of thin_client today
 * is process_control, which means that MD5 will be computed in thin_client
 * mode.
 *
 * The options for this memory issue are to mmap the memory and unmap it after
 * use rather than use the heap or to use a moderate sized buffer and read the
 * file in chunks, which is wat I have chosen.  If startup performance becomes
 * a problem because of this play with bigger pages or just use mmap and munmap.
 *
 * Measurements on my laptop on a total of 5076 executables showed,
 *  Average size        : 233 kb
 *  Standard deviation  : 684 kb
 *  Median size         : 68 kb!
 *  80% percentile      : 205 kb
 *  90% percentile      : 469 kb
 * So a buffer of 16 kb seems reasonable, even though the data is not based
 * on frequency of usage.
 *
 * FIXME: use nt_map_view_of_section with SEC_MAPPED && !SEC_IMAGE.
 */
#    define MD5_FILE_READ_BUF_SIZE (4 * PAGE_SIZE)

/* Reads 'file', computes MD5 hash for it, which is returned in hash_buf
 * when successful and true is returned.  On failure false is returned and
 * hash_buf contents invalid.
 * Note: Reads in file in 16k chunks to avoid private/committed memory
 * increase.
 */
bool
get_md5_for_file(const char *file, char *hash_buf /* OUT */)
{
    ssize_t bytes_read;
    int i;
    file_t fd;
    char *file_buf;
    struct MD5Context md5_cxt;
    unsigned char md5_buf[MD5_STRING_LENGTH / 2];

    if (file == NULL || hash_buf == NULL)
        return false;

    fd = os_open((char *)file, OS_OPEN_READ);
    if (fd == INVALID_FILE)
        return false;

    d_r_md5_init(&md5_cxt);
    file_buf =
        (char *)heap_alloc(GLOBAL_DCONTEXT, MD5_FILE_READ_BUF_SIZE HEAPACCT(ACCT_OTHER));
    while ((bytes_read = os_read(fd, file_buf, MD5_FILE_READ_BUF_SIZE)) > 0) {
        ASSERT(CHECK_TRUNCATE_TYPE_uint(bytes_read));
        d_r_md5_update(&md5_cxt, (byte *)file_buf, (uint)bytes_read);
    }
    d_r_md5_final(md5_buf, &md5_cxt);

    /* Convert 16-byte signature into 32-byte string, which is how MD5 is
     * usually printed/used. n is 3, 2 for chars & 1 for the '\0';
     */
    for (i = 0; i < BUFFER_SIZE_ELEMENTS(md5_buf); i++)
        snprintf(hash_buf + (i * 2), 3, "%02X", md5_buf[i]);

    /* Just be safe & terminate the buffer; assuming it has 33 chars! */
    hash_buf[MD5_STRING_LENGTH] = '\0';

    heap_free(GLOBAL_DCONTEXT, file_buf, MD5_FILE_READ_BUF_SIZE HEAPACCT(ACCT_OTHER));
    os_close(fd);
    return true;
}
#endif /* PROCESS_CONTROL */

/* Computes and caches MD5 for the application if it isn't there; returns it. */
/* Note: this function isn't under PROCESS_CONTROL even though that is what
 *       uses this because md5 for the executable may be needed in future.
 *       Also, this avoids having to define 2 types of process start events.
 *       Further, if we decide to remove process control in future leaving the
 *       md5 in the start event will prevent needless backward compatibility
 *       problems caused by removing md5 from it.
 */
const char *
get_application_md5(void)
{
    /* Though exe_md5 is used in the process start event, it is currently set
     * only by process_control.  BTW, 1 for the terminating '\0'.
     */
    static char exe_md5[MD5_STRING_LENGTH + 1] = { 0 };

#ifdef PROCESS_CONTROL
    /* MD5 is computed only if process control is turned on, otherwise the cost
     * isn't paid (roughly 10ms for a 350kb exe), i.e. "" is returned.
     */
    if (exe_md5[0] == '\0') {
        if (IS_PROCESS_CONTROL_ON()) {
            DEBUG_DECLARE(bool res;)
#    ifdef WINDOWS
            /* FIXME - inefficient, we use stack buffer here to convert wchar to char,
             * and later we'll use another stack buffer to convert char to wchar to
             * open the file.  That said this is only done once (either at startup or
             * for process_control nudge [app_stack]) so as long as the stack doesn't
             * overflow it doesn't really matter. */
            char exe_name[MAXIMUM_PATH];
            snprintf(exe_name, BUFFER_SIZE_ELEMENTS(exe_name), "%ls",
                     get_own_unqualified_name());
            NULL_TERMINATE_BUFFER(exe_name);
#    else
            /* FIXME - will need to strip out qualifications if we add those to Linux */
            const char *exe_name = get_application_name();
#    endif

            /* Change protection to make .data writable; this is a nop at init
             * time - which is the most common case.  For the nudge case we
             * will pay full price of data protection if MD5 hasn't been
             * computed yet; this is rare, so it is ok.
             */
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            DEBUG_DECLARE(res =) get_md5_for_file(exe_name, exe_md5);
            ASSERT(res && strlen(exe_md5) == MD5_STRING_LENGTH);
            NULL_TERMINATE_BUFFER(exe_md5);            /* just be safe */
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT); /* restore protection */
        }
    } else {
        /* If it isn't null then it must be a full MD5 and process control
         * should be turned on.
         */
        ASSERT(strlen(exe_md5) == MD5_STRING_LENGTH);
        ASSERT(IS_PROCESS_CONTROL_ON());
    }
#else
    /* If there is no process control, for now, app MD5 isn't computed. */
    ASSERT(exe_md5[0] == '\0');
#endif
    return exe_md5;
}

/* Producing a single MD5 digest for a given readable memory region.
 *
 * An empty region is not expected, though legal, since produces a
 * constant value.
 */
void
get_md5_for_region(const byte *region_start, uint len,
                   unsigned char digest[MD5_RAW_BYTES] /* OUT */)
{
    struct MD5Context md5_cxt;
    d_r_md5_init(&md5_cxt);
    ASSERT(region_start != NULL);
    ASSERT_CURIOSITY(len != 0);

    if (region_start != NULL && len != 0)
        d_r_md5_update(&md5_cxt, region_start, len);
    d_r_md5_final(digest, &md5_cxt);
    ASSERT_NOT_TESTED();
}

bool
md5_digests_equal(const byte digest1[MD5_RAW_BYTES], const byte digest2[MD5_RAW_BYTES])
{
    return (memcmp(digest1, digest2, MD5_RAW_BYTES) == 0);
}

/* calculates intersection of two regions defined as open ended intervals
 * [region1_start, region1_start + region1_len) \intersect
 * [region2_start, region2_start + region2_len)
 *
 * intersection_len is set to 0 if the regions do not overlap
 * otherwise returns the intersecting region
 * [intersection_start, intersection_start + intersection_len)
 */
void
region_intersection(app_pc *intersection_start /* OUT */,
                    size_t *intersection_len /* OUT */, const app_pc region1_start,
                    size_t region1_len, const app_pc region2_start, size_t region2_len)
{
    /* intersection */
    app_pc intersection_end =
        MIN(region1_start + region1_len, region2_start + region2_len);
    ASSERT(intersection_start != NULL);
    ASSERT(intersection_len != NULL);
    *intersection_start = MAX(region1_start, region2_start);

    /* set length as long as result is a proper intersecting region,
     * max(0, intersection_end - intersection_start) if signed
     */
    *intersection_len = (intersection_end > *intersection_start)
        ? (intersection_end - *intersection_start)
        : 0;
}
/***************************************************************************/
#ifdef CALL_PROFILE

typedef struct _profile_callers_t {
    app_pc caller[MAX_CALL_PROFILE_DEPTH];
    uint count;
    struct _profile_callers_t *next;
} profile_callers_t;

/* debug-only so neverprot */
DECLARE_NEVERPROT_VAR(static profile_callers_t *profcalls, NULL);
DECLARE_CXTSWPROT_VAR(static mutex_t profile_callers_lock,
                      INIT_LOCK_FREE(profile_callers_lock));

/* Usage:
 * Simply place a profile_callers() call in the routine you wish to profile.
 * You MUST build without optimizations to enable call stack walking.
 * Results go to a separate log file and are dumped only at exit.
 * FIXME: combine w/ a generalized mutex_collect_callstack()?
 */
void
profile_callers()
{
    profile_callers_t *entry;
    uint *pc;
    uint num = 0;
    app_pc our_ebp = 0;
    app_pc caller[MAX_CALL_PROFILE_DEPTH];
    app_pc saferead[2];
    if (DYNAMO_OPTION(prof_caller) == 0 || dynamo_exited_and_cleaned /*no heap*/)
        return;
    ASSERT(DYNAMO_OPTION(prof_caller) <= MAX_CALL_PROFILE_DEPTH);
    GET_FRAME_PTR(our_ebp);
    memset(caller, 0, sizeof(caller));
    pc = (uint *)our_ebp;
    /* FIXME: mutex_collect_callstack() assumes caller addresses are in
     * DR and thus are safe to read, but checks for dstack, etc.
     * Should combine the two into a general routine.
     */
    while (pc != NULL && d_r_safe_read((byte *)pc, sizeof(saferead), saferead)) {
        caller[num] = saferead[1];
        num++;
        /* yes I've seen weird recursive cases before */
        if (pc == (uint *)saferead[0] || num >= DYNAMO_OPTION(prof_caller))
            break;
        pc = (uint *)saferead[0];
    }
    /* Assumption: there aren't many unique callstacks being profiled, so a
     * linear search is sufficient!
     * FIXME: make this more performant if necessary
     */
    for (entry = profcalls; entry != NULL; entry = entry->next) {
        bool match = true;
        for (num = 0; num < DYNAMO_OPTION(prof_caller); num++) {
            if (entry->caller[num] != caller[num]) {
                match = false;
                break;
            }
        }
        if (match) {
            entry->count++;
            break;
        }
    }
    if (entry == NULL) {
        entry = global_heap_alloc(sizeof(profile_callers_t) HEAPACCT(ACCT_OTHER));
        memcpy(entry->caller, caller, sizeof(caller));
        entry->count = 1;
        d_r_mutex_lock(&profile_callers_lock);
        entry->next = profcalls;
        profcalls = entry;
        d_r_mutex_unlock(&profile_callers_lock);
    }
}

void
profile_callers_exit()
{
    profile_callers_t *entry, *next;
    file_t file;
    if (DYNAMO_OPTION(prof_caller) > 0) {
        d_r_mutex_lock(&profile_callers_lock);
        file = open_log_file("callprof", NULL, 0);
        for (entry = profcalls; entry != NULL; entry = next) {
            uint num;
            next = entry->next;
            for (num = 0; num < DYNAMO_OPTION(prof_caller); num++) {
                print_file(file, PFX " ", entry->caller[num]);
            }
            print_file(file, "%d\n", entry->count);
            global_heap_free(entry, sizeof(profile_callers_t) HEAPACCT(ACCT_OTHER));
        }
        close_log_file(file);
        profcalls = NULL;
        d_r_mutex_unlock(&profile_callers_lock);
    }
    DELETE_LOCK(profile_callers_lock);
}

#endif /* CALL_PROFILE */

#ifdef STANDALONE_UNIT_TEST

#    ifdef printf
#        undef printf
#    endif
#    define printf(...) print_file(STDERR, __VA_ARGS__)

static void
test_date_conversion_millis(uint64 millis)
{
    dr_time_t dr_time;
    uint64 res;
    convert_millis_to_date(millis, &dr_time);
    convert_date_to_millis(&dr_time, &res);
    if (res != millis ||
        dr_time.day_of_week != (millis / (24 * 60 * 60 * 1000) + 1) % 7 ||
        dr_time.month < 1 || dr_time.month > 12 || dr_time.day < 1 || dr_time.day > 31 ||
        dr_time.hour > 23 || dr_time.minute > 59 || dr_time.second > 59 ||
        dr_time.milliseconds > 999) {
        printf("FAIL : test_date_conversion_millis\n");
        exit(-1);
    }
}

static void
test_date_conversion_day(dr_time_t *dr_time)
{
    uint64 millis;
    dr_time_t res;
    convert_date_to_millis(dr_time, &millis);
    convert_millis_to_date(millis, &res);
    if (res.year != dr_time->year || res.month != dr_time->month ||
        res.day != dr_time->day || res.hour != dr_time->hour ||
        res.minute != dr_time->minute || res.second != dr_time->second ||
        res.milliseconds != dr_time->milliseconds) {
        printf("FAIL : test_date_conversion_day\n");
        exit(-1);
    }
}

/* Tests for double_print(), divide_uint64_print(), and date routines. */
void
unit_test_utils(void)
{
    char buf[128];
    uint c, d;
    const char *s;
    int t;
    dr_time_t dr_time;

#    define DO_TEST(a, b, p, percent, fmt, result)                       \
        divide_uint64_print(a, b, percent, p, &c, &d);                   \
        snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), fmt, c, d);             \
        NULL_TERMINATE_BUFFER(buf);                                      \
        if (strcmp(buf, result) == 0) {                                  \
            printf("PASS\n");                                            \
        } else {                                                         \
            printf("FAIL : \"%s\" doesn't match \"%s\"\n", buf, result); \
            exit(-1);                                                    \
        }

    DO_TEST(1, 20, 3, false, "%u.%.3u", "0.050");
    DO_TEST(2, 5, 2, false, "%3u.%.2u", "  0.40");
    DO_TEST(100, 7, 4, false, "%u.%.4u", "14.2857");
    DO_TEST(475, 1000, 2, true, "%u.%.2u%%", "47.50%");

#    undef DO_TEST
#    define DO_TEST(a, p, fmt, result)                                   \
        double_print(a, p, &c, &d, &s);                                  \
        snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), fmt, s, c, d);          \
        NULL_TERMINATE_BUFFER(buf);                                      \
        if (strcmp(buf, result) == 0) {                                  \
            printf("PASS\n");                                            \
        } else {                                                         \
            printf("FAIL : \"%s\" doesn't match \"%s\"\n", buf, result); \
            exit(-1);                                                    \
        }

    DO_TEST(-2.06, 3, "%s%u.%.3u", "-2.060");
    DO_TEST(2.06, 4, "%s%u.%.4u", "2.0600");
    DO_TEST(.0563, 2, "%s%u.%.2u", "0.05");
    DO_TEST(-.0563, 2, "%s%u.%.2u", "-0.05");
    DO_TEST(23.0456, 5, "%s%4u.%.5u", "  23.04560");
    DO_TEST(-23.0456, 5, "%s%4u.%.5u", "-  23.04560");

#    undef DO_TEST

    EXPECT(BOOLS_MATCH(1, 1), true);
    EXPECT(BOOLS_MATCH(1, 0), false);
    EXPECT(BOOLS_MATCH(0, 1), false);
    EXPECT(BOOLS_MATCH(0, 0), true);
    EXPECT(BOOLS_MATCH(1, 2), true);
    EXPECT(BOOLS_MATCH(2, 1), true);
    EXPECT(BOOLS_MATCH(1, -1), true);

    /* Test each millisecond in first and last 100 seconds. */
    for (t = 0; t < 100000; t++) {
        test_date_conversion_millis(t);
        test_date_conversion_millis(-t - 1);
    }
    /* Test each second in first and last day and a bit. */
    for (t = 0; t < 100000; t++) {
        test_date_conversion_millis(t * 1000);
        test_date_conversion_millis(-(t * 1000) - 1);
    }
    /* Test each day from 1601 to 2148. */
    for (t = 0; t < 200000; t++)
        test_date_conversion_millis((uint64)t * 24 * 60 * 60 * 1000);
    /* Test the first of each month from 1601 to 99999. */
    dr_time.day_of_week = 0; /* not checked */
    dr_time.day = 1;
    dr_time.hour = 0;
    dr_time.minute = 0;
    dr_time.second = 0;
    dr_time.milliseconds = 0;
    for (t = 0; t < (99999 - 1601) * 12; t++) {
        dr_time.year = 1601 + t / 12;
        dr_time.month = 1 + t % 12;
        test_date_conversion_day(&dr_time);
    }
}

#    undef printf

#endif /* STANDALONE_UNIT_TEST */

char *
dr_strdup(const char *str HEAPACCT(which_heap_t which))
{
    char *dup;
    size_t str_len;

    if (str == NULL)
        return NULL;

    str_len = strlen(str) + 1; /* Extra 1 char for the '\0' at the end. */
    dup = (char *)heap_alloc(GLOBAL_DCONTEXT, str_len HEAPACCT(which));
    strncpy(dup, str, str_len);
    dup[str_len - 1] = '\0'; /* Being on the safe side. */
    return dup;
}

#ifdef WINDOWS
/* Allocates a new char *(NOT a new wchar_t*) from a wchar_t* */
char *
dr_wstrdup(const wchar_t *str HEAPACCT(which_heap_t which))
{
    char *dup;
    ssize_t encode_len;
    size_t str_len;
    int res;
    if (str == NULL)
        return NULL;
    /* FIXME: should have a max length and truncate?
     * I'm assuming we're using not directly on external inputs.
     * If we do put in a max length, should do the same for dr_strdup.
     */
    encode_len = utf16_to_utf8_size(str, 0 /*no max*/, NULL);
    if (encode_len < 0)
        str_len = 1;
    else
        str_len = encode_len + 1; /* Extra 1 char for the '\0' at the end. */
    dup = (char *)heap_alloc(GLOBAL_DCONTEXT, str_len HEAPACCT(which));
    if (encode_len >= 0) {
        res = snprintf(dup, str_len, "%S", str);
        if (res < 0 || (size_t)res < str_len - 1) {
            ASSERT_NOT_REACHED();
            if (res < 0)
                dup[0] = '\0';
            /* apparently for some versions of ntdll!_snprintf, if %S
             * conversion hits a non-ASCII char it will write a NULL and
             * snprintf will return -1 (that's the libc behavior) or the
             * number of chars to that point.  we don't want strlen to return
             * fewer chars than we allocated so we fill it in (i#347).
             */
            /* Xref i#347, though we shouldn't get here b/c utf16_to_utf8_size uses
             * the same code.  We fall back on filling with '?'.
             */
            memset(dup + strlen(dup), '?', str_len - 1 - strlen(dup));
        }
    }
    dup[str_len - 1] = '\0'; /* Being on the safe side. */
    /* Ensure when we free we'll pass the same size (i#347) */
    ASSERT(strlen(dup) == str_len - 1);
    return dup;
}
#endif

/* Frees a char *string (NOT a wchar_t*) allocated via dr_strdup or
 * dr_wstrdup that has not been modified since being copied!
 */
void
dr_strfree(const char *str HEAPACCT(which_heap_t which))
{
    size_t str_len;
    ASSERT_CURIOSITY(str != NULL);
    if (str == NULL)
        return;
    str_len = strlen(str) + 1; /* Extra 1 char for the '\0' at the end. */
    heap_free(GLOBAL_DCONTEXT, (void *)str, str_len HEAPACCT(which));
}

/* Merges two unsorted arrays, treating their elements as type void*
 * (but will work on other types of same size as void*)
 * as an intersection if intersect is true or as a union (removing
 * duplicates) otherwise.  Allocates a new array on dcontext's heap
 * for the result and returns it and its size; if the new size is 0,
 * does NOT allocate anything and points the new array at NULL.
 */
void
array_merge(dcontext_t *dcontext, bool intersect /* else union */, void **src1,
            uint src1_num, void **src2, uint src2_num,
            /*OUT*/ void ***dst, /*OUT*/ uint *dst_num HEAPACCT(which_heap_t which))
{
    /* Two passes: one to find number of unique entries, and second to
     * fill them in.
     * FIXME: if this routine is ever on a performance-critical path then
     * we should switch to a temp hashtable and avoid this quadratic cost.
     */
    uint num;
    void **vec = NULL;
    uint i, j;
    DEBUG_DECLARE(uint res;)
    ASSERT(dst != NULL && dst_num != NULL);
    ASSERT(src1 != NULL || src1_num == 0);
    ASSERT(src2 != NULL || src2_num == 0);
    if (src1 == NULL || src2 == NULL || dst == NULL) /* paranoid */
        return;                                      /* FIXME: return a bool? */
    if (src1_num == 0 && src2_num == 0) {
        *dst = NULL;
        *dst_num = 0;
        return;
    }
    num = intersect ? 0 : src1_num;
    for (i = 0; i < src2_num; i++) {
        for (j = 0; j < src1_num; j++) {
            if (src2[i] == src1[j]) {
                if (intersect)
                    num++;
                break;
            }
        }
        if (!intersect && j == src1_num)
            num++;
    }
    if (num > 0) {
        vec = HEAP_ARRAY_ALLOC(dcontext, void *, num, which, PROTECTED);
        if (!intersect)
            memcpy(vec, src1, sizeof(void *) * src1_num);
        DODEBUG(res = num;);
        num = intersect ? 0 : src1_num;
        for (i = 0; i < src2_num; i++) {
            for (j = 0; j < src1_num; j++) {
                if (src2[i] == src1[j]) {
                    if (intersect)
                        vec[num++] = src2[i];
                    break;
                }
            }
            if (!intersect && j == src1_num)
                vec[num++] = src2[i];
        }
        ASSERT(num == res);
    } else {
        ASSERT(intersect);
        ASSERT(vec == NULL);
    }
    *dst = vec;
    *dst_num = num;
}

bool
stats_get_snapshot(dr_stats_t *drstats)
{
    if (!GLOBAL_STATS_ON())
        return false;
    CLIENT_ASSERT(drstats != NULL, "Expected non-null value for parameter drstats.");
    drstats->basic_block_count = GLOBAL_STAT(num_bbs);
    if (drstats->size <= offsetof(dr_stats_t, peak_num_threads)) {
        return true;
    }
    drstats->peak_num_threads = GLOBAL_STAT(peak_num_threads);
    drstats->num_threads_created = GLOBAL_STAT(num_threads_created);
    if (drstats->size <= offsetof(dr_stats_t, synchs_not_at_safe_spot))
        return true;
    drstats->synchs_not_at_safe_spot = GLOBAL_STAT(synchs_not_at_safe_spot);
    if (drstats->size <= offsetof(dr_stats_t, peak_vmm_blocks_unreach_heap))
        return true;
    /* These fields were added all at once. */
    drstats->peak_vmm_blocks_unreach_heap = GLOBAL_STAT(peak_vmm_blocks_unreach_heap);
    drstats->peak_vmm_blocks_unreach_stack = GLOBAL_STAT(peak_vmm_blocks_unreach_stack);
    drstats->peak_vmm_blocks_unreach_special_heap =
        GLOBAL_STAT(peak_vmm_blocks_unreach_special_heap);
    drstats->peak_vmm_blocks_unreach_special_mmap =
        GLOBAL_STAT(peak_vmm_blocks_unreach_special_mmap);
    drstats->peak_vmm_blocks_reach_heap = GLOBAL_STAT(peak_vmm_blocks_reach_heap);
    drstats->peak_vmm_blocks_reach_cache = GLOBAL_STAT(peak_vmm_blocks_reach_cache);
    drstats->peak_vmm_blocks_reach_special_heap =
        GLOBAL_STAT(peak_vmm_blocks_reach_special_heap);
    drstats->peak_vmm_blocks_reach_special_mmap =
        GLOBAL_STAT(peak_vmm_blocks_reach_special_mmap);
    if (drstats->size <= offsetof(dr_stats_t, num_native_signals))
        return true;
#ifdef UNIX
    drstats->num_native_signals = GLOBAL_STAT(num_native_signals);
#else
    drstats->num_native_signals = 0;
#endif
    if (drstats->size <= offsetof(dr_stats_t, num_cache_exits))
        return true;
    drstats->num_cache_exits = GLOBAL_STAT(num_exits);
    return true;
}
