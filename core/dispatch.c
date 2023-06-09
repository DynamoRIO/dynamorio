/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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
 * dispatch.c - central dynamo control manager
 */

#include "globals.h"
#include "link.h"
#include "fragment.h"
#include "fcache.h"
#include "monitor.h"
#include "synch.h"
#include "perscache.h"
#include "native_exec.h"
#include "translate.h"

#include "emit.h"
#include "arch.h"
#include "instrument.h"

#ifdef DGC_DIAGNOSTICS
#    include "instr.h"
#    include "disassemble.h"
#endif

#ifdef RCT_IND_BRANCH
#    include "rct.h"
#endif

#ifdef VMX86_SERVER
#    include "vmkuw.h"
#endif

/* forward declarations */
static void
dispatch_enter_dynamorio(dcontext_t *dcontext);

static bool
dispatch_enter_fcache(dcontext_t *dcontext, fragment_t *targetf);

static void
dispatch_enter_fcache_stats(dcontext_t *dcontext, fragment_t *targetf);

static void
enter_fcache(dcontext_t *dcontext, fcache_enter_func_t entry, cache_pc pc);

static void
dispatch_enter_native(dcontext_t *dcontext);

static void
dispatch_exit_fcache(dcontext_t *dcontext);

static void
dispatch_exit_fcache_stats(dcontext_t *dcontext);

static void
handle_post_system_call(dcontext_t *dcontext);

static void
handle_special_tag(dcontext_t *dcontext);

#ifdef WINDOWS
static void
handle_callback_return(dcontext_t *dcontext);
#endif

/* PR 356503: detect clients making syscalls via sysenter */
static inline void
found_client_sysenter(void)
{
    CLIENT_ASSERT(false,
                  "Is your client invoking raw system calls via vdso sysenter? "
                  "While such behavior is not recommended and can create problems, "
                  "it may work with the -sysenter_is_int80 runtime option.");
}

static bool
exited_due_to_ni_syscall(dcontext_t *dcontext)
{
    if (TESTANY(LINK_NI_SYSCALL_ALL, dcontext->last_exit->flags))
        return true;
    if (TEST(LINK_SPECIAL_EXIT, dcontext->last_exit->flags) &&
        (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_NI_SYSCALL_INT_0x81 ||
         dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_NI_SYSCALL_INT_0x82))
        return true;
    return false;
}

/* This is the central hub of control management in DynamoRIO.
 * It is entered with a clean dstack at startup and after every cache
 * exit, whether normal or kernel-mediated via a trampoline context switch.
 * Having no stack state kept across cache executions avoids
 * self-protection issues with the dstack.
 */
void
d_r_dispatch(dcontext_t *dcontext)
{
    fragment_t *targetf;
    fragment_t coarse_f;

#ifdef HAVE_TLS
#    if defined(UNIX) && defined(X86)
    /* i#2089: the parent of a new thread has TLS in an unstable state
     * and needs to restore it prior to invoking get_thread_private_dcontext().
     */
    if (get_at_syscall(dcontext) && was_thread_create_syscall(dcontext))
        os_clone_post(dcontext);
#    endif
    ASSERT(dcontext == get_thread_private_dcontext() ||
           /* i#813: the app hit our post-sysenter hook while native */
           (dcontext->whereami == DR_WHERE_APP &&
            dcontext->last_exit == get_syscall_linkstub()));
#else
#    ifdef UNIX
    /* CAUTION: for !HAVE_TLS, upon a fork, the child's
     * get_thread_private_dcontext() will return NULL because its thread
     * id is different and tls_table hasn't been updated yet (will be
     * done in post_system_call()).  NULL dcontext thus returned causes
     * logging/core dumping to malfunction; kstats trigger asserts.
     */
    ASSERT(dcontext == get_thread_private_dcontext() || pid_cached != get_process_id());
#    endif
#endif

    dispatch_enter_dynamorio(dcontext);
    LOG(THREAD, LOG_INTERP, 2, "\nd_r_dispatch: target = " PFX "\n", dcontext->next_tag);

    /* This is really a 1-iter loop most of the time: we only iterate
     * when we obtain a target fragment but then fail to enter the
     * cache due to flushing before we get there.
     */
    do {
        if (is_in_dynamo_dll(dcontext->next_tag) ||
            dcontext->next_tag == BACK_TO_NATIVE_AFTER_SYSCALL || dcontext->go_native) {
            handle_special_tag(dcontext);
        }
        /* Neither hotp_only nor thin_client should have any fragment
         * fcache related work to do.
         */
        ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
        targetf = fragment_lookup_fine_and_coarse(dcontext, dcontext->next_tag, &coarse_f,
                                                  dcontext->last_exit);
#ifdef UNIX
        /* i#1276: dcontext->next_tag could be a special stub pc used by
         * DR to maintain control in hybrid execution, in which case the
         * target should be replaced with correct app target.
         */
        if (targetf == NULL && DYNAMO_OPTION(native_exec) &&
            DYNAMO_OPTION(native_exec_opt) && native_exec_replace_next_tag(dcontext))
            continue;
#endif
        do {
            if (targetf != NULL) {
                KSTART(monitor_enter);
                /* invoke monitor to continue or start a trace
                 * may result in changing or nullifying targetf
                 */
                targetf = monitor_cache_enter(dcontext, targetf);
                KSTOP_NOT_MATCHING(monitor_enter); /* or monitor_enter_thci */
            }
            if (targetf != NULL)
                break;
            /* must call outside of USE_BB_BUILDING_LOCK guard for bb_lock_would_have: */
            SHARED_BB_LOCK();
            if (USE_BB_BUILDING_LOCK() || targetf == NULL) {
                /* must re-lookup while holding lock and keep the lock until we've
                 * built the bb and added it to the lookup table
                 * FIXME: optimize away redundant lookup: flags to know why came out?
                 */
                targetf = fragment_lookup_fine_and_coarse(dcontext, dcontext->next_tag,
                                                          &coarse_f, dcontext->last_exit);
            }
            if (targetf == NULL) {
                SELF_PROTECT_LOCAL(dcontext, WRITABLE);
                targetf = build_basic_block_fragment(dcontext, dcontext->next_tag, 0,
                                                     true /*link*/, true /*visible*/,
                                                     false /*!for_trace*/, NULL);
                SELF_PROTECT_LOCAL(dcontext, READONLY);
            }
            if (targetf != NULL && TEST(FRAG_COARSE_GRAIN, targetf->flags)) {
                /* targetf is a static temp fragment protected by bb_building_lock,
                 * so we must make a local copy to use before releasing the lock.
                 * FIXME: best to pass local wrapper to build_basic_block_fragment
                 * and all the way through emit and link?  Would need linkstubs
                 * tailing the fragment_t.
                 */
                ASSERT(USE_BB_BUILDING_LOCK_STEADY_STATE());
                fragment_coarse_wrapper(&coarse_f, targetf->tag,
                                        FCACHE_ENTRY_PC(targetf));
                targetf = &coarse_f;
            }
            SHARED_BB_UNLOCK();
            if (targetf == NULL)
                break;
            /* loop around and re-do monitor check */
        } while (true);

        if (targetf != NULL) {
            if (dispatch_enter_fcache(dcontext, targetf)) {
                /* won't reach here: will re-enter d_r_dispatch() with a clean stack */
                ASSERT_NOT_REACHED();
            } else
                targetf = NULL; /* targetf was flushed */
        }
    } while (true);
    ASSERT_NOT_REACHED();
}

/* returns true if pc is a point at which DynamoRIO should stop interpreting */
bool
is_stopping_point(dcontext_t *dcontext, app_pc arg_pc)
{
#ifdef DR_APP_EXPORTS
    /* TODO i#4720: Find and update other comparisons to function pointers. */
    app_pc pc = PC_AS_JMP_TGT(dr_get_isa_mode(dcontext), arg_pc);
#endif
    if ((arg_pc /*undecorated*/ == BACK_TO_NATIVE_AFTER_SYSCALL &&
         /* case 6253: app may xfer to this "address" in which case pass
          * exception to app
          */
         dcontext->native_exec_postsyscall != NULL)
#ifdef DR_APP_EXPORTS
        || (!automatic_startup &&
            (pc == (app_pc)dynamorio_app_exit ||
             /* FIXME: Is this a holdover from long ago? dymamo_thread_exit
              * should not be called from the cache.
              */
             pc == (app_pc)dynamo_thread_exit || pc == (app_pc)dr_app_stop ||
             pc == (app_pc)dr_app_stop_and_cleanup ||
             pc == (app_pc)dr_app_stop_and_cleanup_with_stats))
#endif
#ifdef WINDOWS
    /* we go all the way to NtTerminateThread/NtTerminateProcess */
#else /* UNIX */
    /* we go all the way to SYS_exit or SYS_{,t,tg}kill(SIGABRT) */
#endif
    )
        return true;

    return false;
}

static void
dispatch_enter_fcache_stats(dcontext_t *dcontext, fragment_t *targetf)
{
#ifdef DEBUG
#    ifdef DGC_DIAGNOSTICS
    if (TEST(FRAG_DYNGEN, targetf->flags) && !is_dyngen_vsyscall(targetf->tag)) {
        char buf[MAXIMUM_SYMBOL_LENGTH];
        bool stack = is_address_on_stack(dcontext, targetf->tag);
        LOG(THREAD, LOG_DISPATCH, 1,
            "Entry into dyngen F%d(" PFX "%s%s) via:", targetf->id, targetf->tag,
            stack ? " stack" : "",
            (targetf->flags & FRAG_DYNGEN_RESTRICTED) != 0 ? " BAD" : "");
        if (!LINKSTUB_FAKE(dcontext->last_exit)) {
            app_pc translated_pc;
            /* can't recreate if fragment is deleted -- but should be fake then */
            ASSERT(!TEST(FRAG_WAS_DELETED, dcontext->last_fragment->flags));
            translated_pc = recreate_app_pc(
                dcontext, EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit),
                dcontext->last_fragment);
            if (translated_pc != NULL) {
                disassemble(dcontext, translated_pc, THREAD);
                print_symbolic_address(translated_pc, buf, sizeof(buf), false);
                LOG(THREAD, LOG_DISPATCH, 1, " %s\n", buf);
            }
            if (!stack &&
                (strstr(buf, "user32.dll") != NULL ||
                 strstr(buf, "USER32.DLL") != NULL)) {
                /* try to find who set up user32 callback */
                dump_mcontext_callstack(dcontext);
            }
            DOLOG(stack ? 1U : 2U, LOG_DISPATCH, {
                LOG(THREAD, LOG_DISPATCH, 1, "Originating bb:\n");
                disassemble_app_bb(dcontext, dcontext->last_fragment->tag, THREAD);
            });
        } else {
            /* FIXME: print type from last_exit */
            LOG(THREAD, LOG_DISPATCH, 1, "\n");
        }
        if (stack) {
            /* try to understand where code is on stack */
            LOG(THREAD, LOG_DISPATCH, 1, "cur esp=" PFX " ebp=" PFX "\n",
                get_mcontext(dcontext)->xsp, get_mcontext(dcontext)->xbp);
            dump_mcontext_callstack(dcontext);
        }
    }
#    endif

    if (d_r_stats->loglevel >= 2 && (d_r_stats->logmask & LOG_DISPATCH) != 0) {
        /* XXX: should use a different mask - and get printed at level 2 when turned on */
        DOLOG(4, LOG_DISPATCH,
              { dump_mcontext(get_mcontext(dcontext), THREAD, DUMP_NOT_XML); });
        DOLOG(6, LOG_DISPATCH, { dump_mcontext_callstack(dcontext); });
        DOKSTATS({ DOLOG(6, LOG_DISPATCH, { kstats_dump_stack(dcontext); }); });
        LOG(THREAD, LOG_DISPATCH, 2, "Entry into F%d(" PFX ")." PFX " %s%s%s",
            targetf->id, targetf->tag, FCACHE_ENTRY_PC(targetf),
            IF_X86_ELSE(
                IF_X64_ELSE(FRAG_IS_32(targetf->flags) ? "(32-bit)" : "", ""),
                IF_ARM_ELSE(FRAG_IS_THUMB(targetf->flags) ? "(T32)" : "(A32)", "")),
            TEST(FRAG_COARSE_GRAIN, targetf->flags) ? "(coarse)" : "",
            ((targetf->flags & FRAG_IS_TRACE_HEAD) != 0) ? "(trace head)" : "",
            ((targetf->flags & FRAG_IS_TRACE) != 0) ? "(trace)" : "");
        LOG(THREAD, LOG_DISPATCH, 2, "%s",
            TEST(FRAG_SHARED, targetf->flags) ? "(shared)" : "");
#    ifdef DGC_DIAGNOSTICS
        LOG(THREAD, LOG_DISPATCH, 2, "%s",
            TEST(FRAG_DYNGEN, targetf->flags) ? "(dyngen)" : "");
#    endif
        LOG(THREAD, LOG_DISPATCH, 2, "\n");

        DOLOG(3, LOG_SYMBOLS, {
            char symbuf[MAXIMUM_SYMBOL_LENGTH];
            print_symbolic_address(targetf->tag, symbuf, sizeof(symbuf), true);
            LOG(THREAD, LOG_SYMBOLS, 3, "\t%s\n", symbuf);
        });
    }
#endif /* DEBUG */
}

