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

static void
set_sigaction_handler(int sig, void *action)
{
    int rc;
    struct sigaction act;
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *)) action;
    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    assert(rc == 0);
}

#ifndef X64
static void
test_non_rt_sigaction(int sig)
{
    /* Test passing NULL to non-rt sigaction, which is used on Android (i#1822) */
    int rc;
    struct sigaction oact;
    /* Set to a bogus value to ensure the kernel returns something for the old action */
    oact.sa_sigaction = (void (*)(int, siginfo_t *, void *)) 42L;
    rc = dynamorio_syscall(SYS_sigaction, 3, sig, NULL, &oact);
    assert(rc == 0);
    assert(oact.sa_sigaction != (void (*)(int, siginfo_t *, void *)) 42L);
}
#endif

int
main(int argc, char **argv)
{
#ifndef X64
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
