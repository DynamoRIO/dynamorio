/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.  All rights reserved.
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
 * thread.c - thread synchronization
 */

#include "globals.h"
#include "synch.h"
#include "instrument.h" /* is_in_client_lib() */
#include "hotpatch.h"   /* hotp_only_in_tramp() */
#include "fragment.h"   /* get_at_syscall() */
#include "fcache.h"     /* in_fcache() */
#include "translate.h"
#include "native_exec.h"

extern vm_area_vector_t *fcache_unit_areas; /* from fcache.c */

static bool started_detach = false; /* set before synchall */
bool doing_detach = false;          /* set after synchall */
thread_id_t detacher_tid = INVALID_THREAD_ID;

static void
synch_thread_yield(void);

/* Thread-local data
 */
typedef struct _thread_synch_data_t {
    /* the following three fields are used to synchronize for detach, suspend
     * thread, terminate thread, terminate process */
    /* synch_lock and pending_synch_count act as a semaphore */
    /* for check_wait_at_safe_spot() must use a spin_mutex_t */
    spin_mutex_t *synch_lock;
    /* we allow pending_synch_count to be read without holding the synch_lock
     * so all updates should be ATOMIC as well as holding the lock */
    int pending_synch_count;
    /* To guarantee that the thread really has this permission you need to hold the
     * synch_lock when you read this value.  If the target thread is suspended, use a
     * trylock, as it could have been suspended while holding synch_lock (i#2805).
     */
    thread_synch_permission_t synch_perm;
    /* Only valid while holding all_threads_synch_lock and thread_initexit_lock.  Set
     * to whether synch_with_all_threads was successful in synching this thread.
     */
    bool synch_with_success;
    /* Case 10101: allows threads waiting_at_safe_spot() to set their own
     * contexts.  This use sometimes requires a full os-specific context, which
     * we hide behind a generic pointer and a size.
     */
    priv_mcontext_t *set_mcontext;
    void *set_context;
    size_t set_context_size;
#ifdef X64
    /* PR 263338: we have to pad for alignment */
    byte *set_context_alloc;
#endif
} thread_synch_data_t;

/* This lock prevents more than one thread from being in the synch_with_all_
 * threads method body at the same time (which would lead to deadlock as they
 * tried to synchronize with each other)
 */
DECLARE_CXTSWPROT_VAR(mutex_t all_threads_synch_lock,
                      INIT_LOCK_FREE(all_threads_synch_lock));

/* pass either mc or both cxt and cxt_size */
static void
free_setcontext(priv_mcontext_t *mc, void *cxt, size_t cxt_size _IF_X64(byte *cxt_alloc))
{
    if (mc != NULL) {
        ASSERT(cxt == NULL);
        global_heap_free(mc, sizeof(*mc) HEAPACCT(ACCT_OTHER));
    } else if (cxt != NULL) {
        ASSERT(cxt_size > 0);
        global_heap_free(IF_X64_ELSE(cxt_alloc, cxt), cxt_size HEAPACCT(ACCT_OTHER));
    }
}

static void
synch_thread_free_setcontext(thread_synch_data_t *tsd)
{
    free_setcontext(tsd->set_mcontext, tsd->set_context,
                    tsd->set_context_size _IF_X64(tsd->set_context_alloc));
    tsd->set_mcontext = NULL;
    tsd->set_context = NULL;
}

void
synch_init(void)
{
}

void
synch_exit(void)
{
    ASSERT(uninit_thread_count == 0);
    DELETE_LOCK(all_threads_synch_lock);
}

void
synch_thread_init(dcontext_t *dcontext)
{
    thread_synch_data_t *tsd = (thread_synch_data_t *)heap_alloc(
        dcontext, sizeof(thread_synch_data_t) HEAPACCT(ACCT_OTHER));
    dcontext->synch_field = (void *)tsd;
    tsd->pending_synch_count = 0;
    tsd->synch_perm = THREAD_SYNCH_NONE;
    tsd->synch_with_success = false;
    tsd->set_mcontext = NULL;
    tsd->set_context = NULL;
    /* the synch_lock is in unprotected memory so that check_wait_at_safe_spot
     * can call the EXITING_DR hook before releasing it */
    tsd->synch_lock = HEAP_TYPE_ALLOC(dcontext, spin_mutex_t, ACCT_OTHER, UNPROTECTED);
    ASSIGN_INIT_SPINMUTEX_FREE(*tsd->synch_lock, synch_lock);
}

void
synch_thread_exit(dcontext_t *dcontext)
{
    thread_synch_data_t *tsd = (thread_synch_data_t *)dcontext->synch_field;
    /* Could be waiting at safe spot when we detach or exit */
    synch_thread_free_setcontext(tsd);
    DELETE_SPINMUTEX(*tsd->synch_lock);
    /* Note that we do need to free this in non-debug builds since, despite
     * appearances, UNPROTECTED_LOCAL is acutally allocated on a global
     * heap. */
    HEAP_TYPE_FREE(dcontext, tsd->synch_lock, spin_mutex_t, ACCT_OTHER, UNPROTECTED);
#ifdef DEBUG
    /* for non-debug we do fast exit path and don't free local heap */
    /* clean up tsd fields here */
    heap_free(dcontext, tsd, sizeof(thread_synch_data_t) HEAPACCT(ACCT_OTHER));
#endif
}

/* Check for a no-xfer permission.  Currently used only for case 6821,
 * where we need to distinguish three groups: unsafe (wait for safe
 * point), safe and translatable, and safe but not translatable.
 */
bool
thread_synch_state_no_xfer(dcontext_t *dcontext)
{
    thread_synch_data_t *tsd = (thread_synch_data_t *)dcontext->synch_field;
    /* We use a trylock in case the thread is suspended holding synch_lock (i#2805). */
    if (spinmutex_trylock(tsd->synch_lock)) {
        bool res = (tsd->synch_perm == THREAD_SYNCH_NO_LOCKS_NO_XFER ||
                    tsd->synch_perm == THREAD_SYNCH_VALID_MCONTEXT_NO_XFER);
        spinmutex_unlock(tsd->synch_lock);
        return res;
    }
    return false;
}

bool
thread_synch_check_state(dcontext_t *dcontext, thread_synch_permission_t desired_perm)
{
    thread_synch_data_t *tsd = (thread_synch_data_t *)dcontext->synch_field;
    /* We support calling this routine from our signal handler when it has interrupted
     * DR and might be holding tsd->synch_lock or other locks.
     * We first check synch_perm w/o a lock and if it's not at least
     * THREAD_SYNCH_NO_LOCKS we do not attempt to grab synch_lock (we'd hit rank order
     * violations).  If that check passes, the only problematic lock is if we already
     * hold synch_lock, so we use test and trylocks there.
     */
    if (desired_perm < THREAD_SYNCH_NO_LOCKS) {
        ASSERT(desired_perm == THREAD_SYNCH_NONE);
        return true;
    }
    if (!THREAD_SYNCH_SAFE(tsd->synch_perm, desired_perm))
        return false;
        /* barrier to keep the 1st check above on this side of the lock below */
#ifdef WINDOWS
    MemoryBarrier();
#else
    __asm__ __volatile__("" : : : "memory");
#endif
    /* We use a trylock in case the thread is suspended holding synch_lock (i#2805).
     * We start with testlock to avoid recursive lock assertions.
     */
    if (!spinmutex_testlock(tsd->synch_lock) && spinmutex_trylock(tsd->synch_lock)) {
        bool res = THREAD_SYNCH_SAFE(tsd->synch_perm, desired_perm);
        spinmutex_unlock(tsd->synch_lock);
        return res;
    }
    return false;
}

/* Only valid while holding all_threads_synch_lock and thread_initexit_lock.  Set to
 * whether synch_with_all_threads was successful in synching this thread.
 * Cannot be called when THREAD_SYNCH_*_AND_CLEANED was requested as the
 * thread-local memory will be freed on success!
 */
bool
thread_synch_successful(thread_record_t *tr)
{
    thread_synch_data_t *tsd;
    ASSERT(tr != NULL && tr->dcontext != NULL);
    ASSERT_OWN_MUTEX(true, &all_threads_synch_lock);
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    tsd = (thread_synch_data_t *)tr->dcontext->synch_field;
    return tsd->synch_with_success;
}

#ifdef UNIX
/* i#2659: the kernel is now doing auto-restart so we have to check for the
 * pc being at the syscall.
 */
static bool
is_after_or_restarted_do_syscall(dcontext_t *dcontext, app_pc pc, bool check_vsyscall)
{
    if (is_after_do_syscall_addr(dcontext, pc))
        return true;
    if (check_vsyscall && pc == vsyscall_sysenter_return_pc)
        return true;
    if (!get_at_syscall(dcontext)) /* rule out having just reached the syscall */
        return false;
    int syslen = syscall_instr_length(dr_get_isa_mode(dcontext));
    if (is_after_do_syscall_addr(dcontext, pc + syslen))
        return true;
    if (check_vsyscall && pc + syslen == vsyscall_sysenter_return_pc)
        return true;
    return false;
}
#endif

bool
is_at_do_syscall(dcontext_t *dcontext, app_pc pc, byte *esp)
{
    app_pc buf[2];
    bool res = d_r_safe_read(esp, sizeof(buf), buf);
    if (!res) {
        ASSERT(res); /* we expect the stack to always be readable */
        return false;
    }

    if (does_syscall_ret_to_callsite()) {
#ifdef WINDOWS
        if (get_syscall_method() == SYSCALL_METHOD_INT && DYNAMO_OPTION(sygate_int)) {
            return (pc == after_do_syscall_addr(dcontext) &&
                    buf[0] == after_do_syscall_code(dcontext));
        } else {
            return pc == after_do_syscall_code(dcontext);
        }
#else
        return is_after_or_restarted_do_syscall(dcontext, pc, false /*!vsys*/);
#endif
    } else if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
#ifdef WINDOWS
        if (pc == vsyscall_after_syscall) {
            if (DYNAMO_OPTION(sygate_sysenter))
                return buf[1] == after_do_syscall_code(dcontext);
            else
                return buf[0] == after_do_syscall_code(dcontext);
        } else {
            /* not at a system call, could still have tos match after_do_syscall
             * either by chance or because we leak that value on the apps stack
             * (a non transparency) */
            ASSERT_CURIOSITY(buf[0] != after_do_syscall_code(dcontext));
            return false;
        }
#else
        /* Even when the main syscall method is sysenter, we also have a
         * do_int_syscall and do_clone_syscall that use int, so check both.
         * Note that we don't modify the stack, so once we do sysenter syscalls
         * inlined in the cache (PR 288101) we'll need some mechanism to
         * distinguish those: but for now if a sysenter instruction is used it
         * has to be do_syscall since DR's own syscalls are ints.
         */
        return is_after_or_restarted_do_syscall(dcontext, pc, true /*vsys*/);
#endif
    }
    /* we can reach here w/ a fault prior to 1st syscall on Linux */
    IF_WINDOWS(ASSERT_NOT_REACHED());
    return false;
}

/* Helper function for at_safe_spot(). Note state for client-owned threads isn't
 * considered valid since it may be holding client locks and doesn't correspond to
 * an actual app state. Caller should handle client-owned threads appropriately. */
static bool
is_native_thread_state_valid(dcontext_t *dcontext, app_pc pc, byte *esp)
{
    /* ref case 3675, the assumption is that if we aren't executing
     * out of dr memory and our stack isn't in dr memory (to disambiguate
     * pc in kernel32, ntdll etc.) then the app has a valid native context.
     * However, we can't call is_dynamo_address() as it (and its children)
     * grab too many different locks, all of which we would have to check
     * here in the same manner as fcache_unit_areas.lock in at_safe_spot().  So
     * instead we just check the pc for the dr dll, interception code, and
     * do_syscall regions and check the stack against the thread's dr stack
     * and the d_r_initstack, all of which we can do without grabbing any locks.
     * That should be sufficient at this point, FIXME try to use something
     * like is_dynamo_address() to make this more maintainable */
    /* For sysenter system calls we also have to check the top of the stack
     * for the after_do_syscall_address to catch the do_syscall @ syscall
     * itself case. */
    ASSERT(esp != NULL);
    ASSERT(is_thread_currently_native(dcontext->thread_record));
#ifdef WINDOWS
    if (pc == (app_pc)thread_attach_takeover) {
        /* We are trying to take over this thread but it has not yet been
         * scheduled.  It was native, and can't hold any DR locks.
         */
        return true;
    }
#endif
    return (!is_in_dynamo_dll(pc) &&
            IF_WINDOWS(!is_part_of_interception(pc) &&)(
                !in_generated_routine(dcontext, pc) ||
                /* we allow native thread to be at do_syscall - for int syscalls the pc
                 * (syscall return point) will be in do_syscall (so in generated routine)
                 * xref case 9333 */
                is_at_do_syscall(dcontext, pc, esp)) &&
            !is_on_initstack(esp) && !is_on_dstack(dcontext, esp) &&
            !is_in_client_lib(pc) &&
            /* xref PR 200067 & 222812 on client-owned native threads */
            !IS_CLIENT_THREAD(dcontext) &&
#ifdef HOT_PATCHING_INTERFACE
            /* Shouldn't be in the middle of executing a hotp_only patch.  The
             * check for being in hotp_dll is DR_WHERE_HOTPATCH because the patch can
             * change esp.
             */
            (dcontext->whereami != DR_WHERE_HOTPATCH &&
             /* dynamo dll check has been done */
             !hotp_only_in_tramp(pc)) &&
#endif
            true /* no effect, simplifies ifdef handling with && above */
    );
}

