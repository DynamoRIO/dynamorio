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

/* case 4347 - testing rebasing
 * make sure DLL and image are conflicting so that DLL goes somewhere else
 *
 *
 * Nothing to worry about: the two NtFlushInstructionCache calls are
 * of (0,0) and of (IAT,IAT_size) there is no explicit flush on a
 * .data section for which the original inquiry was filed.
 *
 * intercept_load_dll: rebase~1.dll
 * syscall: NtMapViewOfSection 0x00350000 size=0x29000 prot=rw-- => 1073741827
 * syscall: NtProtectVirtualMemory process=0xffffffff base=0x00351000 size=0x1b000
 * prot=rw-- 0x4 WARNING: executable region being made writable and non-executable
 * syscall: NtProtectVirtualMemory process=0xffffffff base=0x0036c000 size=0x3000
 * prot=rw-- 0x4 syscall: NtProtectVirtualMemory process=0xffffffff base=0x00377000
 * size=0x2000 prot=rw-- 0x4 syscall: NtProtectVirtualMemory process=0xffffffff
 * base=0x00351000 size=0x1b000 prot=--x- 0x10 WARNING: data region 0x00351000 is being
 * made executable syscall: NtProtectVirtualMemory process=0xffffffff base=0x0036c000
 * size=0x3000 prot=r--- 0x2 syscall: NtProtectVirtualMemory process=0xffffffff
 * base=0x00377000 size=0x2000 prot=r--- 0x2 syscall: NtFlushInstructionCache 0x00000000
 * size=0x0 syscall: NtProtectVirtualMemory process=0xffffffff base=0x003761b4 size=0x18c
 * prot=rw-- 0x4 syscall: NtProtectVirtualMemory process=0xffffffff base=0x00376000
 * size=0x1000 prot=rw-c 0x8 syscall: NtFlushInstructionCache 0x00376000 size=0x18c module
 * is being loaded, ignoring flush intercept_unload_dll:
 * i:\vlk\trees\t4347-rebase\suite\tests\win32\rebased.dll.dll @0x10000000 size 0x29000
 *
 * FIXME: add these to the Makefile
 * # force rebasing conflicts
 * # LDFLAGS_B = /base:"0x00400000"
 * # FIXME: even more evil would be to set to conflict with dynamorio.dll
 *
 * LINK = $(LDFLAGS_B) $(WINLIBS) /link /subsystem:console
 * # FIXME: should also reorder when user32.dll is mapped in
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

const funcptr cf = &foo;
funcptr f = &foo;

void
dlltest(void)
{
    print("dlltest\n");
    f();
    cf();
    f = &bar;
    f();
    cf();
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
