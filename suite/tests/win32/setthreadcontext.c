/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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
#include <windows.h>

DWORD control;
DWORD transfer_addr;

static DWORD WINAPI
ThreadProc1(LPVOID parm)
{
    print("starting thread...\n");

    control = 1;

    __asm {
      onemoretime:
        cmp  control, 1
        je   onemoretime
    }

    print("exiting thread\n");
    return -1;
}

static void
transferProc()
{
    __asm {
        call next_instr
      next_instr:
        cmp  control, 0
        jne  transferout

        pop  edx
        mov  transfer_addr, edx
    }

    print("&next_instr recorded\n");
    return;

transferout:
    print("control has been redirected.\n");
    return;
}

static DWORD WINAPI
setregproc(LPVOID parm)
{
    control = 1;
    __asm {
        mov  ecx, 0xaaaaaaaa
      loopagain:
        cmp  ecx, 0
        jne  loopagain
    }

    print("ecx was set, exiting\n");
    return 0;
}

int
main(void)
{
    HANDLE ht;
    DWORD tid;
    CONTEXT tc;

    control = 0;
    transfer_addr = 0;

    /* call this to obtain the address of next_instr */
    transferProc();

    ht = CreateThread(NULL, 0, &ThreadProc1, NULL, 0, &tid);

    while (control == 0)
        ; /* wait for thread to set control */

    SuspendThread(ht);
    print("thread suspended.\n");

    tc.ContextFlags = CONTEXT_CONTROL;
    GetThreadContext(ht, &tc);

    tc.CXT_XIP = transfer_addr;
    SetThreadContext(ht, &tc);

    ResumeThread(ht);
    WaitForSingleObject(ht, INFINITE);

    /* all over again for CONTEXT_INTEGER */
    control = 0;
    ht = CreateThread(NULL, 0, &setregproc, NULL, 0, &tid);

    while (control == 0)
        ; /* wait for thread to set control */

    SuspendThread(ht);
    print("thread suspended.\n");

    tc.ContextFlags = CONTEXT_INTEGER;
    GetThreadContext(ht, &tc);

    tc.CXT_XCX = 0;
    SetThreadContext(ht, &tc);

    ResumeThread(ht);
    WaitForSingleObject(ht, INFINITE);

    return 0;
}
