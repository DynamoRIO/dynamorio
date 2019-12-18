/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

/* We create a thread in DllMain to test pre-image-entry threads */

#include <windows.h>
#include <process.h> /* for _beginthreadex */
#include "tools.h"

static HANDLE thread;
static DWORD exit_thread;

int WINAPI
run_func(void *arg)
{
    while (!exit_thread) {
        Sleep(200);
    }
    return 0;
}

int __declspec(dllexport) in_lib(int arg)
{
    print("in lib\n");
    return 4;
}

BOOL APIENTRY
DllMain(HANDLE module, DWORD reason_for_call, LPVOID reserved)
{
    int tid;
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH:
        thread = (HANDLE)_beginthreadex(NULL, 0, run_func, NULL, 0, &tid);
        break;
    case DLL_PROCESS_DETACH:
        exit_thread = 1;
        WaitForSingleObject((HANDLE)thread, INFINITE);
        break;
    }
    return TRUE;
}