/* Translates the context mcontext for the given thread trec.  If
 * restore_memory is true, also restores any memory values that were
 * shifted (primarily due to clients).  If restore_memory is true, the
 * caller should always relocate the translated thread, as it may not
 * execute properly if left at its current location (it could be in the
 * middle of client code in the cache).
 * If recreate_app_state() is called, f will be passed through to it.
 *
 * Like any instance where a thread_record_t is used by a thread other than its
 * owner, the caller must hold the thread_initexit_lock to ensure that it
 * remains valid.
 * Requires thread trec is at_safe_spot().
 */
bool
translate_mcontext(thread_record_t *trec, priv_mcontext_t *mcontext, bool restore_memory,
                   fragment_t *f)
{
    thread_synch_data_t *tsd = (thread_synch_data_t *)trec->dcontext->synch_field;
    bool res;
    recreate_success_t success;
    bool native_translate = false;
    ASSERT(tsd->pending_synch_count >= 0);
    /* check if native thread */
    if (is_thread_currently_native(trec)) {
        /* running natively, no need to translate unless at do_syscall for an
         * intercepted-via-trampoline syscall which we allow now for case 9333 */
        if (IS_CLIENT_THREAD(trec->dcontext)) {
            /* don't need to translate anything */
            LOG(THREAD_GET, LOG_SYNCH, 1,
                "translate context, thread " TIDFMT " is client "
                "thread, no translation needed\n",
                trec->id);
            return true;
        }
        if (is_native_thread_state_valid(trec->dcontext, (app_pc)mcontext->pc,
                                         (byte *)mcontext->xsp)) {
#ifdef WINDOWS
            if ((app_pc)mcontext->pc == (app_pc)thread_attach_takeover) {
                LOG(THREAD_GET, LOG_SYNCH, 1,
                    "translate context, thread " TIDFMT " at "
                    "takeover point\n",
                    trec->id);
                thread_attach_translate(trec->dcontext, mcontext, restore_memory);
                return true;
            }
#endif
            if (is_at_do_syscall(trec->dcontext, (app_pc)mcontext->pc,
                                 (byte *)mcontext->xsp)) {
                LOG(THREAD_GET, LOG_SYNCH, 1,
                    "translate context, thread " TIDFMT " running "
                    "natively, at do_syscall so translation needed\n",
                    trec->id);
                native_translate = true;
            } else {
                LOG(THREAD_GET, LOG_SYNCH, 1,
                    "translate context, thread " TIDFMT " running "
                    "natively, no translation needed\n",
                    trec->id);
                return true;
            }
        } else {
            /* now that do_syscall is a safe spot for native threads we shouldn't get
             * here for get context on self, FIXME - is however possible to get here
             * via get_context on unsuspended thread (result of which is technically
             * undefined according to MS), see get_context post sys comments
             * (should prob. synch there in which case can assert here) */
            ASSERT(trec->id != d_r_get_thread_id());
            ASSERT_CURIOSITY(false &&
                             "translate failure, likely get context on "
                             "unsuspended native thread");
            /* we'll just try to translate and hope for the best */
            native_translate = true;
        }
    }
    if (!native_translate) {
        /* check if waiting at a good spot */
        spinmutex_lock(tsd->synch_lock);
        res = THREAD_SYNCH_SAFE(tsd->synch_perm, THREAD_SYNCH_VALID_MCONTEXT);
        spinmutex_unlock(tsd->synch_lock);
        if (res) {
            LOG(THREAD_GET, LOG_SYNCH, 1,
                "translate context, thread " TIDFMT " waiting at "
                "valid mcontext point, copying over\n",
                trec->id);
            DOLOG(2, LOG_SYNCH, {
                LOG(THREAD_GET, LOG_SYNCH, 2, "Thread State\n");
                dump_mcontext(get_mcontext(trec->dcontext), THREAD_GET, DUMP_NOT_XML);
            });
            *mcontext = *get_mcontext(trec->dcontext);
            if (dr_xl8_hook_exists()) {
                if (!instrument_restore_nonfcache_state(trec->dcontext, true, mcontext))
                    return false;
            }
            return true;
        }
    }

    /* In case 4148 we see a thread calling NtGetContextThread on itself, which
     * is undefined according to MS but it does get the syscall address, so it's
     * fine with us.  For other threads the app shouldn't be asking about them
     * unless they're suspended, and the same goes for us.
     */
    ASSERT_CURIOSITY(trec->dcontext->whereami == DR_WHERE_FCACHE ||
                     trec->dcontext->whereami == DR_WHERE_SIGNAL_HANDLER ||
                     native_translate || trec->id == d_r_get_thread_id());
    LOG(THREAD_GET, LOG_SYNCH, 2,
        "translate context, thread " TIDFMT " at pc_recreatable spot translating\n",
        trec->id);
    success = recreate_app_state(trec->dcontext, mcontext, restore_memory, f);
    if (success != RECREATE_SUCCESS_STATE) {
        /* should never happen right?
         * actually it does when deciding whether can deliver a signal
         * immediately (PR 213040).
         */
        LOG(THREAD_GET, LOG_SYNCH, 1,
            "translate context, thread " TIDFMT " unable to translate context at pc"
            " = " PFX "\n",
            trec->id, mcontext->pc);
        SYSLOG_INTERNAL_WARNING_ONCE("failed to translate");
        return false;
    }
    return true;
}

static bool
waiting_at_safe_spot(thread_record_t *trec, thread_synch_state_t desired_state)
{
    thread_synch_data_t *tsd = (thread_synch_data_t *)trec->dcontext->synch_field;
    ASSERT(tsd->pending_synch_count >= 0);
    /* Check if waiting at a good spot.  We can't spin in case the suspended thread is
     * holding this lock (e.g., i#2805).  We only need the lock to check synch_perm.
     */
    if (spinmutex_trylock(tsd->synch_lock)) {
        thread_synch_permission_t perm = tsd->synch_perm;
        bool res = THREAD_SYNCH_SAFE(perm, desired_state);
        spinmutex_unlock(tsd->synch_lock);
        if (res) {
            LOG(THREAD_GET, LOG_SYNCH, 2,
                "thread " TIDFMT " waiting at safe spot (synch_perm=%d)\n", trec->id,
                perm);
            return true;
        }
    } else {
        LOG(THREAD_GET, LOG_SYNCH, 2,
            "at_safe_spot unable to get locks to test if thread " TIDFMT " is waiting "
            "at safe spot\n",
            trec->id);
    }
    return false;
}

static bool
should_suspend_client_thread(dcontext_t *dcontext, thread_synch_state_t desired_state)
{
    /* Marking un-suspendable does not apply to cleaning/terminating */
    ASSERT(IS_CLIENT_THREAD(dcontext));
    return (THREAD_SYNCH_IS_CLEANED(desired_state) || dcontext->client_data->suspendable);
}

/* checks whether the thread trec is at a spot suitable for requested define
 * desired_state
 * Requires that trec thread is suspended */
/* Note that since trec is potentially suspended at an arbitrary point,
 * this function (and any function it calls) cannot call mutex_lock as
 * trec thread may hold a lock.  It is ok for at_safe_spot to return false if
 * it can't obtain a lock on the first try. FIXME : in the long term we may
 * want to go to a locking model that stores the thread id of the owner in
 * which case we can check for this situation directly
 */
bool
at_safe_spot(thread_record_t *trec, priv_mcontext_t *mc,
             thread_synch_state_t desired_state)
{
    bool safe = false;

    if (waiting_at_safe_spot(trec, desired_state))
        return true;

#ifdef ARM
    if (TESTANY(EFLAGS_IT, mc->cpsr)) {
        LOG(THREAD_GET, LOG_SYNCH, 2,
            "thread " TIDFMT " not at safe spot (pc=" PFX " in an IT block) for %d\n",
            trec->id, mc->pc, desired_state);
        return false;
    }
#endif
    /* check if suspended at good spot */
    /* FIXME: right now don't distinguish between suspend and term privileges
     * even though suspend is stronger requirement, are the checks below
     * sufficient */
    /* FIXME : check with respect to flush, should be ok */
    /* test fcache_unit_areas.lock (from fcache.c) before calling recreate_app_state
     * since it calls in_fcache() which uses the lock (if we are in_fcache()
     * assume other locks are not a problem (so is_dynamo_address is fine)) */
    /* Right now the only dr code that ends up in the cache is our DLL main
     * (which we'll reduce/get rid of with libc independence), our takeover
     * from preinject return stack, and the callback.c interception code.
     * FIXME : test for just these and ASSERT(!is_dynamo_address) otherwise */
    if (is_thread_currently_native(trec)) {
        /* thread is running native, verify is not in dr code */
        /* We treat client-owned threads (such as a client nudge thread) as native and
         * consider them safe if they are in the client_lib.  Since they might own client
         * locks that could block application threads from progressing, we synchronize
         * with them last.  FIXME - xref PR 231301 - since we can't disambiguate
         * client->ntdll/gencode which is safe from client->dr->ntdll/gencode which isn't
         * we disallow both.  This could hurt synchronization efficiency if the client
         * owned thread spent most of its execution time calling out of its lib to ntdll
         * routines or generated code. */
        if (IS_CLIENT_THREAD(trec->dcontext)) {
            safe = (trec->dcontext->client_data->client_thread_safe_for_synch ||
                    is_in_client_lib(mc->pc)) &&
                /* Do not cleanup/terminate a thread holding a client lock (PR 558463) */
                /* Actually, don't consider a thread holding a client lock to be safe
                 * at all (PR 609569): client should use
                 * dr_client_thread_set_suspendable(false) if its thread spends a lot
                 * of time holding locks.
                 */
                (!should_suspend_client_thread(trec->dcontext, desired_state) ||
                 trec->dcontext->client_data->mutex_count == 0);
        }
        if (is_native_thread_state_valid(trec->dcontext, mc->pc, (byte *)mc->xsp)) {
            safe = true;
            /* We should always be able to translate a valid native state, but be
             * sure to check before thread_attach_exit().
             */
            ASSERT(translate_mcontext(trec, mc, false /*just querying*/, NULL));
#ifdef WINDOWS
            if (mc->pc == (app_pc)thread_attach_takeover &&
                THREAD_SYNCH_IS_CLEANED(desired_state)) {
                /* The takeover data will be freed at process exit, but we might
                 * clean up a thread mid-run, so make sure we free the data.
                 */
                thread_attach_exit(trec->dcontext, mc);
            }
#endif
        }
    } else if (desired_state == THREAD_SYNCH_TERMINATED_AND_CLEANED &&
               trec->dcontext->whereami == DR_WHERE_FCACHE &&
               trec->dcontext->client_data->at_safe_to_terminate_syscall) {
        /* i#1420: At safe to terminate syscall like dr_sleep in a clean call.
         * XXX: A thread in dr_sleep might not be safe to terminate for some
         * corner cases: for example, a client may hold a lock and then go sleep,
         * terminating it may mess the client up for not releasing the lock.
         * We limit this to the thread being in fcache (i.e., from a clean call)
         * to rule out some corner cases.
         */
        safe = true;
    } else if ((!WRITE_LOCK_HELD(&fcache_unit_areas->lock) &&
                /* even though we only need the read lock, if our target holds it
                 * and a 3rd thread requests the write lock, we'll hang if we
                 * ask for the read lock (case 7493)
                 */
                !READ_LOCK_HELD(&fcache_unit_areas->lock)) &&
               recreate_app_state(trec->dcontext, mc, false /*just query*/, NULL) ==
                   RECREATE_SUCCESS_STATE &&
               /* It's ok to call is_dynamo_address even though it grabs many
                * locks because recreate_app_state succeeded.
                */
               !is_dynamo_address(mc->pc)) {
        safe = true;
    }
    if (safe) {
        ASSERT(trec->dcontext->whereami == DR_WHERE_FCACHE ||
               trec->dcontext->whereami == DR_WHERE_SIGNAL_HANDLER ||
               is_thread_currently_native(trec));
        LOG(THREAD_GET, LOG_SYNCH, 2,
            "thread " TIDFMT " suspended at safe spot pc=" PFX "\n", trec->id, mc->pc);

        return true;
    }
    LOG(THREAD_GET, LOG_SYNCH, 2,
        "thread " TIDFMT " not at safe spot (pc=" PFX ") for %d\n", trec->id, mc->pc,
        desired_state);
    return false;
}

