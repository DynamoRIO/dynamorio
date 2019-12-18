/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2007 VMware, Inc.  All rights reserved.
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
/* case 9385 test */
/* based on runall/processchain.c */
/* should compare with win32/threadinjection.c */

/* TODO: need to test this both with -early_inject and without so that
 * we can exercise the way we are running winlogon.exe and maybe other
 * interrupted chain services
 */

/* TWEAKS:
 * depth (controlled by STRESS=1 or PERF=1)
 * roundrobin
 * extra_threads
 * sleep_under_ldrlock in earlythread.dll.c
 */

#include "tools.h"
#include "Windows.h"

#define VERBOSE 0
#define VERY_VERBOSE 0

/* undefine this for a performance test */

#ifdef NIGHTLY_REGRESSION
#    define DEPTH 5
#else
#    ifdef PERF
#        define DEPTH 10 /* PERF */
#    else
#        define DEPTH 100 /* STRESS */
#    endif
#endif

static int depth = DEPTH;

/* whether we'll forcefully change which thread goes first */
/* note that as seen with case 9467 Win2003 does allow at least
 * CtrlRoutine threads to start earlier than the primary thread, so it
 * may be a possible natural order as well.
 */
int roundrobin = 1;

int extra_threads = 5;

static int
main(int argc, char **argv)
{
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    HANDLE hThread_child;
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
#if VERY_VERBOSE
        /* this is non-deterministic with respect to second thread execution */
        print("subprocess %d running.\n", depth);
#endif

        _snprintf(cmdline, sizeof(cmdline), "%s %d", argv[0], depth - 1);
        /* this thread could do some work or just sleep a little */
        Sleep(10);
#if VERBOSE
        print("done some sleeping\n");
#endif
    }

    if (depth != 0) {

        /* FIXME: may want to add CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED
         * so we can send GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, child_group)
         * and see if that ever gets delivered before the first process thread in any
         * grandchild
         */

        if (!CreateProcess(argv[0], cmdline, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL,
                           NULL, &si, &pi))
            print("CreateProcess failure\n");
        else {
            DWORD exit_code = 0;

            DWORD proc_affinity;
            DWORD sys_affinity;

            static const char *mylib = "earlythread.dll.dll";
            /* note we're too lazy to allocate this string in the
             * child, but if exe is not ASLRed in child we should be fine
             */

            /* creating a thread to LoadLibrary */

            hThread_child = CreateRemoteThread(pi.hProcess, 0, 0, &LoadLibrary,
                                               (void *)mylib, CREATE_SUSPENDED, NULL);

            if (hThread_child == NULL) {
                print("Error in CreateRemoteThread(Code %d)\n", GetLastError());
            }

#if PROCAFFINITY /* case 4181 - can't be set to 0 */
            if (!GetProcessAffinityMask(pi.hProcess, &proc_affinity, &sys_affinity)) {
                print("GetProcessAffinityMask() failure %d\n", GetLastError());
            }
#    if VERBOSE
            print("GetProcessAffinityMask = %x %x\n", proc_affinity, sys_affinity);
#    endif

            if (!SetProcessAffinityMask(pi.hProcess, 0)) {
                print("SetProcessAffinityMask(0) failure %d\n", GetLastError());
            }

            /* restore original */
            if (!SetProcessAffinityMask(pi.hProcess, proc_affinity)) {
                print("SetProcessAffinityMask(original) failure %d\n", GetLastError());
                /*
                 * 1:001> !error c000000d
                 * Error code: (NTSTATUS) 0xc000000d (3221225485) - An invalid parameter
                 * was passed to a service or function.
                 */
            }
#endif /* PROCAFFINITY */

            /* FIXME: should play around with the order of these */
            /* resume initial thread */
            if (!roundrobin || (depth % 2) == 0) { /* even rounds primary goes first */
                result = ResumeThread(pi.hThread);
                if (result == -1) {
                    print("ResumeThread primary thread failure %d\n", GetLastError());
                }

                /* FIXME: should play around with extra sleep if too
                 * deterministic otherwise */
                Sleep(depth * 10);
            } else {
                /* we'll wake up after injected thread */
            }

            result = ResumeThread(hThread_child);
            if (result == -1) {
                print("ResumeThread second thread failure %d\n", GetLastError());
            }

            if (roundrobin &&
                ((depth % 2) == 1)) { /* odd rounds injected thread goes first */
                /* FIXME: may want to be able to disable this */
                Sleep(depth * 10);

                result = ResumeThread(pi.hThread);
                if (result == -1) {
                    print("ResumeThread primary thread failure %d\n", GetLastError());
                }
            }

            while (extra_threads-- < 0) {
                /* inject a few more threads for kicks  */
                HANDLE extra_child =
                    CreateRemoteThread(pi.hProcess, 0, 0, &LoadLibrary, (void *)mylib,
                                       0 /* ready to run */, NULL);
                /* do we really have to wait on them? no.. */
#if WAITEXTRA
                WaitForSingleObject(extra_child, INFINITE);
#endif
            }

            if (hThread_child != NULL) {
                WaitForSingleObject(hThread_child, INFINITE);
                CloseHandle(hThread_child);
#if VERBOSE
                print("done with second thread\n");
#endif
            }

#if VERBOSE
            print("%d: waiting for child process\n", depth);
#endif
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