/* Executes a target fragment in the fragment cache */
static bool
dispatch_enter_fcache(dcontext_t *dcontext, fragment_t *targetf)
{
    fcache_enter_func_t fcache_enter;
    ASSERT(targetf != NULL);
    /* ensure we don't take over when we should be going native */
    ASSERT(dcontext->native_exec_postsyscall == NULL);

    /* We wait until here, rather than at cache exit time, to do lazy
     * linking so we can link to newly created fragments.
     */
    if (dcontext->last_exit == get_coarse_exit_linkstub() ||
        /* We need to lazy link if either of src or tgt is coarse */
        (LINKSTUB_DIRECT(dcontext->last_exit->flags) &&
         TEST(FRAG_COARSE_GRAIN, targetf->flags))) {
        coarse_lazy_link(dcontext, targetf);
    }

    if (!enter_nolinking(dcontext, targetf, true)) {
        /* not actually entering cache, so back to couldbelinking */
        enter_couldbelinking(dcontext, NULL, true);
        LOG(THREAD, LOG_DISPATCH, 2, "Just flushed targetf, next_tag is " PFX "\n",
            dcontext->next_tag);
        STATS_INC(num_entrances_aborted);
        /* shared entrance cannot-tell-if-deleted -> invalidate targetf
         * but then may double-do the trace!
         * FIXME: for now, we abort every time, ok to abort twice (first time
         * b/c there was a real flush of targetf), but could be perf hit.
         */
        trace_abort(dcontext);
        return false;
    }

    dispatch_enter_fcache_stats(dcontext, targetf);

    /* FIXME: for now we do this before the synch point to avoid complexity of
     * missing a KSTART(fcache_* for cases like NtSetContextThread where a thread
     * appears back at d_r_dispatch() from the synch point w/o ever entering the cache.
     * To truly fix we need to have the NtSetContextThread handler determine
     * whether its suspended target is at this synch point or in the cache.
     */
    DOKSTATS({
        /* stopped in dispatch_exit_fcache_stats */
        if (TEST(FRAG_IS_TRACE, targetf->flags))
            KSTART(fcache_trace_trace);
        else
            KSTART(fcache_default); /* fcache_bb_bb or fcache_bb_trace */
        /* FIXME: overestimates fcache time by counting in
         * fcache_enter/fcache_return for it - proper reading of this
         * value should discount the minimal cost of
         * fcache_enter/fcache_return for actual code cache times
         */
        /* FIXME: asynch events currently continue their current kstat
         * until they get back to d_r_dispatch, so in-fcache kstats are counting
         * the in-DR trampoline execution time!
         */
    });

    /* synch point for suspend, terminate, and detach */
    /* assumes mcontext is valid including errno but not pc (which we fix here)
     * assumes that thread is holding no locks
     * also assumes past enter_nolinking, so could_be_linking is false
     * for safety with respect to flush */
    /* a fast check before the heavy lifting */
    if (should_wait_at_safe_spot(dcontext)) {
        /* FIXME : we could put this synch point in enter_fcache but would need
         * to use SYSCALL_PC for syscalls (see issues with that in win32/os.c)
         */
        priv_mcontext_t *mcontext = get_mcontext(dcontext);
        cache_pc save_pc = mcontext->pc;
        /* FIXME : implementation choice, we could do recreate_app_pc
         * (fairly expensive but this is rare) instead of using the tag
         * which is a little hacky but should always be right */
        mcontext->pc = targetf->tag;
        /* could be targeting interception code or our dll main, would be
         * incorrect for GetContextThread and racy for detach, though we
         * would expect it to be very rare */
        if (!is_dynamo_address(mcontext->pc)) {
            check_wait_at_safe_spot(dcontext, THREAD_SYNCH_VALID_MCONTEXT);
            /* If we don't come back here synch-er is responsible for ensuring
             * our kstat stack doesn't get off (have to do a KSTART here) -- we
             * don't want to do the KSTART of fcache_* before this to avoid
             * counting synch time.
             */
        } else {
            LOG(THREAD, LOG_SYNCH, 1,
                "wait_at_safe_spot - unable to wait, targeting dr addr " PFX,
                mcontext->pc);
            STATS_INC(no_wait_entries);
        }
        mcontext->pc = save_pc;
    }

#ifdef UNIX
    /* We store this for purposes like signal unlinking (i#2019) */
    dcontext->asynch_target = dcontext->next_tag;
#endif

#if defined(UNIX) && defined(DEBUG)
    /* i#238/PR 499179: check that libc errno hasn't changed.  It's
     * not worth actually saving+restoring since to we'd also need to
     * preserve on clean calls, a perf hit.  Better to catch all libc
     * routines that need it and wrap just those.
     */
    ASSERT(
        get_libc_errno() == dcontext->libc_errno ||
        /* w/ private loader, our errno is disjoint from app's */
        INTERNAL_OPTION(private_loader) ||
        /* only when pthreads is loaded does libc switch to a per-thread
         * errno, so our raw thread tests end up using the same errno
         * for each thread!
         */
        check_filter("linux.thread;linux.clone", get_short_name(get_application_name())));
#endif

#if defined(UNIX) && !defined(DGC_DIAGNOSTICS) && defined(X86)
    /* i#107: handle segment register usage conflicts between app and dr:
     * if the target fragment has an instr that updates the segment selector,
     * update the corresponding information maintained by DR.
     */
    if (INTERNAL_OPTION(mangle_app_seg) && TEST(FRAG_HAS_MOV_SEG, targetf->flags)) {
        os_handle_mov_seg(dcontext, targetf->tag);
    }
#endif

    ASSERT(dr_get_isa_mode(dcontext) ==
           FRAG_ISA_MODE(targetf->flags)
               IF_X64(||
                      (dr_get_isa_mode(dcontext) == DR_ISA_IA32 &&
                       !FRAG_IS_32(targetf->flags) && DYNAMO_OPTION(x86_to_x64))));
    if (TEST(FRAG_SHARED, targetf->flags))
        fcache_enter = get_fcache_enter_shared_routine(dcontext);
    else
        fcache_enter = get_fcache_enter_private_routine(dcontext);

    enter_fcache(
        dcontext,
        (fcache_enter_func_t)
        /* DEFAULT_ISA_MODE as we want the ISA mode of our gencode */
        convert_data_to_function(PC_AS_JMP_TGT(DEFAULT_ISA_MODE, (app_pc)fcache_enter)),
#ifdef AARCH64
        /* Entry to fcache requires indirect branch. */
        PC_AS_JMP_TGT(FRAG_ISA_MODE(targetf->flags), FCACHE_PREFIX_ENTRY_PC(targetf))
#else
        PC_AS_JMP_TGT(FRAG_ISA_MODE(targetf->flags), FCACHE_ENTRY_PC(targetf))
#endif
    );
#ifdef UNIX
    if (dcontext->signals_pending > 0) {
        /* i#2019: the fcache_enter generated code starts with a check for pending
         * signals, allowing the signal handling code to simply queue signals that
         * arrive in DR code and only attempt to unlink for interruption points known
         * to be safe for unlinking.
         */
        KSTOP_NOT_MATCHING(fcache_default);
        dcontext->whereami = DR_WHERE_DISPATCH;
        enter_couldbelinking(dcontext, NULL, true);
        dcontext->next_tag = dcontext->asynch_target;
        LOG(THREAD, LOG_DISPATCH, 2,
            "Signal arrived while in DR: aborting fcache_enter; next_tag is " PFX "\n",
            dcontext->next_tag);
        STATS_INC(num_entrances_aborted);
        trace_abort(dcontext);
        receive_pending_signal(dcontext);
        return false;
    }
#endif
    ASSERT_NOT_REACHED();
    return false;
}

/* Enters the cache at the specified entrance routine to execute the
 * target pc.
 * Does not return.
 * Caller must do a KSTART to avoid kstats stack mismatches.
 * FIXME: only allow access to fcache_enter routine through here?
 * Indirect routine needs special treatment for handle_callback_return
 */
static void
enter_fcache(dcontext_t *dcontext, fcache_enter_func_t entry, cache_pc pc)
{
    ASSERT(!is_couldbelinking(dcontext));
    ASSERT(entry != NULL);
    ASSERT(pc != NULL);
    ASSERT(check_should_be_protected(DATASEC_RARELY_PROT));
    /* CANNOT hold any locks across cache execution, as our thread synch
     * assumes none are held
     */
    ASSERT_OWN_NO_LOCKS();
    ASSERT(dcontext->try_except.try_except_state == NULL);

    /* prepare to enter fcache */
    LOG(THREAD, LOG_DISPATCH, 4, "fcache_enter = " PFX ", target = " PFX "\n", entry, pc);
    set_fcache_target(dcontext, pc);
    ASSERT(pc != NULL);

#ifdef PROFILE_RDTSC
    if (dynamo_options.profile_times) {
        /* prepare to enter fcache */
        dcontext->prev_fragment = NULL;

        /* top ten cache times */
        dcontext->cache_frag_count = (uint64)0;
        dcontext->cache_enter_time = get_time();
    }
#endif

    dcontext->whereami = DR_WHERE_FCACHE;
    /* XXX i#5383: Audit these calls and ensure they cover all scenarios, are placed
     * at the most efficient level, and are always properly paired.
     * Better to have write calls around block building and linking paths rather than
     * assuming all paths might have written, with a debug query here to make sure no
     * paths were missed?
     */
    PTHREAD_JIT_READ();
    (*entry)(dcontext);
    IF_WINDOWS(ASSERT_NOT_REACHED()); /* returns for signals on unix */
}

/* Handles special tags in DR or elsewhere that do interesting things.
 * All PCs checked in here must be in DR or be BACK_TO_NATIVE_AFTER_SYSCALL.
 * Does not return if we've hit a stopping point; otherwise returns with an
 * updated next_tag for continued dispatch.
 */
static void
handle_special_tag(dcontext_t *dcontext)
{
    if (native_exec_is_back_from_native(dcontext->next_tag)) {
        /* This can happen if we start interpreting a native module. */
        ASSERT(DYNAMO_OPTION(native_exec));
        interpret_back_from_native(dcontext); /* updates next_tag */
    }

    if (is_stopping_point(dcontext, dcontext->next_tag) ||
        /* We don't want this to be part of is_stopping_point() b/c we don't
         * want bb building for state xl8 to look at it.
         */
        dcontext->go_native) {
        LOG(THREAD, LOG_INTERP, 1, "\n%s: thread " TIDFMT " returning to app @" PFX "\n",
            dcontext->go_native ? "Requested to go native"
                                : "Found DynamoRIO stopping point",
            d_r_get_thread_id(), dcontext->next_tag);
#ifdef DR_APP_EXPORTS
        if (dcontext->next_tag == (app_pc)dr_app_stop)
            send_all_other_threads_native();
#endif
        dispatch_enter_native(dcontext);
        ASSERT_NOT_REACHED(); /* noreturn */
    }
}

#if defined(DR_APP_EXPORTS) || defined(UNIX)
static void
dispatch_at_stopping_point(dcontext_t *dcontext)
{
    /* start/stop interface */
    KSTOP_NOT_MATCHING(dispatch_num_exits);

    /* if we stop in middle of tracing, thread-shared state may be messed
     * up (e.g., monitor grabs fragment lock for unlinking),
     * so abort the trace
     */
    if (is_building_trace(dcontext)) {
        LOG(THREAD, LOG_INTERP, 1, "squashing trace-in-progress\n");
        trace_abort(dcontext);
    }

    LOG(THREAD, LOG_INTERP, 1, "\nappstart_cleanup: found stopping point\n");
#    ifdef DEBUG
#        ifdef DR_APP_EXPORTS
    if (dcontext->next_tag == (app_pc)dynamo_thread_exit)
        LOG(THREAD, LOG_INTERP, 1, "\t==dynamo_thread_exit\n");
    else if (dcontext->next_tag == (app_pc)dynamorio_app_exit)
        LOG(THREAD, LOG_INTERP, 1, "\t==dynamorio_app_exit\n");
    else if (dcontext->next_tag == (app_pc)dr_app_stop)
        LOG(THREAD, LOG_INTERP, 1, "\t==dr_app_stop\n");
    else if (dcontext->next_tag == (app_pc)dr_app_stop_and_cleanup)
        LOG(THREAD, LOG_INTERP, 1, "\t==dr_app_stop_and_cleanup\n");
    else if (dcontext->next_tag == (app_pc)dr_app_stop_and_cleanup_with_stats)
        LOG(THREAD, LOG_INTERP, 1, "\t==dr_app_stop_and_cleanup_with_stats\n");
#        endif
#    endif

        /* XXX i#95: should we add an instrument_thread_detach_event()? */

#    ifdef DR_APP_EXPORTS
    /* not_under will be called by dynamo_shared_exit so skip it here. */
    if (dcontext->next_tag != (app_pc)dr_app_stop_and_cleanup &&
        dcontext->next_tag != (app_pc)dr_app_stop_and_cleanup_with_stats)
#    endif
        dynamo_thread_not_under_dynamo(dcontext);
    dcontext->go_native = false;
}
#endif

/* Called when we reach an interpretation stopping point either for
 * start/stop control of DR or for native_exec.  In both cases we give up
 * control and "go native", but we do not clean up the current thread,
 * assuming we will either take control back, or the app will explicitly
 * request we clean up.
 */
