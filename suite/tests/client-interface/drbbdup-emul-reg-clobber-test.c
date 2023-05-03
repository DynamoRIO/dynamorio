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
#include <stdint.h>

int
main(int argc, char **argv)
{
#ifdef X86
    char buf1[1024];
    char buf2[1024];
#    ifdef WINDOWS
#        ifdef X64
    // TODO: Implement on Windows.
    uint64_t x = 0xabcdabcd;
    __movsq((unsigned long long *)buf2, (unsigned long long *)buf1, 0);
#        else
    // TODO: Implement on Windows.
    uint32_t x = 0xabcdabcd;
    __movsd((unsigned long *)buf2, (unsigned long *)buf1, 0);
#        endif
#    else
#        ifdef X64
    uint64_t x = 0x11111111;

    /* Put a magic value in rax to test if it was corrupted. Using rax because
     * it is used to spill flags.
     */
    __asm("lea %[buf1], %%rdi\n\t"
          "lea %[buf2], %%rsi\n\t"
          "mov $0xabcdabcd, %%rax\n\n"
          "mov $10, %%ecx\n\t"
          "rep movsq\n\t"
          "mov %%rax, %[x]\n\t"
          : [x] "=r"(x)
          : [buf1] "m"(buf1), [buf2] "m"(buf2)
          : "ecx", "rdi", "rsi", "rax", "memory");

#        else

    uint32_t x = 0x11111111;

    /* Put a magic value in eax to test if it was corrupted. Using eax because
     * it is used to spill flags.
     */
    __asm("lea %[buf1], %%edi\n\t"
          "lea %[buf2], %%esi\n\t"
          "mov $0xabcdabcd, %%eax\n\n"
          "mov $10, %%ecx\n\t"
          "rep movsd\n\t"
          "mov %%eax, %[x]\n\t"
          : [x] "=r"(x)
          : [buf1] "m"(buf1), [buf2] "m"(buf2)
          : "ecx", "edi", "esi", "eax", "memory");

#        endif
#    endif
#else
    // XXX: not implemented on non-X86 platforms currently.
    uint32_t x = 0xabcdabcd;
#endif
    print("x=%x\n", x);
    print("Hello, world!\n");
    return 0;
}