/* a fast way to tell a thread if it should call check_wait_at_safe_spot
 * if translating context would be expensive */
bool
should_wait_at_safe_spot(dcontext_t *dcontext)
{
    thread_synch_data_t *tsd = (thread_synch_data_t *)dcontext->synch_field;
    return (tsd->pending_synch_count != 0);
}

/* use with care!  normally check_wait_at_safe_spot() should be called instead */
void
set_synch_state(dcontext_t *dcontext, thread_synch_permission_t state)
{
    if (state >= THREAD_SYNCH_NO_LOCKS)
        ASSERT_OWN_NO_LOCKS();

    thread_synch_data_t *tsd = (thread_synch_data_t *)dcontext->synch_field;
    /* We have a wart in the settings here (i#2805): a caller can set
     * THREAD_SYNCH_NO_LOCKS, yet here we're acquiring locks.  In fact if this thread
     * is suspended in between the lock and the unset of synch_perm from
     * THREAD_SYNCH_NO_LOCKS back to THREAD_SYNCH_NONE, it can cause problems.  We
     * have everyone who might query in such a state use a trylock and assume
     * synch_perm is THREAD_SYNCH_NONE if the lock cannot be acquired.
     */
    spinmutex_lock(tsd->synch_lock);
    tsd->synch_perm = state;
    spinmutex_unlock(tsd->synch_lock);
}

/* checks to see if any threads are waiting to synch with this one and waits
 * if they are
 * cur_state - a given permission define from above that describes the current
 *             state of the caller
 * NOTE - Requires the caller is !could_be_linking (i.e. not in an
 * enter_couldbelinking state)
 */
void
check_wait_at_safe_spot(dcontext_t *dcontext, thread_synch_permission_t cur_state)
{
    thread_synch_data_t *tsd = (thread_synch_data_t *)dcontext->synch_field;
    byte cxt[MAX(CONTEXT_HEAP_SIZE_OPAQUE, sizeof(priv_mcontext_t))];
    bool set_context = false;
    bool set_mcontext = false;
    if (tsd->pending_synch_count == 0 || cur_state == THREAD_SYNCH_NONE)
        return;
    ASSERT(tsd->pending_synch_count >= 0);
    DEBUG_DECLARE(app_pc pc = get_mcontext(dcontext)->pc;)
    LOG(THREAD, LOG_SYNCH, 2, "waiting for synch with state %d (pc " PFX ")\n", cur_state,
        pc);
    if (cur_state == THREAD_SYNCH_VALID_MCONTEXT) {
        ASSERT(!is_dynamo_address(pc));
        /* for detach must set this here and now */
        IF_WINDOWS(set_last_error(dcontext->app_errno));
    }
    spinmutex_lock(tsd->synch_lock);
    tsd->synch_perm = cur_state;
    /* Since can be killed, suspended, etc. must call the exit dr hook. But, to
     * avoid races, we must do so before giving up the synch_lock. This is why
     * that lock has to be in unprotected memory. FIXME - for single thread in
     * dr this will lead to rank order violation between dr exclusivity lock
     * and the synch_lock with no easy workaround (real deadlocks possible).
     * Luckily we'll prob. never use that option. */
    if (INTERNAL_OPTION(single_thread_in_DR)) {
        ASSERT_NOT_IMPLEMENTED(false);
    }
    EXITING_DR();
    /* Ref case 5074, for us/app to successfully SetThreadContext at
     * this synch point, this thread can NOT be at a system call. So, for
     * case 10101, we instead have threads that are waiting_at_safe_spot()
     * set their own contexts, allowing us to make system calls here.
     * We don't yet handle the detach case, so it still requires no system
     * calls, including the act of releasing the synch_lock
     * which is why that lock has to be a user mode spin yield lock.
     * FIXME: we could change tsd->synch_lock back to a regular lock
     * once we have detach handling system calls here.
     */
    spinmutex_unlock(tsd->synch_lock);
    while (tsd->pending_synch_count > 0 && tsd->synch_perm != THREAD_SYNCH_NONE) {
        STATS_INC_DC(dcontext, synch_loops_wait_safe);
#ifdef WINDOWS
        if (started_detach) {
            /* We spin for any non-detach synchs encountered during detach
             * since we have no flag telling us this synch is for detach. */
            /* Ref case 5074, can NOT use os_thread_yield here. This must be a user
             * mode spin loop. */
            SPINLOCK_PAUSE();
        } else {
#endif
            /* FIXME case 10100: replace this sleep/yield with a wait_for_event() */
            synch_thread_yield();
#ifdef WINDOWS
        }
#endif
    }
    /* Regain the synch_lock before ENTERING_DR to avoid races with getting
     * suspended/killed in the middle of ENTERING_DR (before synch_perm is
     * reset to NONE). */
    /* Ref case 5074, for detach we still can NOT use os_thread_yield here (no system
     * calls) so don't allow the spinmutex_lock to yield while grabbing the lock. */
    spinmutex_lock_no_yield(tsd->synch_lock);
    ENTERING_DR();
    tsd->synch_perm = THREAD_SYNCH_NONE;
    if (tsd->set_mcontext != NULL || tsd->set_context != NULL) {
        IF_WINDOWS(ASSERT(!started_detach));
        /* Make a local copy */
        ASSERT(sizeof(cxt) >= sizeof(priv_mcontext_t));
        if (tsd->set_mcontext != NULL) {
            set_mcontext = true;
            memcpy(cxt, tsd->set_mcontext, sizeof(*tsd->set_mcontext));
        } else {
            set_context = true;
            memcpy(cxt, tsd->set_context, tsd->set_context_size);
        }
        synch_thread_free_setcontext(tsd); /* sets to NULL for us */
    }
    spinmutex_unlock(tsd->synch_lock);
    LOG(THREAD, LOG_SYNCH, 2, "done waiting for synch with state %d (pc " PFX ")\n",
        cur_state, pc);
    if (set_mcontext || set_context) {
        /* FIXME: see comment in dispatch.c check_wait_at_safe_spot() call
         * about problems with KSTART(fcache_* differences bet the target
         * being at the synch point vs in the cache.
         */
        if (set_mcontext)
            thread_set_self_mcontext((priv_mcontext_t *)cxt);
        else
            thread_set_self_context((void *)cxt);
        ASSERT_NOT_REACHED();
    }
}

/* adjusts the pending synch count */
void
adjust_wait_at_safe_spot(dcontext_t *dcontext, int amt)
{
    thread_synch_data_t *tsd = (thread_synch_data_t *)dcontext->synch_field;
    ASSERT(tsd->pending_synch_count >= 0);
    spinmutex_lock(tsd->synch_lock);
    ATOMIC_ADD(int, tsd->pending_synch_count, amt);
    spinmutex_unlock(tsd->synch_lock);
}

/* Case 10101: Safely sets the context for a target thread that may be waiting at a
 * safe spot, in which case we do not want to directly do a setcontext as the return
 * from the yield or wait system call will mess up the state (case 5074).
 * Assumes that cxt was allocated on the global heap, and frees it, rather than
 * making its own copy (as an optimization).
 * Does not work on the executing thread.
 * Caller must hold thread_initexit_lock.
 * If used on behalf of the app, it's up to the caller to check for privileges.
 */
bool
set_synched_thread_context(thread_record_t *trec,
                           /* pass either mc or both cxt and cxt_size */
                           priv_mcontext_t *mc, void *cxt, size_t cxt_size,
                           thread_synch_state_t desired_state _IF_X64(byte *cxt_alloc)
                               _IF_WINDOWS(NTSTATUS *status /*OUT*/))
{
    bool res = true;
    ASSERT(trec != NULL && trec->dcontext != NULL);
    ASSERT(trec->dcontext != get_thread_private_dcontext());
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
#ifdef WINDOWS
    if (status != NULL)
        *status = STATUS_SUCCESS;
#endif
    if (waiting_at_safe_spot(trec, desired_state)) {
        /* case 10101: to allow system calls in check_wait_at_safe_spot() for
         * performance reasons we have the waiting thread perform its own setcontext.
         */
        thread_synch_data_t *tsd = (thread_synch_data_t *)trec->dcontext->synch_field;
        spinmutex_lock(tsd->synch_lock);
        if (tsd->set_mcontext != NULL || tsd->set_context != NULL) {
            /* Two synchs in a row while still waiting; 2nd takes precedence */
            STATS_INC(wait_multiple_setcxt);
            synch_thread_free_setcontext(tsd);
        }
#ifdef WINDOWS
        LOG(THREAD_GET, LOG_SYNCH, 2,
            "set_synched_thread_context %d to pc " PFX " via %s\n", trec->id,
            (mc != NULL) ? mc->pc : (app_pc)((CONTEXT *)cxt)->CXT_XIP,
            (mc != NULL) ? "mc" : "CONTEXT");
#else
        ASSERT_NOT_IMPLEMENTED(mc != NULL); /* XXX: need sigcontext or sig_full_cxt_t */
#endif
        if (mc != NULL)
            tsd->set_mcontext = mc;
        else {
            ASSERT(cxt != NULL && cxt_size > 0);
            tsd->set_context = cxt;
            tsd->set_context_size = cxt_size;
        }
        IF_X64(tsd->set_context_alloc = cxt_alloc);
        ASSERT(THREAD_SYNCH_SAFE(tsd->synch_perm, desired_state));
        ASSERT(tsd->pending_synch_count >= 0);
        /* Don't need to change pending_synch_count or anything; when thread is
         * resumed it will properly reset everything itself */
        spinmutex_unlock(tsd->synch_lock);
    } else {
        if (mc != NULL) {
            res = thread_set_mcontext(trec, mc);
        } else {
#ifdef WINDOWS
            /* sort of ugly: but NtSetContextThread handling needs the status */
            if (status != NULL) {
                *status = nt_set_context(trec->handle, (CONTEXT *)cxt);
                res = NT_SUCCESS(*status);
            } else
                res = thread_set_context(trec->handle, (CONTEXT *)cxt);
#else
            /* currently there are no callers who don't pass mc: presumably
             * PR 212090 will change that */
            ASSERT_NOT_IMPLEMENTED(false);
#endif
        }
        free_setcontext(mc, cxt, cxt_size _IF_X64(cxt_alloc));
    }
    return res;
}

/* This is used to limit the maximum number of times synch_with_thread or
 * synch_with_all_threads spin yield loops while waiting on an exiting thread.
 * We assert if we ever break out of the loop because of this limit.  FIXME make
 * sure this limit is large enough that if it does ever trigger it's because
 * of some kind of deadlock situation.  Breaking out of the synchronization loop
 * early is a correctness issue.  Right now the limits are large but arbitrary.
 * FIXME : once we are confident about thread synch get rid of these max loop checks.
 * N.B.: the THREAD_SYNCH_SMALL_LOOP_MAX flag causes us to divide these by 10.
 */
#define SYNCH_ALL_THREADS_MAXIMUM_LOOPS (DYNAMO_OPTION(synch_all_threads_max_loops))
#define SYNCH_MAXIMUM_LOOPS (DYNAMO_OPTION(synch_thread_max_loops))

/* Amt of time in ms to wait for threads to get to a safe spot per a loop,
 * see comments in synch_with_yield() on value.  Our default value is 5ms which,
 * depending on the tick resolution could end up being as long as 10 ms. */
#define SYNCH_WITH_WAIT_MS ((int)DYNAMO_OPTION(synch_with_sleep_time))

