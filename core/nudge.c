/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

#include "configure.h"
#include "globals.h"
#include "nudge.h"

#ifdef WINDOWS
#    include "ntdll.h"      /* for our_create_thread(), nt_free_virtual_memory() */
#    include "os_exports.h" /* for detach_helper(), get_stack_bounds() */
#    include "drmarker.h"
#else
#endif /* WINDOWS */

#ifdef HOT_PATCHING_INTERFACE
#    include "hotpatch.h" /* for hotp_nudge_update() */
#endif
#ifdef PROCESS_CONTROL
#    include "moduledb.h" /* for process_control() */
#endif

#include "fragment.h"   /* needed for perscache.h */
#include "perscache.h"  /* for coarse_units_freeze_all() */
#include "instrument.h" /* for instrement_nudge() */
#include "fcache.h"     /* for reset routines */

#ifdef WINDOWS
static void
handle_nudge(dcontext_t *dcontext, nudge_arg_t *arg); /* forward decl */

static void
nudge_terminate_on_dstack(dcontext_t *dcontext)
{
    ASSERT(dcontext == get_thread_private_dcontext());
    if (dcontext->nudge_terminate_process) {
        os_terminate_with_code(dcontext, TERMINATE_PROCESS | TERMINATE_CLEANUP,
                               dcontext->nudge_exit_code);
    } else
        os_terminate(dcontext, TERMINATE_THREAD | TERMINATE_CLEANUP);
    ASSERT_NOT_REACHED();
}

/* This is the target for all nudge threads.
 * CAUTION: generic_nudge_target is added to global_rct_ind_targets table.  If this
 * function is renamed or cloned, update rct_known_targets_init accordingly.
 */
void
generic_nudge_target(nudge_arg_t *arg)
{
    /* Fix for case 5130; volatile forces a 'call' instruction to be generated
     * rather than 'jmp' during optimization.  FIXME: need a standardized &
     * better way of stopping core from emulating itself.
     */
    volatile bool nudge_result;

    /* needed to make sure dr has a specific target to lookup and avoid
     * interpreting when taking over new threads; see leave_call_native().
     */
    nudge_result = generic_nudge_handler(arg);

    /* Should never return. */
    ASSERT_NOT_REACHED();
    os_terminate(NULL, TERMINATE_THREAD); /* just in case */
}

/* exit_process is only honored if dcontext != NULL, and exit_code is only honored
 * if exit_process is true
 */