static void
dispatch_enter_native(dcontext_t *dcontext)
{
    /* The new fcache_enter's clean dstack design makes it usable for
     * entering native execution as well as the fcache.
     */
    fcache_enter_func_t go_native =
        (fcache_enter_func_t)convert_data_to_function(PC_AS_JMP_TGT(
            DEFAULT_ISA_MODE, (app_pc)get_fcache_enter_gonative_routine(dcontext)));
    set_last_exit(dcontext, (linkstub_t *)get_native_exec_linkstub());
    ASSERT_OWN_NO_LOCKS();
    if (dcontext->next_tag == BACK_TO_NATIVE_AFTER_SYSCALL) {
        /* we're simply going native again after an intercepted syscall,
         * not finalizing this thread or anything
         */
        IF_WINDOWS(DEBUG_DECLARE(extern dcontext_t * early_inject_load_helper_dcontext;))
        ASSERT(DYNAMO_OPTION(native_exec_syscalls)); /* else wouldn't have intercepted */

        /* Assert here we have a reason for going back to native (-native_exec and
         * non-empty native_exec_areas, RUNNING_WITHOUT_CODE_CACHE, hotp nudge thread
         * pretending to be native while loading a dll, or on win2k
         * early_inject_init() pretending to be native to find the inject address). */
        ASSERT((DYNAMO_OPTION(native_exec) && native_exec_areas != NULL &&
                !vmvector_empty(native_exec_areas)) ||
               IF_WINDOWS((DYNAMO_OPTION(early_inject) &&
                           early_inject_load_helper_dcontext ==
                               get_thread_private_dcontext()) ||)
                   IF_HOTP(dcontext->nudge_thread ||)
               /* clients requesting native execution come here */
               dr_bb_hook_exists() ||
               dcontext->currently_stopped || RUNNING_WITHOUT_CODE_CACHE());
        ASSERT(dcontext->native_exec_postsyscall != NULL);
        LOG(THREAD, LOG_ASYNCH, 1, "Returning to native " PFX " after a syscall\n",
            dcontext->native_exec_postsyscall);
        dcontext->next_tag =
            PC_AS_JMP_TGT(dr_get_isa_mode(dcontext), dcontext->native_exec_postsyscall);
        if (!dcontext->currently_stopped)
            dynamo_thread_not_under_dynamo(dcontext);
        dcontext->native_exec_postsyscall = NULL;
        LOG(THREAD, LOG_DISPATCH, 2,
            "Entry into native_exec after intercepted syscall\n");
        /* restore state as though never came out for syscall */
        KSTOP_NOT_MATCHING_DC(dcontext, dispatch_num_exits);
#ifdef KSTATS
        if (!dcontext->currently_stopped)
            KSTART_DC(dcontext, fcache_default);
#endif
        enter_nolinking(dcontext, NULL, true);
    } else {
#if defined(DR_APP_EXPORTS) || defined(UNIX)
        dcontext->next_tag = PC_AS_JMP_TGT(dr_get_isa_mode(dcontext), dcontext->next_tag);
        dispatch_at_stopping_point(dcontext);
        enter_nolinking(dcontext, NULL, false);
#else
        ASSERT_NOT_REACHED();
#endif
    }
    set_fcache_target(dcontext, dcontext->next_tag);
    dcontext->whereami = DR_WHERE_APP;
#ifdef UNIX
    do {
        (*go_native)(dcontext);
        /* If fcache_enter returns, there's a pending signal.  It must
         * be an alarm signal so we drop it as the simplest solution.
         */
        ASSERT(dcontext->signals_pending > 0);
        dcontext->signals_pending = 0;
    } while (true);
#else
    (*go_native)(dcontext);
#endif
    ASSERT_NOT_REACHED();
}

static void
set_next_tag_to_prior_syscall(dcontext_t *dcontext)
{
    /* We need to remember both the post-syscall resumption point and
     * the fact that we need to execute a syscall, but we only have
     * a single PC field to place it into inside our sigreturn frame
     * and other places.  Our solution is to point back at the
     * syscall instruction itself.  The walk-backward scheme here is a
     * little hacky perhaps.  We'll make a bb just for this syscall, which
     * will not know the syscall number: but any re-execution in a loop
     * will go back to the main bb.
     */
    dcontext->next_tag -= syscall_instr_length(
        dcontext->last_fragment == NULL ? DEFAULT_ISA_MODE
                                        : FRAG_ISA_MODE(dcontext->last_fragment->flags));
    ASSERT(is_syscall_at_pc(dcontext, dcontext->next_tag));
}

static void
dispatch_enter_dynamorio(dcontext_t *dcontext)
{
    /* We're transitioning to DynamoRIO from somewhere: either the fcache,
     * the kernel (DR_WHERE_TRAMPOLINE), or the app itself via our start/stop API.
     * N.B.: set whereami to DR_WHERE_APP iff this is the first d_r_dispatch() entry
     * for this thread!
     */
    dr_where_am_i_t wherewasi = dcontext->whereami;
#if defined(UNIX) && !defined(X64)
    if (!(wherewasi == DR_WHERE_FCACHE || wherewasi == DR_WHERE_TRAMPOLINE ||
          wherewasi == DR_WHERE_APP) &&
        get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        /* This is probably our own syscalls hitting our own sysenter
         * hook (PR 212570), since we're not completely user library
         * independent (PR 206369).
         * The primary calls I'm worried about are dl{open,close}.
         * Note that we can't go jump to vsyscall_syscall_end_pc here b/c
         * fcache_return cleared the dstack, so we can't really recover.
         * We could put in a custom exit stub and return routine and recover,
         * but we need to get library independent anyway so it's not worth it.
         */
        /* PR 356503: clients using libraries that make syscalls can end up here */
        found_client_sysenter();
        ASSERT_BUG_NUM(
            206369, false && "DR's own syscall (via user library) hit the sysenter hook");
    }
#endif
    ASSERT(wherewasi == DR_WHERE_FCACHE || wherewasi == DR_WHERE_TRAMPOLINE ||
           wherewasi == DR_WHERE_APP ||
           /* If the thread was waiting at check_wait_at_safe_point when getting
            * suspended, we were in dispatch (ref i#3427). We will be here after the
            * thread's context is being reset proactively (due to some -reset_at_*
            * option) or before sending it native.
            */
           ((dcontext->go_native || dcontext->last_exit == get_reset_linkstub()) &&
            wherewasi == DR_WHERE_DISPATCH));
    dcontext->whereami = DR_WHERE_DISPATCH;
    ASSERT_LOCAL_HEAP_UNPROTECTED(dcontext);
    ASSERT(check_should_be_protected(DATASEC_RARELY_PROT));
    /* CANNOT hold any locks across cache execution, as our thread synch
     * assumes none are held
     */
    ASSERT_OWN_NO_LOCKS();

#if defined(UNIX) && defined(DEBUG)
    /* i#238/PR 499179: check that libc errno hasn't changed */
    /* w/ private loader, our errno is disjoint from app's */
    if (!INTERNAL_OPTION(private_loader))
        dcontext->libc_errno = get_libc_errno();
    os_enter_dynamorio();
#endif

    DOLOG(2, LOG_INTERP, {
        if (wherewasi == DR_WHERE_APP) {
            LOG(THREAD, LOG_INTERP, 2, "\ninitial d_r_dispatch: target = " PFX "\n",
                dcontext->next_tag);
            dump_mcontext_callstack(dcontext);
            dump_mcontext(get_mcontext(dcontext), THREAD, DUMP_NOT_XML);
        }
    });

    /* We have to perform some tasks with last_exit early, before we
     * become couldbelinking -- the rest are done in dispatch_exit_fcache().
     * It's ok to de-reference last_exit since even though deleter may assume
     * no one has ptrs to it, cannot delete until we're officially out of the
     * cache, which doesn't happen until enter_couldbelinking -- still kind of
     * messy that we're violating assumption of no ptrs...
     */

    if (wherewasi == DR_WHERE_APP) { /* first entrance */
        if (dcontext->last_exit == get_syscall_linkstub()) {
            /* i#813: the app hit our post-sysenter hook while native.
             * XXX: should we try to process ni syscalls here?  But we're only
             * seeing post- and not pre-.
             */
            LOG(THREAD, LOG_INTERP, 2, "hit post-sysenter hook while native\n");
            ASSERT(dcontext->currently_stopped || IS_CLIENT_THREAD(dcontext));
            dcontext->next_tag = BACK_TO_NATIVE_AFTER_SYSCALL;
            dcontext->native_exec_postsyscall =
                IF_UNIX_ELSE(vsyscall_sysenter_displaced_pc, vsyscall_syscall_end_pc);
        } else {
            ASSERT(dcontext->last_exit == get_starting_linkstub() ||
                   /* The start/stop API will set this linkstub. */
                   IF_APP_EXPORTS(dcontext->last_exit == get_native_exec_linkstub() ||)
                   /* new thread */
                   IF_WINDOWS_ELSE_0(dcontext->last_exit == get_asynch_linkstub()));
        }
    } else {
        /* MUST be set, if only to a fake linkstub_t */
        ASSERT(dcontext->last_exit != NULL);
        /* cache last_exit's fragment */
        dcontext->last_fragment = linkstub_fragment(dcontext, dcontext->last_exit);

        /* If we exited from an indirect branch then dcontext->next_tag
         * already has the next tag value; otherwise we must set it here,
         * before we might dive back into the cache for a system call.
         */
        if (LINKSTUB_DIRECT(dcontext->last_exit->flags)) {
            if (INTERNAL_OPTION(cbr_single_stub)) {
                linkstub_t *nxt = linkstub_shares_next_stub(
                    dcontext, dcontext->last_fragment, dcontext->last_exit);
                if (nxt != NULL) {
                    /* must distinguish the two based on eflags */
                    dcontext->last_exit = linkstub_cbr_disambiguate(
                        dcontext, dcontext->last_fragment, dcontext->last_exit, nxt);
                    ASSERT(dcontext->last_fragment ==
                           linkstub_fragment(dcontext, dcontext->last_exit));
                    STATS_INC(cbr_disambiguations);
                }
            }

            dcontext->next_tag =
                EXIT_TARGET_TAG(dcontext, dcontext->last_fragment, dcontext->last_exit);
        } else {
            /* get src info from coarse ibl exit into the right place */
            if (DYNAMO_OPTION(coarse_units)) {
                if (is_ibl_sourceless_linkstub((const linkstub_t *)dcontext->last_exit))
                    set_coarse_ibl_exit(dcontext);
                else if (DYNAMO_OPTION(use_persisted) &&
                         dcontext->last_exit == get_coarse_exit_linkstub()) {
                    /* i#670: for frozen unit, shift from persist-time mod base
                     * to use-time mod base
                     */
                    coarse_info_t *info = dcontext->coarse_exit.dir_exit;
                    ASSERT(info != NULL);
                    if (info->mod_shift != 0 &&
                        dcontext->next_tag >= info->base_pc + info->mod_shift &&
                        dcontext->next_tag < info->end_pc + info->mod_shift) {
                        dcontext->next_tag -= info->mod_shift;
                        LOG(THREAD, LOG_INTERP, 3, "adjusted shifted-coarse tag to %p\n",
                            dcontext->next_tag);
                    }
                }
            }
        }

        dispatch_exit_fcache_stats(dcontext);
        /* Maybe-permanent native transitions (dr_app_stop()) have to pop kstack,
         * and thus so do temporary native_exec transitions.  Thus, for neither
         * is there anything to pop here.
         */
        if (dcontext->last_exit != get_native_exec_linkstub() &&
            dcontext->last_exit != get_native_exec_syscall_linkstub())
            KSTOP_NOT_MATCHING(dispatch_num_exits);
    }
    /* KSWITCHed next time around for a better explanation */
    KSTART_DC(dcontext, dispatch_num_exits);

    if (wherewasi != DR_WHERE_APP) { /* if not first entrance */
        if (get_at_syscall(dcontext))
            handle_post_system_call(dcontext);

#ifdef X86
        /* If the next basic block starts at a debug register value,
         * we fire a single step exception before getting to the basic block. */
        if (debug_register_fire_on_addr(dcontext->next_tag)) {
            LOG(THREAD, LOG_DISPATCH, 2, "Generates single step before " PFX "\n",
                dcontext->next_tag);
            os_forge_exception(dcontext->next_tag, SINGLE_STEP_EXCEPTION);
            ASSERT_NOT_REACHED();
        }
#endif

        /* A non-ignorable syscall or cb return ending a bb must be acted on
         * We do it here to avoid becoming couldbelinking twice.
         *
         */
        if (exited_due_to_ni_syscall(dcontext) ||
            instrument_invoke_another_syscall(dcontext)) {
            if (IF_UNIX_ELSE(dcontext->signals_pending > 0, false)) {
                /* Avoid running the pre-handler and aborting the fcache_enter w/o
                 * a good way to undo the pre-handler.
                 */
                set_next_tag_to_prior_syscall(dcontext);
            } else {
                handle_system_call(dcontext);
                /* We'll return here if decided to skip the syscall; else, back to
                 *  d_r_dispatch.
                 */
            }
        }
#ifdef WINDOWS
        else if (TEST(LINK_CALLBACK_RETURN, dcontext->last_exit->flags)) {
            handle_callback_return(dcontext);
            ASSERT_NOT_REACHED();
        }
#endif

#ifdef AARCH64
        if (dcontext->last_exit == get_selfmod_linkstub()) {
            app_pc begin = (app_pc)dcontext->local_state->spill_space.r2;
            app_pc end = (app_pc)dcontext->local_state->spill_space.r3;
            dcontext->next_tag = (app_pc)dcontext->local_state->spill_space.r4;
            flush_fragments_from_region(dcontext, begin, end - begin, true,
                                        NULL /*flush_completion_callback*/,
                                        NULL /*user_data*/);
        }
#endif

        if (TEST(LINK_SPECIAL_EXIT, dcontext->last_exit->flags)) {
            if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_SELFMOD) {
                /* Case 8177: If we have a flushed fragment hit a self-write, we
                 * cannot delete it in our self-write handler (b/c of case 3559's
                 * incoming links union).  But, our self-write handler needs to be
                 * nolinking and needs to check sandbox2ro_threshold.  So, we do our
                 * self-write check first, but we don't actually delete there for
                 * FRAG_WAS_DELETED fragments.
                 */
                SELF_PROTECT_LOCAL(dcontext, WRITABLE);
                /* this fragment overwrote its original memory image */
                fragment_self_write(dcontext);
                /* FIXME: optimize this to stay writable if we're going to
                 * be exiting d_r_dispatch as well -- no very quick check though
                 */
                SELF_PROTECT_LOCAL(dcontext, READONLY);
            } else if (dcontext->upcontext.upcontext.exit_reason >=
                           EXIT_REASON_FLOAT_PC_FNSAVE &&
                       dcontext->upcontext.upcontext.exit_reason <=
                           EXIT_REASON_FLOAT_PC_XSAVE64) {
                float_pc_update(dcontext);
                STATS_INC(float_pc_from_dispatch);
                /* Restore */
                dcontext->upcontext.upcontext.exit_reason = EXIT_REASON_SELFMOD;
            } else if (dcontext->upcontext.upcontext.exit_reason ==
                       EXIT_REASON_SINGLE_STEP) {
                /* Delete basic block to generate only one single step exception. */
                ASSERT(!TEST(FRAG_SHARED, dcontext->last_fragment->flags));
                fragment_delete(dcontext, dcontext->last_fragment, FRAGDEL_ALL);
                /* Restore */
                dcontext->upcontext.upcontext.exit_reason = EXIT_REASON_SELFMOD;
                /* Forge single step exception with right address. */
                os_forge_exception(dcontext->next_tag, SINGLE_STEP_EXCEPTION);
                ASSERT_NOT_REACHED();
            } else if (dcontext->upcontext.upcontext.exit_reason ==
                       EXIT_REASON_RSEQ_ABORT) {
#ifdef LINUX
                rseq_process_native_abort(dcontext);
#else
                ASSERT_NOT_REACHED();
#endif
                /* Unset the reason. */
                dcontext->upcontext.upcontext.exit_reason = EXIT_REASON_SELFMOD;
            } else {
                /* When adding any new reason, be sure to clear exit_reason,
                 * as selfmod exits do not bother to set the reason field to
                 * 0 for performance reasons (they are assumed to be more common
                 * than any other "special exit").
                 */
                ASSERT_NOT_REACHED();
            }
        }
    }

    /* make sure to tell flushers that we are now going to be mucking
     * with link info
     */
    if (!enter_couldbelinking(dcontext, dcontext->last_fragment, true)) {
        LOG(THREAD, LOG_DISPATCH, 2, "Just flushed last_fragment\n");
        /* last_fragment flushed, but cannot access here to copy it
         * to fake linkstub_t, so assert that callee did (either when freeing or
         * when noticing pending deletion flag)
         */
        ASSERT(LINKSTUB_FAKE(dcontext->last_exit));
    }

    if (wherewasi != DR_WHERE_APP) { /* if not first entrance */
        /* now fully process the last cache exit as couldbelinking */
        dispatch_exit_fcache(dcontext);
    }
}

