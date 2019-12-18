/* **********************************************************
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

/* tests for indirect call and indirect jump RCT policies */

/* FIXME: this should really be in security-common yet
 *    rct_ind_{call,jump} is not yet supported on Linux, and all *
 *    assembly needs to be recoded for gcc anyways */
#include "tools.h"

#include <ctype.h>

typedef int (*fconvert)(int c);
typedef int (*fmult)(int);

int
foo(int a, int lower)
{
    fconvert f = toupper;
    int res;
    if (lower)
        f = tolower;
    res = f(a);
    print("%c\n", res);
    return res;
}

int
f2(int a)
{
    return 2 * a;
}

int
f3(int a)
{
    return 3 * a;
}

int
f7(int a)
{
    return 7 * a;
}

int
bar(int a, fmult f)
{
    int x = f2(a);
    int y = f3(a);
    int z = f(a);

    print("%d\n", z);
    return z;
}

/*  Writable yet initialized data indeed needs to be processed.*/
fmult farr[2] = { f2, f7 };

static void
test_good_indcalls()
{
    foo('a', true);  // a
    foo('a', false); // 'A'
    foo('Z', true);  // 'z'
    bar(5, f2);      // 10
    bar(7, f3);      // 21
    bar(7, f3);      // 21
}

int some_global = 123456;

int
precious(int arg)
{
    /* this is a very important function - also not address taken by itself */
#ifdef USER32 /* map user32.dll for a RunAll test */
    MessageBeep(0);
#endif
    print("PRECIOUS function shouldn't be reachable! ATTACK SUCCESSFUL!\n");
    /* can continue */
    return -666;
}

int
good(int *arg)
{
    print("this is a normal function %d\n", *arg);
    return 1;
}

int
good2(int *arg)
{
    print("this is another normal function %d, but shouldn't be called!\n", *arg);
    return 2;
}

static void
test_PLT_with_indcalls(int table_index)
{
    table_index *= 5; /* general instructions */
    print("calling via PLT-style call\n");
    __asm {
        push offset some_global // note that data references will also be absolute!

        push offset plt_ind_call_table // note that this will also be absolute!
        pop eax
        add eax, table_index

        call eax // THIS indirect call should succeed at going only to the first
                     // instruction in this pseudo jump-table
        jmp after_plt_call
    plt_ind_call_table:
        jmp good // label address taken
        jmp precious //  this address is not address taken
        jmp good2 //  this address is not address taken

    after_plt_call:
        add esp,4            // arg
    }
}

static void
test_PLT_with_indjumps(int table_index)
{
    table_index *= 5; /* general instructions */
    print("calling via PLT-style indirect jmp\n");
    __asm {
        push offset some_global // note that data references will also be absolute!

        push offset plt_ind_call_table // note that this will also be absolute!
        pop eax
        add eax, table_index

        push offset after_plt_call // in fact desired location
                /* FIXME: note however that we're not going to like this with a .C
                 * violation! unless we start allowing all addresses added with 'push
                 * offset'
                 */
        jmp eax // THIS indirect call should succeed at going only to the
                        // first instruction in this pseudo jump-table
    plt_ind_call_table:
        jmp good
        jmp precious
        jmp good2

    after_plt_call:
        add esp,4               // arg
    }
}

/* TODO: need to add a test that a jump instead of a return is OK */

__declspec(naked) int jmp_instead_of_ret()
{
    __asm {
        mov eax, 42
        add esp, 4 // is it safe to use the stack in-between these two ops?
        jmp dword ptr [esp-4]
    }
}

static void
test_jmp_instead_of_ret()
{
    print("the answer is %d\n", jmp_instead_of_ret());
}

int
main()
{
    INIT();

    test_good_indcalls();
    test_jmp_instead_of_ret();

    test_PLT_with_indcalls(0);
    test_PLT_with_indcalls(1); /* VIOLATION expected */
    test_PLT_with_indcalls(2); /* VIOLATION expected */

    test_PLT_with_indjumps(0);
    test_PLT_with_indjumps(1); /* VIOLATION expected */

    print("done\n");
}
