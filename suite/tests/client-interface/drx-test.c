/* **********************************************************
 * Copyright (c) 2013-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2003 VMware, Inc.  All rights reserved.
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

#define _CRT_SECURE_NO_WARNINGS 1

#include "tools.h"

#ifdef UNIX
#    include <sys/wait.h>
#    include <stdlib.h>
#    include <unistd.h>
#    include <string.h>
#    include <signal.h>
#endif

#ifdef WINDOWS
static void
fatal_error(const char *function)
{
    char message[256];

    _snprintf(message, BUFFER_SIZE_ELEMENTS(message),
              "Function %s() failed!\nError code 0x%x.\nExiting now.\n", function,
              GetLastError());
    print(message);
    exit(1);
}
#endif

int
main(int argc, char **argv)
{
#ifdef WINDOWS
#    define CMDLINE_SIZE (MAX_PATH /*argv[0]*/ + 20 /*" %p"*/)
    HANDLE event;
    char cmdline[CMDLINE_SIZE];

    if (argc == 1) {
        /* parent process */
        STARTUPINFO si = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION pi;
        HANDLE job, job2, job3;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit = {
            0,
        };
        DWORD exitcode = (DWORD)-1;

        /* For synchronization we create an inherited event */
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE /*inherit*/ };
        event = CreateEvent(&sa, FALSE /*manual reset*/, FALSE /*start unset*/, NULL);
        if (event == NULL)
            print("Failed to create event");

        _snprintf(cmdline, BUFFER_SIZE_ELEMENTS(cmdline), "%s %p", argv[0], event);

        print("creating child #1\n");
        if (!CreateProcess(argv[0], cmdline, NULL, NULL, TRUE /*inherit handles*/, 0,
                           NULL, NULL, &si, &pi))
            fatal_error("CreateProcess");
        if (WaitForSingleObject(event, INFINITE) == WAIT_FAILED)
            fatal_error("WaitForSingleObject");
        print("terminating child #1 by NtTerminateProcess\n");
        if (!TerminateProcess(pi.hProcess, 42))
            fatal_error("TerminateProcess");
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            fatal_error("WaitForSingleObject");
        GetExitCodeProcess(pi.hProcess, &exitcode);
        print("child #1 exit code = %d\n", exitcode);
        if (!ResetEvent(event))
            fatal_error("ResetEvent");

        print("creating child #2\n");
        /* In an msys shell, CREATE_BREAKAWAY_FROM_JOB is required because the shell uses
         * job objects and by default does not have permission to break away (i#1454).
         */
        if (!CreateProcess(argv[0], cmdline, NULL, NULL, TRUE /*inherit handles*/,
                           CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB, NULL, NULL, &si,
                           &pi)) {
            print("CreateProcess |%s| |%s| failure: 0x%x\n", argv[0], cmdline,
                  GetLastError());
        }
        job = CreateJobObject(NULL, "drx-test job");
        if (!AssignProcessToJobObject(job, pi.hProcess))
            fatal_error("AssignProcessToJobObject");
        if (!ResumeThread(pi.hThread))
            fatal_error("ResumeThread");
        if (!CloseHandle(pi.hThread))
            fatal_error("CloseHandle");
        if (!(WaitForSingleObject(event, INFINITE) != WAIT_FAILED))
            fatal_error("WaitForSingleObject");
        print("terminating child #2 by NtTerminateJobObject\n");
        if (!TerminateJobObject(job, 123456))
            fatal_error("TerminateJobObject");
        if (!CloseHandle(job))
            fatal_error("CloseHandle");
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            fatal_error("WaitForSingleObject");
        GetExitCodeProcess(pi.hProcess, &exitcode);
        print("child #2 exit code = %d\n", exitcode);
        if (!ResetEvent(event))
            fatal_error("ResetEvent");

        print("creating child #3\n");
        if (!CreateProcess(argv[0], cmdline, NULL, NULL, TRUE /*inherit handles*/,
                           CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB, NULL, NULL, &si,
                           &pi))
            print("CreateProcess failure\n");
        job = CreateJobObject(NULL, "drx-test job");
        if (!AssignProcessToJobObject(job, pi.hProcess))
            fatal_error("AssignProcessToJobObject");
        limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limit,
                                     sizeof(limit)))
            fatal_error("SetInformationJobObject");
        if (!ResumeThread(pi.hThread))
            fatal_error("ResumeThread");
        if (!CloseHandle(pi.hThread))
            fatal_error("CloseHandle");
        if (WaitForSingleObject(event, INFINITE) == WAIT_FAILED)
            fatal_error("WaitForSingleObject");
        print("terminating child #3 by closing job handle\n");
        if (!CloseHandle(job))
            fatal_error("CloseHandle");
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            fatal_error("WaitForSingleObject");
        if (!GetExitCodeProcess(pi.hProcess, &exitcode))
            fatal_error("GetExitCodeProcess");
        print("child #3 exit code = %d\n", exitcode);

        /* Test DuplicateHandle (DrMem i#1401) */
        print("creating child #4\n");
        if (!CreateProcess(argv[0], cmdline, NULL, NULL, TRUE /*inherit handles*/,
                           CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB, NULL, NULL, &si,
                           &pi))
            fatal_error("CreateProcess");
        job = CreateJobObject(NULL, "drx-test job");
        if (!AssignProcessToJobObject(job, pi.hProcess))
            fatal_error("AssignProcessToJobObject");
        limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limit,
                                     sizeof(limit)))
            fatal_error("SetInformationJobObject");
        if (!DuplicateHandle(GetCurrentProcess(), job, GetCurrentProcess(), &job2, 0,
                             FALSE, DUPLICATE_SAME_ACCESS))
            fatal_error("DuplicateHandle");
        if (!DuplicateHandle(GetCurrentProcess(), job, GetCurrentProcess(), &job3, 0,
                             FALSE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS))
            fatal_error("DuplicateHandle");
        if (!ResumeThread(pi.hThread))
            fatal_error("ResumeThread");
        if (!CloseHandle(pi.hThread))
            fatal_error("CloseHandle");
        if (WaitForSingleObject(event, INFINITE) == WAIT_FAILED)
            fatal_error("WaitForSingleObject");
        print("terminating child #4 by closing both job handles\n");
        if (!CloseHandle(job2))
            fatal_error("CloseHandle");
        if (!CloseHandle(job3))
            fatal_error("CloseHandle");
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            fatal_error("WaitForSingleObject");
        if (!GetExitCodeProcess(pi.hProcess, &exitcode))
            fatal_error("GetExitCodeProcess");
        print("child #4 exit code = %d\n", exitcode);
    } else { /* child process */
        int iter = 0;
        if (sscanf(argv[1], "%p", &event) != 1) {
            print("Failed to obtain event handle from %s\n", argv[1]);
            return -1;
        }
        if (!SetEvent(event))
            print("Failed to set event\n");
        /* spin until parent kills us or we time out */
        while (iter++ < 12) {
            Sleep(5000);
        }
    }

    CloseHandle(event);

#else /* WINDOWS */

    int pipefd[2];
    pid_t cpid;
    char buf = 0;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    print("creating child\n");
    cpid = fork();
    if (cpid == -1) {
        perror("fork");
        exit(1);
    } else if (cpid > 0) {
        /* parent */
        int status;
        close(pipefd[1]); /* close unused write end */
        if (read(pipefd[0], &buf, sizeof(buf)) <= 0) {
            perror("pipe read failed");
            exit(1);
        }
        print("terminating child by sending SIGKILL\n");
        kill(cpid, SIGKILL);
        wait(&status); /* wait for child */
        close(pipefd[0]);
        print("child exit code = %d\n", status);
    } else {
        /* child */
        int iter = 0;
        close(pipefd[0]); /* close unused read end */
        write(pipefd[1], &buf, sizeof(buf));
        close(pipefd[1]);
        /* spin until parent kills us or we time out */
        while (iter++ < 12) {
            sleep(5);
        }
    }

#endif /* UNIX */

    return 0;
}
