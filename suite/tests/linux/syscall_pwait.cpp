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

#ifndef LINUX
#    error Test of Linux-only system calls
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <sched.h>
#include <functional>
#include "thread.h"
#include "condvar.h"

typedef struct {
    sigset_t *sigmask;
    size_t sizemask;
} data_t;

typedef struct {
    pthread_t main_thread;
    bool immediately;
    int sig;
} args_t;

static struct timespec sleeptime;
static void *ready_to_listen;
static args_t args;

static void
signal_handler(int sig, siginfo_t *siginfo, void *context)
{
    print("Signal received: %d\n", sig);
}

THREAD_FUNC_RETURN_TYPE
kick_off_child_func(void *arg)
{
    args_t args = *(args_t *)arg;
    if (!args.immediately) {
        /* waste some time */
        nanosleep(&sleeptime, NULL);
    }
    pthread_kill(args.main_thread, args.sig);
    signal_cond_var(ready_to_listen);
    return THREAD_FUNC_RETURN_ZERO;
}

pthread_t
kick_off_child_signal(int count, pthread_t main_thread, bool immediately)
{
    pthread_t child_thread;
    reset_cond_var(ready_to_listen);
    args.main_thread = main_thread;
    args.immediately = immediately;
    if (count == 1) {
        args.sig = SIGUSR1;
        child_thread = create_thread(kick_off_child_func, &args);
    } else if (count == 2) {
        args.sig = SIGUSR2;
        child_thread = create_thread(kick_off_child_func, &args);
    }
    if (immediately) {
        /* This makes sure that the signal is pending
         * in the kernel after return of this call.
         */
        wait_cond_var(ready_to_listen);
    }
    return child_thread;
}

void
execute_subtest(pthread_t main_thread, sigset_t *test_set,
                std::function<int(bool nullsigmask)> psyscall, bool nullsigmask)
{
    int count;
    for (int i = 0; i < 2; ++i) {
        count = 0;
        while (count++ < 2) {
            /* XXX i#3240: DR currently does not handle the atomicity aspect of this
             * system call. Once it does, please include this in this test or add a new
             * test.
             */
            sigset_t pre_syscall_set = {
                0, /* Set padding to 0 so we can use memcmp */
            };
            /* immediately == true sends the signal before the system call is executed
             * such that the signal is in pending state once we start the call.
             * immediately == false adds a delay before sending the signal such that the
             * signal arrives while we are in the system call, but there is no check to
             * verifiy whether it arrived "late enough".
             */
            bool immediately = i == 0 ? true : false;
            if (immediately && nullsigmask) {
                /* The immediately test must be skipped if sigmask is NULL. */
                continue;
            }
            pthread_t child_thread =
                kick_off_child_signal(count, main_thread, immediately);
            pthread_sigmask(SIG_SETMASK, NULL, &pre_syscall_set);
            if (psyscall(nullsigmask) == -1) {
                if (errno != EINTR)
                    perror("expected EINTR");
            } else {
                perror("expected interruption of syscall");
            }
            sigset_t post_syscall_set = {
                0, /* Set padding to 0 so we can use memcmp */
            };
            pthread_sigmask(SIG_SETMASK, NULL, &post_syscall_set);
            if (memcmp(&pre_syscall_set, &post_syscall_set, sizeof(pre_syscall_set)) !=
                0) {
                print("sigmask mismatch\n");
                exit(1);
            }
            pthread_join(child_thread, NULL);
        }
    }
}

