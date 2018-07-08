/* **********************************************************
 * Copyright (c) 2004-2005 VMware, Inc.  All rights reserved.
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
#include <tchar.h>
#include <assert.h>

#define MB_SERVICE_NOTIFICATION 0x00200000L

/* calls detach for the passed in process id */
DWORD __cdecl main(DWORD argc, char *argv[], char *envp[])
{
    int i;
    for (i = 0; i < 100; i++) {
        MessageBoxW(NULL, L"\\??\\c:\foo.txt", L"none",
                    MB_OK | MB_TOPMOST | MB_SERVICE_NOTIFICATION);
        MessageBoxW(NULL, L"\\??\\r", L"\\??\\c:\foo.txt",
                    MB_OK | MB_TOPMOST | MB_SERVICE_NOTIFICATION);
        MessageBoxW(NULL, L"\\??\\r:", L"none",
                    MB_OK | MB_TOPMOST | MB_SERVICE_NOTIFICATION);
    }
    return 0;
}
