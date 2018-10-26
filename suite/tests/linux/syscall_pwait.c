/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
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
 * test of epoll_pwait (xref i#2759, i#3240)
 */

#include "tools.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>

static void
signal_handler(int sig, siginfo_t *siginfo, void *context)
{
    print("signal received: %d\n", sig);
}

int
main(int argc, char *argv[])
{
    struct sigaction act;
    sigset_t new_set;

    INIT();

    intercept_signal(SIGUSR1, (handler_3_t)signal_handler, true);
    intercept_signal(SIGUSR2, (handler_3_t)signal_handler, true);
    print("handlers for signals: %d, %d\n", SIGUSR1, SIGUSR2);
    sigemptyset(&new_set);
    sigaddset(&new_set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &new_set, NULL);
    print("signal blocked: %d\n", SIGUSR1);

    sigset_t test_set;
    sigfillset(&test_set);
    sigdelset(&test_set, SIGUSR1);
    sigdelset(&test_set, SIGUSR2);

    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 500 * 1000 * 1000;

    int pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR2);
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR1);
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR1);
        return 0;
    }

    int epoll_fd = epoll_create1(02000000);
    struct epoll_event events;
    int count = 0;
    while (count++ < 3) {
        /* XXX i#3240: DR currently does not handle the atomicity aspect of this system
         * call. Once it does, please include this in this test or add a new test. */
        epoll_pwait(epoll_fd, &events, 24, -1, &test_set);
    }

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR2);
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR1);
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR1);
        return 0;
    }

    count = 0;
    while (count++ < 3) {
        /* XXX i#3240: DR currently does not handle the atomicity aspect of this system
         * call. Once it does, please include this in this test or add a new test. */
        pselect(0, NULL, NULL, NULL, NULL, &test_set);
    }

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR2);
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR1);
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR1);
        return 0;
    }

    count = 0;
    while (count++ < 3) {
        /* XXX i#3240: DR currently does not handle the atomicity aspect of this system
         * call. Once it does, please include this in this test or add a new test. */
        ppoll(NULL, 0, NULL, &test_set);
    }

    return 0;
}
