/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

/* a test for calling NtSetContextThread on a thread at a system call

interestingly here's what happens to the registers (not outputting since not
machine-independent):

suspended@: 00000000 00000000 00334cd0 00420c78 00000000 00000000 0052ff88 0052ffb8
setting to: ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff 0052ff88 ffffffff
result:     00000102 ffffffff 0052ff88 004161db ffffffff ffffffff 0052ff88 ffffffff
*/

#include "tools.h"
#include "Windows.h"

#define VERBOSE 0

DWORD control;
DWORD transfer_addr;

static int reg_eax;
static int reg_ebx;
static int reg_ecx;
static int reg_edx;
static int reg_edi;
static int reg_esi;
static int reg_ebp;
static int reg_esp;

#define TIMER_UNITS_PER_MILLISECOND (1000 * 10) /* 100ns intervals */

static DWORD WINAPI
ThreadProc1(LPVOID parm)
{
    LARGE_INTEGER waittime;
    NTSTATUS res;
    HANDLE e;
    GET_NTDLL(NtWaitForSingleObject,
              (IN HANDLE ObjectHandle, IN BOOLEAN Alertable, IN PLARGE_INTEGER TimeOut));
    print("starting thread...\n");

    e = CreateEvent(NULL, FALSE, FALSE, "foo");
    waittime.QuadPart = -((int)500 * TIMER_UNITS_PER_MILLISECOND);
    control = 1;
    do {
        res = NtWaitForSingleObject(e, false /* not alertable */, &waittime);
    } while (control);
    __asm {
        mov   reg_eax, eax
        mov   reg_ebx, ebx
        mov   reg_ecx, ecx
        mov   reg_edx, edx
        mov   reg_edi, edi
        mov   reg_esi, esi
        mov   reg_esp, esp
        mov   reg_ebp, ebp
    }
    print("res is " PFMT " but shouldn't get here!!!\n", res);
#if VERBOSE
    print("registers: " PFMT " " PFMT " " PFMT " " PFMT " " PFMT " " PFMT " " PFMT
          " " PFMT "\n",
          reg_eax, reg_ebx, reg_ecx, reg_edx, reg_edi, reg_esi, reg_esp, reg_ebp);
#endif
    CloseHandle(e);

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
    __asm {
        mov   reg_eax, eax
        mov   reg_ebx, ebx
        mov   reg_ecx, ecx
        mov   reg_edx, edx
        mov   reg_edi, edi
        mov   reg_esi, esi
        mov   reg_esp, esp
        mov   reg_ebp, ebp
    }
#if VERBOSE
    print("result:     " PFMT " " PFMT " " PFMT " " PFMT " " PFMT " " PFMT " " PFMT
          " " PFMT "\n",
          reg_eax, reg_ebx, reg_ecx, reg_edx, reg_edi, reg_esi, reg_esp, reg_ebp);
#endif
    print("control has been redirected.\n");
    /* don't try to restore stack across sysenter, etc. */
    ExitThread(0);
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

    tc.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    GetThreadContext(ht, &tc);
#if VERBOSE
    print("suspended@: " PFMT " " PFMT " " PFMT " " PFMT " " PFMT " " PFMT " " PFMT
          " " PFMT "\n",
          tc.CXT_XAX, tc.CXT_XBX, tc.CXT_XCX, tc.CXT_XDX, tc.CXT_XDI, tc.CXT_XSI,
          tc.CXT_XSP, tc.CXT_XBP);
#endif
    tc.CXT_XIP = transfer_addr;
    tc.CXT_XAX = 0xffffffff;
    tc.CXT_XBX = 0xffffffff;
    tc.CXT_XCX = 0xffffffff;
    tc.CXT_XDX = 0xffffffff;
    tc.CXT_XDI = 0xffffffff;
    tc.CXT_XSI = 0xffffffff;
    tc.CXT_XBP = 0xffffffff;
#if VERBOSE
    print("setting to: " PFMT " " PFMT " " PFMT " " PFMT " " PFMT " " PFMT " " PFMT
          " " PFMT "\n",
          tc.CXT_XAX, tc.CXT_XBX, tc.CXT_XCX, tc.CXT_XDX, tc.CXT_XDI, tc.CXT_XSI,
          tc.CXT_XSP, tc.CXT_XBP);
#endif
    SetThreadContext(ht, &tc);

    ResumeThread(ht);
    WaitForSingleObject(ht, INFINITE);

    return 0;
}
