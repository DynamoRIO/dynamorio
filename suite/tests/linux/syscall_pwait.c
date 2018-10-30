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
 * Test of ppoll, pselect and epoll_pwait (xref i#2759, i#3240)
 */

#include "tools.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>

static void
signal_handler(int sig, siginfo_t *siginfo, void *context)
{
    print("signal received: %d\n", sig);
}

int
kick_off_child_signals(struct timespec *sleeptime)
{
    /* waste some time */
    nanosleep(sleeptime, NULL);
    kill(getppid(), SIGUSR2);
    /* waste some time */
    nanosleep(sleeptime, NULL);
    kill(getppid(), SIGUSR1);
    /* waste some time */
    nanosleep(sleeptime, NULL);
    kill(getppid(), SIGUSR1);
    return 0;
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
        return kick_off_child_signals(&sleeptime);
    }

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event events;

    print("Testing epoll_pwait\n");

    int count = 0;
    while (count++ < 3) {
        /* XXX i#3240: DR currently does not handle the atomicity aspect of this system
         * call. Once it does, please include this in this test or add a new test.
         */
        sigset_t *test_set_check = &test_set;
        if (epoll_pwait(epoll_fd, &events, 24, -1, &test_set) == -1) {
            if (errno != EINTR)
                perror("expected EINTR");
        } else {
            perror("expected interruption of syscall");
        }
        if (test_set_check != &test_set) {
            /* This check is almost meaningless. What we really had to check is the
             * mask register parameter right after the syscall in the glibc wrapper.
             */
            perror("different mask pointer");
        }
    }

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        return kick_off_child_signals(&sleeptime);
    }

    print("Testing pselect\n");

    count = 0;
    while (count++ < 3) {
        /* XXX i#3240: DR currently does not handle the atomicity aspect of this system
         * call. Once it does, please include this in this test or add a new test.
         */
        sigset_t *test_set_check = &test_set;
        if (pselect(0, NULL, NULL, NULL, NULL, &test_set) == -1) {
            if (errno != EINTR)
                perror("expected EINTR");
        } else {
            perror("expected interruption of syscall");
        }
        if (test_set_check != &test_set) {
            /* This check is almost meaningless. What we really had to check is the
             * mask register parameter right after the syscall in the glibc wrapper.
             */
            perror("different mask pointer");
        }
    }

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        return kick_off_child_signals(&sleeptime);
    }

    print("Testing ppoll\n");

    count = 0;
    while (count++ < 3) {
        /* XXX i#3240: DR currently does not handle the atomicity aspect of this system
         * call. Once it does, please include this in this test or add a new test.
         */
        sigset_t *test_set_check = &test_set;
        if (ppoll(NULL, 0, NULL, &test_set) == -1) {
            if (errno != EINTR)
                perror("expected EINTR");
        } else {
            perror("expected interruption of syscall");
        }
        if (test_set_check != &test_set) {
            /* This check is almost meaningless. What we really had to check is the
             * mask register parameter right after the syscall in the glibc wrapper.
             */
            perror("different mask pointer");
        }
    }

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        return kick_off_child_signals(&sleeptime);
    }

#if defined(X64)

    print("Testing epoll_pwait, preserve mask\n");

    count = 0;
    while (count++ < 3) {
        asm goto("movq %0, %%rax\n\t"
                 "movq %1, %%rdi\n\t"
                 "movq %2, %%rsi\n\t"
                 "movq %3, %%rdx\n\t"
                 "movq %4, %%r10\n\t"
                 "movq %5, %%r8\n\t"
                 "movq %6, %%r9\n\t"
                 "syscall\n\t"
                 "cmp $-4095, %%rax\n\t"
                 "jl %l7\n\t"
                 "cmp %5, %%r8\n\t"
                 "jne %l8\n"
                 :
                 : "r"((int64_t)SYS_epoll_pwait), "rm"((int64_t)epoll_fd), "rm"(&events),
                   "r"(24LL), "r"(-1LL), "rm"(&test_set), "r"((int64_t)(_NSIG / 8))
                 : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9"
                 : syscall_no_error, mask_pointer_different);
    }

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    typedef const struct {
        sigset_t *sigmask;
        size_t sizemask;
    } data_t;

    data_t data = { &test_set, _NSIG / 8 };

    pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        return kick_off_child_signals(&sleeptime);
    }

    print("Testing pselect, preserve mask\n");

    count = 0;
    while (count++ < 3) {
        /* syscall preserves all registers but rax, rcx and r11. Note that we're
         * clobbering rbx which is choosen randomly, in order to save the old mask
         * to perform the check.
         */
        asm goto("movq 0(%6), %%rbx\n\t"
                 "movq %0, %%rax\n\t"
                 "movq %1, %%rdi\n\t"
                 "movq %2, %%rsi\n\t"
                 "movq %3, %%rdx\n\t"
                 "movq %4, %%r10\n\t"
                 "movq %5, %%r8\n\t"
                 "movq %6, %%r9\n\t"
                 "syscall\n\t"
                 "cmpq $-4095, %%rax\n\t"
                 "jl %l7\n\t"
                 "cmpq 0(%%r9), %%rbx\n\t"
                 "jne %l8\n"
                 :
                 : "r"((int64_t)SYS_pselect6), "r"(0LL), "rm"(NULL), "rm"(NULL),
                   "rm"(NULL), "rm"(NULL), "rm"(&data)
                 : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "rbx"
                 : syscall_no_error, mask_pointer_different);
    }

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        return kick_off_child_signals(&sleeptime);
    }

    print("Testing ppoll, preserve mask\n");

    count = 0;
    while (count++ < 3) {
        asm goto("movq %0, %%rax\n\t"
                 "movq %1, %%rdi\n\t"
                 "movq %2, %%rsi\n\t"
                 "movq %3, %%rdx\n\t"
                 "movq %4, %%r10\n\t"
                 "movq %5, %%r8\n\t"
                 "syscall\n\t"
                 "cmp $-4095, %%rax\n\t"
                 "jl %l6\n\t"
                 "cmp %5, %%r8\n\t"
                 "jne %l7\n"
                 :
                 : "r"((int64_t)SYS_ppoll), "rm"(NULL), "r"(0LL), "rm"(NULL),
                   "r"(&test_set), "r"((int64_t)(_NSIG / 8))
                 : "rax", "rdi", "rsi", "rdx", "r10", "r8"
                 : syscall_no_error, mask_pointer_different);
    }

    return 0;

syscall_no_error:
    perror("expected syscall error EINTR");

mask_pointer_different:
    /* This checks whether DR has properly restored the mask parameter after the syscall.
     * I.e. internally, DR may choose to change the parameter prior to the syscall.
     */
    perror("expected syscall to preserve mask parameter");

#endif

    return 0;
}