/* Processing of the last exit from the cache.
 * Invariant: dcontext->last_exit != NULL, though it may be a sentinel (see below).
 *
 * Note that the last exit and its owning fragment may be _fake_, i.e., just
 * a copy of the key fields we typically check, for the following cases:
 *   - last fragment was flushed: fully deleted at cache exit synch point
 *   - last fragment was deleted since it overwrote itself (selfmod)
 *   - last fragment was deleted since it was a private trace building copy
 *   - last fragment was deleted for other reasons?!?
 *   - briefly during trace emitting, nobody should care though
 *   - coarse grain fragment exits, for which we have no linkstub_t or other
 *     extraneous bookkeeping
 *
 * For some cases we do not currently keep the key fields at all:
 *   - last fragment was flushed: detected at write fault
 * And some times we are unable to keep the key fields:
 *   - last fragment was flushed: targeted in ibl via target_deleted path
 * These last two cases are the only exits from fragment for which we
 * do not know the key fields.  For the former, we exit in the middle of
 * a fragment that was already created, so not knowing does not affect
 * security policies or other checks much.  The latter is the most problematic,
 * as we have a number of checks depending on knowing the last exit when indirect.
 *
 * We have other types of exits from the cache that never involved a real
 * fragment, for which we also use fake linkstubs:
 *   - no real last fragment: system call
 *   - no real last fragment: sigreturn
 *   - no real last fragment: native_exec return
 *   - callbacks clear last_exit, but should come out of the cache at a syscall
 *       (bug 2464 was back when tried to carry last_exit through syscall)
 *       so this will end up looking like the system call case
 */
static void
dispatch_exit_fcache(dcontext_t *dcontext)
{
    /* case 7966: no distinction of islinking-ness for hotp_only & thin_client */
    ASSERT(RUNNING_WITHOUT_CODE_CACHE() || is_couldbelinking(dcontext));

#if defined(WINDOWS) && defined(DEBUG)
    if (should_swap_teb_nonstack_fields()) {
        ASSERT(!is_dynamo_address(dcontext->app_fls_data));
        ASSERT(dcontext->app_fls_data == NULL ||
               dcontext->app_fls_data != dcontext->priv_fls_data);
        ASSERT(!is_dynamo_address(dcontext->app_nt_rpc));
        ASSERT(dcontext->app_nt_rpc == NULL ||
               dcontext->app_nt_rpc != dcontext->priv_nt_rpc);
        ASSERT(!is_dynamo_address(dcontext->app_nls_cache));
        ASSERT(!is_dynamo_address(dcontext->app_stack_limit) ||
               IS_CLIENT_THREAD(dcontext));
        ASSERT(!is_dynamo_address((byte *)dcontext->app_stack_base - 1) ||
               IS_CLIENT_THREAD(dcontext));
        ASSERT((SWAP_TEB_STACKBASE() &&
                is_dynamo_address((byte *)d_r_get_tls(TOP_STACK_TIB_OFFSET) - 1)) ||
               (!SWAP_TEB_STACKBASE() &&
                !is_dynamo_address((byte *)d_r_get_tls(TOP_STACK_TIB_OFFSET) - 1)));
        ASSERT((SWAP_TEB_STACKLIMIT() &&
                is_dynamo_address(d_r_get_tls(BASE_STACK_TIB_OFFSET))) ||
               (!SWAP_TEB_STACKLIMIT() &&
                !is_dynamo_address(d_r_get_tls(BASE_STACK_TIB_OFFSET))));
        /* DrMi#1723: ensure client hitting app guard page updated TEB.StackLimit.
         * Unfortunately this does happen with fiber code that updates TEB before
         * swapping the stack in the next bb so we make it a curiosity.
         */
        ASSERT_CURIOSITY_ONCE(
            (SWAP_TEB_STACKLIMIT() &&
             get_mcontext(dcontext)->xsp >= (reg_t)dcontext->app_stack_limit) ||
            (!SWAP_TEB_STACKLIMIT() &&
             get_mcontext(dcontext)->xsp >= (reg_t)d_r_get_tls(BASE_STACK_TIB_OFFSET)));
        ASSERT(dcontext->app_nls_cache == NULL ||
               dcontext->app_nls_cache != dcontext->priv_nls_cache);
    }
    if (should_swap_teb_static_tls()) {
        ASSERT(!is_dynamo_address(dcontext->app_static_tls));
        ASSERT(dcontext->app_static_tls == NULL ||
               dcontext->app_static_tls != dcontext->priv_static_tls);
    }
#endif

    if (LINKSTUB_INDIRECT(dcontext->last_exit->flags)) {
        /* indirect branch exit processing */

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
        /* PR 204770: use trace component bb tag for RCT source address */
        app_pc src_tag = dcontext->last_fragment->tag;
        if (!LINKSTUB_FAKE(dcontext->last_exit) &&
            TEST(FRAG_IS_TRACE, dcontext->last_fragment->flags)) {
            /* FIXME: should we call this for direct exits as well, up front? */
            src_tag = get_trace_exit_component_tag(dcontext, dcontext->last_fragment,
                                                   dcontext->last_exit);
        }
#endif

#ifdef RETURN_AFTER_CALL
        /* This is the permission check for any new return target, it
         * also double checks the findings of the indirect lookup
         * routine
         */
        if (dynamo_options.ret_after_call &&
            TEST(LINK_RETURN, dcontext->last_exit->flags)) {
            /* ret_after_call will raise a security violation on failure */
            SELF_PROTECT_LOCAL(dcontext, WRITABLE);
            ret_after_call_check(dcontext, dcontext->next_tag, src_tag);
            SELF_PROTECT_LOCAL(dcontext, READONLY);
        }
#endif /* RETURN_AFTER_CALL */

#ifdef RCT_IND_BRANCH
        /* permission check for any new indirect call or jump target */
        /* we care to detect violations only if blocking or at least
         * reporting the corresponding branch types
         */
        if (TESTANY(OPTION_REPORT | OPTION_BLOCK, DYNAMO_OPTION(rct_ind_call)) ||
            TESTANY(OPTION_REPORT | OPTION_BLOCK, DYNAMO_OPTION(rct_ind_jump))) {
            if ((EXIT_IS_CALL(dcontext->last_exit->flags) &&
                 TESTANY(OPTION_REPORT | OPTION_BLOCK, DYNAMO_OPTION(rct_ind_call))) ||
                (EXIT_IS_JMP(dcontext->last_exit->flags) &&
                 TESTANY(OPTION_REPORT | OPTION_BLOCK, DYNAMO_OPTION(rct_ind_jump)))) {
                /* case 4995: current shared syscalls implementation
                 * reuses the indirect jump table and marks its
                 * fake linkstub as such.
                 */
                if (LINKSTUB_FAKE(dcontext->last_exit) /* quick check */ &&
                    IS_SHARED_SYSCALLS_LINKSTUB(dcontext->last_exit)) {
                    ASSERT(IF_WINDOWS_ELSE(DYNAMO_OPTION(shared_syscalls), false));
                    ASSERT(EXIT_IS_JMP(dcontext->last_exit->flags));
                } else {
                    /* rct_ind_branch_check will raise a security violation on failure */
                    rct_ind_branch_check(dcontext, dcontext->next_tag, src_tag);
                }
            }
        }
#endif /* RCT_IND_BRANCH */

        /* Update IBL target tables for any indirect branch exit.
         * Do not bother to try to update on an exit due to a signal
         * (so signals_pending>0; for <0 we're in the handler).
         */
        if (IF_UNIX_ELSE(dcontext->signals_pending <= 0, true)) {
            SELF_PROTECT_LOCAL(dcontext, WRITABLE);
            /* update IBL target table if target is a valid IBT */
            /* FIXME: This is good for modularity but adds
             * extra lookups in the fragment table.  If it is
             * performance problem can we do it better?
             * Probably best to get bb2bb to work better and
             * not worry about optimizing DR code.
             */
            fragment_add_ibl_target(dcontext, dcontext->next_tag,
                                    extract_branchtype(dcontext->last_exit->flags));
            /* FIXME: optimize this to stay writable if we're going to
             * be building a bb as well -- no very quick check though
             */
            SELF_PROTECT_LOCAL(dcontext, READONLY);
        }
    } /* LINKSTUB_INDIRECT */
    else if (dcontext->last_exit == get_ibl_deleted_linkstub()) {
        /* We don't know which table it was, so we update all of them.  Otherwise
         * we'll keep coming back here on hits in the outdated table.
         */
        fragment_update_ibl_tables(dcontext);
    }

    /* ref bug 2323, we need monitor to restore last fragment now,
     * before we break out of the loop to build a new fragment
     * ASSUMPTION: all unusual cache exits (asynch events) abort the current
     * trace, so this is the only place we need to restore anything.
     * monitor_cache_enter() asserts that for us.
     * NOTE : we wait till after the cache exit stats and logs to call
     * monitor_cache_exit since it might change the flags of the last
     * fragment and screw up the stats
     */
    monitor_cache_exit(dcontext);

#ifdef SIDELINE
    /* sideline synchronization */
    if (dynamo_options.sideline) {
        thread_id_t tid = d_r_get_thread_id();
        if (pause_for_sideline == tid) {
            d_r_mutex_lock(&sideline_lock);
            if (pause_for_sideline == tid) {
                LOG(THREAD, LOG_DISPATCH | LOG_THREADS | LOG_SIDELINE, 2,
                    "Thread %d waiting for sideline thread\n", tid);
                signal_event(paused_for_sideline_event);
                STATS_INC(num_wait_sideline);
                wait_for_event(resume_from_sideline_event, 0);
                d_r_mutex_unlock(&sideline_lock);
                LOG(THREAD, LOG_DISPATCH | LOG_THREADS | LOG_SIDELINE, 2,
                    "Thread %d resuming after sideline thread\n", tid);
                sideline_cleanup_replacement(dcontext);
            } else
                d_r_mutex_unlock(&sideline_lock);
        }
    }
#endif

#ifdef UNIX
    if (dcontext->signals_pending != 0) {
        /* XXX: We can overflow the app stack if we stack up too many signals
         * by interrupting prev handlers -- exacerbated by RAC lack of
         * caching (case 1858), which causes a cache exit prior to
         * executing every single sigreturn!
         */
        receive_pending_signal(dcontext);
    }
#endif

    /* is ok to put the lock after the null check, this is only
     * place they can be deleted
     */
    if (dcontext->client_data != NULL && dcontext->client_data->to_do != NULL) {
        client_todo_list_t *todo;
        /* FIXME PR 200409: we're removing all API routines that use this
         * todo list so we should never get here
         */
        if (SHARED_FRAGMENTS_ENABLED()) {
            USAGE_ERROR("dr_{delete,replace}_fragment() are incompatible with "
                        "-shared_{bbs,traces} at this time");
        }
        d_r_mutex_lock(&(dcontext->client_data->sideline_mutex));
        todo = dcontext->client_data->to_do;
        while (todo != NULL) {
            client_todo_list_t *next_todo = todo->next;
            fragment_t *f = fragment_lookup(dcontext, todo->tag);
            if (f != NULL) {
                if (todo->ilist != NULL) {
                    /* doing a replacement */
                    fragment_t *new_f;
                    uint orig_flags = f->flags;
                    void *vmlist = NULL;
                    DEBUG_DECLARE(bool ok;)
                    LOG(THREAD, LOG_INTERP, 3,
                        "Going to do a client fragment replacement at " PFX "  F%d\n",
                        f->tag, f->id);
                    /* prevent emit from deleting f, we still need it */
                    /* FIXME: if f is shared we must hold change_linking_lock
                     * for the flags and vm area operations here
                     */
                    ASSERT(!TEST(FRAG_SHARED, f->flags));
                    f->flags |= FRAG_CANNOT_DELETE;
                    DEBUG_DECLARE(ok =)
                    vm_area_add_to_list(dcontext, f->tag, &vmlist, orig_flags, f,
                                        false /*no locks*/);
                    ASSERT(ok); /* should never fail for private fragments */
                    d_r_mangle(dcontext, todo->ilist, &f->flags, true, true);
                    /* mangle shouldn't change the flags here */
                    ASSERT(f->flags == (orig_flags | FRAG_CANNOT_DELETE));
                    new_f = emit_invisible_fragment(dcontext, todo->tag, todo->ilist,
                                                    orig_flags, vmlist);
                    f->flags = orig_flags; /* FIXME: ditto about change_linking_lock */
                    instrlist_clear_and_destroy(dcontext, todo->ilist);
                    fragment_copy_data_fields(dcontext, f, new_f);
                    shift_links_to_new_fragment(dcontext, f, new_f);
                    fragment_replace(dcontext, f, new_f);
                    DOLOG(2, LOG_INTERP, {
                        LOG(THREAD, LOG_INTERP, 3,
                            "Finished emitting replacement fragment %d\n", new_f->id);
                        disassemble_fragment(dcontext, new_f, d_r_stats->loglevel < 3);
                    });
                }
                /* delete [old] fragment */
                if ((f->flags & FRAG_CANNOT_DELETE) == 0) {
                    uint actions;
                    LOG(THREAD, LOG_INTERP, 3, "Client deleting old F%d\n", f->id);
                    if (todo->ilist != NULL) {
                        /* for the fragment replacement case, the fragment should
                         * already be unlinked and removed from the hash table.
                         */
                        actions = FRAGDEL_NO_UNLINK | FRAGDEL_NO_HTABLE;
                    } else {
                        actions = FRAGDEL_ALL;
                    }
                    fragment_delete(dcontext, f, actions);

                    STATS_INC(num_fragments_deleted_client);
                } else {
                    LOG(THREAD, LOG_INTERP, 2, "Couldn't let client delete F%d\n", f->id);
                }
            } else {
                LOG(THREAD, LOG_INTERP, 2,
                    "Failed to delete/replace fragment at tag " PFX
                    " because was already "
                    "deleted\n",
                    todo->tag);
            }

            HEAP_TYPE_FREE(dcontext, todo, client_todo_list_t, ACCT_CLIENT, PROTECTED);
            todo = next_todo;
        }
        dcontext->client_data->to_do = NULL;
        d_r_mutex_unlock(&(dcontext->client_data->sideline_mutex));
    }
}

