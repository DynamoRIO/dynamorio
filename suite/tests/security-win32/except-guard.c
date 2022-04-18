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

/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION) {
        count++;
        print("guard page exception\n");
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}

void
bar(int guard)
{
    if (guard > 0) {
        print("test guard without alloc\n");
    } else {
        print("test without guard\n");
    }
    return;
}

int
main(void)
{
    unsigned char *buf;
    void (*foo)(void);
    int old_prot;

    INIT();

    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);

    print("start of test, count = %d\n", count);

    buf = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    /* 3 nops and a ret */
    buf[0] = '\x90';
    buf[1] = '\x90';
    buf[2] = '\x90';
    buf[3] = '\xc3';
    foo = (void *)buf;

    /* sets allocated buffer with guard page status */
    VirtualProtect(buf, 4096, PAGE_EXECUTE_READWRITE | PAGE_GUARD, &old_prot);
    foo();

    bar(0);
    /* sets static code with guard page status */
    VirtualProtect((unsigned char *)bar, 8, PAGE_EXECUTE_READWRITE | PAGE_GUARD,
                   &old_prot);
    bar(1);
    /* to check that we do not get any more exceptions */
    bar(0);

    print("end of test, count = %d\n", count);

    return 0;
}
