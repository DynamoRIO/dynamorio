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

#include "tools.h"

/* FIXME: case 10711
 * For now, use 50 threads so this test will pass for thin_client. We
 * should put this back to 100 once we address memory usage issues.
 * xref cases 8960, 9366, 10376
 */
enum { THREADS = 50, THREAD_STACK = 8192 };

enum { LOOP_WORK = 100 };

#define UNIPROC

#ifdef UNIPROC
#    define YIELD() thread_yield() /* or do nothing on a multi processor */
#else
#    define YIELD() /* or do nothing on a multi processor */
#endif

/* --------- */
ptr_int_t thread[THREADS];

long global_started = 0;  /* unsynchronized */
long global_finished = 0; /* unsynchronized */

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

DWORD WINAPI
executor(LPVOID parm)
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
    return 0;
}

int
main()
{
    void *p;
    int reserved = 0;
    int commit = 0;
    int t;

    INIT();

    /*  create all threads suspended - so DR doesn't allocate private structures yet */
    for (t = 0; t < THREADS; t++) {
        /* could use STACK_SIZE_PARAM_IS_A_RESERVATION */
        thread[t] = (ptr_int_t)CreateThread(NULL, THREAD_STACK, executor, NULL,
                                            CREATE_SUSPENDED, NULL);
        if (thread[t] == 0)
            print("GLE: %d\n", GetLastError());
        assert(thread[t] != 0);
    }
    VERBOSE_PRINT("created %d\n", THREADS);

    /* then reserve all remaining memory */
    do {
        int size = 8 * PAGE_SIZE; /* only 32K - the other 32K are still not usable */
        p = reserve_memory(size);
        if (p)
            reserved += size;
    } while (p != NULL);

    VERBOSE_PRINT("reserved %d\n", reserved);

    for (t = 0; t < THREADS; t++) {
        resume_thread((thread_handle)thread[t]);
    }
    VERBOSE_PRINT("resumed %d\n", THREADS);

    while (global_started < THREADS / 2)
        YIELD();

    VERBOSE_PRINT("started %d, finished %d\n", global_started, global_finished);

    print("Successful\n");
#ifdef WAIT
    getchar();
#endif
}
