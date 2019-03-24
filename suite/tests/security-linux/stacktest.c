/* **********************************************************
 * Copyright (c) 2003 VMware, Inc.  All rights reserved.
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

/*
 * Written by Solar Designer and placed in the public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void
caller(void (*trampoline)(void))
{
    fprintf(stderr, "Attempting to call a trampoline...\n");

    trampoline();
}

void
do_trampoline(void)
{
    void nested(void)
    {
        fprintf(stderr, "Succeeded.\n");
    }

    caller(nested);
}

void
do_exploit(void)
{
    fprintf(stderr, "Attempting to simulate a buffer overflow exploit...\n");

#ifdef __i386__
    __asm__ __volatile__("movl $1f,%%eax\n\t"
                         ".byte 0x68; popl %%ecx; jmp *%%eax; nop\n\t"
                         "pushl %%esp\n\t"
                         "ret\n\t"
                         "1:"
                         :
                         :
                         : "ax", "cx");
#else
#    error Wrong architecture
#endif

    fprintf(stderr, "Succeeded.\n");
}

#define USAGE                                    \
    "Usage: %s OPTION\n"                         \
    "Non-executable user stack area tests\n\n"   \
    "  -t\tcall a GCC trampoline\n"              \
    "  -e\tsimulate a buffer overflow exploit\n" \
    "  -b\tsimulate an exploit after a trampoline call\n"

void
usage(char *name)
{
    printf(USAGE, name ? name : "stacktest");
    exit(1);
}

int
main(int argc, char **argv)
{
    if (argc != 2) {
        /* default behavior for unit test */
        do_trampoline();
        do_exploit();
        return 0;
    }

    if (argv[1][0] != '-' || strlen(argv[1]) != 2)
        usage(argv[0]);

    switch (argv[1][1]) {
    case 't': do_trampoline(); break;

    case 'b': do_trampoline();

    case 'e': do_exploit(); break;

    default: usage(argv[0]); break;
    }

    return 0;
}
