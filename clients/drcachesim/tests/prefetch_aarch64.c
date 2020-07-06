/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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

#include <stdio.h>

void
f()
{
}

#if defined(__aarch64__)
void
prfm(void)
{
    int d = 1;

    // prfm for read.
    __asm__ __volatile__("prfm pldl1keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pldl1strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pldl2keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pldl2strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pldl3keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pldl3strm,[%0]" : : "r"(d));

    // prfm for instruction.
    __asm__ __volatile__("prfm plil1keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm plil1strm,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm plil2keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm plil2strm,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm plil3keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm plil3strm,[%0]" : : "r"(f));

    // prfm for write.
    __asm__ __volatile__("prfm pstl1keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pstl1strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pstl2keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pstl2strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pstl3keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm pstl3strm,[%0]" : : "r"(d));
}

void
prfum(void)
{
    int d = 1;

    // prfum for read.
    __asm__ __volatile__("prfum pldl1keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pldl1strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pldl2keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pldl2strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pldl3keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pldl3strm,[%0]" : : "r"(d));

    // prfum for instruction.
    __asm__ __volatile__("prfum plil1keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum plil1strm,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum plil2keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum plil2strm,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum plil3keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum plil3strm,[%0]" : : "r"(f));

    // prfum for write.
    __asm__ __volatile__("prfum pstl1keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pstl1strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pstl2keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pstl2strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pstl3keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum pstl3strm,[%0]" : : "r"(d));
}
#endif

int
main(void)
{
#if defined(__aarch64__)
    prfm();
    prfum();
#endif

    printf("Hello, world!\n");
    return 0;
}
