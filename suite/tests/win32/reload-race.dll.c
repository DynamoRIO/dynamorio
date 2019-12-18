/* **********************************************************
 * Copyright (c) 2006 VMware, Inc.  All rights reserved.
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

#include "tools.h"

int
fib(int n)
{
    if (n <= 1)
        return 1;
    return fib(n - 1) + fib(n - 2);
}

int
fact(int n)
{
    if (n <= 1)
        return 1;
    return n * fact(n - 1);
}

int __declspec(dllexport) import_me1(int x)
{
    /* any printing here can not be matched */
#ifdef VERBOSE
    print("in import_me1 %d\n", x);
    if (!properly_initalized) {
        print("app race - DLL in invalid state\n");
    }
#endif
    if (x % 2 == 0)
        return fib(x);
    else
        return fact(x);
}

int __declspec(dllexport) import_me2(int x)
{
    /* any printing here can not be matched */
#ifdef VERBOSE
    print("in import_me2 %d\n", x);
    if (!properly_initalized) {
        print("app race - DLL in invalid state\n");
    }
#endif
    /* note that our IAT may not be properly initialized
     * (if DLL is not bound to kernel32.dll
     * 100271dc  000273b6
     * 0:001> da reload_race_dll+000273b6
     * 100273b6  "..Sleep"
     */
    /* so we may target the RVA here */
    thread_yield();
    /* .C shouldn't be shown on the way back */
    if (x % 2 == 0)
        return fib(x);
    else
        return fact(x);
}

int properly_initalized;

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH: properly_initalized = 1; break;
    case DLL_PROCESS_DETACH: properly_initalized = 0; break;
    }
    return TRUE;
}
