/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2003 VMware, Inc.  All rights reserved.
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

int
main(int argc, char **argv)
{
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    char cmdline[4096];

    if (argc >= 2) {
        int i;

        sprintf_s(cmdline, sizeof(cmdline), "\"%s\"", argv[1]);
        for (i = 2; i < argc; ++i) {
            /* This is n^2, but of course it doesn't matter. */
            strcat_s(cmdline, sizeof(cmdline), " ");
            strcat_s(cmdline, sizeof(cmdline), argv[i]);
        }

        fprintf(stderr, "creating subprocess %s\n", cmdline);
        fflush(stderr);

        if (!CreateProcess(argv[1], cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si,
                           &pi)) {
            fprintf(stderr, "CreateProcess failure\n");
            fflush(stderr);
        } else
            WaitForSingleObject(pi.hProcess, INFINITE);
    } else {
        fprintf(stderr, "Usage: %s <process to run> [args for child]\n", argv[0]);
        fflush(stderr);
    }

    fprintf(stderr, "parent done\n");
    fflush(stderr);
    return 0;
}
