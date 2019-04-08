/* **********************************************************
 * Copyright (c) 2013-2017 Google, Inc.  All rights reserved.
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

#include "tools.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/signalfd.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

static void
signal_handler(int sig)
{
    /* We shouldn't get here: all signals in this app should go to the signalfd files */
    print("Error: in handler for signal %d\n", sig);
}

static void
test_signalfd(int sig)
{
    int ret, sigfd, sigfd2;
    sigset_t mask;
    struct signalfd_siginfo siginfo;

    intercept_signal(sig, (handler_3_t)signal_handler, false);

    sigemptyset(&mask);
    sigaddset(&mask, sig);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    sigfd = signalfd(-1, &mask, 0);
    if (sigfd < 0) {
        print("signalfd failed: %m");
        exit(1);
    }

    /* Test 2nd fd mapped to same signal */
    sigfd2 = signalfd(-1, &mask, 0);
    if (sigfd2 < 0) {
        print("signalfd failed: %m");
        exit(1);
    }

    print("about to send signal %d\n", sig);
    ret = kill(getpid(), sig);
    if (ret < 0) {
        print("kill failed: %m");
        exit(1);
    }
    if (sig >= 32) {
        /* Real-time: we can send 2 at once */
        print("about to send 2nd signal %d\n", sig);
        ret = kill(getpid(), sig);
        if (ret < 0) {
            print("kill failed: %m");
            exit(1);
        }
    }

    print("about to read from 1st fd for signal %d\n", sig);
    ret = read(sigfd, &siginfo, sizeof(siginfo));
    if (ret <= 0)
        print("ret: %d\n", ret);
    else {
        print("successful read: signal = %d, source is %s\n", siginfo.ssi_signo,
              siginfo.ssi_pid == getpid() ? "this process" : "another process");
    }

    if (sig < 32) {
        /* Non-real-time: have to send 2nd after 1st consumed */
        print("about to send 2nd signal %d\n", sig);
        ret = kill(getpid(), sig);
        if (ret < 0) {
            print("kill failed: %m");
            exit(1);
        }
    }
    print("about to read from 2nd fd for signal %d\n", sig);
    ret = read(sigfd2, &siginfo, sizeof(siginfo));
    if (ret <= 0)
        print("ret: %d\n", ret);
    else {
        print("successful read: signal = %d, source is %s\n", siginfo.ssi_signo,
              siginfo.ssi_pid == getpid() ? "this process" : "another process");
    }

    /* Undo everything */
    sigemptyset(&mask);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    close(sigfd);
    intercept_signal(sig, (handler_3_t)NULL, false);
}

int
main(void)
{
    test_signalfd(SIGXCPU);
    test_signalfd(SIGUSR1);
    test_signalfd(SIGSEGV);
    test_signalfd(44);
    return 0;
}
