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

/* gbop-test.c */

#include <windows.h>
#include "tools.h"

#ifdef VERBOSE
#    define pf print
#else
#    define pf \
        if (0) \
        print
#endif

#define REPORT(res)                                                      \
    pf("res=%x\n", res);                                                 \
    print("res=%s\n", (res == kernel32_base) ? "kernel32_base" : "BAD"); \
    res = 0;

int
main()
{
    char *arg0 = "kernel32.dll";
    ptr_uint_t res;
    ptr_uint_t stack = 0;
    ptr_uint_t kernel32_base;

    INIT();

    print("plain C call\n");
    res = (ptr_uint_t)LoadLibrary(arg0);

    kernel32_base = res;
    REPORT(res);

    print("plain asm call\n");

    __asm {
        push arg0
                /* note using the IAT directly */
        call dword ptr LoadLibraryA
        mov res, eax
    }
    REPORT(res);

    print("pretend on stack\n");
    /* note that this is a pseudo attack, in fact we get a .C
     * violation on the return back, while GBOP will react to it
     * immediately before executing the target.
     */
    __try {
        __asm {
            /* to ensure that xax isn't clobbered while executing random
             * code on the stack we set up a ud2 on the stack so that
             * we get an exception right away
             */
            push 000b0fh /* ud2 */
            mov ebx, esp
            push arg0
            push ebx
            jmp dword ptr LoadLibraryA
            mov res, eax
        }
    } __except ((res = (GetExceptionInformation())->ContextRecord->CXT_XAX),
                EXCEPTION_EXECUTE_HANDLER) {
        print("exception since not cleaning up stack\n");
    }
    REPORT(res);

    __asm {
        mov stack, esp
    }
    /* now make some portion of the stack look good - this will be
     * forever in this mode
     */
    NTFlush((char *)(stack - 0x1000), 0x2000);

    print("pretend on flushed stack\n");
    /* note that this is a pseudo attack, in fact we get a .C
     * violation on the return back, while GBOP will react to it
     * immediately before executing the target.
     */
    __try {
        __asm {
            /* to ensure that xax isn't clobbered while executing random
             * code on the stack we set up a ud2 on the stack so that
             * we get an exception right away
             */
            push 000b0fh /* ud2 */
            mov ebx, esp
            push arg0
            push ebx
            jmp dword ptr LoadLibraryA
            mov res, eax
        }
    } __except ((res = (GetExceptionInformation())->ContextRecord->CXT_XAX),
                EXCEPTION_EXECUTE_HANDLER) {
        print("exception since not cleaning up stack\n");
    }
    REPORT(res);

    print("pretend in image but not after call\n");
    __try {
        __asm {
                push arg0
                /* note this is a true replacement of a CALL instruction
                 * but we don't currently allow this - may change with GBOP_IS_JMP
                 */
                push offset after_jmp
                jmp dword ptr LoadLibraryA
                mov res, eax
        }
    } __except ((res = (GetExceptionInformation())->ContextRecord->CXT_XAX),
                EXCEPTION_EXECUTE_HANDLER) {
        /* stack should be properly cleaned up */
        print("native exception unexpected, unless detected as violation\n");
    }
    REPORT(res);

    goto done;

    __asm {
        jmp after_jmp
    after_jmp:
        mov res, eax

    }
    REPORT(res);

    print("JMP allowed!\n");

done:
    print("done\n");
    return 0;
}
