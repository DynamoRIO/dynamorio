/* **********************************************************
 * Copyright (c) 2012-2013 Google, Inc.  All rights reserved.
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

/*
 * test nudging another process (i#742)
 * also tests dr_exit_process() (i#743)
 */

#define _CRT_SECURE_NO_WARNINGS 1

#include "tools.h"

#ifdef UNIX
#    include <sys/wait.h> /* for wait */
#    include <stdlib.h>
#    include <unistd.h>
#    include <string.h>
#    include <assert.h>
#else
#    include "windows.h"
#endif

volatile int val;

static void
foo(void)
{
    /* avoid inlining */
    val = 4;
}

static void
child_is_ready(void)
{
    NOP_NOP_CALL(foo);
}

int
main(int argc, char **argv)
{
#ifdef UNIX
    int pipefd[2];
    char buf = 0;
    pid_t child;
    /* i#1799-c#1: call foo to avoid foo being optimized away by clang */
    foo();
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }
    child = fork();
    if (child < 0) {
        perror("ERROR on fork");
    } else if (child > 0) {
        /* parent */
        pid_t result;
        int status = 0;
        /* wait for child to start up */
        close(pipefd[1]); /* close unused write end */
        if (read(pipefd[0], &buf, sizeof(buf)) <= 0) {
            perror("pipe read failed");
            exit(1);
        }
        /* notify client */
        child_is_ready();
        /* don't print here: could be out-of-order wrt client prints */
        result = waitpid(child, &status, 0);
        assert(result == child);
        print("child has exited with status %d\n",
              WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    } else {
        /* client nudge handler will terminate us earlier */
        int left = 20;
        close(pipefd[0]); /* close unused read end */
        /* notify parent */
        write(pipefd[1], &buf, sizeof(buf));
        close(pipefd[1]);
        while (left > 0)
            left = sleep(left); /* unfortunately, nudge signal interrupts us */
    }
#else
#    define CMDLINE_SIZE (MAX_PATH /*argv[0]*/ + 20 /*" %p"*/)
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    char cmdline[CMDLINE_SIZE];
    HANDLE event;
    if (argc == 1) {
        /* parent */

        /* for synchronization we create an inherited event */
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE /*inherit*/ };
        event = CreateEvent(&sa, FALSE /*manual reset*/, FALSE /*start unset*/, NULL);
        if (event == NULL)
            print("Failed to create event");

        _snprintf(cmdline, BUFFER_SIZE_ELEMENTS(cmdline), "%s %p", argv[0], event);
        if (!CreateProcess(argv[0], cmdline, NULL, NULL, TRUE /*inherit handles*/, 0,
                           NULL, NULL, &si, &pi))
            print("ERROR on CreateProcess\n");
        else {
            int status;
            /* wait for child */
            WaitForSingleObject(event, INFINITE);
            /* notify client */
            child_is_ready();
            /* wait for termination */
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, (LPDWORD)&status);
            print("child has exited with status %d\n", status);
            CloseHandle(pi.hProcess);
        }
    } else {
        /* child */
        if (sscanf(argv[1], "%p", &event) != 1) {
            print("Failed to obtain event handle from %s\n", argv[1]);
            return -1;
        }
        if (!SetEvent(event))
            print("Failed to set event\n");
        Sleep(20000);
    }
#endif
    print("app exiting\n");
    return 0;
}