/* for use by synch_with_* routines to wait for thread(s) */
static void
synch_thread_yield()
{
    /* xref 9400, 9488 - os_thread_yield() works ok on an UP machine, but on an MP machine
     * yield might not actually do anything (in which case we burn through to the max
     * loop counts pretty quick).  We actually do want to wait a reasonable amt of time
     * since the target thread might be doing some long latency dr operation (like
     * dumping 500kb of registry into a forensics file) so we have the option to sleep
     * instead. */
    uint num_procs = get_num_processors();
    ASSERT(num_procs != 0);
    if ((num_procs == 1 && DYNAMO_OPTION(synch_thread_sleep_UP)) ||
        (num_procs > 1 && DYNAMO_OPTION(synch_thread_sleep_MP))) {
        os_thread_sleep(SYNCH_WITH_WAIT_MS);
    } else {
        os_thread_yield();
    }
}

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
 * caller state, then initexit_lock may be released and re-acquired
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
                  thread_synch_permission_t caller_state,
                  thread_synch_state_t desired_state, uint flags)
{
    thread_id_t my_id = d_r_get_thread_id();
    uint loop_count = 0;
    int expect_exiting = 0;
    thread_record_t *my_tr = thread_lookup(my_id), *trec = NULL;
    dcontext_t *dcontext = NULL;
    priv_mcontext_t mc;
    thread_synch_result_t res = THREAD_SYNCH_RESULT_NOT_SAFE;
    bool first_loop = true;
    IF_UNIX(bool actually_suspended = true;)
    const uint max_loops = TEST(THREAD_SYNCH_SMALL_LOOP_MAX, flags)
        ? (SYNCH_MAXIMUM_LOOPS / 10)
        : SYNCH_MAXIMUM_LOOPS;

    ASSERT(id != my_id);
    /* Must set ABORT or IGNORE.  Only caller can RETRY as need a new
     * set of threads for that, hoping problematic one is short-lived.
     */
    ASSERT(
        TESTANY(THREAD_SYNCH_SUSPEND_FAILURE_ABORT | THREAD_SYNCH_SUSPEND_FAILURE_IGNORE,
                flags) &&
        !TESTALL(THREAD_SYNCH_SUSPEND_FAILURE_ABORT | THREAD_SYNCH_SUSPEND_FAILURE_IGNORE,
                 flags));

    if (my_tr != NULL) {
        dcontext = my_tr->dcontext;
        expect_exiting = dcontext->is_exiting ? 1 : 0;
        ASSERT(exiting_thread_count >= expect_exiting);
    } else {
        /* calling thread should always be a known thread */
        ASSERT_NOT_REACHED();
    }

    LOG(THREAD, LOG_SYNCH, 2,
        "Synching with thread " TIDFMT ", giving %d, requesting %d, blocking=%d\n", id,
        caller_state, desired_state, block);

    if (!hold_initexit_lock)
        d_r_mutex_lock(&thread_initexit_lock);

    while (true) {
        /* get thread record */
        /* FIXME : thread id recycling is possible that this could be a
         * different thread, perhaps we should take handle instead of id
         * FIXME: use the new num field of thread_record_t?
         */
        LOG(THREAD, LOG_SYNCH, 3, "Looping on synch with thread " TIDFMT "\n", id);
        trec = thread_lookup(id);
        /* We test the exiting thread count to avoid races between terminate/
         * suspend thread (current thread, though we could be here for other
         * reasons) and an exiting thread (who might no longer be on the all
         * threads list) who is still using shared resources (ref case 3121) */
        if ((trec == NULL && exiting_thread_count == expect_exiting) ||
            loop_count++ > max_loops) {
            /* make sure we didn't exit the loop without synchronizing, FIXME :
             * in release builds we assume the synchronization is failing and
             * continue without it, but that is dangerous.
             * It is now up to the caller to handle this, and some use
             * small loop counts and abort on failure, so only a curiosity. */
            ASSERT_CURIOSITY(loop_count < max_loops);
            LOG(THREAD, LOG_SYNCH, 3,
                "Exceeded loop count synching with thread " TIDFMT "\n", id);
            goto exit_synch_with_thread;
        }
        DOSTATS({
            if (trec == NULL && exiting_thread_count > expect_exiting) {
                LOG(THREAD, LOG_SYNCH, 2, "Waiting for an exiting thread\n");
                STATS_INC(synch_yields_for_exiting_thread);
            }
        });
#ifdef UNIX
        if (trec != NULL && trec->execve) {
            /* i#237/PR 498284: clean up vfork "threads" that invoked execve.
             * There should be no race since vfork suspends the parent.
             */
            res = THREAD_SYNCH_RESULT_SUCCESS;
            actually_suspended = false;
            break;
        }
#endif
        if (trec != NULL) {
            if (first_loop) {
                adjust_wait_at_safe_spot(trec->dcontext, 1);
                first_loop = false;
            }
            if (!os_thread_suspend(trec)) {
                /* FIXME : eventually should be a real assert once we figure out
                 * how to handle threads with low privilege handles */
                /* For dr_api_exit, we may have missed a thread exit. */
                ASSERT_CURIOSITY_ONCE(
                    (trec->dcontext->currently_stopped IF_APP_EXPORTS(|| dr_api_exit)) &&
                    "Thead synch unable to suspend target"
                    " thread, case 2096?");
                res = (TEST(THREAD_SYNCH_SUSPEND_FAILURE_IGNORE, flags)
                           ? THREAD_SYNCH_RESULT_SUCCESS
                           : THREAD_SYNCH_RESULT_SUSPEND_FAILURE);
                IF_UNIX(actually_suspended = false);
                break;
            }
            if (!thread_get_mcontext(trec, &mc)) {
                /* FIXME : eventually should be a real assert once we figure out
                 * how to handle threads with low privilege handles */
                ASSERT_CURIOSITY_ONCE(false &&
                                      "Thead synch unable to get_context target"
                                      " thread, case 2096?");
                res = (TEST(THREAD_SYNCH_SUSPEND_FAILURE_IGNORE, flags)
                           ? THREAD_SYNCH_RESULT_SUCCESS
                           : THREAD_SYNCH_RESULT_SUSPEND_FAILURE);
                /* Make sure to not leave suspended if not returning success */
                if (!TEST(THREAD_SYNCH_SUSPEND_FAILURE_IGNORE, flags))
                    os_thread_resume(trec);
                break;
            }
            if (at_safe_spot(trec, &mc, desired_state)) {
                /* FIXME: case 5325 for detach handling and testing */
                IF_WINDOWS(
                    ASSERT_NOT_IMPLEMENTED(!dcontext->aslr_context.sys_aslr_clobbered));
                LOG(THREAD, LOG_SYNCH, 2, "Thread " TIDFMT " suspended in good spot\n",
                    id);
                LOG(trec->dcontext->logfile, LOG_SYNCH, 2,
                    "@@@@@@@@@@@@@@@@@@ SUSPENDED BY THREAD " TIDFMT " synch_with_thread "
                    "@@@@@@@@@@@@@@@@@@\n",
                    my_id);
                res = THREAD_SYNCH_RESULT_SUCCESS;
                break;
            } else {
                RSTATS_INC(synchs_not_at_safe_spot);
            }
            if (!os_thread_resume(trec)) {
                ASSERT_NOT_REACHED();
                res = (TEST(THREAD_SYNCH_SUSPEND_FAILURE_IGNORE, flags)
                           ? THREAD_SYNCH_RESULT_SUCCESS
                           : THREAD_SYNCH_RESULT_SUSPEND_FAILURE);
                break;
            }
        }
        /* don't loop if !block, before we ever release initexit_lock in case
         * caller is holding it and not blocking, (i.e. wants to keep it) */
        if (!block)
            break;
        /* see if someone is waiting for us */
        if (dcontext != NULL && caller_state != THREAD_SYNCH_NONE &&
            should_wait_at_safe_spot(dcontext)) {
            if (trec != NULL)
                adjust_wait_at_safe_spot(trec->dcontext, -1);
            d_r_mutex_unlock(&thread_initexit_lock);
            /* ref case 5552, if we've inc'ed the exiting thread count need to
             * adjust it back before calling check_wait_at_safe_spot since we
             * may end up being killed there */
            if (dcontext->is_exiting) {
                ASSERT(exiting_thread_count >= 1);
                ATOMIC_DEC(int, exiting_thread_count);
            }
            check_wait_at_safe_spot(dcontext, caller_state);
            if (dcontext->is_exiting) {
                ATOMIC_INC(int, exiting_thread_count);
            }
            d_r_mutex_lock(&thread_initexit_lock);
            trec = thread_lookup(id);
            /* Like above, we test the exiting thread count to avoid races
             * between terminate/suspend thread (current thread, though we
             * could be here for other reasons) and an exiting thread (who
             * might no longer be on the all threads list) who is still using
             * shared resources (ref case 3121) */
            if (trec == NULL && exiting_thread_count == expect_exiting) {
                if (!hold_initexit_lock)
                    d_r_mutex_unlock(&thread_initexit_lock);
                return THREAD_SYNCH_RESULT_SUCCESS;
            }
            DOSTATS({
                if (trec == NULL && exiting_thread_count > expect_exiting) {
                    LOG(THREAD, LOG_SYNCH, 2, "Waiting for an exiting thread\n");
                    STATS_INC(synch_yields_for_exiting_thread);
                }
            });
            if (trec != NULL)
                adjust_wait_at_safe_spot(trec->dcontext, 1);
        }
        STATS_INC(synch_yields);
        d_r_mutex_unlock(&thread_initexit_lock);
        /* Note - we only need call the ENTER/EXIT_DR hooks if single thread
         * in dr since we are not really exiting DR here (we just need to give
         * up the exclusion lock for a while to let thread we are trying to
         * synch with make progress towards a safe synch point). */
        if (INTERNAL_OPTION(single_thread_in_DR))
            EXITING_DR(); /* give up DR exclusion lock */
        synch_thread_yield();
        if (INTERNAL_OPTION(single_thread_in_DR))
            ENTERING_DR(); /* re-gain DR exclusion lock */
        d_r_mutex_lock(&thread_initexit_lock);
    }
    /* reset this back to before */
    adjust_wait_at_safe_spot(trec->dcontext, -1);
    /* success!, is suspended (or already exited) put in desired state */
    if (res == THREAD_SYNCH_RESULT_SUCCESS) {
        LOG(THREAD, LOG_SYNCH, 2,
            "Success synching with thread " TIDFMT " performing cleanup\n", id);
        if (THREAD_SYNCH_IS_TERMINATED(desired_state)) {
            if (IF_UNIX_ELSE(!trec->execve, true))
                os_thread_terminate(trec);
#ifdef UNIX
            /* We need to ensure the target thread has received the
             * signal and is no longer using its sigstack or ostd struct
             * before we clean those up.
             */
            /* PR 452168: if failed to send suspend signal, do not spin */
            if (actually_suspended) {
                if (!is_thread_terminated(trec->dcontext)) {
                    /* i#96/PR 295561: use futex(2) if available. Blocks until
                     * the thread gets terminated.
                     */
                    os_wait_thread_terminated(trec->dcontext);
                }
            } else
                ASSERT(TEST(THREAD_SYNCH_SUSPEND_FAILURE_IGNORE, flags));
#endif
        }
        if (THREAD_SYNCH_IS_CLEANED(desired_state)) {
            dynamo_other_thread_exit(trec _IF_WINDOWS(false));
        }
    }
exit_synch_with_thread:
    if (!hold_initexit_lock)
        d_r_mutex_unlock(&thread_initexit_lock);
    return res;
}

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
 * NOTE - when !all_synched, if desired_synch_state is not cleaned or synch result is
 * ignored, the caller is reponsible for resuming threads that are suspended,
 * freeing allocation for threads array and releasing locks
 * Caller should call end_synch_with_all_threads when finished to accomplish that.
 */
