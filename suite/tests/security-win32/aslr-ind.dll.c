/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2008 VMware, Inc.  All rights reserved.
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

/* case 7017 - showing off ASLR prevention */

#include "tools.h"
#include <windows.h>

typedef int (*fiptr)();

#define NAKED __declspec(naked)

/* could be an export but instead is address taken callback */
NAKED
/* PR 229292: must be static to avoid having an ILT entry in debug build */
static int
funny_target()
{
    __asm {
        mov eax,1
        jmp short over
        mov eax,2 /* bad target */
over:
        add eax,eax
        add eax,eax
        add eax,eax
        add eax,eax
        ret
    }
}

fiptr __declspec(dllexport) giveme_target(int arg)
{
    print("ready to go %d\n", arg);
    return funny_target;
}

void __declspec(dllexport) precious(void)
{
    print("PRECIOUS in a DLL, ATTACK SUCCESSFUL!\n");
    /* stack not clean have to stop here */
    exit(1);
}

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH:
        /* nothing */
        break;
    }
    return TRUE;
}
