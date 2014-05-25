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

#include "tools.h"
#include <windows.h>
#include <assert.h>

DWORD control;
DWORD start_addr;

static DWORD WINAPI
ThreadProc1(LPVOID parm)
{
    print("starting thread...\n");
    /* asm so we have compiler-independent control over pc range here */
    __asm {
        mov  eax, 0xdeadbeef
        call next_instr
      next_instr:
        pop  edx
        mov  start_addr, edx
      wait4control:
        cmp  control, 0
        je   wait4control
    }
    print("exiting thread\n");
}

int
main(void)
{
    HANDLE ht;
    DWORD tid;
    CONTEXT tc;

    control = 0;
    start_addr = 0;

    ht = CreateThread(NULL, 0, &ThreadProc1, NULL, 0, &tid);

    while (start_addr == 0)
        ; /* wait for thread to set start_addr */

    SuspendThread(ht);

    memset(&tc, 0, sizeof(tc));
    tc.ContextFlags = CONTEXT_CONTROL;
    GetThreadContext(ht, &tc);
    assert(tc.ContextFlags == CONTEXT_CONTROL);
    /* 16 bytes is through je wait4control */
    if (tc.CXT_XIP >= start_addr && tc.CXT_XIP <= start_addr + 0x10)
        print("eip is valid\n");
    else
        print("invalid eip: %p\n", (void *)tc.CXT_XIP);

    /* test non-CONTROL flags */
    memset(&tc, 0, sizeof(tc));
    tc.ContextFlags = CONTEXT_INTEGER;
    GetThreadContext(ht, &tc);
    assert(tc.ContextFlags == CONTEXT_INTEGER);
    if (tc.CXT_XAX == 0xdeadbeef)
        print("eax is valid\n");
    else
        print("invalid eax: %p\n", (void *)tc.CXT_XAX);

    control = 1; /* stop thread */
    ResumeThread(ht);
    WaitForSingleObject(ht, INFINITE);

    /* try getting own context (technically you're not supposed to do this, msdn says
     * result is undefined, but we should handle it reasonably */
    tc.ContextFlags = CONTEXT_FULL;
    GetThreadContext(GetCurrentThread(), &tc);
    /* FIXME - ok we got some result, what can we do to check it? we shouldn't screw up
     * the reg state, so ignore that, pc should be the post syscall pc (based on
     * observations of native behavior) so either in ntdll.dll or vsyscall page */
    print("get context self eip > 0x74000000? %s\n",
          tc.CXT_XIP > 0x74000000 ? "yes" : "no");

    print("done\n");

    return 0;
}