/* stats and logs on why we exited the code cache */
static void
dispatch_exit_fcache_stats(dcontext_t *dcontext)
{
#if defined(DEBUG) || defined(KSTATS)
    fragment_t *next_f;
    fragment_t *last_f;
    fragment_t coarse_f;
#endif

#ifdef PROFILE_RDTSC
    if (dynamo_options.profile_times) {
        int i, j;
        uint64 end_time, total_time;
        profile_fragment_dispatch(dcontext);

        /* top ten cache times */
        end_time = get_time();
        total_time = end_time - dcontext->cache_enter_time;
        for (i = 0; i < 10; i++) {
            if (total_time > dcontext->cache_time[i]) {
                /* insert */
                for (j = 9; j > i; j--) {
                    dcontext->cache_time[j] = dcontext->cache_time[j - 1];
                    dcontext->cache_count[j] = dcontext->cache_count[j - 1];
                }
                dcontext->cache_time[i] = total_time;
                dcontext->cache_count[i] = dcontext->cache_frag_count;
                break;
            }
        }
    }
#endif

    /* A count of cache exits is a useful enough metric to gauge performance
     * problems that we pay for a counter in release build.
     */
    RSTATS_INC(num_exits);
#if defined(DEBUG) || defined(KSTATS)
    ASSERT(dcontext->last_exit != NULL);

    /* special exits that aren't from real fragments */

    if (dcontext->last_exit == get_syscall_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from system call\n");
        STATS_INC(num_exits_syscalls);
        /* PR 356503: clients using libraries that make syscalls, invoked from
         * a clean call, will not trigger the whereami check below: so we
         * locate here via mismatching kstat top-of-stack.
         */
        KSTAT_THREAD(fcache_default, {
            if (ks->node[ks->depth - 1].var == pv) {
                found_client_sysenter();
            }
        });
        KSTOP_NOT_PROPAGATED(syscall_fcache);
        return;
    } else if (dcontext->last_exit == get_selfmod_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from fragment via code mod\n");
        STATS_INC(num_exits_code_mod_flush);
        KSWITCH_STOP_NOT_PROPAGATED(fcache_default);
        return;
    } else if (dcontext->last_exit == get_ibl_deleted_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from fragment deleted but hit in ibl\n");
        STATS_INC(num_exits_ibl_deleted);
        KSWITCH_STOP_NOT_PROPAGATED(fcache_default);
        return;
    } else if (dcontext->last_exit == get_asynch_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from asynch event\n");
        STATS_INC(num_exits_asynch);
        /* w/ -shared_syscalls can also be a fragment kstart */
        KSTOP_NOT_MATCHING_NOT_PROPAGATED(syscall_fcache);
        return;
    } else if (dcontext->last_exit == get_native_exec_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from native_exec execution\n");
        STATS_INC(num_exits_native_exec);
        /* may be a quite large kstat count */
        KSWITCH_STOP_NOT_PROPAGATED(native_exec_fcache);
        return;
    } else if (dcontext->last_exit == get_native_exec_syscall_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from native_exec syscall trampoline\n");
        STATS_INC(num_exits_native_exec_syscall);
        /* may be a quite large kstat count */
#    if defined(DEBUG) || defined(KSTATS)
        /* Being native for the start/stop API is different from native_exec:
         * the former has the kstack cleared, so there's nothing to stop here
         * (xref i#813, i#1140).
         */
        if (dcontext->currently_stopped)
            LOG(THREAD, LOG_DISPATCH, 2, "Thread is start/stop native\n");
        else
            KSWITCH_STOP_NOT_PROPAGATED(native_exec_fcache);
#    endif
        return;
    } else if (dcontext->last_exit == get_reset_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit due to %s\n",
            dcontext->go_native ? "request to go native" : "proactive reset");
        DOSTATS({
            if (dcontext->go_native)
                STATS_INC(num_exits_native);
            else
                STATS_INC(num_exits_reset);
        });
        KSWITCH_STOP_NOT_PROPAGATED(fcache_default);
        return;
    }
#    ifdef WINDOWS
    else if (IS_SHARED_SYSCALLS_UNLINKED_LINKSTUB(dcontext->last_exit)) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from unlinked shared syscall\n");
        STATS_INC(num_unlinked_shared_syscalls_exits);
        KSWITCH_STOP_NOT_PROPAGATED(fcache_default);
        return;
    } else if (IS_SHARED_SYSCALLS_LINKSTUB(dcontext->last_exit)) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from shared syscall (%s)\n",
            IS_SHARED_SYSCALLS_TRACE_LINKSTUB(dcontext->last_exit) ? "trace" : "bb");
        DOSTATS({
            if (IS_SHARED_SYSCALLS_TRACE_LINKSTUB(dcontext->last_exit))
                STATS_INC(num_shared_syscalls_trace_exits);
            else
                STATS_INC(num_shared_syscalls_bb_exits);
        });
        KSWITCH_STOP_NOT_PROPAGATED(fcache_default);
        return;
    }
#    endif
#    ifdef HOT_PATCHING_INTERFACE
    else if (dcontext->last_exit == get_hot_patch_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from hot patch routine\n");
        STATS_INC(num_exits_hot_patch);
        KSWITCH_STOP_NOT_PROPAGATED(fcache_default);
        return;
    }
#    endif
    else if (dcontext->last_exit == get_client_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from client redirection\n");
        STATS_INC(num_exits_client_redirect);
        KSWITCH_STOP_NOT_PROPAGATED(fcache_default);
        return;
    }

    /* normal exits from real fragments, though the last_fragment may
     * be deleted and we are working off a copy of its important fields
     */

    /* FIXME: this lookup is needed for KSTATS and STATS_*. STATS_* are only
     * printed at loglevel 1, but maintained at loglevel 0, and if
     * we want an external agent to examine them at 0 we will want
     * to keep this...leaving for now
     */
    next_f = fragment_lookup_fine_and_coarse(dcontext, dcontext->next_tag, &coarse_f,
                                             dcontext->last_exit);
    last_f = dcontext->last_fragment;

    DOKSTATS({
        /* FIXME (case 4988): read top of kstats stack to get src
         * type, and then split by last_fragment type as well
         */
        KSWITCH_STOP_NOT_PROPAGATED(fcache_default);
    });

    if (is_ibl_sourceless_linkstub((const linkstub_t *)dcontext->last_exit)) {
        if (DYNAMO_OPTION(coarse_units)) {
            LOG(THREAD, LOG_DISPATCH, 2, "Exit from coarse ibl from tag " PFX ": %s %s",
                dcontext->coarse_exit.src_tag,
                TEST(FRAG_IS_TRACE, last_f->flags) ? "trace" : "bb",
                TEST(LINK_RETURN, dcontext->last_exit->flags)  ? "ret"
                    : EXIT_IS_CALL(dcontext->last_exit->flags) ? "call*"
                                                               : "jmp*");
        } else {
            /* We can get here for -indirect_stubs via client special ibl */
            LOG(THREAD, LOG_DISPATCH, 2, "Exit from sourceless ibl: %s %s",
                TEST(FRAG_IS_TRACE, last_f->flags) ? "trace" : "bb",
                TEST(LINK_RETURN, dcontext->last_exit->flags)  ? "ret"
                    : EXIT_IS_CALL(dcontext->last_exit->flags) ? "call*"
                                                               : "jmp*");
        }
    } else if (dcontext->last_exit == get_coarse_exit_linkstub()) {
        DOLOG(2, LOG_DISPATCH, {
            coarse_info_t *info = dcontext->coarse_exit.dir_exit;
            cache_pc stub;
            ASSERT(info != NULL); /* though not initialized to NULL... */
            stub = coarse_stub_lookup_by_target(dcontext, info, dcontext->next_tag);
            LOG(THREAD, LOG_DISPATCH, 2,
                "Exit from sourceless coarse-grain fragment via stub " PFX "\n", stub);
        });
        /* FIXME: this stat is not mutually exclusive of reason-for-exit stats */
        STATS_INC(num_exits_coarse);
    } else if (dcontext->last_exit == get_coarse_trace_head_exit_linkstub()) {
        LOG(THREAD, LOG_DISPATCH, 2,
            "Exit from sourceless coarse-grain fragment targeting trace head");
        /* FIXME: this stat is not mutually exclusive of reason-for-exit stats */
        STATS_INC(num_exits_coarse_trace_head);
    } else {
        LOG(THREAD, LOG_DISPATCH, 2, "Exit from F%d(" PFX ")." PFX, last_f->id,
            last_f->tag, EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit));
    }

    DOSTATS({
        if (TEST(FRAG_IS_TRACE, last_f->flags))
            STATS_INC(num_trace_exits);
        else
            STATS_INC(num_bb_exits);
    });

    LOG(THREAD, LOG_DISPATCH, 2, " %s%s",
        IF_X86_ELSE(IF_X64_ELSE(FRAG_IS_32(last_f->flags) ? "(32-bit)" : "", ""),
                    IF_ARM_ELSE(FRAG_IS_THUMB(last_f->flags) ? "(T32)" : "(A32)", "")),
        TEST(FRAG_SHARED, last_f->flags) ? "(shared)" : "");
    DOLOG(2, LOG_SYMBOLS, {
        char symbuf[MAXIMUM_SYMBOL_LENGTH];
        print_symbolic_address(last_f->tag, symbuf, sizeof(symbuf), true);
        LOG(THREAD, LOG_SYMBOLS, 2, "\t%s\n", symbuf);
    });

