/* **********************************************************
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <process.h>
#include "tools.h"

#define MAX_THREADS 32
//////////////////////////////////////////////////////////////////
// Prototypes
int
main(int argc, char *argv[]); // Thread 1: main
DWORD WINAPI
ThreadProc(LPVOID lpParam); // Thread 2: Sleep
DWORD WINAPI
ThreadProc2(LPVOID lpParam); // Thread 3: Exit immediately
//////////////////////////////////////////////////////////////////

typedef struct _Parameters {
    BOOL bAll;
    BOOL bGetContext;
    BOOL bSetContext;
    BOOL bSuspend;
    BOOL bVerbose;
    int nPID;
    int nSleepTime;
} PARAMETERS, *LPPARAMETERS;

#define YIELD() Sleep(0)

int ThreadNr; /* Number of threads started */

BOOL thread_proc_wait = FALSE;
BOOL thread_proc_waiting = FALSE;

DWORD WINAPI
ThreadProc(LPVOID lpParam)
{

    int nSleepTime;

    nSleepTime = (*(int *)lpParam);

    /* a simple spinning synchronization for the TerminateThread test */
    while (thread_proc_wait) {
        thread_proc_waiting = TRUE;
        YIELD();
    }
    thread_proc_waiting = FALSE;

    if (nSleepTime < 5000) {
        Sleep(nSleepTime);
    }

    return 0;
}

DWORD WINAPI
ThreadProc2(LPVOID lpParam)
{

    ExitThread(-1);

    // You should never get here
    return 0;
}

void
LaunchTest(char szCommandLine[], PARAMETERS myParameters)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (myParameters.bVerbose == TRUE) {
        print("------------------------------------------------------------\n");
        print("Test beginning with options: %s\n", szCommandLine);
        print("------------------------------------------------------------\n");
    }

    if (0 ==
        CreateProcess(NULL, szCommandLine, NULL, NULL, FALSE, 0, NULL, ".", &si, &pi)) {
        print("Error creating process:\n\"%s\"\n", szCommandLine);
    } else if (myParameters.bVerbose == TRUE) {
        print("------------------------------------------------------------\n");
        print("Test completed: %s options executed\n", szCommandLine);
        print("------------------------------------------------------------\n");
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
}

