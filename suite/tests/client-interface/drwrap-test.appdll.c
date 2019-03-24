/* **********************************************************
 * Copyright (c) 2011-2015 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* test wrapping functionality using a library w/ exported routines
 * so they're easy for the client to locate
 */

/***************************************************************************/
#ifndef ASM_CODE_ONLY /* C code */

#    include "tools.h"

#    include <setjmp.h>
jmp_buf mark;

int EXPORT
makes_tailcall(int x); /* in asm */

int EXPORT
runlots(int *x)
{
    if (*x == 1024)
        print("in runlots 1024\n");
    (*x)++;
    return *x;
}

int EXPORT
preonly(int *x)
{
    print("in preonly\n");
    *x = 6;
    return -1;
}

int EXPORT
postonly(int *x)
{
    print("in postonly\n");
    *x = 6;
    return -1;
}

int EXPORT
replaceme(int *x)
{
    print("in replaceme\n");
    *x = 5;
    return -1;
}

int EXPORT
replaceme2(int *x)
{
    print("in replaceme2\n");
    *x = 5;
    return -1;
}

static int
replace_callsite_callee(int *x)
{
    *x = 5;
    return -1;
}

int EXPORT
replace_callsite(int *x)
{
    int y = replace_callsite_callee(x);
    /* just putting in stuff to prevent tailcall */
    if (y == *x)
        *x = y + 1;
    return y;
}

int EXPORT
skipme(int *x)
{
    print("in skipme\n");
    *x = 4;
    return -1;
}

int EXPORT
repeatme(int x)
{
    print("in repeatme with arg %d\n", x);
    return x;
}

int EXPORT
level2(int x)
{
    print("in level2 %d\n", x);
    return x;
}

int EXPORT
level1(int x, int y)
{
    print("in level1 %d %d\n", x, y);
    makes_tailcall(x + y);
    return x;
}

int EXPORT
level0(int x)
{
    print("in level0 %d\n", x);
    print("level1 returned %d\n", level1(x, x * 2));
    return x;
}

int EXPORT
skip_flags(int x, int y)
{
    /* check if arg value is changed by drwrap */
    if (x != 1 || y != 2)
        print("wrong argument %d %d!", x, y);
    return (x + y);
}

void *level2_ptr;

/***************************************************************************
 * test longjmp
 */

void EXPORT
long3(void)
{
    print("long3 A\n");
#    ifdef WINDOWS
    /* test SEH */
    __try {
        *(int *)4 = 42;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
        longjmp(mark, 1);
    }
#    else
    longjmp(mark, 1);
#    endif
    print("  long3 B\n");
}

void EXPORT
long2(void)
{
    print("long2 A\n");
    long3();
    print("  long2 B\n");
}

void EXPORT
long1(void)
{
    print("long1 A\n");
    long2();
    print("  long1 B\n");
}

void EXPORT
long0(void)
{
    print("long0 A\n");
    long1();
    print("  long0 B\n");
}

void EXPORT
longstart(void)
{
    long0();
}

void EXPORT
longdone(void)
{
    print("longdone\n");
}

void
run_tests(void)
{
    int x = 3;
    int res;
    level2_ptr = (void *)level2;
    print("thread.appdll process init\n");
    skip_flags(1, 2);
    print("level0 returned %d\n", level0(37));
    res = skipme(&x);
    print("skipme returned %d and x=%d\n", res, x);
    res = repeatme(x);
    print("repeatme returned %d\n", res);
    res = replaceme(&x);
    print("replaceme returned %d and x=%d\n", res, x);
    res = replaceme2(&x);
    print("replaceme2 returned %d and x=%d\n", res, x);
    res = replace_callsite(&x);
    print("replace_callsite returned %d and x=%d\n", res, x);
    preonly(&x);
    postonly(&x);

    skipme(&x);
    postonly(&x);

    /* test delayed flushing that doesn't flush until 1024 executions */
    x = 0;
    for (res = 0; res < 2048; res++)
        runlots(&x);

    /* test longjmp recovery on pre not post so we call from non-wrapped routine */
    if (setjmp(mark) == 0)
        longstart();
    longdone();
}

#    ifdef WINDOWS
BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH: run_tests(); break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    }
    return TRUE;
}
#    else
int __attribute__((constructor)) so_init(void)
{
    run_tests();
    return 0;
}
#    endif

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

# if defined(UNIX) && defined(X64)
/* to work on x64 w/o ld reloc problems we use a stored address
 * so we're not testing tailcall on x64 linux.
 * (if we use indirect jump, drwrap misses the post-makes_tailcall:
 * it doesn't handle indirect-jump tailcalls)
 */
DECL_EXTERN(level2_ptr)
# else
DECL_EXTERN(level2)
# endif

#define FUNCNAME makes_tailcall
        DECLARE_EXPORTED_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
# if defined(UNIX) && defined(X86) && defined(X64)
        push     REG_XBP  /* Needed only to maintain 16-byte alignment. */
        call     PTRSZ SYMREF(level2_ptr)
        pop      REG_XBP
        ret
# else
        JUMP     GLOBAL_REF(level2)
# endif
# ifndef ARM
        ret /* won't get here */
# endif
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif /* ASM_CODE_ONLY */
