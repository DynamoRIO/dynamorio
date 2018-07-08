/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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
#include <pthread.h>

static pthread_cond_t condvar;
static bool child_ready;
static pthread_mutex_t lock;
static int num_received;

#define OURSIG 42 /* a real-time signal */

static void
handler(int sig, siginfo_t *info, void *ucxt)
{
    print("in handler %d\n", sig);
    if (sig == OURSIG) {
        num_received++;
        if (num_received >= 2)
            pthread_exit(NULL);
        else {
            /* take up some time to try and ensure we get a nested signal */
            sleep(1);
        }
    }
}

static void *
thread_routine(void *arg)
{
    int rc;
    struct sigaction act;
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))handler;
    rc = sigemptyset(&act.sa_mask); /* do not block signals within handler */
    ASSERT_NOERR(rc);
    act.sa_flags = SA_SIGINFO;
    rc = sigaction(OURSIG, &act, NULL);
    ASSERT_NOERR(rc);

    pthread_mutex_lock(&lock);
    child_ready = true;
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&lock);

    /* wait for parent to send us SIGTERM */
    while (true)
        sleep(10);
}

int
main(int argc, char **argv)
{
    pthread_t thread;
    void *retval;
    struct timespec sleeptime;

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&condvar, NULL);

    if (pthread_create(&thread, NULL, thread_routine, NULL) != 0) {
        perror("failed to create thread");
        exit(1);
    }

    pthread_mutex_lock(&lock);
    while (!child_ready)
        pthread_cond_wait(&condvar, &lock);
    pthread_mutex_unlock(&lock);

    print("sending 2 signals\n");
    pthread_kill(thread, OURSIG);
    /* XXX i#1803: DR may incorrectly drop the signal if more than one are delivered
     * together.  We delay sending the second signal until the first one is delivered
     * as a temporary workaround.
     */
    if (num_received == 0)
        sched_yield();
    pthread_kill(thread, OURSIG);

    if (pthread_join(thread, &retval) != 0)
        perror("failed to join thread");

    pthread_cond_destroy(&condvar);
    pthread_mutex_destroy(&lock);

    print("all done\n");

    return 0;
}
