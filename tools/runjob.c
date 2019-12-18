/* **********************************************************
 * Copyright (c) 2007 VMware, Inc.  All rights reserved.
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

#define _WIN32_WINNT 0x0500
/* for Jobs on Win2000+ */

#include <windows.h>
#include <assert.h>
#include <stdio.h>

#include "share.h"

#define FP stderr

#ifndef USING_SDK

typedef struct _IO_COUNTERS {
    ULONGLONG ReadOperationCount;
    ULONGLONG WriteOperationCount;
    ULONGLONG OtherOperationCount;
    ULONGLONG ReadTransferCount;
    ULONGLONG WriteTransferCount;
    ULONGLONG OtherTransferCount;
} IO_COUNTERS;
typedef IO_COUNTERS *PIO_COUNTERS;

/* FIXME: this is really screwed up: they have added a field in the
 * object and
 * LastErrorValue: (Win32) 0x18 (24) - The program issued a command but the command length
 * is incorrect.
 */
typedef struct _NEW_JOBOBJECT_BASIC_LIMIT_INFORMATION {
    LARGE_INTEGER PerProcessUserTimeLimit;
    LARGE_INTEGER PerJobUserTimeLimit;
    DWORD LimitFlags;
    SIZE_T MinimumWorkingSetSize;
    SIZE_T MaximumWorkingSetSize;
    DWORD ActiveProcessLimit;
    ULONG *Affinity;
    DWORD PriorityClass;
    /* VC98 has only the above fields */

    DWORD SchedulingClass;
} NEW_JOBOBJECT_BASIC_LIMIT_INFORMATION;

typedef struct _JOBOBJECT_EXTENDED_LIMIT_INFORMATION {
    NEW_JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
    IO_COUNTERS IoInfo;
    SIZE_T ProcessMemoryLimit;
    SIZE_T JobMemoryLimit;
    SIZE_T PeakProcessMemoryUsed;
    SIZE_T PeakJobMemoryUsed;
} JOBOBJECT_EXTENDED_LIMIT_INFORMATION, *PJOBOBJECT_EXTENDED_LIMIT_INFORMATION;

#endif

void
StartRestrictedProcess(LPTSTR app_name, LPTSTR app_cmdline, SIZE_T pagefile_limit)
{
    // based on http://www.microsoft.com/msj/0399/jobkernelobj/jobkernelobj.aspx

    // Create a job kernel object
    HANDLE hjob = CreateJobObject(NULL, NULL);

    NEW_JOBOBJECT_BASIC_LIMIT_INFORMATION jobli = { 0 };
    JOBOBJECT_BASIC_UI_RESTRICTIONS jobuir;

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobext = { 0 };
    BOOL ok;

    // Place some restrictions on processes in the job

    // First, set some basic restrictions

    // The process always runs in the idle priority class
    jobli.PriorityClass = IDLE_PRIORITY_CLASS;

    // The job cannot use more than 10 second of CPU time
    jobli.PerJobUserTimeLimit.QuadPart = 10 * 10000000; // 1 second in 100-ns intervals

    // These are two 2 restrictions I want placed on the job (process)
    jobli.LimitFlags = JOB_OBJECT_LIMIT_PRIORITY_CLASS | JOB_OBJECT_LIMIT_JOB_TIME;
    ok = SetInformationJobObject(hjob, JobObjectBasicLimitInformation, &jobli,
                                 sizeof(jobli));
    if (!ok) {
        fprintf(FP, "GLE %d", GetLastError());
    }

    // Second, set some UI restrictions
    jobuir.UIRestrictionsClass = JOB_OBJECT_UILIMIT_NONE; // A fancy 0

    // The process can’t logoff the system
    jobuir.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_EXITWINDOWS;

    // The process can’t access USER object (like other windows) in the system
    jobuir.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_HANDLES;

    ok = SetInformationJobObject(hjob, JobObjectBasicUIRestrictions, &jobuir,
                                 sizeof(jobuir));
    if (!ok) {
        fprintf(FP, "GLE %d", GetLastError());
    }

    // actually we really want to set page file limits as well
    // and we'll set separately
    // The process always runs in the idle priority class
    jobext.ProcessMemoryLimit = pagefile_limit;
    jobext.BasicLimitInformation.LimitFlags =
        0x00000100 /* JOB_OBJECT_LIMIT_PROCESS_MEMORY */;

    ok = SetInformationJobObject(hjob, 9 /* FIXME: JobObjectExtendedLimitInformation */,
                                 &jobext, sizeof(jobext));
    if (!ok) {
        fprintf(FP, "GLE %d", GetLastError());
    }

    // Spawn the process that is to be in the job
    // Note: You must first spawn the process and then place the process in the job.
    //        This means that the process's thread must be initially suspended so that
    //        it can't execute any code outside of the job's restrictions.
    CreateProcess(app_name, app_cmdline, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL,
                  &si, &pi);

    // Place the process in the job.
    // Note: if this process spawns any children, the children are
    //        automatically associated with the same job
    AssignProcessToJobObject(hjob, pi.hProcess);

    // Now, we can allow the child process’s thread to execute code.
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    // Wait for the process to terminate or for all the job’s allotted CPU time to be used
    {
        DWORD dw;
        HANDLE h[2];
        h[0] = pi.hProcess;
        h[1] = hjob;
        dw = WaitForMultipleObjects(2, h, FALSE, INFINITE);
        switch (dw - WAIT_OBJECT_0) {
        case 0:
            // The process has terminated
            break;
        case 1:
            // All of the job's allotted CPU time or other limits were used
            fprintf(stderr, "job limit reached\n");
            break;
        }
    }
    // Clean-up properly
    CloseHandle(pi.hProcess);
    CloseHandle(hjob);
}

// FIXME: note that we can use a Job limit to precisely alot the CPU time
int
usage(char *us)
{
    fprintf(FP, "Usage: %s [-p page_limit in MB] [-kb] <program> <args...>\n", us);
    fflush(FP);
    return 0;
}

int __cdecl main(int argc, char *argv[], char *envp[])
{
    LPTSTR app_name = NULL;
    TCHAR full_app_name[2 * MAX_PATH];
    TCHAR app_cmdline[2 * MAX_PATH];
    int arg_offs = 1;

    int allocation_unit = 1024 * 1024;
    int pagelimit = 10 * allocation_unit; /* 10 MB */

    if (argc < 2) {
        return usage(argv[0]);
    }
    while (argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-p") == 0) {
            if (argc <= arg_offs + 1)
                return usage(argv[0]);
            pagelimit = atoi(argv[arg_offs + 1]) * allocation_unit;
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-kb") == 0) {
            allocation_unit = 1024;
            arg_offs += 1;
        }
    }

    /* FIXME: watch out for the usual /Program BUG */
    _snwprintf(full_app_name, BUFFER_SIZE_ELEMENTS(full_app_name), L"%hs",
               argv[arg_offs]);

    /* FIXME: only one argument taken */
    _snwprintf(app_cmdline, BUFFER_SIZE_ELEMENTS(app_cmdline), L"%hs %hs", argv[arg_offs],
               argv[arg_offs + 1]);

    StartRestrictedProcess(NULL /* FIXME: full_app_name */, app_cmdline, pagelimit);
    fprintf(FP, "done\n");
    fflush(FP);
    return 0;
}
