/* **********************************************************
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

/* single RET - many targets */
/* the target function however should have a lot of independent paths to evaluate
 * fib() is NOT a good example
 */

/* got 66s native, vs 79s DR  = 1m6s 1m19s ~ 20%*/
/* with another compare in the body went to 30% */
/* (/ 21 16.0) */
/* (/ 29 22.0) 1.380952380952381 for THREE compare's in a single body  */
/* (/ 59 43.0) 1.37 for THREE compare's in a single body  */

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
#    define ITER 400 * 1000
#else
#    define ITER 400 * 100000 // *100
#endif

//#define MORE  more fanout
//#define TOO_MUCH or more depth and more work

int
compare(const void *arg1, const void *arg2)
{
    /* Compare all of both strings: */
    return
#ifdef WINDOWS
        _stricmp
#else
        strcasecmp
#endif
        (*(char **)arg1, *(char **)arg2);
}

int
sort()
{
    int argc = 5;
    const char *argv[] = { "one", "two", "three", "five", "six", "unsorted" };

#ifdef TOO_MUCH
    /* Sort remaining args using Quicksort algorithm: */
    qsort((void *)argv, (size_t)argc, sizeof(char *), compare);
#endif

    compare(&argv[4], &argv[2]);
    compare(&argv[3], &argv[2]);
    return compare(&argv[1], &argv[2]);
}

int
main(int argc, char **argv)
{
    int i;

    INIT();
    USE_USER32();

    /* now a little more realistic depths that fit in the RSB */
    for (i = 0; i <= ITER; i++) {
        sort();
        sort();
        sort();
        /* more pronounced with a few more - though three is sufficiently visible */
#ifdef MORE
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
        sort();
#endif
    }

    i = sort();
    print("sort() = %s\n", i > 0 ? ">" : (i == 0 ? "=" : "<"));

    print("done\n");
}
