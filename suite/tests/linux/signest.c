/* **********************************************************
 * Copyright (c) 2015-2021 Google, Inc.  All rights reserved.
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
 * test of nested signals
 */

#include "tools.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include "condvar.h"

static void *child_started;
static void *received_nonest_signal;
static void *received_nest_signal;
static void *sent_nest_signal2;
static volatile int in_handler;
static volatile bool saw_nest;
static volatile bool sideline_exit;

#define SIG_NONEST SIGUSR1
#define SIG_NEST SIGUSR2

static void
handler_nonest(int sig, siginfo_t *info, void *ucxt)
{
    /* Increment in the 1st block so we'll go back to DR for code discovery,
     * hitting the i#4998 issue.
     */
    ++in_handler;
    if (sig != SIG_NONEST)
        print("invalid signal for nonest handler\n");
    if (in_handler > 1)
        print("incorrectly nested signal!\n");
    signal_cond_var(received_nonest_signal);
    /* Return to DR again to further test i#4998. */
    struct sigaction act;
    int rc = sigaction(SIG_NEST, NULL, &act);
    ASSERT_NOERR(rc);
    --in_handler;
}

static void
handler_nest(int sig, siginfo_t *info, void *ucxt)
{
    /* Similarly to handler_nonest: increment first to better detect nesting. */
    ++in_handler;
    if (sig != SIG_NEST)
        print("invalid signal for nest handler\n");
    if (in_handler > 1)
        saw_nest = true;
    else {
        signal_cond_var(received_nest_signal);
        wait_cond_var(sent_nest_signal2);
    }
    /* Return to DR to check pending signals. */
    struct sigaction act;
    int rc = sigaction(SIG_NEST, NULL, &act);
    ASSERT_NOERR(rc);
    --in_handler;
}

static void *
thread_routine(void *arg)
{
    int rc;
    struct sigaction act;
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))handler_nonest;
    rc = sigfillset(&act.sa_mask); /* Block all other signals. */
    ASSERT_NOERR(rc);
    act.sa_flags = SA_SIGINFO; /* *Do* block the same signal (no SA_NODEFER). */
    rc = sigaction(SIG_NONEST, &act, NULL);
    ASSERT_NOERR(rc);

    rc = sigdelset(&act.sa_mask, SIG_NEST);
    ASSERT_NOERR(rc);
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))handler_nest;
    act.sa_flags = SA_SIGINFO | SA_NODEFER; /* Do *not* block the same signal. */
    rc = sigaction(SIG_NEST, &act, NULL);
    ASSERT_NOERR(rc);

    signal_cond_var(child_started);

    while (!sideline_exit) {
        /* Spend as much time in DR as possible so signals will accumulate. */
        rc = sigaction(SIG_NEST, NULL, &act);
        ASSERT_NOERR(rc);
    }
    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t thread;
    void *retval;
    struct timespec sleeptime;

    child_started = create_cond_var();
    received_nonest_signal = create_cond_var();
    received_nest_signal = create_cond_var();
    sent_nest_signal2 = create_cond_var();

    if (pthread_create(&thread, NULL, thread_routine, NULL) != 0) {
        perror("failed to create thread");
        exit(1);
    }

    wait_cond_var(child_started);

    /* Send multiple signals and try to get at least 2 to queue up while the thread
     * is in DR, replicating i#4998.
     */
    print("sending no-nest signals\n");
    for (int i = 0; i < 5; i++)
        pthread_kill(thread, SIG_NONEST);

    wait_cond_var(received_nonest_signal);

    /* Use cond vars to deliver a signal while the thread is inside its handler. */
    print("sending nestable signals\n");
    pthread_kill(thread, SIG_NEST);
    wait_cond_var(received_nest_signal);
    pthread_kill(thread, SIG_NEST);
    signal_cond_var(sent_nest_signal2);

    sideline_exit = true;
    if (pthread_join(thread, &retval) != 0)
        perror("failed to join thread");

    destroy_cond_var(child_started);
    destroy_cond_var(received_nonest_signal);
    destroy_cond_var(received_nest_signal);
    destroy_cond_var(sent_nest_signal2);

    print("saw %snesting\n", saw_nest ? "" : "no ");
    print("all done\n");

    return 0;
}
