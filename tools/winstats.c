/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2001-2002 Massachusetts Institute of Technology
 *
 * This file is part of RIO, a research tool that is being supplied
 * "as is" without any accompanying services or improvements from MIT.
 * MIT makes no representations or warranties, express or implied.
 * Permission is hereby granted for non-commercial use of RIO provided
 * that you preserve this copyright notice, and you do not mention
 * the copyright holders in advertising related to RIO without
 * their permission.
 */
/*
 * winstats.c -- runs a target executable and gives memory stats at completion
 * build like so: cl /nologo winstats.c user32.lib imagehlp.lib
 */

/* FIXME: Unicode support?!?! */

/* xref PR 231843, we target 2003 to get a version of PROCESS_ALL_ACCESS that
 * works for all platforms. */
#define _WIN32_WINNT _WIN32_WINNT_WS03

#include <windows.h>
#include <commdlg.h>
#include <imagehlp.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <tchar.h>

#include "configure.h"
#include "globals_shared.h"

#include "ntdll.h"

/* global vars */
static int limit; /* in seconds */
static BOOL showmem;
static double wallclock; /* in seconds */

#define FP stderr

static void
print_mem_stats(HANDLE h)
{
    long secs = (long)wallclock;
    VM_COUNTERS mem;
    int cpu;
    if (!get_process_load_ex(h, &cpu, NULL))
        cpu = -1;
    get_process_mem_stats(h, &mem);
#if VERBOSE
    printf("Process Memory Statistics:\n");
    printf("\tPeak virtual size:         %6d KB\n", mem.PeakVirtualSize / 1024);
    printf("\tPeak working set size:     %6d KB\n", mem.PeakWorkingSetSize / 1024);
    printf("\tPeak paged pool usage:     %6d KB\n", mem.QuotaPeakPagedPoolUsage / 1024);
    printf("\tPeak non-paged pool usage: %6d KB\n",
           mem.QuotaPeakNonPagedPoolUsage / 1024);
    printf("\tPeak pagefile usage:       %6d KB\n", mem.PeakPagefileUsage / 1024);
#endif
    /* Elapsed real (wall clock) time.  */
    if (secs >= 3600) { /* One hour -> h:m:s.  */
        fprintf(FP, "%ld:%02ld:%02ldelapsed ", secs / 3600, (secs % 3600) / 60,
                secs % 60);
    } else {
        fprintf(FP, "%ld:%02ld.%02ldelapsed ", /* -> m:s.  */
                secs / 60, secs % 60, 0 /* for now */);
    }
    fprintf(FP, "%d%%CPU \n", cpu);
    fprintf(FP, "(%zu tot, %zu RSS, %zu paged, %zu non, %zu swap)k\n",
            mem.PeakVirtualSize / 1024, mem.PeakWorkingSetSize / 1024,
            mem.QuotaPeakPagedPoolUsage / 1024, mem.QuotaPeakNonPagedPoolUsage / 1024,
            mem.PeakPagefileUsage / 1024);
}

#define DEBUGPRINT 0

/* FIXME: would like ^C to kill child process, it doesn't.
 * also, child process seems able to read stdin but not to write
 * to stdout or stderr (in fact it dies if it tries)
 */
#define HANDLE_CONTROL_C 0

static void
debugbox(char *msg)
{
    MessageBox(NULL, TEXT(msg), TEXT("Inject Progress"), MB_OK | MB_TOPMOST);
}

#if HANDLE_CONTROL_C
BOOL WINAPI
HandlerRoutine(DWORD dwCtrlType //  control signal type
)
{
    printf("Inside HandlerRoutine!\n");
    fflush(stdout);
    /*    GenerateConsoleCtrlEvent(dwCtrlType, phandle);*/
    return TRUE;
}
#endif

int
usage(char *us)
{
    printf("Usage: %s [-s limit_sec | -m limit_min | -h limit_hr]\n"
           "      [-silent] <program> <args...>\n",
           us);
    return 0;
}

#define MAX_CMDLINE 2048

/* we must be a console application in order to have the process
 * we launch NOT get a brand new console window!
 */
