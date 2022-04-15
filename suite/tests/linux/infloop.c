/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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

/* An app that stays up long enough for testing nudges. */
#include "tools.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>

static void
signal_handler(int sig)
{
    if (sig == SIGTERM) {
        print("done\n");
    }
    exit(1);
}

int
main(int argc, const char *argv[])
{
    int arg_offs = 1;
    long long counter = 0;
    bool for_attach = false, block = false;
    while (arg_offs < argc && argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-v") == 0) {
            /* enough verbosity to satisfy runall.cmake: needs an initial and a
             * final line
             */
            intercept_signal(SIGTERM, (handler_3_t)signal_handler, false);
            print("starting\n");
            arg_offs++;
        } else if (strcmp(argv[arg_offs], "-attach") == 0) {
            for_attach = true;
            arg_offs++;
        } else if (strcmp(argv[arg_offs], "-block") == 0) {
            block = true;
            arg_offs++;
        } else
            return 1;
    }

    int pipefd[2];
    if (block) {
        /* Create something we can block reading.  Stdin is too risky: in the
         * test suite it has data.
         */
        if (pipe(pipefd) == -1) {
            perror("pipe");
            return 1;
        }
    }

    while (1) {
        /* XXX i#38: We're seeing mprotect fail strangely on attach right before
         * DR takes over.  For now we avoid it in that test.
         */
        if (!for_attach) {
            /* workaround for PR 213040 and i#1087: prevent loop from being coarse
             * by using a non-ignorable system call
             */
            protect_mem((void *)signal_handler, 1, ALLOW_READ | ALLOW_EXEC);
        }
        /* Don't spin forever to avoid hosing machines if test harness somehow
         * fails to kill.  15 billion syscalls takes ~ 1 minute.
         */
        counter++;
        if (counter > 15 * 1024 * 1024 * 1024LL) {
            print("hit max iters\n");
            break;
        }
        if (block) {
            /* Make a blocking syscall, but not forever to again guard against
             * a runaway test.
             */
            struct timeval timeout;
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            fd_set set;
            FD_ZERO(&set);
            FD_SET(pipefd[0], &set);
            int res = select(pipefd[0] + 1, &set, NULL, NULL, &timeout);
            /* For some kernels: our attach interrupts the syscall (which is not an
             * auto-restart syscall) and returns EINTR.  Don't print on EINTR nor on a
             * timeout as both can happen depending on the timing of the attach.
             */
            if (res == -1 && errno != EINTR)
                perror("select error");

            /* XXX i#38: We may want a test of an auto-restart syscall as well
             * once the injector handles that.
             */
        }
    }

    if (block) {
        close(pipefd[0]);
        close(pipefd[1]);
    }

    return 0;
}
