/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2008 VMware, Inc.  All rights reserved.
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

#if defined(X64) && defined(WINDOWS)
/* FIXME i#29: doesn't work on x64 natively: we're messing up the SEH w/ our
 * retaddr trick in ring()
 */
#endif

#include "tools.h"
#include <setjmp.h>

#ifdef UNIX
#    include <unistd.h>
#    include <signal.h>
#    include <ucontext.h>
#    include <errno.h>
#endif

SIGJMP_BUF mark;
int where; /* 0 = normal, 1 = segfault SIGLONGJMP */

void
ring(void **retaddr_p, ptr_int_t x)
{
    print("looking at ring " PFX "\n", x);
    *retaddr_p = (void *)x;
}

ptr_int_t
foo()
{
    print("in foo\n");
    return 1;
}

ptr_int_t
bar()
{
    print("in bar\n");
    return 3;
}

ptr_int_t
twofoo()
{
    ptr_int_t a = foo();
    print("first foo a=" SZFMT "\n", a);

    a += foo();
    print("second foo a=" SZFMT "\n", a);
    return a;
}

#ifdef UNIX
static void
signal_handler(int sig)
{
    if (sig == SIGSEGV) {
#    if VERY_VERBOSE
        print("Got seg fault\n");
#    endif
        SIGLONGJMP(mark, 1);
    }
    exit(-1);
}
#else
#    include <windows.h>
/* top-level exception handler */
static LONG
custom_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
#    if VERY_VERBOSE
        print("Got segfault\n");
#    endif
        SIGLONGJMP(mark, 1);
    }
#    if VERBOSE
    print("Exception occurred, process about to die silently\n");
#    endif
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#endif

int
invalid_ret(int x)
{
    ptr_int_t bad_retaddr = x; /* Sign extend x on X64. */
    where = SIGSETJMP(mark);
    if (where == 0) {
        call_with_retaddr((void *)ring, bad_retaddr);
        print("unexpectedly we came back!");
    } else {
        print("fault caught on " PFX "\n", x);
    }
    return 0;
}

int
main()
{
    INIT();

#ifdef UNIX
    intercept_signal(SIGSEGV, (handler_3_t)signal_handler, false);
#else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)custom_top_handler);
#endif

    print("starting good function\n");
    twofoo();
    print("starting bad function\n");

    invalid_ret(1);          /* zero page */
    invalid_ret(0);          /* NULL */
    invalid_ret(0x00badbad); /* user mode */
    invalid_ret(0x7fffffff); /* user mode */
    invalid_ret(0x80000000); /* kernel addr */
    invalid_ret(0xbadbad00); /* kernel addr */
    invalid_ret(0xfffffffe); /* just bad */
    invalid_ret(0xffffffff); /* just bad */

    print("all done [not seen]\n");
}
