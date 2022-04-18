/* **********************************************************
 * Copyright (c) 2011-2015 Google, Inc.  All rights reserved.
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

#include "tools.h"
#include <stdio.h>

#ifdef WINDOWS

void
create_proc(char *cmd, char *arg1)
{
    PROCESS_INFORMATION pinfo;
    STARTUPINFO sinfo;
    char real_cmdline[1024];
    /* Windows wants argv[0] on cmdline. */
    _snprintf_s(real_cmdline, sizeof(real_cmdline), sizeof(real_cmdline), "%s %s", cmd,
                arg1);
    GetStartupInfo(&sinfo);
    if (!CreateProcess(cmd,          /* application name */
                       real_cmdline, /* command line */
                       NULL,         /* new proc cannot be inherited */
                       NULL,         /* new thread cannot be inherited */
                       TRUE,         /* inherit handles from this proc */
                       0,            /* no creation flags */
                       NULL,         /* use environment of this proc */
                       NULL,         /* same directory of this proc */
                       &sinfo,       /* start up info */
                       &pinfo        /* out: process information */
                       )) {
        print("ERROR creating new process: %s %s\n", cmd, arg1);
        exit(1);
    }
    /* Wait for the child for at least 90 secs (to avoid flakiness when
     * running the test suite: i#1414).
     */
    WaitForSingleObject(pinfo.hProcess, 90 * 1000);
}

#else

#    include <unistd.h>
#    include <sys/wait.h>
#    include <errno.h>

void
create_proc(const char *cmd, const char *arg1)
{
    const char *argv[3];
    pid_t pid;
    argv[0] = cmd;
    argv[1] = arg1;
    argv[2] = NULL;

    pid = fork();
    if (pid == 0) {
        /* child */
        execv(cmd, (char **)argv);
        print("EXEC FAILED: %s\n", strerror(errno));
        exit(1);
    } else {
        waitpid(pid, 0, 0);
    }
}
#endif

int
main(int argc, char **argv)
{
    if (argc == 1) {
        print("parent\n");
        create_proc(argv[0], "child");
        print("parent exiting\n");
    } else if (argc == 2 && strcmp(argv[1], "child") == 0) {
        print("child\n");
    }
}
