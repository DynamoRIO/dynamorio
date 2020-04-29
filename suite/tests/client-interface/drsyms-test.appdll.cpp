/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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

#include "configure.h"
#if defined(UNIX) || defined(_MSC_VER)
#    include "tools.h"
#else /* cygwin/mingw */
#    include <windows.h>
#    include <stdio.h>
#    define print(...) fprintf(stderr, __VA_ARGS__)
/* we want PE exports so dr_get_proc_addr finds them */
#    define EXPORT __declspec(dllexport)
#    define NOINLINE __declspec(noinline)
#endif

NOINLINE void
stack_trace(void)
{
#if defined(UNIX) || defined(_MSC_VER)
    /* i#1801-c#2: call to page_align to make sure tools.h is included in the binary.
     * This test should pass with any page size from 1 KiB to 256 MiB.
     */
    if (page_align((char *)0x4ffffc12) != (char *)0x50000000)
        print("page_align is wrong!\n");
#endif
}

NOINLINE int
dll_public(int a)
{
    stack_trace();
    return a + 1;
}

static NOINLINE int
dll_static(int a)
{
    return dll_public(a + 1);
}

extern "C" EXPORT int
dll_export(int a)
{
    return dll_static(a + 1);
}
