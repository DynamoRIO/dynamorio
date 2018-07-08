/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

static char *
get_short_name(char *exename)
{
    char *exe;
    exe = strrchr(exename, '\\');
    if (exe == NULL) {
        exe = strrchr(exename, '/');
        if (exe == NULL)
            exe = exename;
    } else
        exe++; /* skip backslash */
    return exe;
}

static char *modules[] = {
    "dynamorio.dll", "win32.dll.exe", "win32.dll.dll.dll", "kernel32.dll", "ntdll.dll",
};

static int num_modules = sizeof(modules) / sizeof(char *);

static BOOL module_found[sizeof(modules) / sizeof(char *)];

int
check_mbi(MEMORY_BASIC_INFORMATION *mbi)
{
    int nLen, i, num_found = 0;
    char szModName[MAX_PATH];
    if (mbi->Type != MEM_IMAGE || mbi->AllocationBase != mbi->BaseAddress ||
        mbi->AllocationBase == NULL) {
        nLen = 0;
    } else {
        nLen = GetModuleFileNameA((HINSTANCE)mbi->AllocationBase, szModName,
                                  (sizeof(szModName) / sizeof(szModName[0])));
    }
    if (nLen > 0) {
        char *nm = get_short_name(szModName);
        for (i = 0; i < num_modules; i++) {
            if (_stricmp(nm, modules[i]) == 0) {
                num_found++;
                module_found[i] = TRUE;
#if VERBOSE
                print("Found %s\n", modules[i]);
#endif
            }
        }
#if VERBOSE
        print("%p-%s\n", mbi->AllocationBase, szModName);
#endif
    }
    return num_found;
}

void
print_modules()
{
    /* Try to print out addresses of all loaded dlls */
    PBYTE pb = NULL;
    MEMORY_BASIC_INFORMATION mbi;
    int i, num_found = 0;
    memset(module_found, 0, sizeof(module_found));
#if VERBOSE
    print("\nLoaded Modules:");
#endif
    while (VirtualQuery(pb, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        num_found += check_mbi(&mbi);
        pb += mbi.RegionSize;
    }
#if VERBOSE
    print("\n\n");
#endif
    print("Found %d of %d expected modules\n", num_found, num_modules);
    for (i = 0; i < num_modules; i++) {
        if (!module_found[i]) {
            print("Didn't find module %s\n", modules[i]);
        }
    }

    /* dr intentionally screws up QueryVirtualMemory calls on the dr dll to
     * hide from walks like the above, check our dll address directly to see
     * if we are still on the module list */
    mbi.Type = MEM_IMAGE;
    mbi.BaseAddress = (PVOID)0x71000000; /* release */
    mbi.AllocationBase = (PVOID)0x71000000;
    num_found += check_mbi(&mbi);
    mbi.BaseAddress = (PVOID)0x15000000; /* debug */
    mbi.AllocationBase = (PVOID)0x15000000;
    num_found += check_mbi(&mbi);

    print("Found %d of %d expected modules\n", num_found, num_modules);
    for (i = 0; i < num_modules; i++) {
        if (!module_found[i]) {
            print("Didn't find module %s\n", modules[i]);
        }
    }

    fflush(stdout);
}

int
main()
{
    HANDLE lib;
    print_modules();
    lib = LoadLibrary("win32.dll.dll.dll");
    if (lib == NULL) {
        print("error loading library\n");
    } else {
        BOOL res;
        BOOL(WINAPI * proc)(DWORD);
        print("loaded win32.dll.dll.dll\n");
#if VERBOSE
        print("library is at " PFX "\n", lib);
        fflush(stdout);
#endif
        print_modules();
        proc = (BOOL(WINAPI *)(DWORD))GetProcAddress(lib, (LPCSTR) "import_me");
        res = (*proc)(5);
        print("Called import_me with 5, result is %d\n", res);
        FreeLibrary(lib);
        print("freed library\n");
        print_modules();
        fflush(stdout);
    }
    return 0;
}
