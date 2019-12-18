/* **********************************************************
 * Copyright (c) 2011-2018 Google, Inc.  All rights reserved.
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

HMODULE
myload(char *lib)
{
    HMODULE hm = LoadLibrary(lib);
    if (hm == NULL)
        print("error loading library %s\n", lib);
    else if (GetProcAddress(hm, "data_attack") == NULL) {
        /* wrong dll */
        return NULL;
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

    lib1 = myload("win32.rebased.dll.dll");
    /* We used to just load the 8.3 name, but the Win8+ loader no longer loads a
     * separate copy that way.  Now we make an explicit separate copy.
     */
    lib2 = myload("win32.rebased2.dll.dll");
    if (lib1 == lib2) {
        print("there is a problem - should have collided, maybe missing\n");
    }

    FreeLibrary(lib1);
    FreeLibrary(lib2);

    return 0;
}