int
main(int argc, char *argv[])
{
    struct sigaction act;
    sigset_t block_set;

    INIT();

    sleeptime.tv_sec = 1;
    sleeptime.tv_nsec = 0;

    intercept_signal(SIGUSR1, (handler_3_t)signal_handler, true);
    intercept_signal(SIGUSR2, (handler_3_t)signal_handler, true);
    print("Handlers for signals: %d, %d\n", SIGUSR1, SIGUSR2);
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGUSR2);
    sigaddset(&block_set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &block_set, NULL);
    /* We need to block the signals for the purpose of this test, so that
     * the p* system call will unblock it as part of its execution.
     */
    print("Signal blocked: %d\n", SIGUSR2);
    print("Signal blocked: %d\n", SIGUSR1);

    sigset_t test_set;
    sigfillset(&test_set);
    sigdelset(&test_set, SIGUSR1);
    sigdelset(&test_set, SIGUSR2);

    ready_to_listen = create_cond_var();
    pthread_t main_thread = pthread_self();

    print("Testing epoll_pwait\n");

    auto psyscall_epoll_pwait = [test_set](bool nullsigmask) -> int {
        int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        struct epoll_event events;
        return epoll_pwait(epoll_fd, &events, 24, -1, nullsigmask ? NULL : &test_set);
    };
    execute_subtest(main_thread, &test_set, psyscall_epoll_pwait, false);

    print("Testing pselect\n");

    auto psyscall_pselect = [test_set](bool nullsigmask) -> int {
        return pselect(0, NULL, NULL, NULL, NULL, nullsigmask ? NULL : &test_set);
    };
    execute_subtest(main_thread, &test_set, psyscall_pselect, false);

    print("Testing ppoll\n");

    auto psyscall_ppoll = [test_set](bool nullsigmask) -> int {
        return ppoll(NULL, 0, NULL, nullsigmask ? NULL : &test_set);
    };
    execute_subtest(main_thread, &test_set, psyscall_ppoll, false);

    print("Testing epoll_pwait failure\n");

    /* XXX: The following failure tests will 'hang' if syscall succeeds, due to the
     * nature of the syscall. Maybe change this into something that will rather fail
     * immediately.
     */

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event events;
    if (syscall(SYS_epoll_pwait, epoll_fd, &events, 24LL, -1LL, &test_set, 0) == 0) {
        print("expected syscall failure");
        exit(1);
    } else if (errno != EINVAL) {
        print("wrong errno code");
        exit(1);
    }

    print("Testing pselect failure\n");

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    data_t data_wrong = { &test_set, 0 };

    if (syscall(SYS_pselect6, 0, NULL, NULL, NULL, NULL, &data_wrong) == 0) {
        print("expected syscall failure");
        exit(1);
    } else if (errno != EINVAL) {
        print("wrong errno code");
        exit(1);
    }

    print("Testing ppoll failure\n");

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    if (syscall(SYS_ppoll, NULL, 0, NULL, &test_set, 0) == 0) {
        print("expected syscall failure");
        exit(1);
    } else if (errno != EINVAL) {
        print("wrong errno code");
        exit(1);
    }

