/* **********************************************************
 * Copyright (c) 2006 VMware, Inc.  All rights reserved.
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

/* case 7572 testing non-committed pages in a dll
 * this dll is /fixed, so even -rct_reloc will not find the references
 */

/*
To build need to align to 0x2000 to get non-committed pages as padding

cl /nologo /Zi /I. tools.c /Od /MTd win32/secalign.dll.c kernel32.lib user32.lib gdi32.lib
winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib
odbc32.lib odbccp32.lib /link /subsystem:console /dll /out:win32/secalign.dll.dll
/implib:win32/secalign.dll.lib /align:0x2000 /driver

new link.exe has /section, accomplishes same thing, still need /driver:
"/c/Program Files/Microsoft Visual Studio 8/VC/bin/link.exe" win32/secalign.dll.obj
/subsystem:console /dll /out:win32/secalign.dll.dll /implib:win32/secalign.dll.lib
/section:.text,,align=0x2000 /driver
*/

#include "tools.h"
#include <windows.h>

/* we don't need an explicit jmp* or call* since CRT ends up using a call* */

/* Linker requires /driver if specifying /align.
 * Documentation says that "The linker will perform some special
 * optimizations if this option is selected." -- not sure if any other changes.
 */
#pragma comment(linker, "/fixed /align:0x2000 /driver")

/* our Makefile expects a .lib */
int __declspec(dllexport) make_a_lib(int arg)
{
    return 1;
}

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH: print("in fixed dll\n"); break;
    }
    return TRUE;
}
