/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
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

#include "tools.h" /* for print() */

#include <assert.h>

#ifdef UNIX
# include <unistd.h>
# include <signal.h>
# include <ucontext.h>
# include <errno.h>
#endif

#include <setjmp.h>
static SIGJMP_BUF mark;
static int count = 0;

#ifdef UNIX
static void
signal_handler(int sig)
{
    if (sig == SIGSEGV) {
        if (count == 0) {
            print("Got a seg fault: recovering\n");
            count++;
            SIGLONGJMP(mark, 1);
        } else
            print("Got a seg fault: aborting\n");
    }
    abort();
}
#else
# include <windows.h>
/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS * pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (count == 0) {
            print("Got a seg fault: recovering\n");
            count++;
            SIGLONGJMP(mark, 1);
        } else
            print("Got a seg fault: aborting\n");
    }
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#endif

int
main(int argc, char *argv[])
{
#ifdef UNIX
    intercept_signal(SIGSEGV, (handler_3_t) signal_handler, false);
#else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER) our_top_handler);
#endif

    print("Segfault about to happen\n");

    /* First fault: test i#1045 where the fault overlaps a page boundary */
    if (SIGSETJMP(mark) == 0) {
        char *buf = allocate_mem(PAGE_SIZE*2, ALLOW_READ|ALLOW_WRITE);
        protect_mem(buf + PAGE_SIZE, PAGE_SIZE, ALLOW_READ);
        *((volatile int *)(buf + PAGE_SIZE - 1)) = 42;
    }

    *((volatile int *)0) = 4;

    print("SHOULD NEVER GET HERE\n");

    return 0;
}
