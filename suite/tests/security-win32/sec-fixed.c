/* **********************************************************
 * Copyright (c) 2006-2010 VMware, Inc.  All rights reserved.
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
    HMODULE lib;
    HMODULE cmd;
    int res;
    lib = myload("security-win32.sec-fixed.dll.dll");
    FreeLibrary(lib);

    /* unclear what this code is supposed to do on other platforms
     * FIXME: move to its own load-exe
     */
    /* real use seen of PCHealth\HelpCtr\Binaries\HelpCtr.exe */

    cmd = LoadLibraryExW(L"cmd.exe", NULL, LOAD_LIBRARY_AS_DATAFILE);
    assert(cmd != NULL);
    res = FreeLibrary(cmd);
    assert(res);
    print("cmd.exe as data\n");

    /* FIXME: for some reason the loader reuses the exe - if we ask for cmd.exe again here
     */
    cmd = LoadLibraryExW(L"calc.exe", NULL,
                         DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);
    assert(cmd != NULL);
    res = FreeLibrary(cmd);
    assert(res);
    print("calc.exe as data and no resolve\n");

    /* note that windbg will show only this one as a module */
    cmd = LoadLibraryExW(L"rundll32.exe", NULL, DONT_RESOLVE_DLL_REFERENCES);
    assert(cmd != NULL);
    res = FreeLibrary(cmd);
    assert(res);
    print("rundl32.exe as no resolve\n");

    print("done\n");

    return 0;
}
