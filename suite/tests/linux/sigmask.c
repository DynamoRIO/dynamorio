/* **********************************************************
 * Copyright (c) 2016-2022 Google, Inc.  All rights reserved.
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
 * Test signal masks.
 */

#include "configure.h"
#ifndef UNIX
#    error UNIX-only
#endif
#include "tools.h"
#include "thread.h"
#include "condvar.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h> /* itimer */

static void *child_ready;
static void *child_exit;
static volatile bool should_exit;
static pthread_t unblocked_thread;

#define MAGIC_VALUE 0xdeadbeef

static void
handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    /* We go ahead and use locks which is unsafe in general code but we have controlled
     * timing of our signals here.
     */
    if (sig == SIGWINCH) {
#ifdef MACOS
        /* Make the output match for template simplicity. */
        siginfo->si_code = SI_QUEUE;
        siginfo->si_value.sival_ptr = (void *)MAGIC_VALUE;
#endif
        print("in handler for signal %d from %d value %p\n", sig, siginfo->si_code,
              siginfo->si_value);
    } else
        print("in handler for signal %d\n", sig);
    signal_cond_var(child_ready);
    if (sig == SIGUSR2)
        pthread_exit(NULL);
}

static void *
thread_routine(void *arg)
{
    intercept_signal(SIGUSR1, (handler_3_t)handler, false);
    intercept_signal(SIGWINCH, (handler_3_t)handler, false);
    intercept_signal(SIGUSR2, (handler_3_t)handler, false);

    signal_cond_var(child_ready);

    sigset_t set;
    sigemptyset(&set);
    while (true) {
        sigsuspend(&set);
    }
    return NULL;
}

static void
alarm_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (pthread_self() == unblocked_thread) {
        if (sig != SIGALRM)
            print("Unexpected signal %d\n", sig);
#ifdef LINUX
        /* We take advantage of DR's lack of transparency where its reroute uses tkill
         * but the original was process-wide so we can detect a rerouted signal.
         * Without the logic in DR to not reroute a signal when blocked due to
         * being inside a handler, this code is triggered and the print fails the
         * test. Unfortunately we have no simple way of checking this on Mac.
         */
        if (siginfo->si_code == SI_TKILL)
            print("signal from tkill (rerouted?) not expected\n");
#endif
    } else {
        if (sig == SIGALRM && !should_exit) {
            signal_cond_var(child_ready);
            /* Sit in the handler with SIGALRM blocked. */
            wait_cond_var(child_exit);
        }
    }
}

static void *
test_alarm_signals(void *arg)
{
    /* Test alarm signals not being rerouted from handlers. */
    pthread_t init_thread = (pthread_t)arg;
    unblocked_thread = pthread_self();
    intercept_signal(SIGALRM, alarm_handler, false);

    /* Get init thread inside its handler. */
    pthread_kill(init_thread, SIGALRM);
    wait_cond_var(child_ready);
    reset_cond_var(child_ready);

    print("init thread now inside handler: setting up itimer\n");
    struct itimerval t;
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 10000;
    t.it_value.tv_sec = 0;
    t.it_value.tv_usec = 10000;
    int res = setitimer(ITIMER_REAL, &t, NULL);
    if (res != 0)
        perror("setitimer failed");
    /* Let a bunch of real-time signals arrive. */
    for (int i = 0; i < 10; i++)
        thread_sleep(25);
    /* Turn off the itimer. */
    memset(&t, 0, sizeof(t));
    res = setitimer(ITIMER_REAL, &t, NULL);
    if (res != 0)
        perror("setitimer failed");

    /* Exit. */
    print("done with itimer; exiting\n");
    should_exit = true;
    signal_cond_var(child_exit);

    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t thread;
    void *retval;

    child_ready = create_cond_var();
    child_exit = create_cond_var();

    if (pthread_create(&thread, NULL, thread_routine, NULL) != 0) {
        perror("failed to create thread");
        exit(1);
    }

    wait_cond_var(child_ready);
    /* Impossible to have child notify us when inside sigsuspend but it should get
     * there pretty quickly after it signals condvar.
     */
    reset_cond_var(child_ready);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGWINCH);
    int res = sigprocmask(SIG_BLOCK, &set, NULL);
    if (res != 0)
        perror("sigprocmask failed");

    /* Send a signal to the whole process.  This often is delivered to the
     * main (current) thread under DR where it's not blocked, causing a hang without
     * the rerouting of i#2311.
     * It would be nice to have a guarantee that the signal will come here but
     * that doesn't seem possible.
     */
    print("sending %d\n", SIGUSR1);
    kill(getpid(), SIGUSR1);
    wait_cond_var(child_ready);
    reset_cond_var(child_ready);

    print("sending %d with value\n", SIGWINCH);
    union sigval value;
    value.sival_ptr = (void *)MAGIC_VALUE;
#ifdef MACOS
    /* sigqueue is not available on Mac. */
    kill(getpid(), SIGWINCH);
#else
    sigqueue(getpid(), SIGWINCH, value);
#endif
    wait_cond_var(child_ready);
    reset_cond_var(child_ready);

    /* Tell thread to exit. */
    pthread_kill(thread, SIGUSR2);
    if (pthread_join(thread, &retval) != 0)
        perror("failed to join thread");

    /* Test alarm signal rerouting.  Since process-wide signals are overwhelmingly
     * delivered to the initial thread (I can't get them to go anywhere else!), we
     * need this thread to be the one sitting in a SIGALRM handler while we test whether
     * signals are rerouted from there.  We create a thread to put us in the handler
     * and drive the test.
     * It would be nice to have a guarantee that the signal will come here but
     * that doesn't seem possible.
     */
    if (pthread_create(&thread, NULL, test_alarm_signals, (void *)pthread_self()) != 0) {
        perror("failed to create thread");
        exit(1);
    }
    sigemptyset(&set);
    while (!should_exit) {
        /* We expect just one signal but best practice is to always loop. */
        sigsuspend(&set);
    }
    if (pthread_join(thread, &retval) != 0)
        perror("failed to join thread");

    destroy_cond_var(child_ready);
    destroy_cond_var(child_exit);

    print("all done\n");

    return 0;
}
