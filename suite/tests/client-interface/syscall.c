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
#include <stdio.h>

#ifdef WINDOWS
#    include <windows.h>
#    define EXPORT __declspec(dllexport)

char *
get_systemroot(char **env)
{
    const char sroot[] = "SYSTEMROOT=";
    int len = sizeof(sroot) - 1;
    int i;

    for (i = 0; env[i] != NULL; i++) {
        if (!strncmp(env[i], sroot, len)) {
            return env[i] + len;
        }
    }

    return "C:\\WINDOWS";
}
#else
#    define EXPORT __attribute__((visibility("default")))
#endif

EXPORT
void
start_monitor()
{
}

EXPORT
void
stop_monitor()
{
}

#ifdef WINDOWS
void
create_proc(char *cmd, char *cmdline, STARTUPINFO *sinfo)
{
    PROCESS_INFORMATION pinfo;
    if (!CreateProcess(cmd,     /* application name */
                       cmdline, /* command line */
                       NULL,    /* new proc cannot be inherited */
                       NULL,    /* new thread cannot be inherited */
                       TRUE,    /* inherit handles from this proc */
                       0,       /* no creation flags */
                       NULL,    /* use environment of this proc */
                       NULL,    /* same directory of this proc */
                       sinfo,   /* start up info */
                       &pinfo   /* out: process information */
                       )) {

        fprintf(stderr, "ERROR creating new process: %s %s\n", cmd, cmdline);
        exit(1);
    }
}
#endif

int
main(int argc, char **argv, char **env)
{
#ifdef WINDOWS
    STARTUPINFO sinfo;

    /* This test prints out all system calls.  Creating a new process
     * seems to be a good way to cause a bunch of them to execute.
     * Launch cmd.exe /c exit.
     */
    char cmd[MAX_PATH];
    char *cmdline = "/c exit";
    sprintf(cmd, "%s\\system32\\cmd.exe", get_systemroot(env));

    /* It seems that CreateProcess follows a slightly different
     * control-flow when executing the first time under DR vs.
     * native.  This phenomenon is probably due to DR calling certain
     * ntdll routines itself.  We'll just call CreateProcess twice and
     * trace the system calls for the 2nd invocation only.
     */
    GetStartupInfo(&sinfo);
    create_proc(cmd, cmdline, &sinfo);
#endif

    /* dummy marker to inform client lib to start monitoring */
    start_monitor();

#ifdef WINDOWS
    create_proc(cmd, cmdline, &sinfo);
#else
    /* for linux this is really just a module iterator and dr_get_proc_address
     * test; strace.* does syscall testing
     */
    fprintf(stderr, "syscall.c test\n");
#endif

    /* dummy marker to inform client lib to stop monitoring */
    stop_monitor();

    return 0;
}
