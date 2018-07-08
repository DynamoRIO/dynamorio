/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2010 VMware, Inc.  All rights reserved.
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

/* case 3105 test cases for rapidly starting and terminating threads */
#include "tools.h"
#include "thread.h"

typedef unsigned char *app_pc;

/* not including main thread */
#ifndef NIGHTLY_REGRESSION
#    define NIGHTLY_REGRESSION
#endif

#ifdef NIGHTLY_REGRESSION
#    define SAFE_NATIVE
enum { TOTAL_THREADS = 2 };
#else
// #define SAFE_NATIVE  // brutal NtTerminateProcess(0
enum { TOTAL_THREADS = 200 };
#endif

// #define VERBOSE

#define ALL_RACES /* anything goes: start/stop */

#ifdef SAFE_NATIVE
/* nothing fancy:
 * start up some threads and then exit the process
 */
enum { ROUNDS = 1 };
#else
/* calling NtTerminateProcess(0) is in very unsafe - although often works well */
enum { ROUNDS = 10 };
#endif

#ifdef ALL_RACES
/* for thread start races terminate early */
enum {
    WAIT_TO_START = 1,
    WAIT_TO_FINISH = TOTAL_THREADS / 10,
};
#else
/* for thread stop races wait for everyone to start */
enum {
    WAIT_TO_START = TOTAL_THREADS,
    WAIT_TO_FINISH = TOTAL_THREADS / 10,
};
#endif

#define UNIPROC

enum { LOOP_WORK = 100 };

/* --------- */
HANDLE thread[TOTAL_THREADS];

long global_started = 0;  /* unsynchronized */
long global_finished = 0; /* unsynchronized */

#ifdef UNIPROC
#    define YIELD() thread_yield() /* or do nothing on a multi processor */
#else
#    define YIELD() /* or do nothing on a multi processor */
#endif

int
compare(const void *arg1, const void *arg2)
{
    /* Compare all of both strings: */
    return _stricmp(*(char **)arg1, *(char **)arg2);
}

void
sort()
{
    int argc = 5;
    char *argv[] = { "one", "two", "three", "five", "six", "unsorted" };
#ifdef VERY_VERBOSE
    int i;
#endif

    /* Sort remaining args using Quicksort algorithm: */
    qsort((void *)argv, (size_t)argc, sizeof(char *), compare);

#ifdef VERY_VERBOSE
    /* Output sorted list: */
    for (i = 0; i < argc; ++i)
        print(" %s", argv[i]);
    print("\n");
#endif
}

unsigned int __stdcall executor(void *arg)
{
    int w;
    sort(); /* do some work */
    InterlockedIncrement(&global_started);

    for (w = 0; w < LOOP_WORK; w++) {
        sort(); /* do more work */
        if (w % 10 == 0)
            YIELD();
    }
    InterlockedIncrement(&global_finished);

    /* TODO: could thread_suspend(a paired thread) */
    return 0;
}

int
main()
{
    int round;
    int t;

    GET_NTDLL(NtTerminateProcess,
              (IN HANDLE ProcessHandle OPTIONAL, IN NTSTATUS ExitStatus));

    INIT();

    for (round = 0; round < ROUNDS; round++) {
#ifdef VERBOSE
        print("round %d\n", round);
#endif
        if (round > 0) {
            /* clean up first */
            NtTerminateProcess(0 /* everyone but me */, 666);
#ifdef VERBOSE
            print("all alone again %d\n", round);
#endif VERBOSE
        }

        global_started = 0;
        global_finished = 0;

        for (t = 0; t < TOTAL_THREADS; t++) {
            thread[t] = (HANDLE)create_thread(executor, NULL);
            if (thread[t] == NULL)
                print("GLE: %d\n", GetLastError());
            assert(thread[t] != NULL);
        }

#ifdef VERBOSE
        print("started %d threads\n", TOTAL_THREADS);
#else
        print("started some threads\n");
#endif

        /* wait for some to start */
        while (global_started < WAIT_TO_START)
            YIELD();

        /* wait for some work to get done */
        while (global_finished < WAIT_TO_FINISH)
            YIELD();
#ifdef VERBOSE
        print("some %d work, done %d\n", global_started, global_finished);
#endif
    }

    print("done\n");
    return 0;
}

/*

What is this about?

The variable pointed to by the Addend parameter must be aligned on a
32-bit boundary; otherwise, this function will fail on multiprocessor
x86 systems and any non-x86 systems.

0:000> u kernel32!interlockedincrement
kernel32!InterlockedIncrement:
7c80977b 8b4c2404         mov     ecx,[esp+0x4]
7c80977f b801000000       mov     eax,0x1
7c809784 f00fc101         lock    xadd [ecx],eax
7c809788 40               inc     eax
7c809789 c20400           ret     0x4
7c80978c 8d4900           lea     ecx,[ecx]


when setting THREADS=1000
---------------------------
threadexit.exe - Illegal System DLL Relocation
---------------------------
The system DLL user32.dll was relocated in memory. The application will not run properly.
The relocation occurred because the DLL Dynamically Allocated Memory occupied an address
range reserved for Windows system DLLs. The vendor supplying the DLL should be contacted
for a new DLL.
---------------------------
OK
---------------------------
Assertion failed: thread[t] != NULL, file win32/threadexit.c, line 180

LastStatusValue: (NTSTATUS) 0xc0000017 - {Not Enough Quota}  Not enough virtual memory or
paging file quota is available to complete the specified operation.



*/
