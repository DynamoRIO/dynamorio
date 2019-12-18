/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2010 VMware, Inc.  All rights reserved.
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

/* threaded version of selfmod.c to test
 * cache consistency concurrency cases (say that 3 times fast)
 */

#include <windows.h>
#include <process.h> /* for _beginthreadex */
#include <stdio.h>
#include "tools.h"

#define ITERS 10
#define NUM_THREADS 12
int go_threads = 0;

void
foo(int iters)
{
    /* executes iters iters, by modifying the iters bound using
     * self-modifying code
     */
    int total;
    while (!go_threads)
        Sleep(1);
#ifdef UNIX
    asm("  movl %0, %%ecx" : : "r"(iters));
    asm("  call next_inst");
    asm("next_inst:");
    asm("  pop %edx");
    /* add to retaddr: 1 == pop
     *                 3 == mov ecx into target
     *                 1 == opcode of target movl
     */
    asm("  movl %ecx, 0x5(%edx)");  /* the modifying store */
    asm("  movl $0x12345678,%eax"); /* this instr's immed gets overwritten */
    asm("  movl $0x0,%ecx");        /* counter for diagnostics */
    asm("repeata:");
    asm("  decl %eax");
    asm("  inc  %ecx");
    asm("  cmpl $0x0,%eax");
    asm("  jnz repeata");
    asm("  movl %%ecx, %0" : "=r"(total));
#else
    __asm {
        mov  ecx, iters
        call next_inst
      next_inst:
        pop  edx
            /* add to retaddr: 1 == pop
             *                 3 == mov ecx into target
             *                 1 == opcode of target movl
             */
        mov  dword ptr [edx + 0x5], ecx /* the modifying store */
        mov  eax,0x12345678 /* this instr's immed gets overwritten */
        mov  ecx,0x0 /* counter for diagnostics */
      repeata:
        dec  eax
        inc  ecx
        cmp  eax,0x0
        jnz  repeata
        mov  total,ecx
    }
#endif
#if VERBOSE
    print("Executed 0x%x iters\n", total);
#endif
}

int WINAPI
run_func(void *arg)
{
    int i;
    for (i = 0; i < ITERS; i++) {
        foo(0xabcd);
        foo(0x1234);
        foo(0xef01);
    }
}

int
main()
{
    int tid, i;
    unsigned long hThread[NUM_THREADS];

    INIT();

    print("starting up\n");

    /* make foo code writable */
    protect_mem(foo, PAGE_SIZE, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    // Note that main and the exception handler __except_handler3 are on this page too

    for (i = 0; i < NUM_THREADS; i++)
        hThread[i] = _beginthreadex(NULL, 0, run_func, NULL, 0, &tid);
    go_threads = 1;
    for (i = 0; i < NUM_THREADS; i++)
        WaitForSingleObject((HANDLE)hThread[i], INFINITE);

    print("all done\n");
}
