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

void
prfm(void)
{
    int d = 1;

    // prfm for read.
    __asm__ __volatile__("prfm\tpldl1keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpldl1strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpldl2keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpldl2strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpldl3keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpldl3strm,[%0]" : : "r"(d));

    // prfm for instruction.
    __asm__ __volatile__("prfm\tplil1keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm\tplil1strm,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm\tplil2keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm\tplil2strm,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm\tplil3keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfm\tplil3strm,[%0]" : : "r"(f));

    // prfm for write.
    __asm__ __volatile__("prfm\tpstl1keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpstl1strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpstl2keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpstl2strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpstl3keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfm\tpstl3strm,[%0]" : : "r"(d));
}
void
prfum(void)
{
    int d = 1;

    // prfum for read.
    __asm__ __volatile__("prfum\tpldl1keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpldl1strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpldl2keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpldl2strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpldl3keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpldl3strm,[%0]" : : "r"(d));

    // prfum for instruction.
    __asm__ __volatile__("prfum\tplil1keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum\tplil1strm,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum\tplil2keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum\tplil2strm,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum\tplil3keep,[%0]" : : "r"(f));
    __asm__ __volatile__("prfum\tplil3strm,[%0]" : : "r"(f));

    // prfum for write.
    __asm__ __volatile__("prfum\tpstl1keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpstl1strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpstl2keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpstl2strm,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpstl3keep,[%0]" : : "r"(d));
    __asm__ __volatile__("prfum\tpstl3strm,[%0]" : : "r"(d));
}

int
main(void)
{
    prfm();
    prfum();
    printf("Hello, world!\n");
    return 0;
}
