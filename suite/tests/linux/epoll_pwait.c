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

static void
signalHandler(int sig, siginfo_t *siginfo, void *context)
{
    print("signal received: %d\n", sig);
}

int
main(int argc, char *argv[])
{
    struct sigaction act;
    sigset_t new_set;

    INIT();
    memset(&act, '\0', sizeof(act));

    act.sa_sigaction = &signalHandler;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("sigaction failed\n");
        return 1;
    }
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        perror("sigaction failed\n");
        return 1;
    }
    print("handlers for signals: %d, %d\n", SIGUSR1, SIGUSR2);
    sigemptyset(&new_set);
    sigaddset(&new_set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &new_set, NULL);
    print("signal blocked: %d\n", SIGUSR1);

    int pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        struct timespec sleeptime;
        sleeptime.tv_sec = 1;
        sleeptime.tv_nsec = 0;
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

    int epollFD = epoll_create1(02000000);
    struct epoll_event events;

    int count = 0;
    while (count++ < 3) {
        sigset_t empty_set;
        sigemptyset(&empty_set);
        /* XXX i#3240: DR currently does not handle the atomicity aspect of this system
         * call. Once it does, please include this in this test or add a new test. */
        epoll_pwait(epollFD, &events, 24, -1, &empty_set);
    };
    return 0;
}
