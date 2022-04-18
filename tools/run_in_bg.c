/* **********************************************************
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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

/*
 * Runs an app in the background (since cmake scripts can't do so).
 * Takes in the app path and arguments (as separate parameters)
 * and launches the app as a separate process.  Prints the process id
 * of that process to a file, if requested.  On Linux prints the
 * process id to stdout if the -pid option is not provided.
 *
 * Note that we can't easily print the pid to stoud on Windows: we
 * can't dup stdout prior to spawning b/c then cmake will wait for the
 * child to close that descriptor, which will never happen.  If we use
 * CreateFile("CONOUT$") it opens the raw console and doesn't work
 * within cygwin shells.  We could _spawnv another tool and have it
 * _execv, but simpler to use pid file.
 */

#include "configure.h"
#ifdef WINDOWS
#    define _CRT_SECURE_NO_WARNINGS 1
#    define _CRT_SECURE_NO_DEPRECATE 1
#    include <windows.h>
#    include <process.h>
#    include <io.h>
#else
#    include <unistd.h>
#    include <sys/types.h>
#    include <string.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef WINDOWS
/* use ISO C++-conformant names */
#    define dup _dup
#    define fileno _fileno
#    define open _open
#    define close _close
#endif

/* S_IREAD and others are not defined in Android */
#ifndef S_IREAD
#    define S_IREAD S_IRUSR
#    define S_IWRITE S_IWUSR
#    define S_IEXEC S_IXUSR
#endif

int
usage(const char *us)
{
    fprintf(stderr,
            "Usage: %s [-env <var> <value>] [-out <file>] [-pid <file>] <program> "
            "<args...>\n",
            us);
    return 1;
}

static const char *
mygetenv(const char *var)
{
#ifdef WINDOWS
    static char buf[2048];
    int len = GetEnvironmentVariable(var, buf, sizeof(buf) / sizeof(buf[0]));
    if (len == 0 || len > sizeof(buf) / sizeof(buf[0]))
        return NULL;
    else
        return (const char *)buf;
#else
    return getenv(var);
#endif
}

#ifdef WINDOWS
/* GetProcessId() is XP+ only, and our DDK w2k libpath puts
 * the w2k kernel32.lib ahead of compiler's kernel32.lib.
 * For running tests on win2k we'll want win2k support anyway, so
 * we directly query ntdll.
 */
typedef LONG NTSTATUS;
typedef LONG KPRIORITY;

#    define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

typedef struct _PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PVOID PebBaseAddress;
    ULONG_PTR AffinityMask;
    KPRIORITY BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;

NTSYSAPI NTSTATUS NTAPI
NtQueryInformationProcess(IN HANDLE ProcessHandle, IN ULONG ProcessInformationClass,
                          OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
                          OUT PULONG ReturnLength OPTIONAL);

static int
process_id_from_handle(HANDLE h)
{
    PROCESS_BASIC_INFORMATION info;
    NTSTATUS res;
    ULONG got;
    memset(&info, 0, sizeof(info));
    res = NtQueryInformationProcess(h, 0 /*ProcessBasicInformation*/, &info,
                                    sizeof(PROCESS_BASIC_INFORMATION), &got);
    if (!NT_SUCCESS(res) || got != sizeof(PROCESS_BASIC_INFORMATION))
        return -1;
    else /* though pointer-sized field, we truncate to int */
        return (int)info.UniqueProcessId;
}
#endif

static void
redirect_stdouterr(const char *outfile)
{
    int newout = -1;
    int stdout_fd = fileno(stdout);
    int stderr_fd = fileno(stderr);
    if (outfile != NULL) {
        newout = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, S_IREAD | S_IWRITE);
        if (newout < 0) {
            perror("open new stdout failed");
            exit(1);
        }
    }
    /* we must close these in order for cmake's execute_process()
     * to stop waiting on the parent (setsid() or grandchild make
     * no difference)
     */
    /* somehow closing both and then dup'ing twice fails under
     * some circumstances so we do one at a time
     */
    if (close(stdout_fd) != 0 || (outfile != NULL && dup(newout) != stdout_fd)) {
        fprintf(stderr, "stdout redirect FAILED");
        exit(1);
    }
    if (close(stderr_fd) != 0 || (outfile != NULL && dup(newout) != stderr_fd)) {
        fprintf(stderr, "stderr redirect FAILED");
        exit(1);
    }
}

int
main(int argc, char *argv[])
{
#ifdef WINDOWS
    HANDLE hchild;
    int child;
#else
    pid_t child;
#endif
    int rc;
    int arg_offs = 1;
    const char *outfile = NULL;
    const char *pidfile = NULL;

    if (argc < 2) {
        return usage(argv[0]);
    }
    while (argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-env") == 0) {
            if (argc <= arg_offs + 2)
                return usage(argv[0]);
#if VERBOSE
            fprintf(stderr, "setting env var \"%s\" to \"%s\"\n", argv[arg_offs + 1],
                    argv[arg_offs + 2]);
#endif
#ifdef WINDOWS
            rc = SetEnvironmentVariable(argv[arg_offs + 1], argv[arg_offs + 2]);
#else
            rc = setenv(argv[arg_offs + 1], argv[arg_offs + 2], 1 /*overwrite*/);
#endif
            if (rc != 0 ||
                strcmp(mygetenv(argv[arg_offs + 1]), argv[arg_offs + 2]) != 0) {
                fprintf(stderr, "error in setenv of \"%s\" to \"%s\"\n",
                        argv[arg_offs + 1], argv[arg_offs + 2]);
                fprintf(stderr, "setenv returned %d\n", rc);
                fprintf(stderr, "env var \"%s\" is now \"%s\"\n", argv[arg_offs + 1],
                        mygetenv(argv[arg_offs + 1]));
                exit(1);
            }
            arg_offs += 3;
        } else if (strcmp(argv[arg_offs], "-out") == 0) {
            if (argc <= arg_offs + 1)
                return usage(argv[0]);
            outfile = argv[arg_offs + 1];
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-pid") == 0) {
            if (argc <= arg_offs + 1)
                return usage(argv[0]);
            pidfile = argv[arg_offs + 1];
            arg_offs += 2;
        } else {
            return usage(argv[0]);
        }
        if (argc - arg_offs < 1)
            return usage(argv[0]);
    }

#ifdef UNIX
    child = fork();
    if (child < 0) {
        perror("ERROR on fork");
    } else if (child == 0) {
        /* redirect std{out,err} */
        redirect_stdouterr(outfile);
        execv(argv[arg_offs], argv + arg_offs /*include app*/);
        fprintf(stderr, "execv of %s FAILED", argv[arg_offs]);
        exit(1);
    }
    /* else, parent, and we continue below */
    if (pidfile == NULL)
        printf("%d\n", child);
#else
    /* redirect std{out,err} */
    redirect_stdouterr(outfile);
    /* Do we want _P_DETACH instead of _P_NOWAIT?  _P_DETACH doesn't return the
     * child handle though.
     */
    hchild = (HANDLE)_spawnv(_P_NOWAIT, argv[arg_offs], argv + arg_offs /*include app*/);
    if (hchild == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "_spawnv of %s FAILED", argv[arg_offs]);
        exit(1);
    }
    child = process_id_from_handle(hchild);
#endif
    if (pidfile != NULL) {
        FILE *f = fopen(pidfile, "w");
        if (f == NULL) {
            perror("open pidfile failed");
            exit(1);
        }
        fprintf(f, "%d\n", child);
        fclose(f);
    }
    return 0;
}
