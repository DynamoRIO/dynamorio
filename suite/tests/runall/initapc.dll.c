/* **********************************************************
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

/* initapc.dll: tests having DllMain of a statically-linked dll send an APC
 * (thus the APC is prior to the image entry point)
 * (the APC code is borrowed from the older test winapc.c)
 */
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <winbase.h> /* for QueueUserAPC */
#include "tools.h"

void __declspec(dllexport) import_me(int x)
{
    print("initapc.dll:import_me(%d)\n", x);
}

static int result = 0;
static int apc_arg = 0;

static void WINAPI
apc_func(unsigned long arg)
{
    result += 100;
    apc_arg = arg;
}

static void
send_apc()
{
    int res = QueueUserAPC(apc_func, GetCurrentThread(), 37);
    print("QueueUserAPC returned %d\n", res);
    /* an alertable system call so we receive the APC */
    res = SleepEx(100, 1);
    /* is going to return 192 since received apc during sleep call
     * well technically 192 is io completion interruption, but seems to
     * report that for any interrupting APC */
    print("SleepEx returned %d\n", res);
    print("Apc arg = %d\n", apc_arg);
    print("Result = %d\n", result);
}

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH:
        /* make sure we import user32.dll, for AppInit injection */
        GetParent(NULL);
        print("initapc.dll:DllMain()\n");
        send_apc();
        break;
    }
    return TRUE;
}
