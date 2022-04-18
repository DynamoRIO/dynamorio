/* **********************************************************
 * Copyright (c) 2018-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"
#ifdef UNIX
#    include <sys/time.h>
#    ifdef MACOS
#        include <sys/syscall.h>
#    else
#        include <syscall.h>
#    endif
#else
#    error NYI
#endif

/* We do not synchronize access because we assume the itimer is shared
 * for the thread group, there's just one thread group, and the itimer
 * signal is blocked while in the handler.
 */
static int buckets[DR_WHERE_LAST];

/* Currently only written to but we may add checks on this once i#4669 failures
 * are fixed.
 */
static int xl8_failures;

/* test PR 368737: add client timer support */
static void
event_timer(void *drcontext, dr_mcontext_t *mcontext)
{
    dr_fprintf(STDERR, "client event_timer fired\n");
}

/* Test i#140: add client pc sampling support */
static void
event_sample(void *drcontext, dr_mcontext_t *mcontext)
{
    void *tag;
    dr_where_am_i_t whereami = dr_where_am_i(drcontext, mcontext->pc, &tag);
    buckets[whereami]++;
    DR_ASSERT(mcontext->pc != NULL &&
              /* Ensure DR wrote the value and it's not the uninit 0xab pattern. */
              mcontext->pc != (app_pc)IF_X64_ELSE(0xabababababababab, 0xabababab));
#if VERBOSE
    dr_fprintf(STDERR, "sample: %p %d %p\n", mcontext->pc, whereami, tag);
#endif
    if (whereami == DR_WHERE_FCACHE) {
        /* Ask for translation to test i#4669. */
        app_pc xl8 = dr_app_pc_from_cache_pc(mcontext->pc);
        if (xl8 == NULL)
            ++xl8_failures;
    }
}

static void
post_syscall_event(void *drcontext, int sysnum)
{
    if (sysnum != SYS_setitimer)
        return;
    /* Test i#2805: now that the app's alarm is set up, we want to try to hit the
     * race window where a signal enters record_pending_signal() while the thread
     * is marked as a safe spot yet holds its synch_lock in the middle of
     * dr_mark_safe_to_suspend(,false)).  If we hit the window, without the
     * proper i#2805 fix, we see a hang (or rank order violations in debug).
     */
    static volatile bool test_i2805;
    if (!test_i2805) {
        int i;
#define TEST_ITERS 500000
        test_i2805 = true;
        for (i = 0; i < TEST_ITERS; ++i) {
            dr_mark_safe_to_suspend(dr_get_current_drcontext(), true);
            dr_mark_safe_to_suspend(dr_get_current_drcontext(), false);
        }
    }
}

static bool
filter_syscall_event(void *drcontext, int sysnum)
{
    return sysnum == SYS_setitimer;
}

static void
exit_event(void)
{
    int i;
    int total = 0;
    for (i = 0; i < DR_WHERE_LAST; i++) {
        total += buckets[i];
#if VERBOSE
        dr_fprintf(STDERR, "bucket %d: %d\n", i, buckets[i]);
#endif
    }
    DR_ASSERT(total > 0);
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_register_exit_event(exit_event);
    dr_register_post_syscall_event(post_syscall_event);
    dr_register_filter_syscall_event(filter_syscall_event);
    dr_track_where_am_i();
    DR_ASSERT(dr_is_tracking_where_am_i());
    if (!dr_set_itimer(ITIMER_REAL, 25, event_timer))
        dr_fprintf(STDERR, "unable to set timer callback\n");
    /* Test pc sampling (i#140). */
    if (!dr_set_itimer(ITIMER_VIRTUAL, 10, event_sample))
        dr_fprintf(STDERR, "unable to set timer callback\n");
}