#    if defined(DEBUG) && defined(DGC_DIAGNOSTICS)
    if (TEST(FRAG_DYNGEN, last_f->flags) && !is_dyngen_vsyscall(last_f->tag)) {
        char buf[MAXIMUM_SYMBOL_LENGTH];
        bool stack = is_address_on_stack(dcontext, last_f->tag);
        app_pc translated_pc;
        print_symbolic_address(dcontext->next_tag, buf, sizeof(buf), false);
        LOG(THREAD, LOG_DISPATCH, 1,
            "Exit from dyngen F%d(" PFX "%s%s) w/ %s targeting " PFX " %s:", last_f->id,
            last_f->tag, stack ? " stack" : "",
            (last_f->flags & FRAG_DYNGEN_RESTRICTED) != 0 ? " BAD" : "",
            LINKSTUB_DIRECT(dcontext->last_exit->flags) ? "db" : "ib", dcontext->next_tag,
            buf);
        /* FIXME: risky if last fragment is deleted -- should check for that
         * here and instead just print type from last_exit, since recreate
         * may fail
         */
        translated_pc = recreate_app_pc(
            dcontext, EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit),
            dcontext->last_fragment);
        if (translated_pc != NULL) {
            disassemble(dcontext, translated_pc, THREAD);
            LOG(THREAD, LOG_DISPATCH, 1, "\n");
        }
        DOLOG(stack ? 1U : 2U, LOG_DISPATCH, {
            LOG(THREAD, LOG_DISPATCH, 1, "DGC bb:\n");
            disassemble_app_bb(dcontext, last_f->tag, THREAD);
        });
    }
#    endif /* defined(DEBUG) && defined(DGC_DIAGNOSTICS) */

#    ifdef UNIX
    if (dcontext->signals_pending > 0) {
        /* this may not always be the reason...the interrupted fragment
         * field is modularly hidden in unix/signal.c though
         */
        LOG(THREAD, LOG_DISPATCH, 2, " (interrupted by delayable signal)");
        STATS_INC(num_exits_dir_signal);
    } else
#    endif
        if (LINKSTUB_INDIRECT(dcontext->last_exit->flags)) {
#    ifdef RETURN_AFTER_CALL
        bool ok = false;
#    endif
        STATS_INC(num_exits_ind_total);
        if (next_f == NULL) {
            LOG(THREAD, LOG_DISPATCH, 2, " (target " PFX " not in cache)",
                dcontext->next_tag);
            STATS_INC(num_exits_ind_good_miss);
            KSWITCH(num_exits_ind_good_miss);
        } else if (is_building_trace(dcontext) &&
                   !TEST(LINK_LINKED, dcontext->last_exit->flags)) {
            LOG(THREAD, LOG_DISPATCH, 2, " (in trace-building mode)");
            STATS_INC(num_exits_ind_trace_build);
        } else if (TEST(FRAG_WAS_DELETED, last_f->flags) || !INTERNAL_OPTION(link_ibl)) {
            LOG(THREAD, LOG_DISPATCH, 2, " (src unlinked)");
            STATS_INC(num_exits_ind_src_unlinked);
        } else {
            LOG(THREAD, LOG_DISPATCH, 2,
                " (target " PFX " in cache but not lookup table)", dcontext->next_tag);
            STATS_INC(num_exits_ind_bad_miss);

            if (TEST(FRAG_IS_TRACE, last_f->flags)) {
                STATS_INC(num_exits_ind_bad_miss_trace);
                if (next_f && TEST(FRAG_IS_TRACE, next_f->flags)) {
                    STATS_INC(num_exits_ind_bad_miss_trace2trace);
                    KSWITCH(num_exits_ind_bad_miss_trace2trace);
                } else if (next_f && !TEST(FRAG_IS_TRACE, next_f->flags)) {
                    if (!TEST(FRAG_IS_TRACE_HEAD, next_f->flags)) {
                        STATS_INC(num_exits_ind_bad_miss_trace2bb_nth);
                        KSWITCH(num_exits_ind_bad_miss_trace2bb_nth);
                    } else {
                        STATS_INC(num_exits_ind_bad_miss_trace2bb_th);
                        KSWITCH(num_exits_ind_bad_miss_trace2bb_th);
                    }
                }
            } else {
                STATS_INC(num_exits_ind_bad_miss_bb);
                if (next_f && TEST(FRAG_IS_TRACE, next_f->flags)) {
                    STATS_INC(num_exits_ind_bad_miss_bb2trace);
                    KSWITCH(num_exits_ind_bad_miss_bb2trace);
                } else if (next_f && !TEST(FRAG_IS_TRACE, next_f->flags)) {
                    DOSTATS({
                        if (TEST(FRAG_IS_TRACE_HEAD, next_f->flags))
                            STATS_INC(num_exits_ind_bad_miss_bb2bb_th);
                    });
                    STATS_INC(num_exits_ind_bad_miss_bb2bb);
                    KSWITCH(num_exits_ind_bad_miss_bb2bb);
                }
            }
        }
        DOSTATS({
            if (!TEST(FRAG_IS_TRACE, last_f->flags))
                STATS_INC(num_exits_ind_non_trace);
        });
#    ifdef RETURN_AFTER_CALL
        /* split by ind branch type */
        if (TEST(LINK_RETURN, dcontext->last_exit->flags)) {
            LOG(THREAD, LOG_DISPATCH, 2, " (return from " PFX " non-trace tgt " PFX ")",
                EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit),
                dcontext->next_tag);
            STATS_INC(num_exits_ret);
            DOSTATS({
                if (TEST(FRAG_IS_TRACE, last_f->flags))
                    STATS_INC(num_exits_ret_trace);
            });
        } else if (TESTANY(LINK_CALL | LINK_JMP, dcontext->last_exit->flags)) {
            LOG(THREAD, LOG_DISPATCH, 2, " (ind %s from " PFX " non-trace tgt " PFX ")",
                EXIT_IS_CALL(dcontext->last_exit->flags) ? "call" : "jmp",
                EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit),
                dcontext->next_tag);
            DOSTATS({
                if (EXIT_IS_CALL(dcontext->last_exit->flags)) {
                    STATS_INC(num_exits_ind_call);
                } else if (EXIT_IS_JMP(dcontext->last_exit->flags)) {
                    STATS_INC(num_exits_ind_jmp);
                } else
                    ASSERT_NOT_REACHED();
            });
        } else if (!ok) {
            LOG(THREAD, LOG_DISPATCH, 2,
                "WARNING: unknown indirect exit from " PFX ", in %s fragment " PFX,
                EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit),
                (TEST(FRAG_IS_TRACE, last_f->flags)) ? "trace" : "bb", last_f);
            STATS_INC(num_exits_ind_unknown);
            ASSERT_NOT_REACHED();
        }
#    endif   /* RETURN_AFTER_CALL */
    } else { /* DIRECT LINK */
        ASSERT(LINKSTUB_DIRECT(dcontext->last_exit->flags) ||
               IS_COARSE_LINKSTUB(dcontext->last_exit));

        if (exited_due_to_ni_syscall(dcontext)) {
            LOG(THREAD, LOG_DISPATCH, 2, " (block ends with syscall)");
            STATS_INC(num_exits_dir_syscall);
            /* FIXME: it doesn't matter whether next_f exists or not we're still in "
             * a syscall
             */
            KSWITCH(num_exits_dir_syscall);
        }
#    ifdef WINDOWS
        else if (TEST(LINK_CALLBACK_RETURN, dcontext->last_exit->flags)) {
            LOG(THREAD, LOG_DISPATCH, 2, " (block ends with callback return)");
            STATS_INC(num_exits_dir_cbret);
        }
#    endif
        else if (next_f == NULL) {
            LOG(THREAD, LOG_DISPATCH, 2, " (target " PFX " not in cache)",
                dcontext->next_tag);
            STATS_INC(num_exits_dir_miss);
            KSWITCH(num_exits_dir_miss);
        }
        /* for SHARED_FRAGMENTS_ENABLED(), we do not grab the change_linking_lock
         * for our is_linkable call since that leads to a lot of
         * contention (and we don't want to go to a read-write model
         * when most uses, and all non-debug uses, are writes!).
         * instead, since we don't want to change state, we have no synch
         * at all, which is ok since the state could have changed anyway
         * (see comment at end of cases below)
         */
#    ifdef DEBUG
        else if (IS_COARSE_LINKSTUB(dcontext->last_exit)) {
            LOG(THREAD, LOG_DISPATCH, 2, " (not lazily linked yet)");
        } else if (!is_linkable(dcontext, dcontext->last_fragment, dcontext->last_exit,
                                next_f, false /*don't own link lock*/,
                                false /*do not change trace head state*/)) {
            STATS_INC(num_exits_dir_nolink);
            LOG(THREAD, LOG_DISPATCH, 2, " (cannot link F%d->F%d)", last_f->id,
                next_f->id);
            if (is_building_trace(dcontext) &&
                !TEST(LINK_LINKED, dcontext->last_exit->flags)) {
                LOG(THREAD, LOG_DISPATCH, 2, " (in trace-building mode)");
                STATS_INC(num_exits_dir_trace_build);
            }
#        ifndef TRACE_HEAD_CACHE_INCR
            else if (TEST(FRAG_IS_TRACE_HEAD, next_f->flags)) {
                LOG(THREAD, LOG_DISPATCH, 2, " (target F%d is trace head)", next_f->id);
                STATS_INC(num_exits_dir_trace_head);
            }
#        endif
            else if ((last_f->flags & FRAG_SHARED) != (next_f->flags & FRAG_SHARED)) {
                LOG(THREAD, LOG_DISPATCH, 2, " (cannot link shared to private)",
                    last_f->id, next_f->id);
                STATS_INC(num_exits_dir_nolink_sharing);
            }
#        ifdef DGC_DIAGNOSTICS
            else if ((next_f->flags & FRAG_DYNGEN) != (last_f->flags & FRAG_DYNGEN)) {
                LOG(THREAD, LOG_DISPATCH, 2, " (cannot link DGC to non-DGC)", last_f->id,
                    next_f->id);
            }
#        endif
            else if (INTERNAL_OPTION(nolink)) {
                LOG(THREAD, LOG_DISPATCH, 2, " (nolink option is on)");
            } else if (!TEST(FRAG_LINKED_OUTGOING, last_f->flags)) {
                LOG(THREAD, LOG_DISPATCH, 2, " (F%d is unlinked-out)", last_f->id);
            } else if (!TEST(FRAG_LINKED_INCOMING, next_f->flags)) {
                LOG(THREAD, LOG_DISPATCH, 2, " (F%d is unlinked-in)", next_f->id);
            } else {
                LOG(THREAD, LOG_DISPATCH, 2, " (unknown reason)");
                /* link info could have changed after we exited cache so
                 * this is probably not a problem, not much we can do
                 * to distinguish race from real problem, so no assertion.
                 * race can happen even w/ single_thread_in_DR.
                 */
                STATS_INC(num_exits_dir_race);
            }
        }
#        ifdef TRACE_HEAD_CACHE_INCR
        else if (TEST(FRAG_IS_TRACE_HEAD, next_f->flags)) {
            LOG(THREAD, LOG_DISPATCH, 2, " (trace head F%d now hot!)", next_f->id);
            STATS_INC(num_exits_dir_trace_hot);
        }
#        endif
        else if (TEST(FRAG_IS_TRACE, next_f->flags) && TEST(FRAG_SHARED, last_f->flags)) {
            LOG(THREAD, LOG_DISPATCH, 2,
                " (shared trace head shadowed by private trace F%d)", next_f->id);
            STATS_INC(num_exits_dir_nolink_sharing);
        } else if (dcontext->next_tag == last_f->tag && next_f != last_f) {
            /* invisible emission and replacement */
            LOG(THREAD, LOG_DISPATCH, 2, " (self-loop in F%d, replaced by F%d)",
                last_f->id, next_f->id);
            STATS_INC(num_exits_dir_self_replacement);
        } else if (TEST(FRAG_COARSE_GRAIN, next_f->flags) &&
                   !TEST(FRAG_COARSE_GRAIN, last_f->flags)) {
            LOG(THREAD, LOG_DISPATCH, 2, " (fine fragment targeting coarse trace head)");
            /* FIXME: We would assert that FRAG_IS_TRACE_HEAD is set, but
             * we have no way of setting that up for fine to coarse links
             */
            /* stats are done in monitor_cache_enter() */
        } else {
            LOG(THREAD, LOG_DISPATCH, 2, " (UNKNOWN DIRECT EXIT F%d." PFX "->F%d)",
                last_f->id, EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit),
                next_f->id);
            /* link info could have changed after we exited cache so
             * this is probably not a problem, not much we can do
             * to distinguish race from real problem, so no assertion.
             * race can happen even w/ single_thread_in_DR.
             */
            STATS_INC(num_exits_dir_race);
        }