bool
synch_with_all_threads(thread_synch_state_t desired_synch_state,
                       /*OUT*/ thread_record_t ***threads_out,
                       /*OUT*/ int *num_threads_out, thread_synch_permission_t cur_state,
                       /* FIXME: turn the ThreadSynch* enums into bitmasks and merge
                        * into flags param */
                       uint flags)
{
    /* Case 8815: we cannot use the OUT params themselves internally as they
     * may be volatile, so we need our own values until we're ready to return
     */
    bool threads_are_stale = true;
    thread_record_t **threads = NULL;
    int num_threads = 0;
    /* we record ids from before we gave up thread_initexit_lock */
    thread_id_t *thread_ids_temp = NULL;
    int num_threads_temp = 0, i, j, expect_self_exiting = 0;
    /* synch array contains a SYNCH_WITH_ALL_ value for each thread */
    uint *synch_array = NULL, *synch_array_temp = NULL;
    enum {
        SYNCH_WITH_ALL_NEW = 0,
        SYNCH_WITH_ALL_NOTIFIED = 1,
        SYNCH_WITH_ALL_SYNCHED = 2,
    };
    bool all_synched = false;
    thread_id_t my_id = d_r_get_thread_id();
    uint loop_count = 0;
    thread_record_t *tr = thread_lookup(my_id);
    dcontext_t *dcontext = NULL;
    uint flags_one; /* flags for synch_with_thread() call */
    thread_synch_result_t synch_res;
    const uint max_loops = TEST(THREAD_SYNCH_SMALL_LOOP_MAX, flags)
        ? (SYNCH_ALL_THREADS_MAXIMUM_LOOPS / 10)
        : SYNCH_ALL_THREADS_MAXIMUM_LOOPS;
    /* We treat client-owned threads as native but they don't have a clean native state
     * for us to suspend them in (they are always in client or dr code).  We need to be
     * able to suspend such threads so that they're !couldbelinking and holding no dr
     * locks. We make the assumption that client-owned threads that are in the client
     * library (or are in a dr routine that has set dcontext->client_thread_safe_to_sync)
     * meet this requirement (see at_safe_spot()).  As such, all we need to worry about
     * here are client locks the client-owned thread might hold that could block other
     * threads from reaching safe spots.  If we only suspend client-owned threads once
     * all other threads are taken care of then this is not a problem. FIXME - xref
     * PR 231301 on issues that arise if the client thread spends most of its time
     * calling out of its lib to dr API, ntdll, or generated code functions. */
    bool finished_non_client_threads;

    ASSERT(!dynamo_all_threads_synched);
    /* flag any caller who does not give up enough permissions to avoid livelock
     * with other synch_with_all_threads callers
     */
    ASSERT_CURIOSITY(cur_state >= THREAD_SYNCH_NO_LOCKS_NO_XFER);
    /* also flag anyone asking for full mcontext w/o possibility of no_xfer,
     * which can also livelock
     */
    ASSERT_CURIOSITY(desired_synch_state < THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT
                     /* detach currently violates this: bug 8942 */
                     || started_detach);

    /* must set exactly one of these -- FIXME: better way to check? */
    ASSERT(
        TESTANY(THREAD_SYNCH_SUSPEND_FAILURE_ABORT | THREAD_SYNCH_SUSPEND_FAILURE_IGNORE |
                    THREAD_SYNCH_SUSPEND_FAILURE_RETRY,
                flags) &&
        !TESTALL(THREAD_SYNCH_SUSPEND_FAILURE_ABORT | THREAD_SYNCH_SUSPEND_FAILURE_IGNORE,
                 flags) &&
        !TESTALL(THREAD_SYNCH_SUSPEND_FAILURE_ABORT | THREAD_SYNCH_SUSPEND_FAILURE_RETRY,
                 flags) &&
        !TESTALL(THREAD_SYNCH_SUSPEND_FAILURE_IGNORE | THREAD_SYNCH_SUSPEND_FAILURE_RETRY,
                 flags));
    flags_one = flags;
    /* we'll do the retry */
    if (TEST(THREAD_SYNCH_SUSPEND_FAILURE_RETRY, flags)) {
        flags_one &= ~THREAD_SYNCH_SUSPEND_FAILURE_RETRY;
        flags_one |= THREAD_SYNCH_SUSPEND_FAILURE_ABORT;
    }

    if (tr != NULL) {
        dcontext = tr->dcontext;
        expect_self_exiting = dcontext->is_exiting ? 1 : 0;
        ASSERT(exiting_thread_count >= expect_self_exiting);
    } else {
        /* calling thread should always be a known thread */
        ASSERT_NOT_REACHED();
    }

    LOG(THREAD, LOG_SYNCH, 1,
        "synch with all threads my id = " SZFMT
        " Giving %d permission and seeking %d state\n",
        my_id, cur_state, desired_synch_state);

    /* grab all_threads_synch_lock */
    /* since all_threads synch doesn't give any permissions this is necessary
     * to prevent deadlock in the case of two threads trying to synch with all
     * threads at the same time  */
    /* FIXME: for DEADLOCK_AVOIDANCE, to preserve LIFO, should we
     * exit DR, trylock, then immediately enter DR?  introducing any
     * race conditions in doing so?
     * Ditto on all other os_thread_yields in this file!
     */
    while (!d_r_mutex_trylock(&all_threads_synch_lock)) {
        LOG(THREAD, LOG_SYNCH, 2, "Spinning on all threads synch lock\n");
        STATS_INC(synch_yields);
        if (dcontext != NULL && cur_state != THREAD_SYNCH_NONE &&
            should_wait_at_safe_spot(dcontext)) {
            /* ref case 5552, if we've inc'ed the exiting thread count need to
             * adjust it back before calling check_wait_at_safe_spot since we
             * may end up being killed there */
            if (dcontext->is_exiting) {
                ASSERT(exiting_thread_count >= 1);
                ATOMIC_DEC(int, exiting_thread_count);
            }
            check_wait_at_safe_spot(dcontext, cur_state);
            if (dcontext->is_exiting) {
                ATOMIC_INC(int, exiting_thread_count);
            }
        }
        LOG(THREAD, LOG_SYNCH, 2, "Yielding on all threads synch lock\n");
        /* Note - we only need call the ENTER/EXIT_DR hooks if single thread
         * in dr since we are not really exiting DR here (we just need to give
         * up the exclusion lock for a while to let thread we are trying to
         * synch with make progress towards a safe synch point). */
        if (INTERNAL_OPTION(single_thread_in_DR))
            EXITING_DR(); /* give up DR exclusion lock */
        os_thread_yield();
        if (INTERNAL_OPTION(single_thread_in_DR))
            ENTERING_DR(); /* re-gain DR exclusion lock */
    }

    d_r_mutex_lock(&thread_initexit_lock);
    /* synch with all threads */
    /* FIXME: this should be a do/while loop - then we wouldn't have
     * to initialize all the variables above
     */
    while (threads_are_stale || !all_synched ||
           exiting_thread_count > expect_self_exiting || uninit_thread_count > 0) {
        if (threads != NULL) {
            /* Case 8941: must free here rather than when yield (below) since
             * termination condition can change between there and here
             */
            ASSERT(num_threads > 0);
            global_heap_free(threads,
                             num_threads *
                                 sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
            /* be paranoid */
            threads = NULL;
            num_threads = 0;
        }
        get_list_of_threads(&threads, &num_threads);
        threads_are_stale = false;
        synch_array = (uint *)global_heap_alloc(num_threads *
                                                sizeof(uint) HEAPACCT(ACCT_THREAD_MGT));
        for (i = 0; i < num_threads; i++) {
            synch_array[i] = SYNCH_WITH_ALL_NEW;
        }
        /* Fixme : an inefficient algorithm, but is not as bad as it seems
         * since it is very unlikely that many threads have started or ended
         * and the list threads routine always puts them in the same order
         */
        /* on first loop num_threads_temp == 0 */
        for (i = 0; i < num_threads_temp; i++) {
            /* care only if we have already notified or synched thread */
            if (synch_array_temp[i] != SYNCH_WITH_ALL_NEW) {
                for (j = 0; j < num_threads; j++) {
                    /* FIXME : os recycles thread ids, should have stronger
                     * check here, could check dcontext equivalence, (but we
                     * recycle those to), probably should check threads_temp
                     * handle and be sure thread is still alive since the id
                     * won't be recycled then */
                    if (threads[j]->id == thread_ids_temp[i]) {
                        synch_array[j] = synch_array_temp[i];
                        break;
                    }
                }
            }
        }
        /* free old synch list, old thread id list */
        if (num_threads_temp > 0) {
            global_heap_free(thread_ids_temp,
                             num_threads_temp *
                                 sizeof(thread_id_t) HEAPACCT(ACCT_THREAD_MGT));
            global_heap_free(synch_array_temp,
                             num_threads_temp * sizeof(uint) HEAPACCT(ACCT_THREAD_MGT));
            num_threads_temp = 0;
        }

        all_synched = true;
        LOG(THREAD, LOG_SYNCH, 3, "Looping over all threads (%d threads)\n", num_threads);
        finished_non_client_threads = true;
        for (i = 0; i < num_threads; i++) {
            if (threads[i]->id != my_id && synch_array[i] != SYNCH_WITH_ALL_SYNCHED &&
                !IS_CLIENT_THREAD(threads[i]->dcontext)) {
                finished_non_client_threads = false;
                break;
            }
        }
        /* make a copy of the thread ids (can't just keep the thread list
         * since it consists of pointers to live thread_record_t structs).
         * we must make the copy before synching b/c cleaning up a thread
         * involves freeing its thread_record_t.
         */
        thread_ids_temp = (thread_id_t *)global_heap_alloc(
            num_threads * sizeof(thread_id_t) HEAPACCT(ACCT_THREAD_MGT));
        for (i = 0; i < num_threads; i++)
            thread_ids_temp[i] = threads[i]->id;
        num_threads_temp = num_threads;
        synch_array_temp = synch_array;

        for (i = 0; i < num_threads; i++) {
            /* do not de-ref threads[i] after synching if it was cleaned up! */
            if (synch_array[i] != SYNCH_WITH_ALL_SYNCHED && threads[i]->id != my_id) {
                if (!finished_non_client_threads &&
                    IS_CLIENT_THREAD(threads[i]->dcontext)) {
                    all_synched = false;
                    continue; /* skip this thread for now till non-client are finished */
                }
                if (IS_CLIENT_THREAD(threads[i]->dcontext) &&
                    (TEST(flags, THREAD_SYNCH_SKIP_CLIENT_THREAD) ||
                     !should_suspend_client_thread(threads[i]->dcontext,
                                                   desired_synch_state))) {
                    /* PR 609569: do not suspend this thread.
                     * Avoid races between resume_all_threads() and
                     * dr_client_thread_set_suspendable() by storing the fact.
                     *
                     * For most of our synchall purposes we really want to prevent
                     * threads from acting on behalf of the application, and make
                     * sure we can relocate them if in the code cache.  DR itself is
                     * thread-safe, and while a synchall-initiator will touch
                     * thread-private data for threads it suspends, having some
                     * threads it does not suspend shouldn't cause any problems so
                     * long as it doesn't touch their thread-private data.
                     */
                    synch_array[i] = SYNCH_WITH_ALL_SYNCHED;
                    threads[i]->dcontext->client_data->left_unsuspended = true;
                    continue;
                }
                /* speed things up a tad */
                if (synch_array[i] != SYNCH_WITH_ALL_NOTIFIED) {
                    ASSERT(synch_array[i] == SYNCH_WITH_ALL_NEW);
                    adjust_wait_at_safe_spot(threads[i]->dcontext, 1);
                    synch_array[i] = SYNCH_WITH_ALL_NOTIFIED;
                }
                LOG(THREAD, LOG_SYNCH, 2,
                    "About to try synch with thread #%d/%d " TIDFMT "\n", i, num_threads,
                    threads[i]->id);
                synch_res =
                    synch_with_thread(threads[i]->id, false, true, THREAD_SYNCH_NONE,
                                      desired_synch_state, flags_one);
                if (synch_res == THREAD_SYNCH_RESULT_SUCCESS) {
                    LOG(THREAD, LOG_SYNCH, 2, "Synch succeeded!\n");
                    /* successful synch */
                    synch_array[i] = SYNCH_WITH_ALL_SYNCHED;
                    if (!THREAD_SYNCH_IS_CLEANED(desired_synch_state))
                        adjust_wait_at_safe_spot(threads[i]->dcontext, -1);
                } else {
                    LOG(THREAD, LOG_SYNCH, 2, "Synch failed!\n");
                    all_synched = false;
                    if (synch_res == THREAD_SYNCH_RESULT_SUSPEND_FAILURE) {
                        if (TEST(THREAD_SYNCH_SUSPEND_FAILURE_ABORT, flags))
                            goto synch_with_all_abort;
                    } else
                        ASSERT(synch_res == THREAD_SYNCH_RESULT_NOT_SAFE);
                }
            } else {
                LOG(THREAD, LOG_SYNCH, 2, "Skipping synch with thread " TIDFMT "\n",
                    thread_ids_temp[i]);
            }
        }

        if (loop_count++ >= max_loops)
            break;
        /* We test the exiting thread count to avoid races between exit
         * process (current thread, though we could be here for detach or other
         * reasons) and an exiting thread (who might no longer be on the all
         * threads list) who is still using shared resources (ref case 3121) */
        if (!all_synched || exiting_thread_count > expect_self_exiting ||
            uninit_thread_count > 0) {
            DOSTATS({
                if (all_synched && exiting_thread_count > expect_self_exiting) {
                    LOG(THREAD, LOG_SYNCH, 2, "Waiting for an exiting thread %d %d %d\n",
                        all_synched, exiting_thread_count, expect_self_exiting);
                    STATS_INC(synch_yields_for_exiting_thread);
                } else if (all_synched && uninit_thread_count > 0) {
                    LOG(THREAD, LOG_SYNCH, 2, "Waiting for an uninit thread %d %d\n",
                        all_synched, uninit_thread_count);
                    STATS_INC(synch_yields_for_uninit_thread);
                }
            });
            STATS_INC(synch_yields);

            /* release lock in case some other thread waiting on it */
            d_r_mutex_unlock(&thread_initexit_lock);
            LOG(THREAD, LOG_SYNCH, 2, "Not all threads synched looping again\n");
            /* Note - we only need call the ENTER/EXIT_DR hooks if single
             * thread in dr since we are not really exiting DR here (we just
             * need to give up the exclusion lock for a while to let thread we
             * are trying to synch with make progress towards a safe synch
             * point). */
            if (INTERNAL_OPTION(single_thread_in_DR))
                EXITING_DR(); /* give up DR exclusion lock */
            synch_thread_yield();
            if (INTERNAL_OPTION(single_thread_in_DR))
                ENTERING_DR(); /* re-gain DR exclusion lock */
            d_r_mutex_lock(&thread_initexit_lock);
            /* We unlock and lock the thread_initexit_lock, so threads might be stale. */
            threads_are_stale = true;
        }
    }
    /* case 9392: callers passing in ABORT expect a return value of failure
     * to correspond w/ no suspended threads, a freed threads array, and no
     * locks being held, so we go through the abort path
     */
    if (!all_synched && TEST(THREAD_SYNCH_SUSPEND_FAILURE_ABORT, flags))
        goto synch_with_all_abort;
synch_with_all_exit:
    /* make sure we didn't exit the loop without synchronizing, FIXME : in
     * release builds we assume the synchronization is failing and continue
     * without it, but that is dangerous.
     * It is now up to the caller to handle this, and some use
     * small loop counts and abort on failure, so only a curiosity. */
    ASSERT_CURIOSITY(loop_count < max_loops);
    ASSERT(threads != NULL);
    /* Since the set of threads can change we don't set the success field
     * until we're passing back the thread list.
     * We would use an tsd field directly instead of synch_array except
     * for THREAD_SYNCH_*_CLEAN where tsd is freed.
     */
    ASSERT(synch_array != NULL);
    if (!THREAD_SYNCH_IS_CLEANED(desired_synch_state)) { /* else unsafe to access tsd */
        for (i = 0; i < num_threads; i++) {
            if (threads[i]->id != my_id) {
                thread_synch_data_t *tsd;
                ASSERT(threads[i]->dcontext != NULL);
                tsd = (thread_synch_data_t *)threads[i]->dcontext->synch_field;
                tsd->synch_with_success = (synch_array[i] == SYNCH_WITH_ALL_SYNCHED);
            }
        }
    }
    global_heap_free(synch_array, num_threads * sizeof(uint) HEAPACCT(ACCT_THREAD_MGT));
    if (num_threads_temp > 0) {
        global_heap_free(thread_ids_temp,
                         num_threads_temp *
                             sizeof(thread_id_t) HEAPACCT(ACCT_THREAD_MGT));
    }
    /* FIXME case 9333: on all_synch failure we do not free threads array if
     * synch_result is ignored.  Callers are responsible for resuming threads that are
     * suspended and freeing allocation for threads array
     */
    if ((!all_synched && TEST(THREAD_SYNCH_SUSPEND_FAILURE_ABORT, flags)) ||
        THREAD_SYNCH_IS_CLEANED(desired_synch_state)) {
        global_heap_free(
            threads, num_threads * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
        threads = NULL;
        num_threads = 0;
    }
    LOG(THREAD, LOG_SYNCH, 1, "Finished synch with all threads: result=%d\n",
        all_synched);
    DOLOG(1, LOG_SYNCH, {
        if (all_synched) {
            LOG(THREAD, LOG_SYNCH, 1,
                "\treturning holding initexit_lock and all_threads_synch_lock\n");
        }
    });
    *threads_out = threads;
    *num_threads_out = num_threads;
    dynamo_all_threads_synched = all_synched;
    ASSERT(exiting_thread_count - expect_self_exiting == 0);
    /* FIXME case 9392: where on all_synch failure we do not release the locks in the
     * non-abort exit path */
    return all_synched;

synch_with_all_abort:
    /* undo everything! */
    for (i = 0; i < num_threads; i++) {
        DEBUG_DECLARE(bool ok;)
        if (threads[i]->id != my_id) {
            if (synch_array[i] == SYNCH_WITH_ALL_SYNCHED) {
                bool resume = true;
                if (IS_CLIENT_THREAD(threads[i]->dcontext) &&
                    threads[i]->dcontext->client_data->left_unsuspended) {
                    /* PR 609569: we did not suspend this thread */
                    resume = false;
                }
                if (resume) {
                    DEBUG_DECLARE(ok =)
                    os_thread_resume(threads[i]);
                    ASSERT(ok);
                }
                /* ensure synch_with_success is set to false on exit path,
                 * even though locks are released and not fully valid
                 */
                synch_array[i] = SYNCH_WITH_ALL_NEW;
            } else if (synch_array[i] == SYNCH_WITH_ALL_NOTIFIED) {
                adjust_wait_at_safe_spot(threads[i]->dcontext, -1);
            }
        }
    }
    d_r_mutex_unlock(&thread_initexit_lock);
    d_r_mutex_unlock(&all_threads_synch_lock);
    ASSERT(exiting_thread_count - expect_self_exiting == 0);
    ASSERT(!all_synched); /* ensure our OUT values will be NULL,0
                             for THREAD_SYNCH_SUSPEND_FAILURE_ABORT */
    goto synch_with_all_exit;
}

/* Assumes that the threads were suspended with synch_with_all_threads()
 * and thus even is_thread_currently_native() threads were suspended.
 * Assumes that the caller will free up threads if it is dynamically allocated.
 */
void
resume_all_threads(thread_record_t **threads, const uint num_threads)
{
    uint i;
    thread_id_t my_tid;

    ASSERT_OWN_MUTEX(true, &all_threads_synch_lock);
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);

    if (threads == NULL || num_threads == 0)
        return;

    my_tid = d_r_get_thread_id();
    for (i = 0; i < num_threads; i++) {
        if (my_tid == threads[i]->id)
            continue;
        if (IS_CLIENT_THREAD(threads[i]->dcontext) &&
            threads[i]->dcontext->client_data->left_unsuspended) {
            /* PR 609569: we did not suspend this thread */
            threads[i]->dcontext->client_data->left_unsuspended = false;
            continue;
        }

        /* This routine assumes that each thread in the array was suspended, so
         * each one has to successfully resume.
         */
        DEBUG_DECLARE(bool res =) os_thread_resume(threads[i]);
        ASSERT(res);
    }
}

/* Should be called to clean up after synch_with_all_threads as otherwise
 * dynamo_all_threads_synched will be left as true.
 * If resume is true, resumes the threads in the threads array.
 * Unlocks thread_initexit_lock and all_threads_synch_lock.
 * If threads != NULL, frees the threads array.
 */
void
end_synch_with_all_threads(thread_record_t **threads, uint num_threads, bool resume)
{
    /* dynamo_all_threads_synched will be false if synch failed */
    ASSERT_CURIOSITY(dynamo_all_threads_synched);
    ASSERT(OWN_MUTEX(&all_threads_synch_lock) && OWN_MUTEX(&thread_initexit_lock));
    dynamo_all_threads_synched = false;
    if (resume) {
        ASSERT(threads != NULL);
        resume_all_threads(threads, num_threads);
    }
    /* if we knew whether THREAD_SYNCH_*_CLEANED was specified we could set
     * synch_with_success to false, but it's unsafe otherwise
     */
    d_r_mutex_unlock(&thread_initexit_lock);
    d_r_mutex_unlock(&all_threads_synch_lock);
    if (threads != NULL) {
        global_heap_free(
            threads, num_threads * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
    }
}

/* Resets a thread's context to start interpreting anew.
 * ASSUMPTION: the thread is currently suspended.
 * This was moved here from fcache_reset_all_caches_proactively simply to
 * get access to win32-private CONTEXT-related routines
 */
void
translate_from_synchall_to_dispatch(thread_record_t *tr, thread_synch_state_t synch_state)
{
    /* we do not have to align priv_mcontext_t */
    priv_mcontext_t *mc = global_heap_alloc(sizeof(*mc) HEAPACCT(ACCT_OTHER));
    bool free_cxt = true;
    dcontext_t *dcontext = tr->dcontext;
    app_pc pre_translation;
    ASSERT(OWN_MUTEX(&all_threads_synch_lock) && OWN_MUTEX(&thread_initexit_lock));
    /* FIXME: would like to assert that suspendcount is > 0 but how? */
    ASSERT(thread_synch_successful(tr));

    DEBUG_DECLARE(bool res =) thread_get_mcontext(tr, mc);
    ASSERT(res);
    pre_translation = (app_pc)mc->pc;
    LOG(GLOBAL, LOG_CACHE, 2, "\trecreating address for " PFX "\n", mc->pc);
    LOG(THREAD, LOG_CACHE, 2,
        "translate_from_synchall_to_dispatch: being translated from " PFX "\n", mc->pc);
    if (get_at_syscall(dcontext)) {
        /* Don't need to do anything as shared_syscall and do_syscall will not
         * change due to a reset and will have any inlined ibl updated.  If we
         * did try to send these guys back to d_r_dispatch, have to set asynch_tag
         * (as well as next_tag since translation looks only at that), restore
         * TOS to asynch_target/esi (unless still at reset state), and have to
         * figure out how to avoid post-syscall processing for those who never
         * did pre-syscall processing (i.e., if at shared_syscall) (else will
         * get wrong dcontext->sysnum, etc.)
         * Not to mention that after resuming the kernel will finish the
         * syscall and clobber several registers, making it hard to set a
         * clean state (xref case 6113, case 5074, and notes below)!
         * It's just too hard to redirect while at a syscall.
         */
        LOG(GLOBAL, LOG_CACHE, 2, "\tat syscall so not translating\n");
        /* sanity check */
        ASSERT(is_after_syscall_address(dcontext, pre_translation) ||
               IF_WINDOWS_ELSE(pre_translation == vsyscall_after_syscall,
                               is_after_or_restarted_do_syscall(dcontext, pre_translation,
                                                                true /*vsys*/)));
#if defined(UNIX) && defined(X86_32)
        if (pre_translation == vsyscall_sysenter_return_pc ||
            pre_translation + SYSENTER_LENGTH == vsyscall_sysenter_return_pc) {
            /* Because we remove the vsyscall hook on a send_all_other_threads_native()
             * yet have no barrier to know the threads have run their own go-native
             * code, we want to send them away from the hook, to our gencode.
             */
            if (pre_translation == vsyscall_sysenter_return_pc)
                mc->pc = after_do_shared_syscall_addr(dcontext);
            else if (pre_translation + SYSENTER_LENGTH == vsyscall_sysenter_return_pc)
                mc->pc = get_do_int_syscall_entry(dcontext);
            /* exit stub and subsequent fcache_return will save rest of state */
            DEBUG_DECLARE(res =)
            set_synched_thread_context(dcontext->thread_record, mc, NULL, 0,
                                       synch_state _IF_X64((void *)mc) _IF_WINDOWS(NULL));
            ASSERT(res);
            /* cxt is freed by set_synched_thread_context() or target thread */
            free_cxt = false;
        }
#endif
        IF_AARCHXX({
            if (INTERNAL_OPTION(steal_reg_at_reset) != 0) {
                /* We don't want to translate, just update the stolen reg values */
                arch_mcontext_reset_stolen_reg(dcontext, mc);
                DEBUG_DECLARE(res =)
                set_synched_thread_context(dcontext->thread_record, mc, NULL, 0,
                                           synch_state _IF_X64((void *)mc)
                                               _IF_WINDOWS(NULL));
                ASSERT(res);
                /* cxt is freed by set_synched_thread_context() or target thread */
                free_cxt = false;
            }
        });
    } else {
        DEBUG_DECLARE(res =) translate_mcontext(tr, mc, true /*restore memory*/, NULL);
        ASSERT(res);
        if (!thread_synch_successful(tr) || mc->pc == 0) {
            /* Better to risk failure on accessing a freed cache than
             * to have a guaranteed crash by sending to NULL.
             * FIXME: it's possible the real translation is NULL,
             * but if so should be fine to leave it there since the
             * current eip should also be NULL.
             */
            ASSERT_NOT_REACHED();
            goto translate_from_synchall_to_dispatch_exit;
        }
        LOG(GLOBAL, LOG_CACHE, 2, "\ttranslation pc = " PFX "\n", mc->pc);
        ASSERT(!is_dynamo_address((app_pc)mc->pc) && !in_fcache((app_pc)mc->pc));
        IF_AARCHXX({
            if (INTERNAL_OPTION(steal_reg_at_reset) != 0) {
                /* XXX: do we need this?  Will signal.c will fix it up prior
                 * to sigreturn from suspend handler?
                 */
                arch_mcontext_reset_stolen_reg(dcontext, mc);
            }
        });
        /* We send all threads, regardless of whether was in DR or not, to
         * re-interp from translated cxt, to avoid having to handle stale
         * local state problems if we simply resumed.
         * We assume no KSTATS or other state issues to deal with.
         * FIXME: enter hook w/o an exit?
         */
        dcontext->next_tag = (app_pc)mc->pc;
        /* FIXME PR 212266: for linux if we're at an inlined syscall
         * we may have problems: however, we might be able to rely on the kernel
         * not clobbering any registers besides eax (which is ok: reset stub
         * handles it), though presumably it's allowed to write to any
         * caller-saved registers.  We may need to change inlined syscalls
         * to set at_syscall (see comments below as well).
         */
        if (pre_translation ==
                IF_WINDOWS_ELSE(vsyscall_after_syscall, vsyscall_sysenter_return_pc) &&
            !waiting_at_safe_spot(dcontext->thread_record, synch_state)) {
            /* FIXME case 7827/PR 212266: shouldn't translate for this case, right?
             * should have -ignore_syscalls set at_syscall and eliminate
             * this whole block of code
             */
            /* put the proper retaddr back on the stack, as we won't
             * be doing the ret natively to regain control, but rather
             * will interpret it
             */
            /* FIXME: ensure readable and writable? */
            app_pc cur_retaddr = *((app_pc *)mc->xsp);
            app_pc native_retaddr;
            ASSERT(cur_retaddr != NULL);
            /* must be ignore_syscalls (else, at_syscall will be set) */
            IF_WINDOWS(ASSERT(DYNAMO_OPTION(ignore_syscalls)));
            ASSERT(get_syscall_method() == SYSCALL_METHOD_SYSENTER);
            /* For DYNAMO_OPTION(sygate_sysenter) we need to restore both stack
             * values and fix up esp, but we can't do it here since the kernel
             * will change esp... incompatible w/ -ignore_syscalls anyway
             */
            IF_WINDOWS(ASSERT_NOT_IMPLEMENTED(!DYNAMO_OPTION(sygate_sysenter)));
            /* may still be at syscall from a prior reset -- don't want to grab
             * locks for in_fcache so we determine via the translation
             */
            ASSERT_NOT_TESTED();
            native_retaddr = recreate_app_pc(dcontext, cur_retaddr, NULL);
            if (native_retaddr != cur_retaddr) {
                LOG(GLOBAL, LOG_CACHE, 2, "\trestoring TOS to " PFX " from " PFX "\n",
                    native_retaddr, cur_retaddr);
                *((app_pc *)mc->xsp) = native_retaddr;
            } else {
                LOG(GLOBAL, LOG_CACHE, 2,
                    "\tnot restoring TOS since still at previous reset state " PFX "\n",
                    cur_retaddr);
            }
        }
        /* Send back to d_r_dispatch.  Rather than setting up last_exit in eax here,
         * we point to a special routine to save the correct eax -- in fact it's
         * simply a direct exit stub.  Originally this was b/c we tried to
         * translate threads at system calls, and the kernel clobbers eax (and
         * ecx/edx for sysenter, though preserves eip setcontext change: case
         * 6113, case 5074) in finishing the system call, but now that we don't
         * translate them we've kept the stub approach.  It's actually faster
         * for the stub itself to save eax and set the linkstub than for us to
         * emulate it here, anyway.
         * Note that a thread in check_wait_at_safe_spot() spins and will NOT be
         * at a syscall, avoiding problems there (case 5074).
         */
        mc->pc = (app_pc)get_reset_exit_stub(dcontext);
        /* We need to set ARM mode to match the reset exit stub. */
        IF_ARM(dr_isa_mode_t prior_mode);
        IF_ARM(dr_set_isa_mode(dcontext, DR_ISA_ARM_A32, &prior_mode));
        LOG(GLOBAL, LOG_CACHE, 2, "\tsent to reset exit stub " PFX "\n", mc->pc);
        /* The reset exit stub expects the stolen reg to contain the TLS base address.
         * But the stolen reg was restored to the application value during
         * translate_mcontext.
         */
        IF_AARCHXX({
            /* Preserve the translated value from mc before we clobber it. */
            dcontext->local_state->spill_space.reg_stolen = get_stolen_reg_val(mc);
            set_stolen_reg_val(mc, (reg_t)os_get_dr_tls_base(dcontext));
        });
#ifdef WINDOWS
        /* i#25: we could have interrupted thread in DR, where has priv fls data
         * in TEB, and fcache_return blindly copies into app fls: so swap to app
         * now, just in case.  DR routine can handle swapping when already app.
         */
        swap_peb_pointer(dcontext, false /*to app*/);
#endif
        /* exit stub and subsequent fcache_return will save rest of state */
        DEBUG_DECLARE(res =)
        set_synched_thread_context(dcontext->thread_record, mc, NULL, 0,
                                   synch_state _IF_X64((void *)mc) _IF_WINDOWS(NULL));
        ASSERT(res);
        /* cxt is freed by set_synched_thread_context() or target thread */
        free_cxt = false;
        /* Now that set_synched_thread_context() recorded the mode for the reset
         * exit stub, restore for the post-exit-stub execution.
         */
        IF_ARM(dr_set_isa_mode(dcontext, prior_mode, NULL));
    }
translate_from_synchall_to_dispatch_exit:
    if (free_cxt) {
        global_heap_free(mc, sizeof(*mc) HEAPACCT(ACCT_OTHER));
    }
}

/***************************************************************************
 * Detach and similar operations
 */

/* Atomic variable to prevent multiple threads from trying to detach at
 * the same time.
 */
DECLARE_CXTSWPROT_VAR(static volatile int dynamo_detaching_flag, LOCK_FREE_STATE);

void
send_all_other_threads_native(void)
{
    thread_record_t **threads;
    dcontext_t *my_dcontext = get_thread_private_dcontext();
    int i, num_threads;
    bool waslinking;
    /* We're forced to use an asynch model due to not being able to call
     * dynamo_thread_not_under_dynamo, which has a bonus of making it easier
     * to handle other threads asking for synchall.
     * This is why we don't ask for THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT.
     */
    const thread_synch_state_t desired_state =
        THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER;

    ASSERT(dynamo_initialized && !dynamo_exited && my_dcontext != NULL);
    LOG(my_dcontext->logfile, LOG_ALL, 1, "%s\n", __FUNCTION__);
    LOG(GLOBAL, LOG_ALL, 1, "%s: cur thread " TIDFMT "\n", __FUNCTION__,
        d_r_get_thread_id());

    waslinking = is_couldbelinking(my_dcontext);
    if (waslinking)
        enter_nolinking(my_dcontext, NULL, false);

#ifdef WINDOWS
    /* Ensure new threads will go straight to native */
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    init_apc_go_native_pause = true;
    init_apc_go_native = true;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

    wait_for_outstanding_nudges();
#endif

    instrument_pre_detach_event();

    /* Suspend all threads except those trying to synch with us */
    if (!synch_with_all_threads(desired_state, &threads, &num_threads,
                                THREAD_SYNCH_NO_LOCKS_NO_XFER,
                                THREAD_SYNCH_SUSPEND_FAILURE_IGNORE)) {
        REPORT_FATAL_ERROR_AND_EXIT(FAILED_TO_SYNCHRONIZE_THREADS, 2,
                                    get_application_name(), get_application_pid());
    }

    ASSERT(mutex_testlock(&all_threads_synch_lock) &&
           mutex_testlock(&thread_initexit_lock));

#ifdef WINDOWS
    /* Let threads waiting at APC point go native */
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    init_apc_go_native_pause = false;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
#endif

#ifdef WINDOWS
    /* FIXME i#95: handle outstanding callbacks where we've put our retaddr on
     * the app stack.  This should be able to share
     * detach_helper_handle_callbacks() code.  Won't the old single-thread
     * dr_app_stop() have had this same problem?  Since we're not tearing
     * everything down, can we solve it by waiting until we hit
     * after_shared_syscall_code_ex() in a native thread?
     */
    ASSERT_NOT_IMPLEMENTED(get_syscall_method() != SYSCALL_METHOD_SYSENTER);
#endif

    for (i = 0; i < num_threads; i++) {
        if (threads[i]->dcontext == my_dcontext ||
            is_thread_currently_native(threads[i]) ||
            /* FIXME i#2784: we should suspend client threads for the duration
             * of the app being native to avoid problems with having no
             * signal handlers in place.
             */
            IS_CLIENT_THREAD(threads[i]->dcontext))
            continue;

        /* Because dynamo_thread_not_under_dynamo() has to be run by the owning
         * thread, the simplest solution is to send everyone back to d_r_dispatch
         * with a flag to go native from there, rather than directly setting the
         * native context.
         */
        threads[i]->dcontext->go_native = true;

        if (thread_synch_state_no_xfer(threads[i]->dcontext)) {
            /* Another thread trying to synch with us: just let it go.  It will
             * go native once it gets back to d_r_dispatch which will be before it
             * goes into the cache.
             */
            continue;
        } else {
            LOG(my_dcontext->logfile, LOG_ALL, 1, "%s: sending thread %d native\n",
                __FUNCTION__, threads[i]->id);
            LOG(threads[i]->dcontext->logfile, LOG_ALL, 1,
                "**** requested by thread %d to go native\n", my_dcontext->owning_thread);
            /* This won't change a thread at a syscall, so we rely on the thread
             * going to d_r_dispatch and then going native when its syscall exits.
             *
             * FIXME i#95: That means the time to go native is, unfortunately,
             * unbounded.  This means that dr_app_cleanup() needs to synch the
             * threads and force-xl8 these.  We should share code with detach.
             * Right now we rely on the app joining all its threads *before*
             * calling dr_app_cleanup(), or using dr_app_stop_and_cleanup[_with_stats]().
             * This also means we have a race with unhook_vsyscall in
             * os_process_not_under_dynamorio(), which we solve by redirecting
             * threads at syscalls to our gencode.
             */
            translate_from_synchall_to_dispatch(threads[i], desired_state);
        }
    }

    end_synch_with_all_threads(threads, num_threads, true /*resume*/);

    os_process_not_under_dynamorio(my_dcontext);

    if (waslinking)
        enter_couldbelinking(my_dcontext, NULL, false);
    return;
}

void
detach_on_permanent_stack(bool internal, bool do_cleanup, dr_stats_t *drstats)
{
    dcontext_t *my_dcontext;
    thread_record_t **threads;
    thread_record_t *my_tr = NULL;
    int i, num_threads, my_idx = -1;
    thread_id_t my_id;
#ifdef WINDOWS
    bool detach_stacked_callbacks;
    bool *cleanup_tpc;
#endif
    DEBUG_DECLARE(bool ok;)
    DEBUG_DECLARE(int exit_res;)

    /* synch-all flags: */
    uint flags = 0;
#ifdef WINDOWS
    /* For Windows we may fail to suspend a thread (e.g., privilege
     * problems), and in that case we want to just ignore the failure.
     */
    flags |= THREAD_SYNCH_SUSPEND_FAILURE_IGNORE;
#elif defined(UNIX)
    /* For Unix, such privilege problems are rarer but we would still prefer to
     * continue if we hit a problem.
     */
    flags |= THREAD_SYNCH_SUSPEND_FAILURE_IGNORE;
#endif

    /* i#297: we only synch client threads after process exit event. */
    flags |= THREAD_SYNCH_SKIP_CLIENT_THREAD;

    ENTERING_DR();

    /* dynamo_detaching_flag is not really a lock, and since no one ever waits
     * on it we can't deadlock on it either.
     */
    if (!atomic_compare_exchange(&dynamo_detaching_flag, LOCK_FREE_STATE, LOCK_SET_STATE))
        return;

    instrument_pre_detach_event();

    /* Unprotect .data for exit cleanup.
     * XXX: more secure to not do this until we've synched, but then need
     * alternative prot for started_detach and init_apc_go_native*
     */
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);

    ASSERT(!started_detach);
    started_detach = true;

    if (!internal) {
        synchronize_dynamic_options();
        if (!DYNAMO_OPTION(allow_detach)) {
            started_detach = false;
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
            dynamo_detaching_flag = LOCK_FREE_STATE;
            SYSLOG_INTERNAL_ERROR("Detach called without the allow_detach option set");
            EXITING_DR();
            return;
        }
    }

    ASSERT(dynamo_initialized);
    ASSERT(!dynamo_exited);

    my_id = d_r_get_thread_id();
    my_dcontext = get_thread_private_dcontext();
    if (my_dcontext == NULL) {
        /* We support detach after just dr_app_setup() with no start. */
        ASSERT(!dynamo_started);
        my_tr = thread_lookup(my_id);
        ASSERT(my_tr != NULL);
        my_dcontext = my_tr->dcontext;
        os_process_under_dynamorio_initiate(my_dcontext);
        os_process_under_dynamorio_complete(my_dcontext);
        dynamo_thread_under_dynamo(my_dcontext);
        ASSERT(get_thread_private_dcontext() == my_dcontext);
    }
    ASSERT(my_dcontext != NULL);

    LOG(GLOBAL, LOG_ALL, 1, "Detach: thread %d starting detach process\n", my_id);
    SYSLOG(SYSLOG_INFORMATION, INFO_DETACHING, 2, get_application_name(),
           get_application_pid());

    /* synch with flush */
    if (my_dcontext != NULL)
        enter_threadexit(my_dcontext);

#ifdef WINDOWS
    /* Signal to go native at APC init here.  Set pause first so that threads
     * will wait till we are ready for them to go native (after ntdll unpatching).
     * (To avoid races these must be set in this order!)
     */
    init_apc_go_native_pause = true;
    init_apc_go_native = true;
    /* XXX i#2611: there is still a race for threads caught between init_apc_go_native
     * and dynamo_thread_init adding to all_threads: this just reduces the risk.
     * Unfortunately we can't easily use the UNIX solution of uninit_thread_count
     * since we can't distinguish internally vs externally created threads.
     */
    os_thread_yield();
    wait_for_outstanding_nudges();
#endif

#ifdef UNIX
    /* i#2270: we ignore alarm signals during detach to reduce races. */
    signal_remove_alarm_handlers(my_dcontext);
#endif

    /* suspend all DR-controlled threads at safe locations */
    if (!synch_with_all_threads(THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT, &threads,
                                &num_threads,
                                /* Case 6821: allow other synch-all-thread uses
                                 * that beat us to not wait on us. We still have
                                 * a problem if we go first since we must xfer
                                 * other threads.
                                 */
                                THREAD_SYNCH_NO_LOCKS_NO_XFER, flags)) {
        REPORT_FATAL_ERROR_AND_EXIT(FAILED_TO_SYNCHRONIZE_THREADS, 2,
                                    get_application_name(), get_application_pid());
    }

    /* Now we own the thread_initexit_lock.  We'll release the locks grabbed in
     * synch_with_all_threads below after cleaning up all the threads in case we
     * need to grab it during process exit cleanup.
     */
    ASSERT(mutex_testlock(&all_threads_synch_lock) &&
           mutex_testlock(&thread_initexit_lock));

    ASSERT(!doing_detach);
    doing_detach = true;
    detacher_tid = d_r_get_thread_id();

#ifdef HOT_PATCHING_INTERFACE
    /* In hotp_only mode, we must remove patches when detaching; we don't want
     * to leave in all our hooks and detach; that will definitely crash the app.
     */
    if (DYNAMO_OPTION(hotp_only))
        hotp_only_detach_helper();
#endif

#ifdef WINDOWS
    /* XXX: maybe we should re-check for additional threads that passed the init_apc
     * lock but weren't yet initialized and so didn't show up on the list?
     */

    LOG(GLOBAL, LOG_ALL, 1,
        "Detach : about to unpatch ntdll.dll and fix memory permissions\n");
    detach_remove_image_entry_hook(num_threads, threads);
    if (!INTERNAL_OPTION(noasynch)) {
        /* We have to do this here, before client exit events, as we're letting
         * threads go native next.  We thus will not detect crashes during client
         * exit during detach.
         */
        callback_interception_unintercept();
    }
#endif

    if (!DYNAMO_OPTION(thin_client))
        revert_memory_regions();
#ifdef UNIX
    unhook_vsyscall();
#endif
    LOG(GLOBAL, LOG_ALL, 1,
        "Detach : unpatched ntdll.dll and fixed memory permissions\n");
#ifdef WINDOWS
    /* Release the APC init lock and let any threads waiting there go native */
    LOG(GLOBAL, LOG_ALL, 1, "Detach : Releasing init_apc_go_native_pause\n");
    init_apc_go_native_pause = false;
#endif

    /* perform exit tasks that require full thread data structs */
    dynamo_process_exit_with_thread_info();

#ifdef WINDOWS
    /* We need to record a bool indicating whether we can free each thread's
     * resources fully or whether we need them for callback cleanup.
     */
    cleanup_tpc =
        (bool *)global_heap_alloc(num_threads * sizeof(bool) HEAPACCT(ACCT_OTHER));
    /* Handle any outstanding callbacks */
    detach_stacked_callbacks = detach_handle_callbacks(num_threads, threads, cleanup_tpc);
#endif

    LOG(GLOBAL, LOG_ALL, 1, "Detach: starting to translate contexts\n");
    for (i = 0; i < num_threads; i++) {
        priv_mcontext_t mc;
        if (threads[i]->dcontext == my_dcontext) {
            my_idx = i;
            my_tr = threads[i];
            continue;
        } else if (IS_CLIENT_THREAD(threads[i]->dcontext)) {
            /* i#297 we will kill client-owned threads later after app exit events
             * in dynamo_shared_exit().
             */
            continue;
        } else if (detach_do_not_translate(threads[i])) {
            LOG(GLOBAL, LOG_ALL, 2, "Detach: not translating " TIDFMT "\n",
                threads[i]->id);
        } else {
            LOG(GLOBAL, LOG_ALL, 2, "Detach: translating " TIDFMT "\n", threads[i]->id);
            DEBUG_DECLARE(ok =)
            thread_get_mcontext(threads[i], &mc);
            ASSERT(ok);
            /* For a thread at a syscall, we use SA_RESTART for our suspend signal,
             * so the kernel will adjust the restart point back to the syscall for us
             * where expected.  This is an artifical signal we're introducing, so an
             * app that assumes no signals and assumes its non-auto-restart syscalls
             * don't need loops could be broken.
             */
            LOG(GLOBAL, LOG_ALL, 3,
                /* Having the code bytes can help diagnose post-detach where the code
                 * cache is gone.
                 */
                "Detach: pre-xl8 pc=%p (%02x %02x %02x %02x %02x), xsp=%p "
                "for thread " TIDFMT "\n",
                mc.pc, *mc.pc, *(mc.pc + 1), *(mc.pc + 2), *(mc.pc + 3), *(mc.pc + 4),
                mc.xsp, threads[i]->id);
            DEBUG_DECLARE(ok =)
            translate_mcontext(threads[i], &mc, true /*restore mem*/, NULL /*f*/);
            ASSERT(ok);

            if (!threads[i]->under_dynamo_control) {
                LOG(GLOBAL, LOG_ALL, 1,
                    "Detach : thread " TIDFMT " already running natively\n",
                    threads[i]->id);
                /* we do need to restore the app ret addr, for native_exec */
                if (!DYNAMO_OPTION(thin_client) && DYNAMO_OPTION(native_exec) &&
                    !vmvector_empty(native_exec_areas)) {
                    put_back_native_retaddrs(threads[i]->dcontext);
                }
            }
            detach_finalize_translation(threads[i], &mc);

            LOG(GLOBAL, LOG_ALL, 1, "Detach: pc=" PFX " for thread " TIDFMT "\n", mc.pc,
                threads[i]->id);
            ASSERT(!is_dynamo_address(mc.pc) && !in_fcache(mc.pc));
            /* XXX case 7457: if the thread is suspended after it received a fault
             * but before the kernel copied the faulting context to the user mode
             * structures for the handler, it could result in a codemod exception
             * that wouldn't happen natively!
             */
            DEBUG_DECLARE(ok =)
            thread_set_mcontext(threads[i], &mc);
            ASSERT(ok);

            /* i#249: restore app's PEB/TEB fields */
            IF_WINDOWS(restore_peb_pointer_for_thread(threads[i]->dcontext));
        }
        /* Resumes the thread, which will do kernel-visible cleanup of
         * signal state. Resume happens within the synch_all region where
         * the thread_initexit_lock is held so that we can clean up thread
         * data later.
         */
#ifdef UNIX
        os_signal_thread_detach(threads[i]->dcontext);
#endif
        LOG(GLOBAL, LOG_ALL, 1, "Detach: thread " TIDFMT " is being resumed as native\n",
            threads[i]->id);
        os_thread_resume(threads[i]);
    }

    ASSERT(my_idx != -1 || !internal);
#ifdef UNIX
    LOG(GLOBAL, LOG_ALL, 1, "Detach: waiting for threads to fully detach\n");
    for (i = 0; i < num_threads; i++) {
        if (i != my_idx && !IS_CLIENT_THREAD(threads[i]->dcontext))
            os_wait_thread_detached(threads[i]->dcontext);
    }
#endif

    if (!do_cleanup)
        return;

    /* Clean up each thread now that everyone has gone native. Needs to be
     * done with the thread_initexit_lock held, which is true within a synched
     * region.
     */
    for (i = 0; i < num_threads; i++) {
        if (i != my_idx && !IS_CLIENT_THREAD(threads[i]->dcontext)) {
            LOG(GLOBAL, LOG_ALL, 1, "Detach: cleaning up thread " TIDFMT " %s\n",
                threads[i]->id, IF_WINDOWS_ELSE(cleanup_tpc[i] ? "and its TPC" : "", ""));
            dynamo_other_thread_exit(threads[i] _IF_WINDOWS(!cleanup_tpc[i]));
        }
    }

    if (my_idx != -1) {
        /* pre-client thread cleanup (PR 536058) */
        dynamo_thread_exit_pre_client(my_dcontext, my_tr->id);
    }

    LOG(GLOBAL, LOG_ALL, 1, "Detach: Letting secondary threads go native\n");
#ifdef WINDOWS
    global_heap_free(cleanup_tpc, num_threads * sizeof(bool) HEAPACCT(ACCT_OTHER));
    /* XXX: there's a possible race if a thread waiting at APC is still there
     * when we unload our dll.
     */
    os_thread_yield();
#endif
    end_synch_with_all_threads(threads, num_threads, false /*don't resume */);
    threads = NULL;

    LOG(GLOBAL, LOG_ALL, 1, "Detach: Entering final cleanup and unload\n");
    SYSLOG_INTERNAL_INFO("Detaching from process, entering final cleanup");
    if (drstats != NULL)
        stats_get_snapshot(drstats);
    DEBUG_DECLARE(exit_res =)
    dynamo_shared_exit(my_tr _IF_WINDOWS(detach_stacked_callbacks));
    ASSERT(exit_res == SUCCESS);
    detach_finalize_cleanup();

    stack_free(d_r_initstack, DYNAMORIO_STACK_SIZE);

    dynamo_exit_post_detach();

    doing_detach = false;
    started_detach = false;

    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    dynamo_detaching_flag = LOCK_FREE_STATE;
    EXITING_DR();
    options_detach();
}
