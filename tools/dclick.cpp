/* **********************************************************
 * Copyright (c) 2004 VMware, Inc.  All rights reserved.
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

/* dclick.cpp
 * by Derek Bruening
 * May 2002
 *
 * equivalent of DOS shell "start" command: launches a file using the
 * shell as though it were double-clicked in explorer
 *
 * to build, must link in shell32.lib:
 * cl /nologo dclick.cpp shell32.lib
 */

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <assert.h>

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename> <args...>\n", argv[0]);
        return 1;
    }

    TCHAR cwd[MAX_PATH];
    int cwd_res = GetCurrentDirectory(MAX_PATH, cwd);
    assert(cwd_res > 0);

    TCHAR *params = (TCHAR *)malloc(sizeof(TCHAR) * (argc - 1) * MAX_PATH);
    TCHAR *cur = params;
    int i;
    int len, left = (argc - 1) * MAX_PATH;
    for (i = 2; i < argc; i++) {
        len = _sntprintf(cur, left, "%s ", argv[i]);
        if (len < 0) {
            /* hit max */
            cur += left - 1;
            fprintf(stderr, "Warning: hit parameter buffer limit\n");
            break;
        }
        left -= len;
        cur += len;
    }
    *cur = _T('\0');

    fprintf(stderr, "Opening \"%s\" with parameters \"%s\"\n", argv[1], params);
    // tell explorer to "open" the file
    HINSTANCE res =
        ShellExecute(NULL, _T("open"), argv[1], (LPCTSTR)params, cwd, SW_SHOWNORMAL);

    free(params);
    if ((int)res <= 32) {
        int errnum = GetLastError();
        LPVOID lpMsgBuf;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, errnum,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                      (LPTSTR)&lpMsgBuf, 0, NULL);
        // Display the string.
        fprintf(stderr, "Error opening \"%s\":\n\t%s\n", argv[1], lpMsgBuf);
        // Free the buffer.
        LocalFree(lpMsgBuf);
        return 1;
    }
    return 0;
}
