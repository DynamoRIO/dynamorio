/* **********************************************************
 * Copyright (c) 2017 Simorfo, Inc.  All rights reserved.
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
 * * Neither the name of Simorfo nor the names of its contributors may be
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

static int count = 0;

#if 0  /* FIXME i#2249: not handled yet */
static void
test_debug_register(void)
{
    __asm {
        /* some amount of code in here */
        nop
        nop
        nop
        nop
        nop
    }
}

static void
test_eip(void)
{
    print("in test_eip\n");
    __asm {
        /* some amount of code in here */
        nop
        nop
        int 3
        nop
        nop
    }
    print("end of test count = %d\n", count);
    exit(0);
}
#endif /* FIXME i#2249: not handled yet */

/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        count++;
        print("single step seen\n");
        // deactivate breakpoint
        pExceptionInfo->ContextRecord->Dr0 = 0;
        pExceptionInfo->ContextRecord->Dr6 = 0;
        pExceptionInfo->ContextRecord->Dr7 = 0;
        return EXCEPTION_CONTINUE_EXECUTION;
    } else if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        count++;
        print("breakpoint seen\n");
        // deactivate breakpoint
#ifndef X64
        pExceptionInfo->ContextRecord->Eip++;
#else
        pExceptionInfo->ContextRecord->Rip++;
#endif
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}

int
main(void)
{
    HANDLE hThread;
    CONTEXT Context;
#if 0 /* FIXME i#2249: not handled yet */
    bool r;
#endif

    INIT();

    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);

    print("start of test count = %d\n", count);

    hThread = GetCurrentThread();
    Context.ContextFlags = 0;
    print("test dummy SetThreadContext\n");
    if (SetThreadContext(hThread, &Context) == 0) {
        print("error for SetThreadContext\n");
    }

#if 0  /* FIXME i#2249: not handled yet */
    Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(hThread, &Context) != 0) {
        Context.Dr0 = (DWORD) (((char*)test_debug_register) + 4);
        Context.Dr6 = 0xfffe0ff0;
        Context.Dr7 = 0x00000101;
        print("set debug register\n");
        if (SetThreadContext(hThread, &Context) == 0) {
            print("error for SetThreadContext\n");
        }
    }
    test_debug_register();

    Context.ContextFlags = CONTEXT_CONTROL;
    print("set eip\n");
    if (GetThreadContext(hThread, &Context) != 0) {
        /* not sure how to make this compiler indepedent */
        Context.Eip = (DWORD) test_eip;
        r = SetThreadContext(hThread, &Context);
        if (r == 0) {
            print("error for SetThreadContext\n");
        }
    }
#endif /* FIXME i#2249: not handled yet */

    print("end of test count = %d\n", count);

    return 0;
}
