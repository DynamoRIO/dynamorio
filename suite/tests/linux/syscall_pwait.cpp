/* **********************************************************
 * Copyright (c) 2018-2022 Google, Inc.  All rights reserved.
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

/* One bit for each defined signal. */
#define SIGSET_SIZE (_NSIG / 8)

typedef struct {
    const sigset_t *sigmask;
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
        nanosleep(&sleeptime, nullptr);
    }
    pthread_kill(args.main_thread, args.sig);
    signal_cond_var(ready_to_listen);
    return THREAD_FUNC_RETURN_ZERO;
}

pthread_t
kick_off_child_signal(int count, pthread_t main_thread, bool immediately)
{
    pthread_t child_thread = 0;
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
                const std::function<int(bool nullsigmask)> &psyscall, bool nullsigmask)
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
            /* Note that when `immediately && !nullsigmask`, the signal is blocked by
             * the app at this point, and gets delivered to DR and queued as a pending
             * signal before the syscall. So, we rely on DR to return an EINTR to the
             * parent thread from a syscall that unblocks the signal by changing the
             * the sigmask, such as pselect, ppoll, etc. If for some reason, DR does
             * not handle any such syscall, we will see a "hang" because there's no
             * signal left to deliver from the kernel's point of view. This helps in
             * detecting regressions where DR's syscall handling is absent (though we
             * still depend on the signal getting delivered to DR before the syscall).
             * We settle for this instead of adding more complicated testing.
             */
            pthread_t child_thread =
                kick_off_child_signal(count, main_thread, immediately);
            pthread_sigmask(SIG_SETMASK, nullptr, &pre_syscall_set);
            if (psyscall(nullsigmask) == -1) {
                if (errno != EINTR)
                    perror("expected EINTR");
            } else {
                perror("expected interruption of syscall");
            }
            sigset_t post_syscall_set = {
                0, /* Set padding to 0 so we can use memcmp */
            };
            pthread_sigmask(SIG_SETMASK, nullptr, &post_syscall_set);
            if (memcmp(&pre_syscall_set, &post_syscall_set, sizeof(pre_syscall_set)) !=
                0) {
                print("sigmask mismatch\n");
                exit(1);
            }
            pthread_join(child_thread, nullptr);
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
    pthread_sigmask(SIG_BLOCK, &block_set, nullptr);
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
        return epoll_pwait(epoll_fd, &events, 24, -1, nullsigmask ? nullptr : &test_set);
    };
    execute_subtest(main_thread, &test_set, psyscall_epoll_pwait, false);

    print("Testing pselect\n");
    auto psyscall_pselect = [test_set](bool nullsigmask) -> int {
        return pselect(0, nullptr, nullptr, nullptr, nullptr,
                       nullsigmask ? nullptr : &test_set);
    };
    execute_subtest(main_thread, &test_set, psyscall_pselect, false);

    print("Testing raw pselect6_time64\n");
    /* The *_time64 syscalls are defined only on 32-bit. */
#ifdef SYS_pselect6_time64
    auto psyscall_raw_pselect6_time64 = [test_set](bool nullsigmask) -> int {
        data_t data;
        data.sigmask = nullsigmask ? nullptr : &test_set;
        data.sizemask = SIGSET_SIZE;
        return syscall(SYS_pselect6_time64, 0, nullptr, nullptr, nullptr, nullptr, &data);
    };
    execute_subtest(main_thread, &test_set, psyscall_raw_pselect6_time64, false);
#else
    print("Signal received: 10\n");
    print("Signal received: 12\n");
    print("Signal received: 10\n");
    print("Signal received: 12\n");
#endif

    print("Testing ppoll\n");
    auto psyscall_ppoll = [test_set](bool nullsigmask) -> int {
        return ppoll(nullptr, 0, nullptr, nullsigmask ? nullptr : &test_set);
    };
    execute_subtest(main_thread, &test_set, psyscall_ppoll, false);

    print("Testing raw ppoll_time64\n");
#ifdef SYS_ppoll_time64
    auto psyscall_raw_ppoll_time64 = [test_set](bool nullsigmask) -> int {
        return syscall(SYS_ppoll_time64, nullptr, 0, nullptr,
                       nullsigmask ? nullptr : &test_set, SIGSET_SIZE);
    };
    execute_subtest(main_thread, &test_set, psyscall_raw_ppoll_time64, false);