int __cdecl main(int argc, char *argv[], char *envp[])
{
    LPTSTR app_name = NULL;
    TCHAR app_cmdline[MAX_CMDLINE];
    LPTSTR ch;
    int i;
    LPTSTR pszCmdLine = GetCommandLine();
    PLOADED_IMAGE li;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    HANDLE phandle;
    BOOL success;
    DWORD wait_result;
    int arg_offs = 1;
    time_t start_time, end_time;

    STARTUPINFO mysi;
    GetStartupInfo(&mysi);

    /* parse args */
    limit = 0;
    showmem = TRUE;
    if (argc < 2) {
        return usage(argv[0]);
    }
    while (argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-s") == 0) {
            if (argc <= arg_offs + 1)
                return usage(argv[0]);
            limit = atoi(argv[arg_offs + 1]);
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-m") == 0) {
            if (argc <= arg_offs + 1)
                return usage(argv[0]);
            limit = atoi(argv[arg_offs + 1]) * 60;
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-h") == 0) {
            if (argc <= arg_offs + 1)
                return usage(argv[0]);
            limit = atoi(argv[arg_offs + 1]) * 3600;
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-v") == 0) {
            /* just ignore -v, only here for compatibility with texec */
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-silent") == 0) {
            showmem = FALSE;
            arg_offs += 1;
        } else {
            return usage(argv[0]);
        }
        if (limit < 0) {
            return usage(argv[0]);
        }
        if (argc - arg_offs < 1) {
            return usage(argv[0]);
        }
    }

    /* we don't want quotes included in our
     * application path string (app_name), but we do want to put
     * quotes around every member of the command line
     */
    app_name = argv[arg_offs];
    if (app_name[0] == '\"' || app_name[0] == '\'' || app_name[0] == '`') {
        /* remove quotes */
        app_name++;
        app_name[strlen(app_name) - 1] = '\0';
    }
    /* note that we want target app name as part of cmd line
     * (FYI: if we were using WinMain, the pzsCmdLine passed in
     *  does not have our own app name in it)
     * since cygwin shell ends up putting extra quote characters, etc.
     * in pszCmdLine ('"foo"' => "\"foo\""), we can't easily walk past
     * the 1st 2 args (our path and the dll path), so we reconstruct
     * the cmd line from scratch:
     */
    ch = app_cmdline;
    ch += _snprintf(app_cmdline, MAX_CMDLINE, "\"%s\"", app_name);
    for (i = arg_offs + 1; i < argc; i++)
        ch += _snprintf(ch, MAX_CMDLINE - (ch - app_cmdline), " \"%s\"", argv[i]);
    assert(ch - app_cmdline < MAX_CMDLINE);
#if DEBUGPRINT
    printf("Running \"%s\"\n", app_cmdline);
    fflush(stdout);
#endif

#if HANDLE_CONTROL_C
    if (!SetConsoleCtrlHandler(HandlerRoutine, TRUE)) {
        debugbox("SetConsoleCtrlHandler failed");
        return 1;
    }
    {
        int flags;
        /* FIXME: hStdInput apparently is not the right handle...how do I get
         * the right handle?!?
         */
        if (!GetConsoleMode(mysi.hStdInput, &flags)) {
            debugbox("GetConsoleMode failed");
            return 1;
        }
        printf("console mode flags are: 0x%08x\n", flags);
    }
#endif

    if (li = ImageLoad(app_name, NULL)) {
        ImageUnload(li);
    } else {
        _snprintf(app_cmdline, MAX_CMDLINE, "Failed to load executable image \"%s\"",
                  app_name);
        debugbox(app_cmdline);
        return 1;
    }

    /* Launch the application process. */
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = mysi.hStdInput;
    si.hStdOutput = mysi.hStdOutput;
    si.hStdError = mysi.hStdError;

    /* Must specify TRUE for bInheritHandles so child inherits stdin! */
    if (!CreateProcess(app_name, app_cmdline, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL,
                       NULL, &si, &pi)) {
        debugbox("Failed to launch application");
        return 1;
    }

    if ((phandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId)) == NULL) {
        debugbox("Cannot open application process");
        TerminateProcess(pi.hProcess, 0);
        return 1;
    }

#if DEBUGPRINT
    printf("Successful at starting process\n");
    fflush(stdout);
#endif

    success = FALSE;
    __try {
        start_time = time(NULL);

        /* resume the suspended app process so its main thread can run */
        ResumeThread(pi.hThread);

        /* detach from the app process */
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        /* now wait for app process to finish */
        wait_result =
            WaitForSingleObject(phandle, (limit == 0) ? INFINITE : (limit * 1000));
        end_time = time(NULL);
        wallclock = difftime(end_time, start_time);
        if (wait_result == WAIT_OBJECT_0)
            success = TRUE;
        else
            printf("Timeout after %d seconds\n", limit);
    } __finally {
        /* FIXME: this is my attempt to get ^C, but it doesn't work... */
#if HANDLE_CONTROL_C
        if (!success) {
            printf("Terminating child process!\n");
            fflush(stdout);
            TerminateProcess(phandle, 0);
        } else
            printf("Injector exiting peacefully\n");
#endif
        if (showmem)
            print_mem_stats(phandle);
        if (!success) {
            /* kill the child */
            TerminateProcess(phandle, 0);
        }
        CloseHandle(phandle);
#if DEBUGPRINT
        fflush(stdout);
#endif
    }

    return 0;
}
