/* **********************************************************
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

/* aslr-ind.c */
/* FIXME: should make this process start itself so that can test with early injection
 * enabled */

#include <windows.h>
#include "tools.h"

// #define VERBOSE
typedef int (*fiptr)(void);

fiptr __declspec(dllexport) giveme_target(int arg);

fiptr go_where;

void *
get_module_preferred_base(void *module_base)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    dos = (IMAGE_DOS_HEADER *)module_base;
    nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    return (void *)nt->OptionalHeader.ImageBase;
}

void __declspec(dllimport) precious(void);

/* from retexisting.c */
int
ring(int num)
{
    print("looking at ring\n");
    *(int *)(&num - 1) = (int)&precious;
    return num;
}

int
main(int argc)
{
    char *base;
    void *hmod = GetModuleHandle("security-win32.aslr-ind.dll.dll");

    INIT();
    USE_USER32();

    print("aslr-ind main()\n");

#ifdef VERBOSE
    print("0x%x\n", hmod);
#endif

    go_where = giveme_target(332);
    print("%d\n", (*go_where)());

    base = get_module_preferred_base(hmod);
    if (base == hmod)
        print("at base, no ASLR\n");
    else {
        print("targeting original base\n");
        go_where = (fiptr)((char *)go_where + (base - (char *)hmod));
    }

    /* in wrong address space, but a good entry!
     * FIXME: we may want
     * to flag that explicitly - could be an FP!
     */
    __try {
        print("%d\n", go_where());
        print("*** invalid indirect call at preferred base!\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("invalid indirect call 1 caught\n");
    }

    /* now this should definitely not be good, no matter aslr:
     *     0012fb20 b801000000       mov     eax,0x1
     *     0012fb25 eb05             jmp
     */
    go_where = (fiptr)((char *)go_where + 7);

    __try {
        print("%d\n", go_where());
        print("*** invalid indirect call allowed!\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("invalid indirect call 2 caught\n");
    }

    print("indirect call done\n");

    __try {
        print("starting bad return function\n");
        ring(1);
        print("*** invalid RET - can't really get here\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("invalid return caught\n");
    }

    /* FIXME: should be able to allocate memory with VirtualAlloc() at
     * the would be location and verify our handling of execution
     * there in that case
     */

    return 0;
}
