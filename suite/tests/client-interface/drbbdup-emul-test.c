/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "tools.h"

int
main(int argc, char **argv)
{
#ifdef X86
    /* Test i#5398 with a zero-iter rep movs loop. */
    char buf1[1024];
    char buf2[1024];
#    ifdef WINDOWS
#        ifdef X64
    __movsq((unsigned long long *)buf2, (unsigned long long *)buf1, 0);
#        else
    __movsd((unsigned long *)buf2, (unsigned long *)buf1, 0);
#        endif
#    else
#        ifdef X64
    __asm("lea %[buf1], %%rdi\n\t"
          "lea %[buf2], %%rsi\n\t"
          "mov $0, %%ecx\n\t"
          "rep movsq\n\t"
          :
          : [ buf1 ] "m"(buf1), [ buf2 ] "m"(buf2)
          : "ecx", "rdi", "rsi", "memory");
#        else
    __asm("lea %[buf1], %%edi\n\t"
          "lea %[buf2], %%esi\n\t"
          "mov $0, %%ecx\n\t"
          "rep movsd\n\t"
          :
          : [ buf1 ] "m"(buf1), [ buf2 ] "m"(buf2)
          : "ecx", "edi", "esi", "memory");
#        endif
#    endif
#endif
    print("Hello, world!\n");
    return 0;
}
