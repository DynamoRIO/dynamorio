/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#ifdef WINDOWS
/* XXX: can we share w/ core somehow? */
typedef enum _THREADINFOCLASS {
    ThreadAmILastThread = 12,
} THREADINFOCLASS;

BOOL
am_I_last_thread(void)
{
    ULONG got;
    BOOL last;
    GET_NTDLL(NtQueryInformationThread,
              (IN HANDLE ThreadHandle, IN THREADINFOCLASS ThreadInformationClass,
               OUT PVOID ThreadInformation, IN ULONG ThreadInformationLength,
               OUT PULONG ReturnLength OPTIONAL));
    if (NT_SUCCESS(NtQueryInformationThread(GetCurrentThread(), ThreadAmILastThread,
                                            &last, sizeof(last), &got)))
        return last;
    return FALSE;
}
#endif

int
main()
{
#ifdef WINDOWS
    HANDLE lib = LoadLibrary("client.thread.appdll.dll");
    if (lib == NULL) {
        print("error loading library\n");
    } else {
        print("loaded library\n");
        /* PR 210591: test transparency by having client create a thread here
         * and ensuring DllMain of the lib isn't notified.
         * 7 nops isn't enough: win7's kernelbase!MultiByteToWideChar has 7.
         * 13 isn't either: win8.1's KERNELBASE!GetEnvironmentStringsW has 13.
         */
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        NOP;
        FreeLibrary(lib);
    }
    /* Test i#1489 by querying for last thread while client thread is active */
    print("i#1489 last-thread test\n");
    if (!am_I_last_thread())
        print("thread transparency error\n");
#else
    /* test creating thread here */
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
#endif
    print("thank you for testing the client interface\n");
}
