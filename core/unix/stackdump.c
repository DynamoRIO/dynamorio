/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2009 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */

/*
 * stackdump.c - support for dumping stack trace of DynamoRIO
 *
 * based on code from David Spuler and idea from Castor Fu
 * basic idea:
 *   fork off a process which core dumps then run a debugger on it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>

#include "../globals.h"
#include "os_private.h"
#ifdef LINUX
#    include "include/syscall.h"
#else
#    include <sys/syscall.h>
#endif

#define DEBUGGER "gdb"
/* add -q to suppress gdb copyright notice */
#define QUIET_MODE "-q"
#define TEMPORARY_FILENAME "/tmp/dynamorio.stackdump"
#define CORE_NAME "core"

#define VERBOSE 0

/* use gdb -x <file> -batch, or redirect stdin?
 * redirecting stdin has issues when where is multi-page, but
 * batch will wait for stdin on multi-page, so we solve that
 * by adding extra <enter>s in our temp file
 */
#define BATCH_MODE 0
#if BATCH_MODE
#    define DEBUGGER_COMMAND "where\nquit\n"
#else
/* FIXME: want to have some <enter>s to handle multi-page,
 * but don't want to repeat where cmd, so I use pwd, which is
 * useful.  Hopefully two pages is enough.
 */
#    define DEBUGGER_COMMAND "where\npwd\nquit\n"
#endif

#ifndef WAIT_ANY
/* WAIT_ANY is not defined in Android system */
#    define WAIT_ANY (-1) /* Any process.  */
#endif

pid_t
wait_syscall(int *status)
{
    return dynamorio_syscall(SYSNUM_NO_CANCEL(SYS_wait4), 4, WAIT_ANY, status, 0, NULL);
}

static int
execve_syscall(const char *exe, const char **argv, char **envp)
{
    return dynamorio_syscall(SYS_execve, 3, exe, argv, envp);
}

int
fork_syscall(void)
{
#if FORK_BROKEN_CASE_4967
    /* FIXME: SYS_fork on dereksha is creating a child whose pid is
     * same as parent but has a different tid, and the abort() to dump
     * core kills the parent process -- looks just like a separate
     * thread, not a separate process!
     *
     * When I use glibc fork() I get the proper behavior.
     * glibc 2.3.3 fork() calls clone() with flags = 0x01200011
     * == CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD, which matches
     * glibc-2.3.3-200405070341/nptl/sysdeps/unix/sysv/linux/i386/fork.c's
     * ARCH_FORK, which is what glibc uses, so why doesn't SYS_fork do
     * same thing?  Instead it simply sets SIGCHLD and not the CLONE_* flags.
     * (see /usr/src/linux/arch/i386/kernel/process.c).  But, trying the CLONE_* flags
     * doesn't do the trick -- libc fork() is doing something extra, and
     * glibc-2.3.3-200405070341/nptl/sysdeps/unix/sysv/linux/fork.c's version of it
     * shows it's doing some funny tricks with the pid!
     *
     * Once figure it out, need to have dynamic check for threading version to know
     * what to do.
     */
#    if 0
    /* from /usr/include/bits/sched.h ifdef __USE_MISC: */
#        define CLONE_CHILD_CLEARTID                     \
            0x00200000 /* Register exit futex and memory \
                        * location to clear.  */
#        define CLONE_CHILD_SETTID                         \
            0x01000000 /* Store TID in userlevel buffer in \
                        * the child.  */
    /* i386/fork.c pass a 5th arg, &THREAD_SELF->tid, is it needed for SETTID? */
    static uint tid;
    return dynamorio_syscall(SYS_clone, 4/* 5 */,
                             /* flags, newsp (if 0 -> cur esp), parent_tidptr,
                              * child_tidptr, something! */
                             CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD,
                             0, NULL, NULL /*, &tid*/);
#    else
    /* my workaround for now: just use libc -- binaries won't be back-compatible though */
    return fork();
#    endif
#else
#    ifdef SYS_fork
    return dynamorio_syscall(SYS_fork, 0);
#    else
    return dynamorio_syscall(SYS_clone, 5, SIGCHLD, NULL, NULL, NULL, NULL);
#    endif
#endif
}

/*-----------------------------------------------------------------------*/
/* Procedure:
   1. Fork off a child process that dumps core; creates the "core" file
   2. Fork off a 2nd child process which:
   2a. Creates a temporary file of input commands for the debugger
   2b. Redirects stdin from this temporary file
   2c. Executes the debugger using the redirected input commands
*/
/*-----------------------------------------------------------------------*/

