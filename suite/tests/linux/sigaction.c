/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
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

/*
 * test of sigaction
 */
#include "tools.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SENTINEL 0x12345678UL

#if !defined(MACOS) && !defined(X64)
typedef struct old_sigaction_t {
    void (*handler)(int, siginfo_t *, void *);
    unsigned int sa_mask;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
} old_sigaction_t;
#endif

static void
test_query(int sig)
{
    /* i#1984: test that the prior action is returned */
    int rc;
    struct sigaction first_act;
    struct sigaction new_act;
    struct sigaction old_act;
    memset((void *)&first_act, 0, sizeof(first_act));
    first_act.sa_sigaction = (void (*)(int, siginfo_t *, void *))SENTINEL;
    sigemptyset(&first_act.sa_mask);
    sigaddset(&first_act.sa_mask, SIGUSR1);
    sigaddset(&first_act.sa_mask, SIGUSR2);
    rc = sigaction(sig, &first_act, NULL);
    assert(rc == 0);

    /* Test with nothing. */
    rc = sigaction(sig, NULL, NULL);
    assert(rc == 0);

    /* Test without a new action. */
    memset((void *)&old_act, 0xff, sizeof(old_act));
    rc = sigaction(sig, NULL, &old_act);
    assert(rc == 0 && old_act.sa_sigaction == first_act.sa_sigaction &&
           /* The flags do not match due to SA_RESTORER. */
           /* The rest of mask is uninit stack values from the libc wrapper. */
           *(long *)&old_act.sa_mask == *(long *)&first_act.sa_mask);

    /* Test with a new action. */
    memset((void *)&old_act, 0xff, sizeof(old_act));
    memset((void *)&new_act, 0, sizeof(new_act));
    new_act.sa_sigaction = (void (*)(int, siginfo_t *, void *))SIG_IGN;
    sigemptyset(&new_act.sa_mask);
    rc = sigaction(sig, &new_act, &old_act);
    assert(rc == 0 && old_act.sa_sigaction == first_act.sa_sigaction &&
           /* The flags do not match due to SA_RESTORER. */
           /* The rest of mask is uninit stack values from the libc wrapper. */
           *(long *)&old_act.sa_mask == *(long *)&first_act.sa_mask);

    /* Test pattern from i#1984 issue and ensure no assert. */
    memset(&new_act, 0, sizeof(new_act));
    memset(&old_act, 0, sizeof(old_act));
    new_act.sa_sigaction = (void (*)(int, siginfo_t *, void *))SENTINEL;
    sigaction(SIGINT, &new_act, 0);
    sigaction(SIGINT, &new_act, &old_act);
    new_act.sa_handler = SIG_IGN;
    sigaction(SIGTSTP, &new_act, &old_act);
}

static void
set_sigaction_handler(int sig, void *action)
{
    int rc;
    struct sigaction act;
    memset((void *)&act, 0, sizeof(act));
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))action;
    /* Arm the signal. */
    rc = sigaction(sig, &act, NULL);
    assert(rc == 0);
}

#if !defined(MACOS) && !defined(X64)
static void
test_non_rt_sigaction(int sig)
{
    int rc;
    old_sigaction_t first_act;
    old_sigaction_t new_act;
    old_sigaction_t old_act;
    memset((void *)&first_act, 0, sizeof(first_act));
    first_act.handler = (void (*)(int, siginfo_t *, void *))SENTINEL;
    first_act.sa_mask |= (1 << SIGUSR1);
    first_act.sa_mask |= (1 << SIGUSR2);
    rc = dynamorio_syscall(SYS_sigaction, 3, sig, &first_act, NULL);
    assert(rc == 0);

    /* Test with nothing. */
    rc = dynamorio_syscall(SYS_sigaction, 3, sig, NULL, NULL);
    assert(rc == 0);

    /* Test passing NULL to non-rt sigaction, which is used on Android (i#1822) */
    memset((void *)&old_act, 0xff, sizeof(old_act));
    rc = dynamorio_syscall(SYS_sigaction, 3, sig, NULL, &old_act);
    assert(rc == 0 && old_act.handler == first_act.handler &&
           /* The flags do not match due to SA_RESTORER. */
           /* The rest of mask is uninit stack values from the libc wrapper. */
           *(long *)&old_act.sa_mask == *(long *)&first_act.sa_mask);

    /* Test with a new action. */
    memset((void *)&old_act, 0xff, sizeof(old_act));
    memset((void *)&new_act, 0, sizeof(new_act));
    new_act.handler = (void (*)(int, siginfo_t *, void *))SIG_IGN;
    rc = dynamorio_syscall(SYS_sigaction, 3, sig, &new_act, &old_act);
    assert(rc == 0 && old_act.handler == first_act.handler &&
           /* The flags do not match due to SA_RESTORER. */
           /* The rest of mask is uninit stack values from the libc wrapper. */
           *(long *)&old_act.sa_mask == *(long *)&first_act.sa_mask);

    /* Clear handler */
    memset((void *)&new_act, 0, sizeof(new_act));
    rc = dynamorio_syscall(SYS_sigaction, 3, sig, &new_act, NULL);
    assert(rc == 0);
}
#endif

int
main(int argc, char **argv)
{
    test_query(SIGTERM);
#if !defined(MACOS) && !defined(X64)
    test_non_rt_sigaction(SIGPIPE);
#endif
    set_sigaction_handler(SIGTERM, (void *)SIG_IGN);
    print("Sending SIGTERM first time\n");
    kill(getpid(), SIGTERM);
    set_sigaction_handler(SIGTERM, (void *)SIG_DFL);
    print("Sending SIGTERM second time\n");
    kill(getpid(), SIGTERM);
    print("Should not be reached\n");
}
