/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
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

#include <windows.h>
#include <stdio.h>
#include "tools.h"

#define VERBOSE 0

typedef void (*funcptr)();

#pragma code_seg(".mycode1")
void
func2()
{
    print("func2\n");
}

funcptr f2 = &func2;

#pragma code_seg(".my_code2") /* 2 will be truncated - up to 8 char limit */
void
func3()
{
    print("exe calling f2\n");
    (*f2)();
    print("exe func3\n");
}

#pragma code_seg(".my_code3")
/* interesting - while the PE file will have an 8 byte section limit ".my_code"
 * this section is still going to be created as different from the .my_code2
 */
void
func4()
{
    print("exe func4\n");
}
#pragma code_seg() /* back in .text */

const funcptr cf = &func3;
funcptr f = &func2;

HMODULE
myload(char *lib)
{
    HMODULE hm = LoadLibrary(lib);
    if (hm == NULL) {
        print("error loading library %s\n", lib);
    } else {
        print("loaded %s\n", lib);
#if VERBOSE
        print("library is at " PFX "\n", hm);
#endif
    }
    return hm;
}

int
main()
{
    HMODULE lib1;
    HMODULE lib2;

    /* same as rebased test */
    lib1 = myload("win32.multisec.dll.dll");
    lib2 = myload("win32.multisec2.dll.dll");
    if (lib1 == lib2) {
        print("there is a problem - should have collided, maybe missing\n");
    }

    f();
    func3();
    func4();

    FreeLibrary(lib1);
    FreeLibrary(lib2);

    return 0;
}
