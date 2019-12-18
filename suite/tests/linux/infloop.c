/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
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
    while (arg_offs < argc && argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-v") == 0) {
            /* enough verbosity to satisfy runall.cmake: needs an initial and a
             * final line
             */
            intercept_signal(SIGTERM, (handler_3_t)signal_handler, false);
            print("starting\n");
            arg_offs++;
        } else
            return 1;
    }

    while (1) {
        /* workaround for PR 213040 and i#1087: prevent loop from being coarse
         * by using a non-ignorable system call
         */
        protect_mem((void *)signal_handler, 1, ALLOW_READ | ALLOW_EXEC);
        /* don't spin forever to avoid hosing machines if test harness somehow
         * fails to kill.  15 billion syscalls takes ~ 1 minute.
         */
        counter++;
        if (counter > 15 * 1024 * 1024 * 1024LL) {
            print("hit max iters\n");
            break;
        }
    }
    return 0;
}
