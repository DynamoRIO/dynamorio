/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2005 VMware, Inc.  All rights reserved.
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

/* threaded version of codemod.c to test
 * cache consistency concurrency cases (say that 3 times fast)
 */

#include "tools.h"
#include <windows.h>
#include <process.h> /* for _beginthreadex */
#include <stdio.h>

#define ITERS 150
#define NUM_THREADS 6
char buf[32];

int WINAPI
run_func(void *arg)
{
    char *foo = buf;
    int i;
    for (i = 0; i < ITERS; i++) {
#ifdef UNIX
        asm("mov  %0,%%eax" : : "r"(foo));
        asm("call *%eax");
#else
        __asm {
            mov   eax, foo
            call  eax
        }
#endif
        buf[1] = 0xc3; /* ret */
        /* we're not testing security here, just consistency, so make it kosher */
        NTFlush(buf, 2);
    }
    return i;
}

int
main()
{
    int tid, i;
    unsigned long hThread[NUM_THREADS];

    INIT();

    print("starting up\n");

    /* set up initial value of code */
    buf[0] = 0xc3; /* ret */
    /* we're not testing security here, just consistency, so make it kosher */
    NTFlush(buf, 1);

    for (i = 0; i < NUM_THREADS; i++)
        hThread[i] = _beginthreadex(NULL, 0, run_func, NULL, 0, &tid);
    for (i = 0; i < NUM_THREADS; i++)
        WaitForSingleObject((HANDLE)hThread[i], INFINITE);

    print("all done\n");
}
