/* **********************************************************
 * Copyright (c) 2018-2019 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

/* An app that announces when it's up, and stays up long enough for testing
 * attach. */
#include "tools.h"
#include <windows.h>

/* Timeout to avoid leaving stale processes in case something goes wrong. */
static VOID CALLBACK
TimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
    print("timed out\n");
    ExitProcess(1);
}

int
main(int argc, const char *argv[])
{
    int arg_offs = 1;
    bool for_detach = false;
    while (arg_offs < argc && argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-detach") == 0) {
            for_detach = true;
            arg_offs++;
        } else
            return 1;
    }

    /* We put the pid into the title so that tools/closewnd can target it
     * uniquely when run in a parallel test suite.
     * runall.cmake assumes this precise title.
     */
    char title[64];
    _snprintf_s(title, BUFFER_SIZE_BYTES(title), BUFFER_SIZE_ELEMENTS(title),
                "Infloop pid=%d", GetProcessId(GetCurrentProcess()));
    SetTimer(NULL, 0, 180 * 1000 /*3 mins*/, TimerProc);

    print("starting attachee\n");
    MessageBoxA(NULL, "DynamoRIO test: will be auto-closed", title, MB_OK);
    if (!for_detach) {
        print("MessageBox closed\n");
    }
    return 0;
}
