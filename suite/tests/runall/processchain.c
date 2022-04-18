/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

/* FIXME: nothing here tests that a child is under DR */
#include "tools.h"
#include "Windows.h"

/* undefine this for a performance test */
#ifndef NIGHTLY_REGRESSION
#    define NIGHTLY_REGRESSION
#endif

#ifdef NIGHTLY_REGRESSION
#    define DEPTH 5
#else
#    define DEPTH 10
#endif

static int depth = DEPTH;

static int
main(int argc, char **argv)
{
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    char cmdline[128];
    DWORD result = 0;

    INIT();
    USE_USER32(); /* can't be in runall otherwise! */

    if (argc == 1) {
        /* normal execution */

        _snprintf(cmdline, sizeof(cmdline), "%s %d", argv[0], depth);

        print("starting chain %d...\n", depth);
    }

    if (argc == 2) {
        depth = atoi(argv[1]);
        print("subprocess %d running.\n", depth);

        _snprintf(cmdline, sizeof(cmdline), "%s %d", argv[0], depth - 1);
    }

    if (depth != 0) {
        if (!CreateProcess(argv[0], cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            print("CreateProcess failure\n");
        else {
            DWORD exit_code = 0;
            print("waiting for child\n");
            WaitForSingleObject(pi.hProcess, INFINITE);
            if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
                print("GetExitCodeProcess failure %d\n", GetLastError());
            } else {
                print("process returned %d\n", exit_code);
            }
        }
    }

    return depth * 10;
    /* make sure not 259 STILL_ACTIVE which may be returned by GetExitCodeProcess() */
}
