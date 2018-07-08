/* **********************************************************
 * Copyright (c) 2005 VMware, Inc.  All rights reserved.
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

/* case 286 testing multiple code sections:
 *
 * Add functions to multiple code segments and have references to them
 * from other, including data segments just like rebased make sure DLL
 * and image are conflicting so that DLL goes somewhere else
 */

#include "tools.h"
#include <windows.h>

typedef void (*funcptr)();

void
foo()
{
    print("foo\n");
}

void
bar()
{
    print("bar\n");
}

void
func1()
{
    print("bar\n");
}

#pragma code_seg(".mycode1")
void
func2()
{
    print("func2\n");
}

#pragma data_seg(".data2")
funcptr f2 = &func2;

#pragma data_seg()

/* VC7 has support for push and pop that we can't use in VC6
 * #pragma code_seg(push, r1, ".mycode2")
 */
#pragma code_seg(".mycode2")
void
func3()
{
    print("calling f2\n");
    (*f2)();
    print("func3\n");
}

#pragma code_seg(".mycode1") /* back to my_code1 */
void
func4()
{
    print("func4\n");
}
#pragma code_seg() /* back in .text */

const funcptr cf = &foo;
funcptr f = &foo;

#pragma data_seg(".data1")
funcptr f4 = &func4;

#pragma data_seg()

void
dlltest(void)
{
    print("dlltest\n");
    f();
    cf();
    f = &bar;
    f();
    cf();
    func3();
    f2 = &func4;
    func3();
    f4();
}

/* our Makefile expects a .lib */
int __declspec(dllexport) data_attack(int arg)
{
    print("data_attack\n");
    /* FIXME: will do this some other time */
    dlltest();
    return 1;
}

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH: dlltest(); break;
    }
    return TRUE;
}
