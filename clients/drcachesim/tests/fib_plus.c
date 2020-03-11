/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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
#include "thread.h"
#include <stdio.h>

/* We use this app to generate a multi-threaded function trace.
 * We could use cond vars to try and make a deterministic yet interleaved
 * thread schedule, but it is simpler to run it once and check in the trace
 * for use in our test.
 */

/* A function with no args. */
char
noargs(void)
{
    return 'B';
}

/* Run a void function we can trace "noret", called inside a nested traced
 * function, for testing indentation of the func_view tool's output.
 */
void
noret_func(int x, int y)
{
    if (x != y)
        noret_func(x + 1, y);
}

int
fib(int n)
{
    if (n <= 1)
        return 1;
    noret_func(n, n + 1);
    thread_yield(); /* Try to get some thread interleaving. */
    noargs();
    return fib(n - 1) + fib(n - 2);
}

static
#ifdef WINDOWS
    unsigned int __stdcall
#else
    void *
#endif
    thread_func(void *arg)
{
    fprintf(stderr, "fib(%d)=%d\n", 5, fib(5));
    return IF_WINDOWS_ELSE(0, NULL);
}

int
main()
{
    thread_t thread1 = create_thread(thread_func, NULL);
    thread_t thread2 = create_thread(thread_func, NULL);
    join_thread(thread1);
    join_thread(thread2);
    return 0;
}
