/* **********************************************************
 * Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
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

#define _CRT_SECURE_NO_WARNINGS 1

#include <windows.h>
#include <stdio.h>
#include "tools.h"

#define VERBOSE 0

char sysroot[MAX_PATH];

static void
myload(char *lib, bool append_to_sysroot)
{
    HANDLE file, mapping;
    SIZE_T size = 0, size_to_map;
    char *map_addr;
    MEMORY_BASIC_INFORMATION mbi;
    char file_name_buf[MAX_PATH];
    char *file_name = lib;

    if (append_to_sysroot) {
        _snprintf(file_name_buf, BUFFER_SIZE_ELEMENTS(file_name_buf), "%s%s", sysroot,
                  lib);
        file_name = file_name_buf;
    }

    file = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        print("error opening file \"%s\", code=%d\n", file_name, GetLastError());
        return;
    }

    mapping = CreateFileMapping(file, NULL, PAGE_WRITECOPY | SEC_IMAGE, 0, 0, NULL);
    if (mapping == NULL) {
        print("error creating mapping for file \"%s\", code=%d\n", file_name,
              GetLastError());
        return;
    }

    map_addr = MapViewOfFile(mapping, FILE_MAP_COPY, 0, 0, 0);
    if (map_addr == NULL) {
        print("error mapping file \"%s\", code=%d\n", file_name, GetLastError());
        return;
    } else {
        print("test map of %s succeeded\n", lib);
    }
    /* FIXME - can't find a good way to get the size of the section, can't use file size
     * since as an image will be larger, msdn says to use VirtualQuery which of course
     * doesn't work on a image.  We'll just walk instead (what we really want is
     * NtQuerySection:SectionBasicInformation but the API routines don't appear to
     * expose that). */
    while (VirtualQuery(map_addr + size, &mbi, sizeof(mbi)) == sizeof(mbi) &&
           mbi.State != MEM_FREE && mbi.AllocationBase == map_addr) {
        size += mbi.RegionSize;
    }
#if VERBOSE
    print("mapping size = " PFX "\n", size);
#endif
    UnmapViewOfFile(map_addr);

    /* FIXME - for additional tests we could call into the section, we could also map
     * at offset and call into as well.  FIXME - check what happes when we ask for
     * non-page multiple (esp if file and/or section alignment is < page. */
    for (size_to_map = PAGE_SIZE; size_to_map <= size; size_to_map += PAGE_SIZE) {
        map_addr = MapViewOfFile(mapping, FILE_MAP_COPY, 0, 0, size_to_map);
        UnmapViewOfFile(map_addr);
    }
}

int
main()
{
    /* Note - if not part of session 0 requires SeCreateGlobalPrivilege for XPsp2
     * 2ksp4 and 2k3 (and presumably Vista). */
    HANDLE hToken = NULL;
    TOKEN_PRIVILEGES Priv;
    DWORD res;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, FALSE,
                         &hToken)) {
        /* can't get thread token, try process token instead */
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
                              &hToken)) {
            print("error opening token, code=%d\n", GetLastError());
        }
    }
    if (hToken != NULL) {
        Priv.PrivilegeCount = 1;
        Priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        LookupPrivilegeValue(NULL, "SeCreateGlobalPrivilege", &Priv.Privileges[0].Luid);
        if (!AdjustTokenPrivileges(hToken, FALSE, &Priv, sizeof(Priv), NULL, 0)) {
            print("error adjusting privileges, code=%d\n", GetLastError());
        }
    }

    res = GetEnvironmentVariable("SYSTEMROOT", sysroot, BUFFER_SIZE_ELEMENTS(sysroot));
    NULL_TERMINATE_BUFFER(sysroot);
    if (res == 0 || res > BUFFER_SIZE_ELEMENTS(sysroot))
        print("Unable to get system root\n");

    /* FIXME - add specially crafted .exe/.dlls that have page boundaries at interesting
     * locations. */

    /* We don't yet safetly handle exports, so we limit the test to .exe's (which usually
     * don't have exports) to excessive test failures.  The case 9717, the driving case
     * for this test, partial maps are limited to only. exe's so we should be ok for
     * now. FIXME */
    myload("\\system32\\user32.dll", true);
#if 0
    /* still need to quiet some aslr asserts */
    myload("\\system32\\shell32.dll", true);
    /* TODO : add more dlls, maybe custom made to have strange boundaries. */
#endif

    /* Test some .exe images, none of these have exports and except explorer.exe none
     * of these have reloc sections either, so we generally expect these to succeed
     * (after the image entry not in module fix).  But, they are still good for showing
     * asserts. */
    myload("\\system32\\calc.exe", true);
    myload("\\system32\\notepad.exe", true);
    myload("\\system32\\svchost.exe", true);
    myload("\\system32\\rundll32.exe", true);
    /* This is the actual troublesome .exe partially mapped in case 9717, it is unusual
     * in that explorer.exe is one of the only .exe's I've seen that have a reloc section
     * (no exports though, so we don't have to worry about that at least). It's the reloc
     * section walk that led to the crash in 9717. */
    myload("\\explorer.exe", true);
    print("done\n");
    return 0;
}
