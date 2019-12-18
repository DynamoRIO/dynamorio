/* **********************************************************
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

/* Copyright (c) 2006-2007 Determina Corp. */

#pragma warning(disable : 4100) //'Reserved' : unreferenced formal parameter
/* shows up in buildtools/VC/8.0/dist/VC/include/vadefs.h
 * supposed to include identifier in the pop pragma and they don't
 */
#pragma warning(disable : 4159) // #pragma pack has popped previously pushed identifier

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

__declspec(dllimport) void dr_early_inject_helper2_dummy_func();

__declspec(dllexport) void dr_early_inject_helper1_dummy_func()
{
    /* nothing: just here so we have export section for easy name finding */
}

BOOL WINAPI /* get link warning 4216 if export via APIENTRY */
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    /* Want to do nothing but still have the dependency, hModule is the base
     * address of this DLL so should never equal -1 */
    if ((LONG_PTR)hModule == -1) {
        /* call does nothing anyways, just inserted so the linker doesn't
         * optimize out the dependency, but we ensure isn't reached anyways */
        dr_early_inject_helper2_dummy_func();
    }
    return TRUE;
}
