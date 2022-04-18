/* **********************************************************
 * Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
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

/* preunload: ensures that drpreinject.dll is unloaded
 */
#include <windows.h>
#include "tools.h"

#define PREINJECT_NAME "drpreinject.dll"
#define PREINJECT_BASE 0x14000000

#define INJECT_ALL_HIVE HKEY_LOCAL_MACHINE
#define INJECT_ALL_KEY "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows"
#define INJECT_ALL_SUBKEY "AppInit_DLLs"

/* two different checks: GetModuleFileNameA and VirtualQuery */
BOOL
ensure_no_preinject()
{
    int len;
    char name[MAX_PATH];
    MEMORY_BASIC_INFORMATION mbi;

    len = GetModuleFileNameA((HINSTANCE)PREINJECT_BASE, name,
                             (sizeof(name) / sizeof(name[0])));
    if (len > 0) {
        print("ERROR: found module %s at " PFX "\n", name, PREINJECT_BASE);
        return FALSE;
    }

    len = VirtualQuery((void *)PREINJECT_BASE, &mbi, sizeof(mbi));
    if (len != sizeof(mbi)) {
        print("ERROR: error querying " PFX "\n", PREINJECT_BASE);
        return FALSE;
    } else {
        if (mbi.State != MEM_FREE) {
            print("ERROR: " PFX " is not MEM_FREE!\n", PREINJECT_BASE);
            return FALSE;
        }
    }
    return TRUE;
}

BOOL
load_preinject()
{
    HKEY hk;
    HMODULE hlib;
    char val[MAX_PATH];
    int res, len = MAX_PATH * sizeof(val[0]);

    res = RegOpenKeyEx(INJECT_ALL_HIVE, INJECT_ALL_KEY, 0, KEY_READ, &hk);
    if (res != ERROR_SUCCESS)
        return FALSE;

    res = RegQueryValueEx(hk, INJECT_ALL_SUBKEY, 0, 0, (LPBYTE)val, &len);
    RegCloseKey(hk);
    if (res != ERROR_SUCCESS)
        return FALSE;

    if (val[0] == '\0') {
        print("ERROR: no preinject library set\n");
        return FALSE;
    }
    print("loading in preinject library\n", val);
    hlib = LoadLibrary(val);
    if (hlib == NULL) {
        /* with the old preinject, this returns NULL b/c its DllMain
         * returns FALSE -- but the new preinject will return success
         */
        if (GetLastError() == ERROR_DLL_INIT_FAILED) {
            print("DLL init routine failed -- are you using an old %s?\n",
                  PREINJECT_NAME);
        }
    }
    return (hlib != NULL);
}

int
main()
{
    HANDLE hlib;
    INIT();

    print("preunload main()\n");

    /* use something from user32 so that AppInit injection works */
    print("using user32: %d\n", IsCharAlpha('4'));

    /* check #1: preinject should be gone */
    if (ensure_no_preinject())
        print("%s not found\n", PREINJECT_NAME);

    /* check #2: re-loading preinject shouldn't cause problems, and it
     * should get unloaded again -- except with a debug build preinject
     * we'll get a popup warning about double injection!  so we only
     * run with release build preinjects, which is the case for any
     * clean-slate runs, like our nightly regressions.
     */
    if (!load_preinject())
        print("failed to load %s\n", PREINJECT_NAME);
    if (ensure_no_preinject())
        print("%s not found\n", PREINJECT_NAME);

    return 0;
}