#    endif /* DEBUG */
    }
    if (dcontext->last_exit == get_deleted_linkstub(dcontext)) {
        LOG(THREAD, LOG_DISPATCH, 2, " (fragment was flushed)");
    }
    LOG(THREAD, LOG_DISPATCH, 2, "\n");
    DOLOG(5, LOG_DISPATCH,
          { dump_mcontext(get_mcontext(dcontext), THREAD, DUMP_NOT_XML); });
    DOLOG(6, LOG_DISPATCH, { dump_mcontext_callstack(dcontext); });
    DOKSTATS({ DOLOG(6, LOG_DISPATCH, { kstats_dump_stack(dcontext); }); });
#endif /* defined(DEBUG) || defined(KSTATS) */
}

/***************************************************************************
 * SYSTEM CALLS
 */

#ifdef UNIX
static void
adjust_syscall_continuation(dcontext_t *dcontext)
{
    /* PR 212570: for linux sysenter, we hooked the sysenter return-to-user-mode
     * point to go to post-do-vsyscall.  So we end up here w/o any extra
     * work pre-syscall; and since we put the hook-displaced code in the nop
     * space immediately after the sysenter instr, which is our normal
     * continuation pc, we have no work to do here either (except for
     * 4.4.8+ kernels: i#1939)!
     */

    bool syscall_method_is_syscall = get_syscall_method() == SYSCALL_METHOD_SYSCALL;

    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
#    if defined(MACOS) && defined(X86)
        if (!dcontext->sys_was_int) {
            priv_mcontext_t *mc = get_mcontext(dcontext);
            LOG(THREAD, LOG_SYSCALLS, 3,
                "post-sysenter: xdx + asynch_target => " PFX " (were " PFX ", " PFX ")\n",
                dcontext->app_xdx, mc->xdx, dcontext->asynch_target);
            mc->xdx = dcontext->app_xdx;
            dcontext->asynch_target = (app_pc)mc->xdx;
        }
#    else
        /* we still see some int syscalls (for SYS_clone in particular) */
        ASSERT(dcontext->sys_was_int ||
               dcontext->asynch_target == vsyscall_syscall_end_pc ||
               /* dr_syscall_invoke_another() hits this */
               dcontext->asynch_target == vsyscall_sysenter_displaced_pc);
        /* i#1939: we do need to adjust for 4.4.8+ kernels */
        if (!dcontext->sys_was_int && vsyscall_sysenter_displaced_pc != NULL) {
            dcontext->asynch_target = vsyscall_sysenter_displaced_pc;
            LOG(THREAD, LOG_SYSCALLS, 3, "%s: asynch_target => " PFX "\n", __FUNCTION__,
                dcontext->asynch_target);
        }
#    endif
    } else if (vsyscall_syscall_end_pc != NULL) {
        /* PR 341469: 32-bit apps (LOL64) on AMD hardware have
         * OP_syscall and OP_sysenter on Intel hardware in a vsyscall page.
         *
         * We added hook on vsyscall page, through that we manually jump to
         * sysenter/syscall return point and go to dispatch.
         *
         * We should adjust target when hardware is AMD, app is 32-bit (LOL64)
         * and system call instruction is OP_syscall.
         */
        if (IF_X86_32((syscall_method_is_syscall &&
                       cpu_info.vendor == VENDOR_AMD) ||) !syscall_method_is_syscall) {
            if (dcontext->asynch_target == vsyscall_syscall_end_pc) {
                ASSERT(vsyscall_sysenter_return_pc != NULL);
                dcontext->asynch_target = vsyscall_sysenter_return_pc;
                LOG(THREAD, LOG_SYSCALLS, 3, "%s: asynch_target => " PFX "\n",
                    __FUNCTION__, dcontext->asynch_target);
            }
        }
    }
}
#endif

/* used to execute a system call instruction in the code cache
 * dcontext->next_tag is store elsewhere and restored after the system call
 * for resumption of execution post-syscall
 */
void
handle_system_call(dcontext_t *dcontext)
{
    fcache_enter_func_t fcache_enter = get_fcache_enter_private_routine(dcontext);
    app_pc do_syscall = (app_pc)get_do_syscall_entry(dcontext);
    bool execute_syscall = true;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    int sysnum = os_normalized_sysnum((int)MCXT_SYSNUM_REG(mc), NULL, dcontext);
    app_pc saved_next_tag = dcontext->next_tag;
    bool repeat = false;
#ifdef WINDOWS
    /* make sure to ask about syscall before pre_syscall, which will swap new mc in! */
    bool use_prev_dcontext = is_cb_return_syscall(dcontext);
#elif defined(X86)
    if (TEST(LINK_NI_SYSCALL_INT, dcontext->last_exit->flags)) {
        LOG(THREAD, LOG_SYSCALLS, 2, "Using do_int_syscall\n");
        do_syscall = (app_pc)get_do_int_syscall_entry(dcontext);
        /* last_exit will be for the syscall so set a flag (could alternatively
         * set up a separate exit stub but this is simpler) */
        dcontext->sys_was_int = true;
#    ifdef VMX86_SERVER
        if (is_vmkuw_sysnum(mc->xax)) {
            /* Even w/ syscall # shift int80 => ENOSYS */
            do_syscall = get_do_vmkuw_syscall_entry(dcontext);
            LOG(THREAD, LOG_SYSCALLS, 2, "Using do_vmkuw_syscall\n");
        }
#    endif
    } else if (TEST(LINK_SPECIAL_EXIT, dcontext->last_exit->flags)) {
        if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_NI_SYSCALL_INT_0x81)
            do_syscall = (app_pc)get_do_int81_syscall_entry(dcontext);
        else {
            ASSERT(dcontext->upcontext.upcontext.exit_reason ==
                   EXIT_REASON_NI_SYSCALL_INT_0x82);
            do_syscall = (app_pc)get_do_int82_syscall_entry(dcontext);
        }
        dcontext->sys_was_int = true;
    } else {
        dcontext->sys_was_int = false;
        IF_NOT_X64(IF_VMX86(ASSERT(!is_vmkuw_sysnum(mc->xax))));
    }
#endif

    /* We invoke here rather than inside pre_syscall() primarily so we can
     * set use_prev_dcontext(), but also b/c the windows and linux uses
     * are identical.  We do want this prior to xbp-param changes for linux
     * sysenter-to-int (PR 313715) since to client this should
     * look like the original sysenter.  For Windows we could put this
     * after sysenter handling but it's not clear which is better: we'll
     * assert if client changes xsp/xdx but that's fine.
     */
    /* set pc so client can tell where syscall invoked from.
     * note that this is pc _after_ syscall instr.
     */
    get_mcontext(dcontext)->pc = get_fcache_target(dcontext);
    /* i#202: ignore native syscalls in early_inject_init() */
    if (IF_WINDOWS(dynamo_initialized &&) !instrument_pre_syscall(dcontext, sysnum)) {
        /* we won't execute post-syscall so we do not need to store
         * dcontext->sys_*
         */
        execute_syscall = false;
        LOG(THREAD, LOG_SYSCALLS, 2, "skipping syscall %d on client request\n",
            MCXT_SYSNUM_REG(mc));
    }
#ifdef WINDOWS
    /* re-set in case client changed the number */
    use_prev_dcontext = is_cb_return_syscall(dcontext);
#endif

    /* some syscalls require modifying local memory
     * FIXME: move this unprot down to those syscalls to avoid unprot-prot-unprot-prot
     * with the new clean dstack design -- though w/ shared_syscalls perhaps most
     * syscalls coming through here will need this
     */
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);

    KSWITCH(num_exits_dir_syscall); /* encapsulates syscall overhead */

    LOG(THREAD, LOG_SYSCALLS, 2,
        "Entry into do_syscall to execute a non-ignorable system call\n");
#ifdef SIDELINE
    /* clear cur-trace field so we don't think cur trace is still running */
    sideline_trace = NULL;
#endif

    /* our flushing design assumes our syscall handlers are nolinking,
     * to avoid multiple-flusher deadlocks
     */
    ASSERT(!is_couldbelinking(dcontext));

    /* we need to store the next pc since entering the fcache will clobber it
     * with the do_syscall entry point.
     * we store in a dcontext slot since some syscalls need to view or modify it
     * (the asynch ones: sigreturn, ntcontinue, etc., hence the name asynch_target).
     * Yes this works with an NtContinue being interrupted in the kernel for an APC --
     * we want to know the NtContinue target, there is no other target to remember.
     * The only problem is if a syscall that modifies asynch_target fails -- then we
     * want the old value, so we store it here.
     */
    dcontext->asynch_target = get_fcache_target(dcontext);

#ifdef WINDOWS
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        /* kernel sends control directly to 0x7ffe0304 so we need
         * to mangle the return address
         */
        /* Ref case 5461 - edx will become top of stack post-syscall */
        ASSERT(get_mcontext(dcontext)->xsp == get_mcontext(dcontext)->xdx);
#    ifdef HOT_PATCHING_INTERFACE
        /* For hotp_only, vsyscall_syscall_end_pc can be NULL as dr will never
         * interp a system call.  Also, for hotp_only, control can came here
         * from native only to do a syscall that was hooked.
         */
        ASSERT(!DYNAMO_OPTION(hotp_only) ||
               (DYNAMO_OPTION(hotp_only) &&
                dcontext->next_tag == BACK_TO_NATIVE_AFTER_SYSCALL));
#    else
        ASSERT(vsyscall_syscall_end_pc != NULL || get_os_version() >= WINDOWS_VERSION_8);
#    endif
        /* NOTE - the stack mangling must match that of intercept_nt_continue()
         * and shared_syscall as not all routines looking at the stack
         * differentiate. */
        if (dcontext->asynch_target == vsyscall_syscall_end_pc ||
            /* win8 x86 syscalls have inlined sysenter routines */
            (get_os_version() >= WINDOWS_VERSION_8 &&
             dcontext->thread_record->under_dynamo_control)) {
#    ifdef HOT_PATCHING_INTERFACE
            /* Don't expect to be here for -hotp_only */
            ASSERT_CURIOSITY(!DYNAMO_OPTION(hotp_only));
#    endif
            ASSERT(dcontext->next_tag != BACK_TO_NATIVE_AFTER_SYSCALL);
            /* currently pc is the ret after sysenter, we need it to be the return point
             * (the ret after the call to the vsyscall sysenter)
             * we do not need to keep the old asynch_target -- if we decide not to do
             * the syscall we just have to pop the retaddr
             */
            dcontext->asynch_target = *((app_pc *)get_mcontext(dcontext)->xsp);
            ASSERT(dcontext->thread_record->under_dynamo_control);
        } else {
            /* else, special case like native_exec_syscall */
            LOG(THREAD, LOG_ALL, 2, "post-sysenter target is non-vsyscall " PFX "\n",
                dcontext->asynch_target);
            ASSERT(DYNAMO_OPTION(native_exec_syscalls) &&
                   !dcontext->thread_record->under_dynamo_control);
        }
        /* FIXME A lack of write access to %esp will generate an exception
         * originating from DR though it's really an app problem (unless we
         * screwed up wildly). Should we call is_writeable(%esp) and force
         * a new UNWRITEABLE_MEMORY_EXECUTION_EXCEPTION so that we don't take
         * the blame?
         */
        if (DYNAMO_OPTION(sygate_sysenter)) {
            /* So stack looks like
             * esp +0 app_ret_addr
             *     +4 app_val1
             * for the case 5441 Sygate hack the sysenter needs to have a ret
             * address that's in ntdll.dll, but we also need to redirect
             * control back to do_syscall. So we mangle to
             * esp +0 sysenter_ret_address (ret in ntdll)
             *     +4 after_do_syscall
             * dc->sysenter_storage app_val1
             * dc->asynch_target app_ret_addr
             * After do_syscall we push app_val1 (since stack is popped twice)
             * and send control to asynch_target (implicitly doing the
             * post_sysenter ret instr).
             */
            dcontext->sysenter_storage =
                *((app_pc *)(get_mcontext(dcontext)->xsp + XSP_SZ));
            *((app_pc *)get_mcontext(dcontext)->xsp) = sysenter_ret_address;
            *((app_pc *)(get_mcontext(dcontext)->xsp + XSP_SZ)) =
                after_do_syscall_code(dcontext);
        } else {
            *((app_pc *)get_mcontext(dcontext)->xsp) = after_do_syscall_code(dcontext);
        }
    }
#endif

#if defined(MACOS) && defined(X86)
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER && !dcontext->sys_was_int) {
        /* The kernel returns control to whatever user-mode places in edx.
         * We want to put this in even if we skip the syscall as we'll still call
         * adjust_syscall_continuation for a skip.
         */
        byte *post_sysenter = after_do_syscall_addr(dcontext);
        priv_mcontext_t *mc = get_mcontext(dcontext);
        dcontext->app_xdx = mc->xdx;
        mc->xdx = (reg_t)post_sysenter;
    }
