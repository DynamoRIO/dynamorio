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

#include <delayimp.h>

#define VERBOSE 0

/* WATCH OUT to not use the bash internal bind */
/* `which bind` -v -u delaybind.exe delaybind.dll.dll */

/* FIXME: I can't get a delay loaded DLL to get bound with bind.exe */

/* support unloading with, otherwise we won't be able to unload the DLL */

/* The following pragma does not seem to be correctly applying these
 * link options, so we'll put them on the command line in the Makefile.
 *
 * #pragma comment(linker, "/delayload:win32.delaybind.dll.dll /DELAY:UNLOAD")
 */

/* do not use /DELAY:NOBIND since that was the purpose of this test */

#pragma comment(lib, "delayimp")
/* using the standard implementation of delay loading in delayimp.lib */

#pragma comment(lib, "win32.delaybind.dll.lib")

/* note that in later Visual Studio __FUnloadDelayLoadedDLL has
 * been renamed to __FUnloadDelayLoadedDLL2 since they had to
 * change the ImgDelayDescr structure to use RVAs instead of
 * pointers.
 */
#define UNLOAD __FUnloadDelayLoadedDLL2

int __declspec(dllimport) make_a_lib(int arg);

int
myloader(void)
{
    int TestReturn;

    // dll will get loaded at this point
    make_a_lib(3);

    // dll will unload at this point
    TestReturn = UNLOAD("win32.delaybind.dll.dll");

    if (TestReturn)
        print("\nDLL was unloaded\n");
    else
        print("\nDLL was not unloaded\n");
    return TestReturn;
}

int
main()
{
    print("starting delaybind\n");

    myloader();

    /* try again */
    myloader();

    print("done with delaybind\n");

    return 0;
}

/* Note that dumpbin from VC98 is crashing of showing imports of say
 * user32.dll on XP SP2, so the format of some fields may have changed!
 */

#ifdef DELAYHLP
/* from Microsoft Visual Studio/VC98/Include/DELAYHLP.CPP */

if (pinh->Signature == IMAGE_NT_SIGNATURE &&
    TimeStampOfImage(pinh) == pidd->dwTimeStamp &&
    FLoadedAtPreferredAddress(pinh, hmod)) {

    OverlayIAT(pidd->pIAT, pidd->pBoundIAT);

    TimeStampOfImage(PIMAGE_NT_HEADERS pinh)
    {
        return pinh->FileHeader.TimeDateStamp;
    }

    FLoadedAtPreferredAddress(PIMAGE_NT_HEADERS pinh, HMODULE hmod)
    {
        return DWORD(hmod) == pinh->OptionalHeader.ImageBase;
    }
#endif
