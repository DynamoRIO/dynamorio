/* **********************************************************
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

/* single RET - many targets */
/*
 * ok - we have 10s for PLAIN_RET
 * - and 26s for the pop ecx/jmp ecx scheme
 * for a single call bar
 *
 *  How to make the difference worse - consider this is going to a single place only?
 * for 3 consecutive call bar
 *   16s RET, vs 41s on POP/JMP ECX = 2.56 same ratio
 *   17s POP/PUSH/RET;  foo_with_extras

*    68s wow a PUSH/JMP paired with RET is 1m8.571s  PLAIN_RET but fancy_CALL
*    48s for a PUSH/JMP paired with a POP/JMP   0m48.597s  -- same ratio here -
        so an extra CALL doesn't hurt as bad as an extra RET!


 * that was for #define ITER 500*100000 // *100

$ cl /O2  /Zi foo.c -I.. /link /incremental:no user32.lib

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

#define GOAL 32 /* recursive fib of course is exponential here */

#ifdef NIGHTLY_REGRESSION
#    define ITER 10 * 1000
#else
#    define ITER 500 * 100000
#endif

#define DEPTH 10

#define PLAIN_RET
//#define JMP_ESP

#define PLAIN_CALL

int
foo(int n)
{ /* CFI */
    __asm {
        /* pay ecx penalty for both cases */
        push ecx
#ifdef PLAIN_CALL
        call bar
        call bar
        call bar
#else /* CALL=PUSH/JMP */
        push offset ac1
        jmp bar
ac1:
        push offset ac2
        jmp bar
ac2:
        push offset ac3
        jmp bar
ac3:
#endif
        pop ecx
        jmp done

bar:
        mov eax, 5
#ifdef PLAIN_RET
        ret
#else
#    ifdef JMP_ESP
        add esp, 4 // is it safe to use the stack in-between these two ops?
        jmp dword ptr [esp-4]
#    else
        pop ecx
        jmp ecx
#    endif
#endif

     done:
    }
    return 5;
}

int
foo_second(int n)
{
    __asm {
        /* pay ecx penalty for both cases */
        push ecx
#ifdef PLAIN_CALL
        call bar
        call bar
        call bar
#else /* CALL=PUSH/JMP */
        push offset ac1
        jmp bar
ac1:
        push offset ac2
        jmp bar
ac2:
        push offset ac3
        jmp bar
ac3:
#endif
        pop ecx
        jmp done

bar:
        mov eax, 5
#ifdef PLAIN_RET
        ret
#else
        pop ecx
        jmp ecx
#endif

     done:
    }
    return 5;
}

int
foo_with_extras(int n)
{
    __asm {
        /* pay ecx penalty for both cases */
        push ecx
        call bar
        call bar
        call bar
        pop ecx
        jmp done

bar:
        mov eax, 5
#ifdef PLAIN_RET
        push ecx /* actually adding one more instruction that the pop/jmp*/
        pop ecx
        ret
#else
        pop ecx
        jmp ecx
#endif

     done:
    }
    return 5;
}

int
foo_first(int n)
{
    __asm {
        /* pay ecx penalty for both cases */
        push ecx
        call bar
        call bar
        call bar
        pop ecx
        jmp done

bar:
        mov eax, 5
#ifdef PLAIN_RET
        ret
#else
        pop ecx
        jmp ecx
#endif

     done:
    }
    return 5;
}

int
main(int argc, char **argv)
{
    int i, t;

    /* now a little more realistic depths that fit in the RSB */
    for (i = 0; i <= ITER; i++) {
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
        t = foo(DEPTH);
    }

    print("foo(%d)=%d\n", DEPTH, t);

    if (argc > 5)
        MessageBeep(0);
}