bool
nudge_thread_cleanup(dcontext_t *dcontext, bool exit_process, uint exit_code)
{
    /* Note - for supporting detach with  clients and nudge threads we need that
     * no lock grabbing or other actions that would interfere with the detaching process
     * occur in the cleanup path here. */

    /* Case 8901: this routine is currently called from the code cache, which may have
     * been reset underneath us, so we can't just blindly return.  This also gives us
     * consistent behavior for handling stack freeing. */

    /* Case 9020: no EXITING_DR() as os_terminate will do that for us */

    /* FIXME - these nudge threads do hit dll mains for thread attach so app may have
     * allocated some TLS memory which won't end up being freed since this won't go
     * through dll main thread detach. The app may also object to unbalanced attach to
     * detach ratio though we haven't seen that in practice. Long term we should take
     * over and redirect the thread at the init apc so it doesn't go through the
     * DllMains to start with. */

    /* We have a general problem on how to free the application stack for nudges.
     * Currently the app/os will never free a nudge thread's app stack:
     *  On NT and 2k ExitThread would normally free the app stack, but we always
     *  terminate nudge threads instead of allowing them to return and exit normally.
     *  On XP and 2k3 none of our nudge creation routines inform csrss of the new thread
     *  (which is who typically frees the stacks).
     * On Vista and Win7 we don't use NtCreateThreadEx to create the nudge threads so
     *  the kernel doesn't free the stack.
     * As such we are left with two options: free the app stack here (nudgee free) or
     *  have the nudge thread creator free the app stack (nudger free).  Going with
     *  nudgee free means we leak exit race nudge stacks whereas if we go with nudger free
     *  for external nudges then we'll leak timed out nudge stacks (for internal nudges
     *  we pretty much have to do nudgee free).  A nudge_arg_t flag is used to specify
     *  which model we use, but currently we always nudgee free.
     * On Win8+ we do use NtCreateThreadEx to create the nudge threads so the kernel
     *  does free the stack.  We could use this on Vista and Win7 too -- should we?
     *  It requires someone to free the argument buffer (NUDGE_FREE_ARG).
     *
     * dynamo_thread_exit_common() is where the app stack is actually freed, not here.
     */

    if (dynamo_exited || !dynamo_initialized || dcontext == NULL) {
        /* FIXME - no cleanup so we'll leak any memory allocated for this thread
         * including the application's stack and arg if we were supposed to free them.
         * We only expect to get here in rare races where the nudge thread was created
         * before dr exited (i.e. before drmarker was freed) but didn't end up getting
         * scheduled till after dr exited. */
        ASSERT(!exit_process); /* shouldn't happen */
#    ifdef WINDOWS
        if (dcontext != NULL)
            swap_peb_pointer(dcontext, false /*to app*/);
#    endif

        os_terminate(dcontext, TERMINATE_THREAD);
    } else {
        /* Nudge threads should exit without holding any locks. */
        ASSERT_OWN_NO_LOCKS();

#    ifdef WINDOWS
        /* We want to remain private during exit (esp client exit and loader_thread_exit
         * calling privlib entries).  Thus we do *not* call swap_peb_pointer().
         * For exit_process, os_loader_exit will swap to app.
         * XXX: For thread exit: somebody should swap to app later: but
         * os_thread_not_under_dynamo() doesn't seem to (unlike UNIX) (and if we
         * change that we should call it *after* loader_thread_exit()!).
         * It's not that important I guess: the thread is exiting.
         */
#    endif

        /* if freeing the app stack we must be on the dstack when we cleanup */
        if (dcontext->free_app_stack && !is_currently_on_dstack(dcontext)) {
            if (exit_process) {
                /* XXX: wasteful to use two dcontext fields just for this.
                 * Extend call_switch_stack to support extra args or sthg?
                 */
                dcontext->nudge_terminate_process = true;
                dcontext->nudge_exit_code = exit_code;
            }
            call_switch_stack(dcontext, dcontext->dstack,
                              (void (*)(void *))nudge_terminate_on_dstack,
                              NULL /* not on d_r_initstack */, false /* don't return */);
        } else {
            /* Already on dstack or nudge creator will free app stack. */
            if (exit_process) {
                os_terminate_with_code(dcontext, TERMINATE_PROCESS | TERMINATE_CLEANUP,
                                       exit_code);
            } else {
                os_terminate(dcontext, TERMINATE_THREAD | TERMINATE_CLEANUP);
            }
        }
    }
    ASSERT_NOT_REACHED(); /* we should never return */
    return true;
}

/* This is the actual nudge handler
 * Notes: This function returns a boolean mainly to fix case 5130; it is not
 *        really necessary.
 */
bool
generic_nudge_handler(nudge_arg_t *arg_dont_use)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    nudge_arg_t safe_arg = { 0 };
    uint nudge_action_mask = 0;

#    ifdef WINDOWS
    /* this routine is run natively via leave_call_native() so there's no
     * cxt switch that swapped for us
     */
    if (dcontext != NULL)
        swap_peb_pointer(dcontext, true /*to priv*/);
#    endif

    /* To be extra safe we use d_r_safe_read() to access the nudge argument, though once
     * we get past the checks below we are trusting its content. */
    ASSERT(arg_dont_use != NULL && "invalid nudge argument");
    if (!d_r_safe_read(arg_dont_use, sizeof(nudge_arg_t), &safe_arg)) {
        ASSERT(false && "invalid nudge argument");
        goto nudge_finished;
    }
    nudge_action_mask = safe_arg.nudge_action_mask;

    /* if needed tell thread exit to free the application stack */
    if (!TEST(NUDGE_NUDGER_FREE_STACK, safe_arg.flags)) {
        dcontext->free_app_stack = true;
    }

    /* FIXME - would be nice to inform nudge creator if we need to nop the nudge. */

    /* Fix for case 5702.  If a nudge thread comes in during process exit,
     * don't process it, i.e., nop it. FIXME - this leaks the app stack and nudge arg
     * if the nudge was supposed to free them. */
    if (dynamo_exited)
        goto nudge_finished;

    /* Node manager will not be able to nudge before reading the drmarker and
     * the dr_marker isn't available before callback_interception_init().
     * Since after callback_interception_init() new threads won't be allowed
     * to progress till dynamo_initialized is set, by the time a nudge thread
     * reaches here dynamo_initialized should be set. */
    ASSERT(dynamo_initialized);
    if (!dynamo_initialized)
        goto nudge_finished;

    /* We should always have a dcontext. */
    ASSERT(dcontext != NULL);
    if (dcontext == NULL)
        goto nudge_finished;

    ENTERING_DR();

    /* Xref case 552, the nudge_target value provides a reasonable measure
     * of security against an attacker leveraging this routine. */
    if (dcontext->nudge_target !=
        (void *)generic_nudge_target
            /* Allow a syscall for our test in debug build. */
            IF_DEBUG(&&!check_filter("win32.tls.exe",
                                     get_short_name(get_application_name())))) {
        /* FIXME - should we report this likely attempt to attack us? need
         * a unit test for this (though will then have to tone this down). */
        ASSERT(false && "unauthorized thread tried to nudge");
        /* If we really are under attack we should terminate immediately and
         * proceed no further. Note we are leaking the app stack and nudge arg if we
         * were supposed to free them. */
        os_terminate(dcontext, TERMINATE_THREAD);
        ASSERT_NOT_REACHED();
    }

    /* Free the arg if requested. */
    if (TEST(NUDGE_FREE_ARG, safe_arg.flags)) {
        nt_free_virtual_memory(arg_dont_use);
    }

    handle_nudge(dcontext, &safe_arg);

