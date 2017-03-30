/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "configure.h"
#ifndef UNIX
# error UNIX-only
#endif
#include "dr_api.h"
#include "tools.h"
#include <pthread.h>
#include "condvar.h"
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/time.h> /* itimer */

#define ALT_STACK_SIZE  (SIGSTKSZ*2)

static int num_bbs;
static int num_signals;

static void *thread_ready;
static void *thread_exit;
static void *got_signal;

SIGJMP_BUF mark;

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGUSR1) {
        print("Got SIGUSR1\n");
        signal_cond_var(got_signal);
    } else if (sig == SIGSEGV) {
        print("Got SIGSEGV\n");
        SIGLONGJMP(mark, 1);
    } else if (sig == SIGPROF) {
        /* Do nothing. */
    } else {
        print("Got unexpected signal %d\n", sig);
    }
}

static void *
thread_func(void *arg)
{
    stack_t sigstack;
    int rc;
    sigstack.ss_sp = (char *) malloc(ALT_STACK_SIZE);
    sigstack.ss_size = ALT_STACK_SIZE;
    sigstack.ss_flags = SS_ONSTACK;
    rc = sigaltstack(&sigstack, NULL);
    ASSERT_NOERR(rc);
    signal_cond_var(thread_ready);
    wait_cond_var(thread_exit);
    free(sigstack.ss_sp);
    return NULL;
}

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
         bool translating)
{
    num_bbs++;
    return DR_EMIT_DEFAULT;
}

static dr_signal_action_t
event_signal(void *drcontext, dr_siginfo_t *info)
{
    dr_atomic_add32_return_sum(&num_signals, 1);
    return DR_SIGNAL_DELIVER;
}

static void
event_exit(void)
{
    dr_fprintf(STDERR, "Saw %s bb events\n", num_bbs > 0 ? "some" : "no");
    /* Unfortunately we have no synch to guarantee we see some alarm
     * signals.
     * FIXME: once we have i#2311 and can ensure alarms only arrive in
     * the 2nd thread, we can use a cond var from the signal handler and
     * ensure we see some.  For now we just hope to occasionally test some
     * races with alarms.
     */
    dr_fprintf(STDERR, "Saw %s signals\n", num_signals >= 2 ? ">=2" : "<2");
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    print("in dr_client_main\n");
    dr_register_bb_event(event_bb);
    dr_register_signal_event(event_signal);
    dr_register_exit_event(event_exit);
}

static int
do_some_work(void)
{
    static int iters = 8192;
    int i;
    double val = num_bbs;
    for (i = 0; i < iters; ++i) {
        val += sin(val);
    }
    return (val > 0);
}

int
main(int argc, const char *argv[])
{
    pthread_t thread;
    struct itimerval timer;
    sigset_t mask;
    int rc;
    intercept_signal(SIGUSR1, signal_handler, true/*sigstack*/);
    intercept_signal(SIGSEGV, signal_handler, true/*sigstack*/);
    thread_ready = create_cond_var();
    thread_exit = create_cond_var();
    got_signal = create_cond_var();

    intercept_signal(SIGPROF, signal_handler, true/*sigstack*/);
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 1000;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 1000;
    rc = setitimer(ITIMER_PROF, &timer, NULL);
    assert(rc == 0);

    pthread_create(&thread, NULL, thread_func, NULL);
    wait_cond_var(thread_ready);

#if 0 /* FIXME i#2311: once fixed in DR we can enable this. */
    /* Block SIGPROF in the main thread to better test races. */
    sigemptyset(&mask);
    sigaddset(&mask, SIGPROF);
    sigprocmask(SIG_BLOCK, &mask, NULL);
#endif

    print("Sending SIGUSR1 pre-DR-init\n");
    pthread_kill(thread, SIGUSR1);
    wait_cond_var(got_signal);
    reset_cond_var(got_signal);

    print("pre-DR init\n");
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());

    print("Sending SIGUSR1 pre-DR-start\n");
    pthread_kill(thread, SIGUSR1);
    wait_cond_var(got_signal);
    reset_cond_var(got_signal);

    print("pre-DR start\n");
    dr_app_start();
    assert(dr_app_running_under_dynamorio());

    if (do_some_work() < 0)
        print("error in computation\n");

    print("Sending SIGUSR1 under DR\n");
    pthread_kill(thread, SIGUSR1);
    wait_cond_var(got_signal);
    reset_cond_var(got_signal);

    print("pre-raise SIGSEGV under DR\n");
    if (SIGSETJMP(mark) == 0)
        *(int *)0x42 = 0;

    print("pre-DR stop\n");
    // i#95: today we don't have full support for separating stop from cleanup:
    // we rely on the app joining threads prior to cleanup.
    // We do support a full detach on dr_app_stop_and_cleanup() which we use here.
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());

    print("Sending SIGUSR1 post-DR-stop\n");
    pthread_kill(thread, SIGUSR1);
    wait_cond_var(got_signal);
    reset_cond_var(got_signal);

    print("pre-raise SIGSEGV native\n");
    if (SIGSETJMP(mark) == 0)
        *(int *)0x42 = 0;

    signal_cond_var(thread_exit);
    pthread_join(thread, NULL);

    print("all done\n");
    return 0;
}