#else
    print("Signal received: 10\n");
    print("Signal received: 12\n");
    print("Signal received: 10\n");
    print("Signal received: 12\n");
#endif

    /* XXX: The following failure tests will 'hang' if syscall succeeds, due to the
     * nature of the syscall. Maybe change this into something that will rather fail
     * immediately.
     */

    print("Testing epoll_pwait failure\n");
    /* waste some time */
    nanosleep(&sleeptime, nullptr);
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event events;
    if (syscall(SYS_epoll_pwait, epoll_fd, &events, 24, -1, &test_set, (size_t)0) == 0) {
        print("expected syscall failure");
        exit(1);
    } else if (errno != EINVAL) {
        print("wrong errno code");
        exit(1);
    }

    print("Testing pselect failure\n");
    /* waste some time */
    nanosleep(&sleeptime, nullptr);
    data_t data_wrong = { &test_set, 0 };
    if (syscall(SYS_pselect6, 0, nullptr, nullptr, nullptr, nullptr, &data_wrong) == 0) {
        print("expected syscall failure");
        exit(1);
    } else if (errno != EINVAL) {
        print("wrong errno code");
        exit(1);
    }

    print("Testing ppoll failure\n");
    /* waste some time */
    nanosleep(&sleeptime, nullptr);
    if (syscall(SYS_ppoll, nullptr, (nfds_t)0, nullptr, &test_set, (size_t)0) == 0) {
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
        asm volatile(
            "mov %7, %%rbx\n\t"
            "movl %[sys_num], %%eax\n\t"
            "movl %[epfd], %%edi\n\t"
            "movq %[events], %%rsi\n\t"
            "movl %[maxevents], %%edx\n\t"
            "movq %[timeout], %%r10\n\t"
            "movq %[sigmask], %%r8\n\t"
            "movq %[ss_len], %%r9\n\t"
            "syscall\n\t"
            "mov $0, %[syscall_error]\n\t"
            "cmp $-4095, %%rax\n\t"
            "jl no_syscall_error%=\n\t"
            "mov $1, %[syscall_error]\n\t"
            "no_syscall_error%=:\n\t"
            "mov $0, %[mask_error]\n\t"
            "cmp %%rbx, %%r8\n\t"
            "je no_mask_error%=\n\t"
            "mov $1, %[mask_error]\n\t"
            "no_mask_error%=:\n"
            /* early-clobber outputs */
            : [ syscall_error ] "=&r"(syscall_error), [ mask_error ] "=&r"(mask_error)
            : [ sys_num ] "rm"(SYS_epoll_pwait), [ epfd ] "rm"(epoll_fd),
              [ events ] "rm"(&events), [ maxevents ] "rm"(24),
              [ timeout ] "rm"((int64_t)-1), [ sigmask ] "rm"(&test_set),
              [ ss_len ] "rm"((size_t)(SIGSET_SIZE))
            : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "rbx", "rcx", "r11");
        if (syscall_error == 0)
            perror("expected syscall error EINTR");
        if (mask_error == 1) {
            /* This checks whether DR has properly restored the mask parameter after
             * the syscall. I.e. internally, DR may choose to change the parameter
             * prior to the syscall.
             */
            perror("expected syscall to preserve mask parameter");
        }
        pthread_join(child_thread, nullptr);
    }

    data_t data = { &test_set, SIGSET_SIZE };

    print("Testing pselect, preserve mask\n");
    count = 0;
    while (count++ < 2) {
        int syscall_error = 0;
        int mask_error = 0;
        pthread_t child_thread = kick_off_child_signal(count, main_thread, true);
        asm volatile(
            "movq 0(%8), %%rbx\n\t"
            "movl %[sys_num], %%eax\n\t"
            "movl %[nfds], %%edi\n\t"
            "movq %[readfds], %%rsi\n\t"
            "movq %[writefds], %%rdx\n\t"
            "movq %[exceptfds], %%r10\n\t"
            "movq %[timeout], %%r8\n\t"
            "movq %[sigmaskstruct], %%r9\n\t"
            "syscall\n\t"
            "mov $0, %[syscall_error]\n\t"
            "cmp $-4095, %%rax\n\t"
            "jl no_syscall_error%=\n\t"
            "mov $1, %[syscall_error]\n\t"
            "no_syscall_error%=:\n\t"
            "mov $0, %[mask_error]\n\t"
            "cmp 0(%%r9), %%rbx\n\t"
            "je no_mask_error%=\n\t"
            "mov $1, %[mask_error]\n\t"
            "no_mask_error%=:\n"
            /* early-clobber ouputs */
            : [ syscall_error ] "=&r"(syscall_error), [ mask_error ] "=&r"(mask_error)
            : [ sys_num ] "rm"(SYS_pselect6), [ nfds ] "rm"(0), [ readfds ] "rm"(nullptr),
              [ writefds ] "rm"(nullptr), [ exceptfds ] "rm"(nullptr),
              [ timeout ] "rm"(nullptr), [ sigmaskstruct ] "r"(&data)
            : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "rbx", "rcx", "r11");
        if (syscall_error == 0)
            perror("expected syscall error EINTR");
        if (mask_error == 1)
            perror("expected syscall to preserve mask parameter");
        pthread_join(child_thread, nullptr);
    }

    print("Testing ppoll, preserve mask\n");
    count = 0;
    while (count++ < 2) {
        int syscall_error = 0;
        int mask_error = 0;
        pthread_t child_thread = kick_off_child_signal(count, main_thread, true);
        asm volatile("movq %6, %%rbx\n\t"
                     "movl %[sys_num], %%eax\n\t"
                     "movq %[fds], %%rdi\n\t"
                     "movq %[nfds], %%rsi\n\t"
                     "movq %[tmo_p], %%rdx\n\t"
                     "movq %[sigmask], %%r10\n\t"
                     "movq %[ss_len], %%r8\n\t"
                     "syscall\n\t"
                     "mov $0, %[syscall_error]\n\t"
                     "cmp $-4095, %%rax\n\t"
                     "jl no_syscall_error%=\n\t"
                     "mov $1, %[syscall_error]\n\t"
                     "no_syscall_error%=:\n\t"
                     "mov $0, %[mask_error]\n\t"
                     "cmp %%rbx, %%r10\n\t"
                     "je no_mask_error%=\n\t"
                     "mov $1, %[mask_error]\n\t"
                     "no_mask_error%=:\n"
                     /* early-clobber outputs */
                     : [ syscall_error ] "=&r"(syscall_error),
                       [ mask_error ] "=&r"(mask_error)
                     : [ sys_num ] "r"(SYS_ppoll), [ fds ] "rm"(nullptr),
                       [ nfds ] "rm"((nfds_t)0), [ tmo_p ] "rm"(nullptr),
                       [ sigmask ] "rm"(&test_set), [ ss_len ] "rm"((size_t)(SIGSET_SIZE))
                     : "rax", "rdi", "rsi", "rdx", "r10", "r8", "rbx", "rcx", "r11");
        if (syscall_error == 0)
            perror("expected syscall error EINTR");
        if (mask_error == 1)
            perror("expected syscall to preserve mask parameter");
        pthread_join(child_thread, nullptr);
    }