#endif

    /* first do the pre-system-call */
    if (execute_syscall && pre_system_call(dcontext)) {
        /* now do the actual syscall instruction */
#ifdef UNIX
        /* FIXME: move into some routine inside unix/?
         * if so, move #include of sys/syscall.h too
         */
        /* We use was_thread_create_syscall even though the syscall has not really
         * happened yet. This is because, for the clone3 syscall, we want to avoid
         * reading the user-provided clone args without a safe-read, so instead we
         * use the flags and sysnum that we saved in dcontext during pre_system_call.
         */
        if (was_thread_create_syscall(dcontext)) {
            /* Code for after clone is in generated code do_clone_syscall. */
            do_syscall = (app_pc)get_do_clone_syscall_entry(dcontext);
        } else if (is_sigreturn_syscall(dcontext)) {
            /* HACK: sigreturn goes straight to fcache_return, which expects
             * app eax to already be in mcontext.  pre-syscall cannot do that since
             * do_syscall needs the syscall num in eax!
             * so we have to do it here (alternative is to be like NtContinue handling
             * with a special entry point, ends up being same sort of thing as here)
             */
            /* pre-sigreturn handler put dest eax in next_tag
             * save it in sys_param1, which is not used already in pre/post
             */
            /* For clients, pre-sigreturn handler took eax after
             * client had chance to change it, so we have the proper value here.
             */
            dcontext->sys_param1 = (reg_t)dcontext->next_tag;
            LOG(THREAD, LOG_SYSCALLS, 3, "for sigreturn, set sys_param1 to " PFX "\n",
                dcontext->sys_param1);
        }
#else
        if (use_prev_dcontext) {
            /* get the current, but now swapped out, dcontext */
            dcontext_t *tmp_dcontext = dcontext;
            LOG(THREAD, LOG_SYSCALLS, 1, "handling a callback return\n");
            dcontext = get_prev_swapped_dcontext(tmp_dcontext);
            LOG(THREAD, LOG_SYSCALLS, 1, "swapped dcontext from " PFX " to " PFX "\n",
                tmp_dcontext, dcontext);
            /* we have special fcache_enter that uses different dcontext,
             * FIXME: but what if syscall fails?  need to unswap dcontexts!
             */
            fcache_enter = get_fcache_enter_indirect_routine(dcontext);
            /* avoid synch errors with d_r_dispatch -- since enter_fcache will set
             * whereami for prev dcontext, not real one!
             */
            tmp_dcontext->whereami = DR_WHERE_FCACHE;
        }
#endif

        SELF_PROTECT_LOCAL(dcontext, READONLY);

        set_at_syscall(dcontext, true);
        KSTART_DC(dcontext, syscall_fcache); /* stopped in dispatch_exit_fcache_stats */
        bool is_ignorable = ignorable_system_call(sysnum, NULL, dcontext);
        do {
#ifdef UNIX
            /* It is difficult to undo some pre-syscall handling, especially for
             * sigreturn's signal mask and clone syscalls.  We go ahead and run the
             * syscall before we deliver the signal for all non-ignorable syscalls.
             * These are nearly all non-blocking so this should not be an issue with
             * signal delay from blocking.  Sigreturn and clone will come back to
             * d_r_dispatch so there's no worry about unbounded delay.
             *
             * TODO i#6105: A signal arriving between the pre-syscall event and the
             * syscall can cause problems for clients.  We should interrupt the
             * syscall with EINTR in that case for non-ignorable syscalls.
             */
            ASSERT((!is_sigreturn_syscall(dcontext) &&
                    !was_thread_create_syscall(dcontext)) ||
                   !is_ignorable);
            if (!is_ignorable && dcontext->signals_pending > 0)
                dcontext->signals_pending = -1;
#endif
            enter_fcache(dcontext,
                         (fcache_enter_func_t)
                         /* DEFAULT_ISA_MODE as we want the ISA mode of our gencode */
                         convert_data_to_function(
                             PC_AS_JMP_TGT(DEFAULT_ISA_MODE, (app_pc)fcache_enter)),
                         PC_AS_JMP_TGT(DEFAULT_ISA_MODE, do_syscall));
#ifdef UNIX
            if (!is_ignorable && dcontext->signals_pending > 0)
                repeat = true;
            else
                break;
#endif
        } while (repeat);
#ifdef UNIX
        if (dcontext->signals_pending != 0) {
            /* i#2019: see comments in dispatch_enter_fcache() */
            KSTOP(syscall_fcache);
            dcontext->whereami = DR_WHERE_DISPATCH;
            set_at_syscall(dcontext, false);
            dcontext->next_tag = saved_next_tag;
            set_next_tag_to_prior_syscall(dcontext);
            /* This only happens for ignorable syscalls so there is no pre-syscall
             * handling to undo or worry about dupilcating when we re-attempt to
             * execute it after the app's signal handler.
             */
            LOG(THREAD, LOG_DISPATCH, 2,
                "Signal arrived in DR: aborting ignorable syscall enter; interrupted " PFX
                "\n",
                dcontext->next_tag);
            STATS_INC(num_entrances_aborted);
            trace_abort(dcontext);
            receive_pending_signal(dcontext);
        } else
#endif
            /* will handle post processing in handle_post_system_call */
            ASSERT_NOT_REACHED();
    } else {
        LOG(THREAD, LOG_DISPATCH, 2, "Skipping actual syscall invocation\n");
        /* give the client its post-syscall event since we won't be calling
         * post_system_call(), unless the client itself was the one who skipped.
         */
        if (execute_syscall) {
            instrument_post_syscall(dcontext, dcontext->sys_num);
        }
#ifdef WINDOWS
        if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
            /* decided to skip syscall -- pop retaddr, restore sysenter storage
             * (if applicable) and set next target */
            get_mcontext(dcontext)->xsp += XSP_SZ;
            if (DYNAMO_OPTION(sygate_sysenter)) {
                *((app_pc *)get_mcontext(dcontext)->xsp) = dcontext->sysenter_storage;
            }
            set_fcache_target(dcontext, dcontext->asynch_target);
        } else if (get_syscall_method() == SYSCALL_METHOD_WOW64 &&
                   get_os_version() == WINDOWS_VERSION_7) {
            /* win7 has an add 4,esp after the call* in the syscall wrapper,
             * so we need to negate it since not making the call*
             */
            get_mcontext(dcontext)->xsp -= XSP_SZ;
        }
#else
        adjust_syscall_continuation(dcontext);
        set_fcache_target(dcontext, dcontext->asynch_target);
#endif
    }
    SELF_PROTECT_LOCAL(dcontext, READONLY);
}

static void
handle_post_system_call(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    bool skip_adjust = false;

    ASSERT(!is_couldbelinking(dcontext));
    ASSERT(get_at_syscall(dcontext));

    set_at_syscall(dcontext, false);

    /* some syscalls require modifying local memory */
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);

#ifdef UNIX
    /* restore mcontext values prior to invoking instrument_post_syscall() */
    if (was_sigreturn_syscall(dcontext)) {
        /* restore app xax/r0 */
        LOG(THREAD, LOG_SYSCALLS, 3,
            "post-sigreturn: setting xax/r0/a0 to " PFX ", asynch_target=" PFX "\n",
            dcontext->sys_param1, dcontext->asynch_target);
        /* FIXME i#3544: Check if this is a proper register to use */
        mc->IF_X86_ELSE(xax, IF_RISCV64_ELSE(a0, r0)) = dcontext->sys_param1;
#    ifdef MACOS
        /* We need to skip the use app_xdx, as we've changed the context.
         * We can't just set app_xdx from handle_sigreturn() as the
         * pre-sysenter code clobbers app_xdx, and we want to handle
         * a failed SYS_sigreturn.
         */
        skip_adjust = true;
#    endif
    }
#endif
    /* i#1661: ensure we set the right pc for dr_get_mcontext() */
    get_mcontext(dcontext)->pc = dcontext->asynch_target;

    post_system_call(dcontext);

    /* restore state for continuation in instruction after syscall */
    /* FIXME: need to handle syscall failure -- those that clobbered asynch_target
     * need to restore it to its previous value, which has to be stored somewhere!
     */
#ifdef WINDOWS
    if (DYNAMO_OPTION(sygate_sysenter) &&
        get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        /* restore sysenter_storage, note stack was popped twice for
         * syscall so need to push the value */
        get_mcontext(dcontext)->xsp -= XSP_SZ;
        *((app_pc *)get_mcontext(dcontext)->xsp) = dcontext->sysenter_storage;
    }
#else
    if (!skip_adjust)
        adjust_syscall_continuation(dcontext);
#endif
    set_fcache_target(dcontext, dcontext->asynch_target);
#ifdef WINDOWS
    /* We no longer need asynch_target so zero it out. Other pieces of DR
     * -- callback & APC handling, detach -- test asynch_target to determine
     * where the next app pc to execute is stored. If asynch_target != 0,
     * it holds the value, else it's in the esi slot.
     */
    dcontext->asynch_target = 0;
#endif

    LOG(THREAD, LOG_SYSCALLS, 3, "finished handling system call\n");

    SELF_PROTECT_LOCAL(dcontext, READONLY);
    /* caller will go back to couldbelinking status */
}

#ifdef WINDOWS
/* in callback.c */
extern void
callback_start_return(priv_mcontext_t *mc);
/* used to execute an int 2b instruction in code cache */
static void
handle_callback_return(dcontext_t *dcontext)
{
    dcontext_t *prev_dcontext;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    fcache_enter_func_t fcache_enter = get_fcache_enter_indirect_routine(dcontext);
    LOG(THREAD, LOG_ASYNCH, 3, "handling a callback return\n");
    /* may have to abort trace -> local heap */
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);
    KSWITCH(num_exits_dir_cbret);
    callback_start_return(mc);
    /* get the current, but now swapped out, dcontext */
    prev_dcontext = get_prev_swapped_dcontext(dcontext);
    SELF_PROTECT_LOCAL(dcontext, READONLY);

    /* obey flushing protocol, plus set whereami (both using real dcontext) */
    dcontext->whereami = DR_WHERE_FCACHE;
    set_at_syscall(dcontext, true); /* will be set to false on other end's post-syscall */
    ASSERT(!is_couldbelinking(dcontext));

    /* if we get an APC it should be after returning to prev cxt, so don't need
     * to worry about asynch_target
     */

    /* make sure set the next_tag of prev_dcontext, not dcontext! */
    set_fcache_target(prev_dcontext, (app_pc)get_do_callback_return_entry(prev_dcontext));
    DOLOG(4, LOG_ASYNCH, {
        LOG(THREAD, LOG_ASYNCH, 3, "passing prev dcontext " PFX ", next_tag " PFX ":\n",
            prev_dcontext, prev_dcontext->next_tag);
        dump_mcontext(get_mcontext(prev_dcontext), THREAD, DUMP_NOT_XML);
    });
    /* make sure to pass prev_dcontext, this is a special fcache enter routine
     * that indirects through the dcontext passed to it (so ignores the switch-to
     * dcontext that callback_start_return swapped into the main dcontext)
     */
    KSTART_DC(dcontext, syscall_fcache); /* continue the interrupted syscall handling */
    (*fcache_enter)(prev_dcontext);
    /* callback return does not return to here! */
    DOLOG(1, LOG_ASYNCH, {
        LOG(THREAD, LOG_SYSCALLS, 1, "ERROR: int 2b returned!\n");
        dump_mcontext(get_mcontext(dcontext), THREAD, DUMP_NOT_XML);
    });
    ASSERT_NOT_REACHED();
}
#endif /* WINDOWS */

/* used to execute a system call instruction in code cache
 * not expected to return
 * caller must set up mcontext with proper system call number and arguments
 */
void
issue_last_system_call_from_app(dcontext_t *dcontext)
{
    LOG(THREAD, LOG_SYSCALLS, 2, "issue_last_system_call_from_app(" PIFX ")\n",
        MCXT_SYSNUM_REG(get_mcontext(dcontext)));

    /* it's up to the caller to let go of the bb building lock if it was held
     * on this path, since not all paths to here hold it
     */

    if (is_couldbelinking(dcontext))
        enter_nolinking(dcontext, NULL, true);
    KSTART(syscall_fcache); /* stopped in dispatch_exit_fcache_stats */
    enter_fcache(
        dcontext,
        (fcache_enter_func_t)
        /* DEFAULT_ISA_MODE as we want the ISA mode of our gencode */
        convert_data_to_function(PC_AS_JMP_TGT(
            DEFAULT_ISA_MODE, (app_pc)get_fcache_enter_private_routine(dcontext))),
        PC_AS_JMP_TGT(DEFAULT_ISA_MODE, get_global_do_syscall_entry()));
    ASSERT_NOT_REACHED();
}

/* Stores the register parameters into the mcontext and calls d_r_dispatch.
 * Checks whether currently on d_r_initstack and if so clears the initstack_mutex.
 * Does not return.
 */
void
transfer_to_dispatch(dcontext_t *dcontext, priv_mcontext_t *mc, bool full_DR_state)
{
    app_pc cur_xsp;
    bool using_initstack = false;
    copy_mcontext(mc, get_mcontext(dcontext));
    GET_STACK_PTR(cur_xsp);
    if (is_on_initstack(cur_xsp))
        using_initstack = true;
#ifdef WINDOWS
    /* i#249: swap PEB pointers unless already in DR state */
    if (!full_DR_state)
        swap_peb_pointer(dcontext, true /*to priv*/);
#endif
    LOG(THREAD, LOG_ASYNCH, 2,
        "transfer_to_dispatch: pc=0x%08x, xsp=" PFX ", on-initstack=%d\n",
        dcontext->next_tag, mc->xsp, using_initstack);

    /* next, want to switch to dstack, and if using d_r_initstack, free mutex.
     * finally, call d_r_dispatch(dcontext).
     * note that we switch to the base of dstack, deliberately squashing
     * what may have been there before, for both new dcontext and reuse dcontext
     * options.
     */
    call_switch_stack(dcontext, dcontext->dstack, (void (*)(void *))d_r_dispatch,
                      using_initstack ? &initstack_mutex : NULL,
                      false /*do not return on error*/);
    ASSERT_NOT_REACHED();
}