void
d_r_stackdump(void)
{
    int pid, core_pid;
    /* get name now -- will be same for children */
    char *exec_name = get_application_name();
    char core_name[128];
    char tmp_name[128];
    snprintf(tmp_name, 128, "%s.%d", TEMPORARY_FILENAME, get_process_id());

#ifdef VMX86_SERVER
    if (os_in_vmkernel_userworld()) {
        return; /* no local gdb, no multithreaded fork */
    }
#endif

#if VERBOSE
    SYSLOG_INTERNAL_ERROR("about to fork parent %d to dump core", get_process_id());
#endif

    /* Fork a child to dump core */
    pid = fork_syscall();
    if (pid == 0) { /* child */
#if VERBOSE
        SYSLOG_INTERNAL_ERROR("about to dump core in process %d parent %d thread " TIDFMT
                              "",
                              get_process_id(), get_parent_id(), d_r_get_thread_id());
#endif
        /* We used to use abort here, but that had lots of complications with
         * pthreads and libc, so now we just dereference NULL.
         */
        if (!set_default_signal_action(SIGSEGV)) {
            SYSLOG_INTERNAL_ERROR("ERROR in setting handler");
            exit_process_syscall(1);
        }
        *(volatile int *)NULL = 0;
#if VERBOSE
        SYSLOG_INTERNAL_ERROR("about to exit process %d", get_process_id());
#endif
        exit_process_syscall(0);
    } else if (pid == -1) {
        SYSLOG_INTERNAL_ERROR("ERROR: could not fork to dump core");
        exit_process_syscall(1);
    }
#if VERBOSE
    SYSLOG_INTERNAL_ERROR("parent %d %d waiting for child %d", get_process_id(),
                          d_r_get_thread_id(), pid);
#endif
    /* Parent continues */
    core_pid = pid;
    while (wait_syscall(NULL) != pid) {
        /* wait for core to be dumped */
    }

#if VERBOSE
    SYSLOG_INTERNAL_ERROR("about to fork 2nd child to run gdb");
#endif
    /* Fork a 2nd child to run gdb */
    pid = fork_syscall();
    if (pid == 0) { /* child */
        file_t fp;
        int fd;
        const char *argv[16];
        int i;

        /* Open a temporary file for the input: the "where" command */
        fp = os_open(tmp_name, OS_OPEN_REQUIRE_NEW | OS_OPEN_WRITE);
        os_write(fp, DEBUGGER_COMMAND, strlen(DEBUGGER_COMMAND));
        os_close(fp);

        fd = open_syscall(tmp_name, O_RDONLY, 0);
        if (fd < 0) {
            SYSLOG_INTERNAL_ERROR("ERROR: open failed on temporary file");
            exit_process_syscall(1);
        }
#if !BATCH_MODE
        /* Redirect stdin from the temporary file */
        close_syscall(0); /* close stdin */
        dup_syscall(fd);  /* replace file descriptor 0 with reference to temp file */
#endif
        close_syscall(fd); /* close the other reference to temporary file */

        /* Find the core file */
        strncpy(core_name, CORE_NAME, 127);
        core_name[127] = '\0'; /* if max no null */
        fd = open_syscall(core_name, O_RDONLY, 0);
        if (fd < 0) {
            snprintf(core_name, 128, "%s.%d", CORE_NAME, core_pid);
            SYSLOG_INTERNAL_ERROR("core not found, trying %s", core_name);
            fd = open_syscall(core_name, O_RDONLY, 0);
            if (fd < 0) {
                SYSLOG_INTERNAL_ERROR("ERROR: no core file found!");
                exit_process_syscall(1);
            }
        }
        close_syscall(fd);

        /* avoid running the debugger under us!
         * FIXME: just remove our libraries, instead of entire env var?
         */
        unsetenv("LD_PRELOAD");

        SYSLOG_INTERNAL_ERROR("-------------------------------------------");
        SYSLOG_INTERNAL_ERROR("stackdump: --- now running the debugger ---");
        SYSLOG_INTERNAL_ERROR("%s %s %s %s", DEBUGGER, QUIET_MODE, exec_name, core_name);
        SYSLOG_INTERNAL_ERROR("-------------------------------------------");
        i = 0;
        /* We rely on /usr/bin/env to do the PATH search for gdb on our behalf.
         */
        argv[i++] = "/usr/bin/env";
        argv[i++] = DEBUGGER;
        argv[i++] = QUIET_MODE;
#if BATCH_MODE
        argv[i++] = "-x";
        argv[i++] = tmp_name;
        argv[i++] = "-batch";
#endif
        argv[i++] = exec_name;
        argv[i++] = core_name;
        argv[i++] = NULL;
        DEBUG_DECLARE(int execve_errno =)
        execve_syscall("/usr/bin/env", argv, our_environ);
        SYSLOG_INTERNAL_ERROR("ERROR: execve failed for debugger: %d", -execve_errno);
        exit_process_syscall(1);
    } else if (pid == -1) {
        SYSLOG_INTERNAL_ERROR("ERROR: could not fork to run debugger");
        exit_process_syscall(1);
    }
    /* Parent continues */
    /* while(wait(NULL)>0) waits for all children, and could hang, so: */
    while (wait_syscall(NULL) != pid) {
        /* empty loop */
    }

    /* Wait for these children to complete before returning */

    while (wait_syscall(NULL) > 0) {
        /* empty loop */
    }

    os_delete_file(tmp_name); /* clean up the temporary file */
    SYSLOG_INTERNAL_ERROR("-------------------------------------------");
}