void
LaunchAllTests(char *argv[], PARAMETERS myParameters)
{
    char szCommandLine[512];

    // Remote thread test
    // Launch a separate process that injects threads into this one
    // Need to pass this process ID in the arguments

    sprintf(szCommandLine, "%s /PID=%d\n", argv[0], GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // GetContextThread()
    // Launch a separate process that creates threads and attempts to GetContextThread()
    // without suspending or pausing

    sprintf(szCommandLine, "%s /PID=%d /GETCONTEXT\n", argv[0], GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // SetContextThread()
    // Launch a separate process that creates threads and attempts to SetContextThread()
    // without suspending or pausing

    sprintf(szCommandLine, "%s /PID=%d /SETCONTEXT\n", argv[0], GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // SuspendThread()
    // Launch a separate process that creates threads and attempts to SuspendThread()
    // and ResumeThread() without pausing

    sprintf(szCommandLine, "%s /PID=%d /SUSPEND\n", argv[0], GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // GetContextThread() with SuspendThread()
    // Launch a separate process that creates threads, suspends them, and attempts
    // to GetContextThread() without pausing

    sprintf(szCommandLine, "%s /PID=%d /GETCONTEXT /SUSPEND\n", argv[0],
            GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // SetContextThread() with SuspendThread()
    // Launch a separate process that creates threads, suspends them, and attempts
    // to SetContextThread() without pausing

    sprintf(szCommandLine, "%s /PID=%d /SETCONTEXT /SUSPEND\n", argv[0],
            GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // SuspendThread() with Sleep()
    // Launch a separate process that creates threads, pauses for 5 seconds, and
    // attempts to SuspendThread() and ResumeThread()

    sprintf(szCommandLine, "%s /PID=%d /SUSPEND /SLEEP=10\n", argv[0],
            GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // GetContextThread() with Sleep()
    // Launch a separate process that creates threads, pauses for 5 seconds, and
    // attempts to GetContextThread()

    sprintf(szCommandLine, "%s /PID=%d /GETCONTEXT /SLEEP=10", argv[0],
            GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // SetContextThread() with Sleep()
    // Launch a separate process that creates threads, pauses for 5 seconds, and
    // attempts to SetContextThread()

    sprintf(szCommandLine, "%s /PID=%d /SETCONTEXT /SLEEP=10", argv[0],
            GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // GetContextThread() with Sleep() & SuspendThread()
    // Launch a separate process that creates threads, pauses for 5 seconds,
    // suspends the thread & attempts to GetContextThread()

    sprintf(szCommandLine, "%s /PID=%d /GETCONTEXT /SLEEP=10 /SUSPEND", argv[0],
            GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);

    // SetContextThread() with Sleep() & SuspendThread()
    // Launch a separate process that creates threads, pauses for 5 seconds,
    // suspends the thread & attempts to SetContextThread()

    sprintf(szCommandLine, "%s /PID=%d /SETCONTEXT /SLEEP=10 /SUSPEND", argv[0],
            GetCurrentProcessId());
    LaunchTest(szCommandLine, myParameters);
}

void
Usage()
{
    print("\nInjectionTests");
    print("\nCreates process, threads & remote threads and tests thread corner cases");
    print("\n");
    print("\nUsage: InjectionTests [Options]");
    print("\n/ALL          - Run all tests");
    print("\n/GETCONTEXT   - Call GetContextThread() after thread creation");
    print("\n/SETCONTEXT   - Call SetContextThread() after thread creation");
    print("\n/SLEEP=n      - Sleep for n milliseconds immediately after thread creation");
    print("\n/SUSPEND      - Call SuspendThread() prior to other thread functions");
    print("\n/PID=n        - Inject threads into remote process with PID n");
    print("\n/VERBOSE      - Enable individual test logging");
    print("\n/HELP, /?     - Prints this message");
    exit(0);
}

void
InitializeArguments(LPPARAMETERS myParameters)
{
    myParameters->bAll = FALSE;
    myParameters->bGetContext = FALSE;
    myParameters->bSetContext = FALSE;
    myParameters->bSuspend = FALSE;
    myParameters->bVerbose = FALSE;
    myParameters->nPID = 0;
    myParameters->nSleepTime = 0;
}

void
ParseArguments(int argc, char *argv[], LPPARAMETERS myParameters)
{
    int i;

    for (i = 1; i < argc; i++) {
        strupr(argv[i]);

        if (!strcmp(argv[i], "/?") || !strcmp(argv[i], "/HELP")) {
            Usage();
        }

        if (!strncmp(argv[i], "/PID=", 5)) {
            // Skip over argument, save PID
            myParameters->nPID = atoi(argv[i] + 5);
        } else if (!strncmp(argv[i], "/SLEEP=", 7)) {
            // Skip over argument, save SleepTime
            myParameters->nSleepTime = atoi(argv[i] + 7);
        } else if (!strncmp(argv[i], "/GETCONTEXT", 11)) {
            myParameters->bGetContext = TRUE;
        } else if (!strncmp(argv[i], "/SETCONTEXT", 11)) {
            myParameters->bSetContext = TRUE;
        } else if (!strncmp(argv[i], "/SUSPEND", 8)) {
            myParameters->bSuspend = TRUE;
        } else if (!strncmp(argv[i], "/VERBOSE", 8)) {
            myParameters->bVerbose = TRUE;
        } else if (!strncmp(argv[i], "/ALL", 4)) {
            myParameters->bAll = TRUE;
        }
    }
}

void
ExerciseThread(HANDLE hThread, PARAMETERS myParameters)
{

    CONTEXT Context;
    BOOL bError;

    if (myParameters.nSleepTime != 0) {
        Sleep(myParameters.nSleepTime);
    }

    if (myParameters.bSuspend == TRUE) {
        if ((SuspendThread(hThread) == -1) && (myParameters.bVerbose == TRUE)) {
            print("Error in SuspendThread()(Code %d)\n", GetLastError());
        }
        if ((ResumeThread(hThread) == -1) && (myParameters.bVerbose == TRUE)) {
            print("Error in ResumeThread() (Code %d)\n", GetLastError());
        }
    }
    if (myParameters.bGetContext == TRUE) {
        bError = GetThreadContext(hThread, &Context);
        if ((bError == 0) && (myParameters.bVerbose == TRUE)) {
            print("Error in GetThreadContext (Code %d)\n", GetLastError());
        }
    }
    if ((myParameters.bSetContext == TRUE) && (myParameters.bVerbose == TRUE)) {
        bError = SetThreadContext(hThread, &Context);
        if ((bError == 0) && (myParameters.bVerbose == TRUE)) {
            print("Error in SetThreadContext (Code %d)\n", GetLastError());
        }
    }
}

int
main(int argc, char *argv[]) /* Thread One */
{

    DWORD dwThreadID;
    HANDLE hProcess;
    HANDLE hThread1, hThread2, hThread3, hThread4;
    char szCommandLine[1024];
    int i;
    PARAMETERS myParameters;

    INIT();
    /* set exception handler */

    strcpy(szCommandLine, "\0");
    ThreadNr = 0;

    InitializeArguments(&myParameters);
    ParseArguments(argc, argv, &myParameters);

    if (argc == 1) {
        myParameters.bAll = TRUE;
    }

    // On initial call, no args are present; execute each subtest below
    if (myParameters.bAll == TRUE) {
        LaunchAllTests(argv, myParameters);
    } else {
        print("Entering thread with options:\n");
        for (i = 1; i < argc; i++) {
            if (!strncmp(argv[i], "/PID", 4)) {
                strcat(szCommandLine, "/PID");
            } else {
                strcat(szCommandLine, argv[i]);
            }
        }
        print("%s\n", szCommandLine);

        do {
            hThread1 = CreateThread(NULL, 0, &ThreadProc, &(myParameters.nSleepTime), 0,
                                    &dwThreadID);
            ExerciseThread(hThread1, myParameters);
            WaitForSingleObject(hThread1, INFINITE);
            ThreadNr++;

            thread_proc_wait = TRUE;
            hThread2 = CreateThread(NULL, 0, &ThreadProc, &(myParameters.nSleepTime), 0,
                                    &dwThreadID);
            while (!thread_proc_waiting) {
                YIELD();
            }
            TerminateThread(hThread2, -1);
            thread_proc_wait = FALSE;
            thread_proc_waiting = FALSE;
            ThreadNr++;

            // ThreadProc2 calls ExitThread() immediately
            hThread3 = CreateThread(NULL, 0, &ThreadProc2, &ThreadNr, 0, &dwThreadID);
            WaitForSingleObject(hThread3, INFINITE);
            ThreadNr++;

            if (hThread1 != NULL) {
                CloseHandle(hThread1);
            }
            if (hThread2 != NULL) {
                CloseHandle(hThread2);
            }
            if (hThread3 != NULL) {
                CloseHandle(hThread3);
            }

        } while (ThreadNr < MAX_THREADS);

        if (myParameters.nPID != 0) {
            // Prints out results in host PID
            hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, myParameters.nPID);
            if ((hProcess == NULL) && (myParameters.bVerbose == TRUE)) {
                print("Error in OpenProcess(Code %d)\n", GetLastError());
            }

            hThread4 = CreateRemoteThread(hProcess, 0, 0, &ThreadProc,
                                          &(myParameters.nSleepTime), 0, &dwThreadID);

            if ((hThread4 == NULL) && (myParameters.bVerbose == TRUE)) {
                print("Error in CreateRemoteThread(Code %d)\n", GetLastError());
            }
            WaitForSingleObject(hThread4, INFINITE);

            if (hThread4 != NULL) {
                CloseHandle(hThread4);
            }
        }
        print("Exiting thread with options:\n");
        print("%s\n", szCommandLine);
    }

    return 0;
}