#endif

    /* Now making sure passing a NULL sigmask works. A NULL sigmask
     * parameter should behave as if it was a non-p* version syscall.
     */

    pthread_sigmask(SIG_UNBLOCK, &block_set, nullptr);

    print("Signal unblocked: %d\n", SIGUSR2);
    print("Signal unblocked: %d\n", SIGUSR1);

    print("Testing epoll_pwait with NULL sigmask\n");
    execute_subtest(main_thread, &test_set, psyscall_epoll_pwait, true);

    print("Testing pselect with NULL sigmask\n");
    execute_subtest(main_thread, &test_set, psyscall_pselect, true);

    print("Testing ppoll with NULL sigmask\n");
    execute_subtest(main_thread, &test_set, psyscall_ppoll, true);

    print("Testing raw epoll_pwait with NULL sigmask\n");
    auto psyscall_raw_epoll_pwait = [test_set](bool nullsigmask) -> int {
        int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        struct epoll_event events;
        return syscall(SYS_epoll_pwait, epoll_fd, &events, 24, 60000, nullptr,
                       (size_t)SIGSET_SIZE);
    };
    execute_subtest(main_thread, &test_set, psyscall_raw_epoll_pwait, true);

    print("Testing raw pselect with NULL sigmask\n");
    auto psyscall_raw_pselect = [test_set](bool nullsigmask) -> int {
        fd_set fds;
        struct timespec ts;
        FD_ZERO(&fds);
        ts.tv_sec = 60;
        ts.tv_nsec = 0;
        data_t data;
        data.sigmask = nullptr;
        data.sizemask = 0;
        return syscall(SYS_pselect6, 0, nullptr, nullptr, &fds, &ts, &data);
    };
    execute_subtest(main_thread, &test_set, psyscall_raw_pselect, true);

    print("Testing raw pselect with NULL struct pointer\n");
    auto psyscall_raw_pselect_nullptr = [test_set](bool nullsigmask) -> int {
        fd_set fds;
        struct timespec ts;
        FD_ZERO(&fds);
        ts.tv_sec = 60;
        ts.tv_nsec = 0;
        return syscall(SYS_pselect6, 0, nullptr, nullptr, &fds, &ts, nullptr);
    };
    execute_subtest(main_thread, &test_set, psyscall_raw_pselect_nullptr, true);

