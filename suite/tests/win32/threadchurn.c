/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
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

/* case 4660 test cases for thread churn */

/* undefine this for a performance test */
#ifndef NIGHTLY_REGRESSION
#    define NIGHTLY_REGRESSION
#endif

#include "tools.h"
#include "thread.h"

typedef unsigned char *app_pc;

#define SILENT /* no writes at all */

/* observe page faults and private size growth, see what happens if I
 * set the Working set size to a small number
 */

enum { SWAP_OUT_AFTER_THREAD = 1 }; /* trims working set down after each thread */
enum { SWAP_OUT_AFTER_BATCH = 1 };  /* trims working set down after batch  */

/*
 * native (0,0) pf delta is 15k,   time 30s
 * native (0,1) pf delta is 50k    time 32s
 * native (1,1) pf delta is 168k   time 45s
 *  native peak private 400k,
 *  dr (0,0) - 2MB peak private, time 12m
 */

#ifdef NIGHTLY_REGRESSION
enum { TOTAL_THREADS = 40 };
#else
enum { TOTAL_THREADS = 20000 };
#endif

enum { BATCH_SIZE = 10 };

enum { ROUNDS = 10 };

#define UNIPROC

enum { LOOP_WORK = 100 };

/* --------- */
thread_t thread[TOTAL_THREADS];

long global_started = 0;  /* unsynchronized */
long global_finished = 0; /* unsynchronized */

#ifdef UNIPROC
#    define YIELD() thread_yield() /* or do nothing on a multi processor */
#else
#    define YIELD() /* or do nothing on a multi processor */
#endif

int
compare(const void *arg1, const void *arg2)
{
    /* Compare all of both strings: */
    return _stricmp(*(char **)arg1, *(char **)arg2);
}

void
sort()
{
    int argc = 5;
    char *argv[] = { "one", "two", "three", "five", "six", "unsorted" };

    /* Sort remaining args using Quicksort algorithm: */
    qsort((void *)argv, (size_t)argc, sizeof(char *), compare);

#ifdef VERY_VERBOSE
    /* Output sorted list: */
    for (i = 0; i < argc; ++i)
        print(" %s", argv[i]);
    print("\n");
#endif
}

unsigned int __stdcall executor(void *arg)
{
    int w;
    sort(); /* do some work */
    InterlockedIncrement(&global_started);

    for (w = 0; w < LOOP_WORK; w++) {
        sort(); /* do more work */
        if (w % 10 == 0)
            YIELD();
    }
    InterlockedIncrement(&global_finished);

    /* TODO: could thread_suspend(a paired thread) */
    return 0;
}

enum { MINSIZE_KB = 500, MAXSIZE_KB = 1000 };

int
main()
{
    int round;
    int b, t;
    int res;

    INIT();

    /* this doesn't do much in fact */
    res = SetProcessWorkingSetSize(GetCurrentProcess(), MINSIZE_KB * 1024,
                                   MAXSIZE_KB * 1024);
    // on Win2003 there is a SetProcessWorkingSetSizeEx that sets
    // QUOTA_LIMITS_HARDWS_ENABLE

    if (res == 0)
        print("SetProcessWorkingSetSize failed GLE: %d\n", GetLastError());

    for (round = 0; round < ROUNDS; round++) {
#ifdef VERBOSE
        print("round %d\n", round);
#endif
        global_started = 0;
        global_finished = 0;

        /* do in a batch */
        for (b = 0; b < TOTAL_THREADS / BATCH_SIZE; b++) {
            for (t = 0; t < BATCH_SIZE; t++) {
                thread[t] = create_thread(executor, NULL);
                if (thread[t] == NULL)
                    print("GLE: %d\n", GetLastError());
                assert(thread[t] != NULL);
            }
            /* now synchronize with all of them - or maybe some? */

#ifdef VERBOSE
            print("started %d threads\n", TOTAL_THREADS);
#else
#    ifndef SILENT
            print("started some threads\n");
#    endif
#endif
            for (t = 0; t < BATCH_SIZE; t++) {
                assert(thread[t] != NULL);
                join_thread(thread[t]);
                thread[t] = NULL; /* in case want to synch with some in a batch, but with
                                     all at the end */

                if (SWAP_OUT_AFTER_THREAD) {
                    res = SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
                }
            }
#ifdef VERBOSE
            print("some %d work, done %d\n", global_started, global_finished);
#endif
            if (SWAP_OUT_AFTER_BATCH) {
                res = SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
            }
        }
    }

    print("done\n");
    return 0;
}
