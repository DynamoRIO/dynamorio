/* **********************************************************
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

/* case 5325 ASLR on DLLs
 * - FIXME: kernel32.dll user32.dll and ntdll.dll should eventually be rebased
 *   and if done statically we wouldn't know that they are fine
 */

/* code inherited from hooker-ntdll.c */

#include "tools.h"
#include <windows.h>

#define VERBOSE 0
#define KNOWN_DLLS 1

void *
get_module_preferred_base(void *module_base)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    dos = (IMAGE_DOS_HEADER *)module_base;
    nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    return (void *)nt->OptionalHeader.ImageBase;
}

int num_checks = 0;
int num_at_base = 0;
int num_no_module = 0;

void
do_check(const char *hook_dll, const char *hookfn)
{
    HANDLE target_mod = GetModuleHandle(hook_dll);
    void *preferred = NULL;
    unsigned char *hooktarget;

    if (target_mod == NULL) {
        HMODULE hmod = LoadLibrary(hook_dll);
        target_mod = GetModuleHandle(hook_dll);
        if (hmod != target_mod || target_mod == NULL) {

            /* NOTE: very funny LoadLibrary("apphelp.dll ") works, but
             * GetModuleHandle("apphelp.dll ") doesn't find the same
             * target!  can't think of a way to exploit ;)
             */
            print("GLE: %d\n", GetLastError());
            print("error: hmod " PFX ", target_mod " PFX "\n", hmod, target_mod);
        }
        if (target_mod == NULL) {
            num_no_module++;
            return;
        }
    }

    hooktarget = (char *)GetProcAddress(target_mod, hookfn);

    if (hooktarget == NULL) {
        print("error: couldn't find %s!%s\n", hook_dll, hookfn);
    } else
        print("%s!%s ok\n", hook_dll, hookfn);

    if (target_mod != NULL)
        preferred = get_module_preferred_base(target_mod);
#if VERBOSE
    print("%s at " PFX ", preferred " PFX "\n", hook_dll, target_mod, preferred);
    print("%s!%s " PFX "\n", hook_dll, hookfn, hooktarget);
#endif
    print("%s at %s base\n", hook_dll,
          (preferred == target_mod) ? "preferred" : "randomized");

    num_checks++;
    if (preferred == target_mod)
        num_at_base++;

    print("all should be good\n");
}

int
main()
{
    INIT();

    do_check("kernel32.dll", "GetProcessHeaps");
    do_check("kernel32.dll", "Sleep");

    do_check("user32.dll", "MessageBeep");
    do_check("user32.dll", "MessageBoxW");

    do_check("win32.aslr-dll.exe", "executable");

    /* Not in KnownDlls! */
    do_check("ntdll.dll", "NtCreateSection");

#if KNOWN_DLLS
    /* all KnownDlls */
    do_check("advapi32.dll", "GetAclInformation");
    do_check("comdlg32.dll", "GetOpenFileNameW");
    do_check("gdi32.dll", "GdiPlayEMF");
    do_check("imagehlp.dll", "ImageRvaToVa");
    // imagehlp!ImageDirectoryEntryToDataEx
    do_check("kernel32.dll", "findexport");
    do_check("lz32.dll", "findexport");
    do_check("ole32.dll", "findexport");
    do_check("oleaut32.dll", "findexport");
    do_check("olecli32.dll", "findexport");
    do_check("olecnv32.dll", "findexport");
    do_check("olesvr32.dll", "findexport");
    /* FIXME: getting an error accessing olethk32 on xp64! */
    do_check("olethk32.dll", "findexport");
    do_check("rpcrt4.dll", "findexport");
    do_check("shell32.dll", "findexport");
    do_check("url.dll", "findexport");
    do_check("urlmon.dll", "findexport");
    do_check("user32.dll", "findexport");
    do_check("version.dll", "findexport");
    do_check("wininet.dll", "findexport");
    do_check("wldap32.dll", "findexport");
    /* transitive closure */
    do_check("apphelp.dll", "findexport");
    do_check("comctl32.dll", "findexport");
    do_check("crypt32.dll", "findexport");
    do_check("cryptui.dll", "findexport");
    do_check("mpr.dll", "findexport");
    do_check("msasn1.dll", "findexport");
    do_check("msvcrt.dll", "findexport");
    do_check("netapi32.dll", "findexport");
    do_check("shdocvw.dll", "findexport");
    do_check("shlwapi.dll", "findexport");
    do_check("userenv.dll", "findexport");
    do_check("wintrust.dll", "findexport");
    do_check("wow32.dll", "findexport");
#endif

    do_check("secur32.dll", "LsaLogonUser");
    do_check("secur32.dll", "MakeSignature");

    /* leave error checking */
    do_check("unknown.dll", "LsaLogonUser");
    do_check("secur32.dll", "MissingExport");

    /* case 8705 sfc.dll, though not present on all platforms */
    do_check("sfc.dll", "SfpVerifyFile");

    print("%d checked\n", num_checks);
    print("%d at base\n", num_at_base);
    print("%d DLL not found\n", num_no_module);
    print("done\n");
}

/* KnownDlls


in Registry
"advapi32"="advapi32.dll"
"comdlg32"="comdlg32.dll"
"gdi32"="gdi32.dll"
"imagehlp"="imagehlp.dll"
"kernel32"="kernel32.dll"
"lz32"="lz32.dll"
"ole32"="ole32.dll"
"oleaut32"="oleaut32.dll"
"olecli32"="olecli32.dll"
"olecnv32"="olecnv32.dll"
"olesvr32"="olesvr32.dll"
"olethk32"="olethk32.dll"
"rpcrt4"="rpcrt4.dll"
"shell32"="shell32.dll"
"url"="url.dll"
"urlmon"="urlmon.dll"
"user32"="user32.dll"
"version"="version.dll"
"wininet"="wininet.dll"
"wldap32"="wldap32.dll"

in WinObj - this is the transitive closure of statically linked for the above DLLs

apphelp.dll   <- wow32.dll      <- olethk32.dll
comctl32.dll  <- comdlg32.dll
crypt32.dll
cryptui.dll
mpr.dll        <- olecli32.dll
msasn1.dll
msvcrt.dll     <- imagehlp.dll
netapi32.dll
shdocvw.dll
shlwapi.dll    <- comdlg32.dll
userenv.dll    <- wow32.dll      <- olethk32.dll
wintrust.dll
wow32.dll      <- olethk32.dll

TOTEST: why is ntvdm.exe not in the list?

TOTEST: lz32.dll has no IMPORTS -> confuses dumpbin and depends

*/
