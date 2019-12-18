/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <winbase.h> /* for QueueUserAPC */
#include <process.h> /* for _beginthreadex */
#include <stdio.h>

static BOOL synch_1 = TRUE;
static BOOL synch_2 = TRUE;
static int result = 0;
static ULONG_PTR apc_arg = 0;

unsigned int WINAPI
run_func(void *arg)
{
    int res;
    synch_2 = FALSE;
    while (synch_1) {
        /* need non alertable thread yield function */
        SwitchToThread();
    }
    /* now the alertable system call */
    res = SleepEx(100, 1);
    /* is going to return 192 since received apc during sleep call
     * well technically 192 is io completion interruption, but seems to
     * report that for any interrupting APC */
    printf("SleepEx returned %d\n", res);
    printf("Apc arg = %d\n", (int)apc_arg);
    printf("Result = %d\n", result);
    fflush(stdout);
    return 0;
}

void WINAPI
apc_func(ULONG_PTR arg)
{
    result += 100;
    apc_arg = arg;
}

int
main()
{
    unsigned int tid;
    HANDLE hThread;
    int res;

    printf("Before _beginthreadex\n");
    fflush(stdout);
    hThread = (HANDLE)_beginthreadex(NULL, 0, run_func, NULL, 0, &tid);

    while (synch_2) {
        SwitchToThread();
    }

    res = QueueUserAPC(apc_func, hThread, 37);
    printf("QueueUserAPC returned %d\n", res);
    fflush(stdout);

    synch_1 = FALSE;

    WaitForSingleObject((HANDLE)hThread, INFINITE);

    printf("After _beginthreadex\n");
    return 0;
}