#if defined(X86) && defined(X64)
    print("Testing raw pselect with NULL struct pointer, inline asm\n");
    auto psyscall_raw_pselect_nullptr_inline_asm = [test_set](bool nullsigmask) -> int {
        fd_set fds;
        struct timespec ts;
        FD_ZERO(&fds);
        ts.tv_sec = 60;
        ts.tv_nsec = 0;
        int syscall_error = 0;
        asm volatile("movl %[sys_num], %%eax\n\t"
                     "movl %[nfds], %%edi\n\t"
                     "movq %[readfds], %%rsi\n\t"
                     "movq %[writefds], %%rdx\n\t"
                     "movq %[exceptfds], %%r10\n\t"
                     "movq %[timeout], %%r8\n\t"
                     "movq %[sigmaskstruct], %%r9\n\t"
                     "syscall\n\t"
                     "movl $0, %[syscall_error]\n\t"
                     "cmp $-4095, %%rax\n\t"
                     "jl no_syscall_error%=\n\t"
                     "movl $-1, %[syscall_error]\n\t"
                     "no_syscall_error%=:\n"
                     /* early-clobber ouputs */
                     : [ syscall_error ] "=&r"(syscall_error)
                     : [ sys_num ] "rm"(SYS_pselect6), [ nfds ] "rm"(0),
                       [ readfds ] "rm"(nullptr), [ writefds ] "rm"(nullptr),
                       [ exceptfds ] "rm"(fds), [ timeout ] "rm"(ts),
                       [ sigmaskstruct ] "rm"(nullptr)
                     : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "rcx", "r11");
        return syscall_error;
    };
    /* We are adding this raw asm version of the same test just in case syscall() does
     * sth funny.
     */
    execute_subtest(main_thread, &test_set, psyscall_raw_pselect_nullptr_inline_asm,
                    true);
#endif

    print("Testing raw pselect6_time64 with NULL sigmask\n");
#ifdef SYS_pselect6_time64
    execute_subtest(main_thread, &test_set, psyscall_raw_pselect6_time64, true);
#else
    print("Signal received: 10\n");
    print("Signal received: 12\n");
#endif

    print("Testing raw ppoll with NULL sigmask\n");
    auto psyscall_raw_ppoll = [](bool nullsigmask) -> int {
        struct timespec ts;
        ts.tv_sec = 60;
        ts.tv_nsec = 0;
        return syscall(SYS_ppoll, nullptr, nullptr, &ts, nullptr);
    };
    execute_subtest(main_thread, &test_set, psyscall_raw_ppoll, true);

    print("Testing raw ppoll_time64 with NULL sigmask\n");
#ifdef SYS_ppoll_time64
    execute_subtest(main_thread, &test_set, psyscall_raw_ppoll_time64, true);
#else
    print("Signal received: 10\n");
    print("Signal received: 12\n");
#endif

    destroy_cond_var(ready_to_listen);
    print("Done\n");
    return 0;
}
