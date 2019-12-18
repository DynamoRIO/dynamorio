/* **********************************************************
 * Copyright (c) 2003-2005 VMware, Inc.  All rights reserved.
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

#include <assert.h>
#include <stdio.h>
#include <math.h>

#ifdef UNIX
#    include <unistd.h>
#    include <signal.h>
#    include <ucontext.h>
#    include <errno.h>
#endif

#include "tools.h" /* for print() */

int
foo()
{
    print("in foo\n");
    exit(1);
}

int
bar()
{
    print("in bar\n");
    return 0;
}

void
vbpop()
{
    print("in vbpop\n");
#ifdef UNIX
    __asm__ volatile(
        "push    $bad_target\n"
        "jmp     some_func\n"
        "some_func:\n"
        "call bar\n"
        // this call needed to get the RET in its own fragment
        // otherwise we actually match the loose pattern in at_vbjmp_exception()
        // which is very similar to what's happening here in at_vbpop_exception()
        "ret\n"
        "bad_target:\n"
        "xor %eax,%eax\n");
#else
    __asm {
            push    offset bad_target
            jmp     some_func
       some_func:
            call bar
            ret
       bad_target:
            xor eax,eax
    }
#endif
    print("vbpop success\n");
}

void
vbjmp()
{
#ifdef UNIX
    __asm__ volatile("mov     $0x38,%eax\n"
                     "cmp     $0xc033,%ax\n"
                     "mov     $0x1d6600,%edx\n"
                     "push    $foo\n"
                     "ret");
#else
    __asm {
            mov     eax,0x38
            cmp     ax,0xc033
            mov     edx,0x1d6600
            push    offset foo
            ret
    }
#endif
}

int
main(int argc, char *argv[])
{
    int i;
    unsigned char original670[] = "\xb8\x38\x00\x00\x00" /* mov     eax,0x38 */
                                  "\x66\x3d\x33\xc0"     /* cmp     ax,0xc033 */
                                  "\xba\x00\x66\x1d\x00" /* mov     edx,0x1d6600 */
                                  "\x68\xfc\xe4\x00\x65" /* push    0x6500e4fc */
                                  "\xc3"                 /* ret */
        ;
    INIT();

    print("VB ret $+1 about to happen\n");
    for (i = 0; i < 10; i++) {
        vbpop();
    }

    print("VB push;ret about to happen\n");
    vbjmp();

    print("SHOULD NEVER GET HERE\n");

    return 0;
}