#if defined(X86) && defined(X64)

    print("Testing epoll_pwait, preserve mask\n");

    int count = 0;
    while (count++ < 2) {
        int syscall_error = 0;
        int mask_error = 0;
        pthread_t child_thread = kick_off_child_signal(count, main_thread, true);
        /* Syscall preserves all registers except rax, rcx and r11. Note that we're
         * clobbering rbx (which is choosen randomly) in order to save the old mask
         * for a mask check. Upon a syscall, DR will modify the sigmask parameter
         * of the call and restore it post syscall. The mask check is making sure
         * that save/restore is done properly.
         */
        asm volatile("mov %7, %%rbx\n\t"
                     "movq %2, %%rax\n\t"
                     "movq %3, %%rdi\n\t"
                     "movq %4, %%rsi\n\t"
                     "movq %5, %%rdx\n\t"
                     "movq %6, %%r10\n\t"
                     "movq %7, %%r8\n\t"
                     "movq %8, %%r9\n\t"
                     "syscall\n\t"
                     "mov $0, %0\n\t"
                     "cmp $-4095, %%rax\n\t"
                     "jl no_syscall_error%=\n\t"
                     "mov $1, %0\n\t"
                     "no_syscall_error%=:\n\t"
                     "mov $0, %1\n\t"
                     "cmp %%rbx, %%r8\n\t"
                     "je no_mask_error%=\n\t"
                     "mov $1, %1\n\t"
                     "no_mask_error%=:\n"
                     /* early-clobber outputs */
                     : "=&r"(syscall_error), "=&r"(mask_error)
                     : "r"((int64_t)SYS_epoll_pwait), "rm"((int64_t)epoll_fd),
                       "rm"(&events), "r"(24LL), "r"(-1LL), "rm"(&test_set),
                       "r"((int64_t)(_NSIG / 8))
                     : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "rbx");
        if (syscall_error == 0)
            perror("expected syscall error EINTR");
        if (mask_error == 1) {
            /* This checks whether DR has properly restored the mask parameter after
             * the syscall. I.e. internally, DR may choose to change the parameter
             * prior to the syscall.
             */
            perror("expected syscall to preserve mask parameter");
        }
        pthread_join(child_thread, NULL);
    }

    data_t data = { &test_set, _NSIG / 8 };

    print("Testing pselect, preserve mask\n");

    count = 0;
    while (count++ < 2) {
        int syscall_error = 0;
        int mask_error = 0;
        pthread_t child_thread = kick_off_child_signal(count, main_thread, true);
        asm volatile("movq 0(%8), %%rbx\n\t"
                     "movq %2, %%rax\n\t"
                     "movq %3, %%rdi\n\t"
                     "movq %4, %%rsi\n\t"
                     "movq %5, %%rdx\n\t"
                     "movq %6, %%r10\n\t"
                     "movq %7, %%r8\n\t"
                     "movq %8, %%r9\n\t"
                     "syscall\n\t"
                     "mov $0, %0\n\t"
                     "cmp $-4095, %%rax\n\t"
                     "jl no_syscall_error%=\n\t"
                     "mov $1, %0\n\t"
                     "no_syscall_error%=:\n\t"
                     "mov $0, %1\n\t"
                     "cmp 0(%%r9), %%rbx\n\t"
                     "je no_mask_error%=\n\t"
                     "mov $1, %1\n\t"
                     "no_mask_error%=:\n"
                     /* early-clobber ouputs */
                     : "=&r"(syscall_error), "=&r"(mask_error)
                     : "r"((int64_t)SYS_pselect6), "r"(0LL), "rm"(NULL), "rm"(NULL),
                       "rm"(NULL), "rm"(NULL), "r"(&data)
                     : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "rbx");
        if (syscall_error == 0)
            perror("expected syscall error EINTR");
        if (mask_error == 1)
            perror("expected syscall to preserve mask parameter");
        pthread_join(child_thread, NULL);
    }

    print("Testing ppoll, preserve mask\n");

    count = 0;
    while (count++ < 2) {
        int syscall_error = 0;
        int mask_error = 0;
        pthread_t child_thread = kick_off_child_signal(count, main_thread, true);
        asm volatile("mov %6, %%rbx\n\t"
                     "movq %2, %%rax\n\t"
                     "movq %3, %%rdi\n\t"
                     "movq %4, %%rsi\n\t"
                     "movq %5, %%rdx\n\t"
                     "movq %6, %%r10\n\t"
                     "movq %7, %%r8\n\t"
                     "syscall\n\t"
                     "mov $0, %0\n\t"
                     "cmp $-4095, %%rax\n\t"
                     "jl no_syscall_error%=\n\t"
                     "mov $1, %0\n\t"
                     "no_syscall_error%=:\n\t"
                     "mov $0, %1\n\t"
                     "cmp %%rbx, %%r10\n\t"
                     "je no_mask_error%=\n\t"
                     "mov $1, %1\n\t"
                     "no_mask_error%=:\n"
                     /* early-clobber outputs */
                     : "=&r"(syscall_error), "=&r"(mask_error)
                     : "r"((int64_t)SYS_ppoll), "rm"(NULL), "r"(0LL), "rm"(NULL),
                       "rm"(&test_set), "r"((int64_t)(_NSIG / 8))
                     : "rax", "rdi", "rsi", "rdx", "r10", "r8", "rbx");
        if (syscall_error == 0)
            perror("expected syscall error EINTR");
        if (mask_error == 1)
            perror("expected syscall to preserve mask parameter");
        pthread_join(child_thread, NULL);
    }

#endif

    /* Now making sure passing a NULL sigmask works. A NULL sigmask
     * parameter should behave as if it was a non-p* version syscall.
     */

    pthread_sigmask(SIG_UNBLOCK, &block_set, NULL);

    print("Signal unblocked: %d\n", SIGUSR2);
    print("Signal unblocked: %d\n", SIGUSR1);

    print("Testing epoll_pwait with NULL sigmask\n");

    execute_subtest(main_thread, &test_set, psyscall_epoll_pwait, true);

    print("Testing pselect with NULL sigmask\n");

    execute_subtest(main_thread, &test_set, psyscall_pselect, true);

    print("Testing ppoll with NULL sigmask\n");

    execute_subtest(main_thread, &test_set, psyscall_ppoll, true);

    destroy_cond_var(ready_to_listen);

    return 0;
}
