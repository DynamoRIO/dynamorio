/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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
#define _IMAGEHLP_SOURCE_ /* export, not import */
#include <dbghelp.h>
#include "dbghelp_imports.h"

BOOL IMAGEAPI
SymInitializeW(__in HANDLE hProcess, __in_opt PCWSTR UserSearchPath,
               __in BOOL fInvadeProcess)
{
    return FALSE;
}

BOOL IMAGEAPI
SymSetSearchPathW(__in HANDLE hProcess, __in_opt PCWSTR SearchPath)
{
    return FALSE;
}

DWORD64
IMAGEAPI
SymLoadModuleExW(__in HANDLE hProcess, __in_opt HANDLE hFile, __in_opt PCWSTR ImageName,
                 __in_opt PCWSTR ModuleName, __in DWORD64 BaseOfDll, __in DWORD DllSize,
                 __in_opt PMODLOAD_DATA Data, __in_opt DWORD Flags)
{
    return 0;
}

BOOL IMAGEAPI
SymGetLineFromAddrW64(__in HANDLE hProcess, __in DWORD64 dwAddr,
                      __out PDWORD pdwDisplacement, __out PIMAGEHLP_LINEW64 Line)
{
    return FALSE;
}
