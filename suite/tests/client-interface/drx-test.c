/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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

int
main(int argc, char **argv)
{
    HANDLE event;
    char cmdline[128];

    if (argc == 1) {
        /* parent process */
        STARTUPINFO si = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION pi;
        HANDLE job;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit = {0,};

        /* For synchronization we create an inherited event */
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE/*inherit*/};
        event = CreateEvent(&sa, FALSE/*manual reset*/, FALSE/*start unset*/, NULL);
        if (event == NULL)
            print("Failed to create event");

        _snprintf(cmdline, BUFFER_SIZE_ELEMENTS(cmdline), "%s %p", argv[0], event);

        print("creating child #1\n");
        if (!CreateProcess(argv[0], cmdline, NULL, NULL, TRUE/*inherit handles*/,
                           0, NULL, NULL, &si, &pi))
            print("CreateProcess failure\n");
        WaitForSingleObject(event, INFINITE);
        print("terminating child #1 by NtTerminateProcess\n");
        TerminateProcess(pi.hProcess, 42);
        WaitForSingleObject(pi.hProcess, INFINITE);
        if (!ResetEvent(event))
            print("Failed to reset event\n");

        print("creating child #2\n");
        if (!CreateProcess(argv[0], cmdline, NULL, NULL, TRUE/*inherit handles*/,
                           CREATE_SUSPENDED, NULL, NULL, &si, &pi))
            print("CreateProcess failure\n");
        job = CreateJobObject(NULL, "drx-test job");
        AssignProcessToJobObject(job, pi.hProcess);
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        WaitForSingleObject(event, INFINITE);
        print("terminating child #2 by NtTerminateJobObject\n");
        TerminateJobObject(job, 123456);
        CloseHandle(job);
        WaitForSingleObject(pi.hProcess, INFINITE);
        if (!ResetEvent(event))
            print("Failed to reset event\n");

        print("creating child #3\n");
        if (!CreateProcess(argv[0], cmdline, NULL, NULL, TRUE/*inherit handles*/,
                           CREATE_SUSPENDED, NULL, NULL, &si, &pi))
            print("CreateProcess failure\n");
        job = CreateJobObject(NULL, "drx-test job");
        AssignProcessToJobObject(job, pi.hProcess);
        limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                     &limit, sizeof(limit)))
            print("SetInformationJobObject failed");
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        WaitForSingleObject(event, INFINITE);
        print("terminating child #3 by closing job handle\n");
        CloseHandle(job);
        WaitForSingleObject(pi.hProcess, INFINITE);
    }
    else { /* child process */
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
    return 0;
}