nudge_finished:
    return nudge_thread_cleanup(dcontext, false /*just thread*/, 0 /*unused*/);
}

#endif /* WINDOWS */

/* This routine may not return */
IF_WINDOWS(static)
void
handle_nudge(dcontext_t *dcontext, nudge_arg_t *arg)
{
    uint nudge_action_mask = arg->nudge_action_mask;

    /* Future version checks would go here. */
    ASSERT_CURIOSITY(arg->version == NUDGE_ARG_CURRENT_VERSION);

    /* Nudge shouldn't start with any locks held.  Do this assert after the
     * dynamo_exited check, other wise the locks may be deleted. */
    ASSERT_OWN_NO_LOCKS();

    STATS_INC(num_nudges);

#ifdef WINDOWS
    /* Linux does this in signal.c */
    SYSLOG_INTERNAL_INFO("received nudge mask=0x%x id=0x%08x arg=0x" ZHEX64_FORMAT_STRING,
                         arg->nudge_action_mask, arg->client_id, arg->client_arg);
#endif

    if (nudge_action_mask == 0) {
        ASSERT_CURIOSITY(false && "Nudge: no action specified");
        return;
    } else if (nudge_action_mask >= NUDGE_GENERIC(PARAMETRIZED_END)) {
        ASSERT(false && "Nudge: unknown nudge action");
        return;
    }

    /* In -thin_client mode only detach and process_control nudges are allowed;
     * case 8888. */
#define VALID_THIN_CLIENT_NUDGES (NUDGE_GENERIC(process_control) | NUDGE_GENERIC(detach))
    if (DYNAMO_OPTION(thin_client)) {
        if (TEST(VALID_THIN_CLIENT_NUDGES, nudge_action_mask)) {
            /* If it is a valid thin client nudge, then disable all others. */
            nudge_action_mask &= VALID_THIN_CLIENT_NUDGES;
        } else {
            return; /* invalid nudge for thin_client, so mute it */
        }
    }

    /* FIXME: NYI action handlers. As implemented move to desired order. */
    if (TEST(NUDGE_GENERIC(upgrade), nudge_action_mask)) {
        /* FIXME: watch out for flushed clean-call fragment */
        nudge_action_mask &= ~NUDGE_GENERIC(upgrade);
        ASSERT_NOT_IMPLEMENTED(false && "case 4179");
    }
    if (TEST(NUDGE_GENERIC(kstats), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(kstats);
        ASSERT_NOT_IMPLEMENTED(false);
    }
#ifdef INTERNAL
    if (TEST(NUDGE_GENERIC(stats), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(stats);
        ASSERT_NOT_IMPLEMENTED(false);
    }
    if (TEST(NUDGE_GENERIC(invalidate), nudge_action_mask)) {
        /* FIXME: watch out for flushed clean-call fragment  */
        nudge_action_mask &= ~NUDGE_GENERIC(invalidate);
        ASSERT_NOT_IMPLEMENTED(false);
    }
    if (TEST(NUDGE_GENERIC(recreate_pc), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(recreate_pc);
        ASSERT_NOT_IMPLEMENTED(false);
    }
    if (TEST(NUDGE_GENERIC(recreate_state), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(recreate_state);
        ASSERT_NOT_IMPLEMENTED(false);
    }
    if (TEST(NUDGE_GENERIC(reattach), nudge_action_mask)) {
        /* FIXME: watch out for flushed clean-call fragment */
        nudge_action_mask &= ~NUDGE_GENERIC(reattach);
        ASSERT_NOT_IMPLEMENTED(false);
    }
#endif /* INTERNAL */
    if (TEST(NUDGE_GENERIC(diagnose), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(diagnose);
        ASSERT_NOT_IMPLEMENTED(false);
    }

    /* Implemented action handlers */
    if (TEST(NUDGE_GENERIC(opt), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(opt);
        synchronize_dynamic_options();
    }
    if (TEST(NUDGE_GENERIC(ldmp), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(ldmp);
        os_dump_core("Nudge triggered ldmp.");
    }
    if (TEST(NUDGE_GENERIC(freeze), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(freeze);
        coarse_units_freeze_all(true /*in-place: FIXME: separate nudge for non?*/);
    }
    if (TEST(NUDGE_GENERIC(persist), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(persist);
        coarse_units_freeze_all(false /*!in-place==persist*/);
    }
    if (TEST(NUDGE_GENERIC(client), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(client);
        instrument_nudge(dcontext, arg->client_id, arg->client_arg);
    }
#ifdef PROCESS_CONTROL
    if (TEST(NUDGE_GENERIC(process_control), nudge_action_mask)) { /* Case 8594 */
        nudge_action_mask &= ~NUDGE_GENERIC(process_control);
        /* Need to synchronize because process control can be switched between
         * on (allow or block list) & off.  FIXME - the nudge mask should specify this,
         * but doesn't hurt to do it again. */
        synchronize_dynamic_options();
        if (IS_PROCESS_CONTROL_ON())
            process_control();

        /* If process control is enforced then control won't come back.  If
         * either -detect_mode is on or if there was nothing to enforce, control
         * comes back in which case it is safe to let remaining nudges be
         * processed because no core state would have been changed. */
    }
#endif
#ifdef HOTPATCHING
    if (DYNAMO_OPTION(hot_patching) && DYNAMO_OPTION(liveshields) &&
        TEST_ANY(NUDGE_GENERIC(policy) | NUDGE_GENERIC(mode) | NUDGE_GENERIC(lstats),
                 nudge_action_mask)) {
        hotp_nudge_update(
            nudge_action_mask &
            (NUDGE_GENERIC(policy) | NUDGE_GENERIC(mode) | NUDGE_GENERIC(lstats)));
        nudge_action_mask &=
            ~(NUDGE_GENERIC(policy) | NUDGE_GENERIC(mode) | NUDGE_GENERIC(lstats));
    }
#endif
#ifdef PROGRAM_SHEPHERDING
    if (TEST(NUDGE_GENERIC(violation), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(violation);
        /* Use nudge mechanism to trigger a security violation at an
         * arbitrary time. Note - is only useful for testing kill process attack
         * handling as this is not an app thread (we injected it). */
        /* see bug 652 for planned improvements */
        security_violation(dcontext, dcontext->next_tag, ATTACK_SIM_NUDGE_VIOLATION,
                           OPTION_BLOCK | OPTION_REPORT);
    }
#endif
    if (TEST(NUDGE_GENERIC(reset), nudge_action_mask)) {
        nudge_action_mask &= ~NUDGE_GENERIC(reset);
        if (DYNAMO_OPTION(enable_reset)) {
            d_r_mutex_lock(&reset_pending_lock);
            /* fcache_reset_all_caches_proactively() will unlock */
            fcache_reset_all_caches_proactively(RESET_ALL);
            /* NOTE - reset is safe since we won't return to the code cache below (we
             * will in fact not return at all). */
        } else {
            SYSLOG_INTERNAL_WARNING("nudge reset ignored since resets are disabled");
        }
    }
#ifdef WINDOWS
    /* The detach handler is last since in the common case it doesn't return. */
    if (TEST(NUDGE_GENERIC(detach), nudge_action_mask)) {
        dcontext->free_app_stack = false;
        nudge_action_mask &= ~NUDGE_GENERIC(detach);
        detach_helper(DETACH_NORMAL_TYPE);
    }
#endif
}

#ifdef UNIX
/* Only touches thread-private data and acquires no lock */
void
nudge_add_pending(dcontext_t *dcontext, nudge_arg_t *nudge_arg)
{
    pending_nudge_t *pending =
        (pending_nudge_t *)heap_alloc(dcontext, sizeof(*pending) HEAPACCT(ACCT_OTHER));
    pending_nudge_t *prev;
    pending->arg = *nudge_arg;
    pending->next = NULL;
    /* Simpler to prepend, but we want FIFO.  Should be rare to have multiple
     * so not worth storing an end pointer.
     */
    DOSTATS({
        if (dcontext->nudge_pending != NULL)
            STATS_INC(num_pending_nudges);
    });
    for (prev = dcontext->nudge_pending; prev != NULL && prev->next != NULL;
         prev = prev->next)
        ;
    if (prev == NULL) {
        dcontext->nudge_pending = pending;
    } else {
        prev->next = pending;
    }
}
#endif

/* send a nudge from DR to the current process or another process */
dr_config_status_t
nudge_internal(process_id_t pid, uint nudge_action_mask, uint64 client_arg,
               client_id_t client_id, uint timeout_ms)
{
    bool internal = (pid == get_process_id());
#ifdef WINDOWS
    HANDLE hthread, hproc;
    bool res;
    void *nudge_target;
    dr_config_status_t status;
    wait_status_t wait;
#else
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif
    nudge_arg_t nudge_arg;
    memset(&nudge_arg, 0, sizeof(nudge_arg));

    nudge_arg.version = NUDGE_ARG_CURRENT_VERSION;
    nudge_arg.nudge_action_mask = nudge_action_mask;
    /* We do not set NUDGE_NUDGER_FREE_STACK so the stack will be freed
     * in the target process, for <=win7.
     */
    nudge_arg.flags = (internal ? NUDGE_IS_INTERNAL : 0);
#ifdef WINDOWS
    if (get_os_version() >= WINDOWS_VERSION_8) {
        /* The kernel owns and frees the stack. */
        nudge_arg.flags |= NUDGE_NUDGER_FREE_STACK;
        /* The arg was placed in a new kernel alloc. */
        nudge_arg.flags |= NUDGE_FREE_ARG;
    }
#endif
    nudge_arg.client_arg = client_arg;
    nudge_arg.client_id = client_id;

    LOG(GLOBAL, LOG_ALL, 1, "Creating internal nudge with action_mask 0x%08x\n",
        nudge_action_mask);

#ifdef WINDOWS
    if (internal) {
        hproc = NT_CURRENT_PROCESS;
        nudge_target = (void *)generic_nudge_target;
    } else {
        dr_marker_t marker;

        hproc = process_handle_from_id(pid);
        if (hproc == NULL)
            return DR_NUDGE_PID_NOT_FOUND;
        if (read_and_verify_dr_marker(hproc, &marker) != DR_MARKER_FOUND) {
            /* if target process is not under DR (or any error getting
             * marker), don't nudge.
             */
            return DR_NUDGE_PID_NOT_INJECTED;
        }
        nudge_target = marker.dr_generic_nudge_target;
    }

    hthread = our_create_thread(hproc, IF_X64_ELSE(true, false), nudge_target, NULL,
                                &nudge_arg, sizeof(nudge_arg_t), 15 * (uint)PAGE_SIZE,
                                12 * (uint)PAGE_SIZE, false, NULL);
    ASSERT(hthread != INVALID_HANDLE_VALUE);
    if (hthread == INVALID_HANDLE_VALUE)
        return DR_FAILURE;

    wait = os_wait_handle(hthread, timeout_ms);
    if (wait == WAIT_SIGNALED || (wait == WAIT_TIMEDOUT && timeout_ms == 0))
        status = DR_SUCCESS;
    else if (wait == WAIT_TIMEDOUT)
        status = DR_NUDGE_TIMEOUT;
    else
        status = DR_FAILURE;

    res = close_handle(hthread);
    ASSERT(res);
    LOG(GLOBAL, LOG_ALL, 1, "Finished creating internal nudge thread\n");

    if (internal)
        close_handle(hproc);

    if (hthread != INVALID_HANDLE_VALUE)
        return DR_SUCCESS;
    else
        return DR_FAILURE;
#else
    if (internal) {
        /* We could send a signal, but that doesn't help matters since the
         * interruption point will be here and not be any potential fragment
         * underneath this clean call if a client invoked this (unless the signal
         * ends up going to another thread: can't control that w/ sigqueue).  So we
         * just document that it's up to the client to bound delivery time.
         */
        if (dcontext == NULL)
            return DR_FAILURE;
        nudge_add_pending(dcontext, &nudge_arg);
        return DR_SUCCESS;
    } else {
        if (send_nudge_signal(pid, nudge_action_mask, client_id, client_arg))
            return DR_SUCCESS;
        else
            return DR_FAILURE;
    }
#endif /* WINDOWS -> UNIX */
}
