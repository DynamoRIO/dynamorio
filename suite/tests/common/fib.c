/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
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

/* undefine this for a performance test */
#ifndef NIGHTLY_REGRESSION
#    define NIGHTLY_REGRESSION
#endif

#include "tools.h"

#include <stdio.h>

#ifdef WINDOWS
/* and compile with user32.lib */
#    include <windows.h>
#endif

#ifdef NIGHTLY_REGRESSION
#    define ITER 10 * 1000
#else
#    define ITER 10 * 200 * 1000 // *100
#endif

#define GOAL 32 /* recursive fib of course is exponential here */

/* now stay a little more realistic depths that fit in the RSB */
#define DEPTH 12

/* ignore overflow errors */
int
fib(int n)
{
    if (n <= 1)
        return 1;
    /* for drcov test, we add a line that won't be executed */
    if (n > 100)
        return 0;
    return fib(n - 1) + fib(n - 2);
}

int
main(int argc, char **argv)
{
    int i, t;

    INIT();
    USE_USER32();

    print("fib(%d)=%d\n", 5, fib(5));
    /* Enable use as a shorter test for tool.drcacheof.func_view. */
    if (argc > 1 && strcmp(argv[1], "only_5") == 0)
        return 0;
    print("fib(%d)=%d\n", 15, fib(15));
    /* deep recursion */
    print("fib(%d)=%d\n", 25, fib(25));

    /* show recursion */
    for (i = 0; i <= GOAL; i++) {
        t = fib(i);
        print("fib(%d)=%d\n", i, t);
    }

    for (i = 0; i <= ITER; i++) {
        t = fib(DEPTH);
    }

    print("fib(%d)=%d\n", DEPTH, t);
}

/*
It is amazing that with default options native=13s dr=12s

only when optimized differences show up in the other direction
$ cl /O2 -I.. fib.c
native=8s dr=11s   ITER=? DEPTH=?

$ cl /O2  /Zi fib.c -I.. /link /incremental:no user32.lib
*/
