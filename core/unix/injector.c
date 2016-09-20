/* **********************************************************
 * Copyright (c) 2012-2016 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Simple reimplementation of the dr_inject API for Linux.
 *
 * To match the Windows API, we fork a child and suspend it before the call to
 * exec.
 */

#include "configure.h"
#include "globals_shared.h"
#include "../config.h"  /* for get_config_val_other_app */
#include "../globals.h"
#ifdef LINUX
# include "include/syscall.h"  /* for SYS_ptrace */
#else
# include <sys/syscall.h>
#endif
#include "instrument.h"
#include "instr.h"
#include "instr_create.h"
#include "decode.h"
#include "disassemble.h"
#include "os_private.h"
#include "module.h"
#include "module_private.h"
#include "dr_inject.h"

#include <assert.h>
#ifndef MACOS
/* If we don't define _EXTERNALIZE_CTYPE_INLINES_*, we get errors vs tolower
 * in globals.h; if we do, we get errors on isspace missing.  We solve that
 * by just by supplying our own isspace.
 */
# include <ctype.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#if defined(LINUX) && defined(AARCH64)
# include <linux/ptrace.h> /* for struct user_pt_regs */
#endif
#include <sys/uio.h> /* for struct iovec */
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef MACOS
# include <spawn.h>
# include <crt_externs.h> /* _NSGetEnviron() */
#endif

/* i#1925: we need to support executing a shell script so we distinguish a
 * non-image from a not-found or unreadable file.
 */
typedef enum {
    PLATFORM_SUCCESS,
    PLATFORM_ERROR_CANNOT_OPEN,
    PLATFORM_UNKNOWN,
} platform_status_t;

#ifdef MACOS
/* The type is just "int", and the values are different, so we use the Linux
 * type name to match the Linux constant names.
 */
# ifndef PT_ATTACHEXC /* New replacement for PT_ATTACH */
#  define PT_ATTACHEXC PT_ATTACH
# endif
enum __ptrace_request {
    PTRACE_TRACEME     = PT_TRACE_ME,
    PTRACE_CONT        = PT_CONTINUE,
    PTRACE_KILL        = PT_KILL,
    PTRACE_ATTACH      = PT_ATTACHEXC,
    PTRACE_DETACH      = PT_DETACH,
    PTRACE_SINGLESTEP  = PT_STEP,
};

static int inline
isspace(int c)
{
    return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
            c == '\v');
}
#endif

static bool verbose = false;

/* Set from a signal handler.
 */
static volatile int timeout_expired;

typedef enum _inject_method_t {
    INJECT_EARLY,       /* Works with self or child. */
    INJECT_LD_PRELOAD,  /* Works with self or child. */
    INJECT_PTRACE       /* Doesn't work with exec_self. */
} inject_method_t;

/* Opaque type to users, holds our state */
typedef struct _dr_inject_info_t {
    process_id_t pid;
    const char *exe;            /* full path of executable */
    const char *image_name;     /* basename of exe */
    const char **argv;          /* array of arguments */
    int pipe_fd;

    bool exec_self;             /* this process will exec the app */
    inject_method_t method;

    bool killpg;
    bool exited;
    int exitcode;
    bool no_emulate_brk; /* is -no_emulate_brk in the option string? */

#ifdef MACOS
    bool spawn_32bit;
#endif
} dr_inject_info_t;

#if defined(LINUX) && !defined(ANDROID) /* XXX i#1290/i#1701: NYI on MacOS/Android */
static bool
inject_ptrace(dr_inject_info_t *info, const char *library_path);

static long
our_ptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data);
#endif

/*******************************************************************************
 * Core compatibility layer
 */

/* Never actually called, but needed to link in config.c. */
const char *
get_application_short_name(void)
{
    ASSERT(false);
    return "";
}

/* Shadow DR's internal_error so assertions work in standalone mode.  DR tries
 * to use safe_read to take a stack trace, but none of its signal handlers are
 * installed, so it will segfault before it prints our error.
 */
void
internal_error(const char *file, int line, const char *expr)
{
    fprintf(stderr, "ASSERT failed: %s:%d (%s)\n", file, line, expr);
    fflush(stderr);
    abort();
}

bool
ignore_assert(const char *assert_stmt, const char *expr)
{
    return false;
}

void
report_dynamorio_problem(dcontext_t *dcontext, uint dumpcore_flag,
                         app_pc exception_addr, app_pc report_ebp,
                         const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "DynamoRIO problem: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    fflush(stderr);
    abort();
}

/*******************************************************************************
 * Injection implementation
 */

/* Environment modifications before executing the child process for LD_PRELOAD
 * injection.
 */
static void
pre_execve_ld_preload(const char *dr_path)
{
    char ld_lib_path[MAX_OPTIONS_STRING];
    const char *last_slash = NULL;
    const char *mode_slash = NULL;
    const char *lib_slash = NULL;
    const char *cur_path = getenv("LD_LIBRARY_PATH");
    const char *cur = dr_path;
    /* Find last three occurrences of '/'. */
    while (*cur != '\0') {
        if (*cur == '/') {
            lib_slash = mode_slash;
            mode_slash = last_slash;
            last_slash = cur;
        }
        cur++;
    }
    /* dr_path should be absolute and have at least three components. */
    ASSERT(lib_slash != NULL && last_slash != NULL);
    ASSERT(strncmp(lib_slash, "/lib32", 5) == 0 ||
           strncmp(lib_slash, "/lib64", 5) == 0);
    /* Put both DR's path and the extension path on LD_LIBRARY_PATH.  We only
     * need the extension path if -no_private_loader is used.
     */
    snprintf(ld_lib_path, BUFFER_SIZE_ELEMENTS(ld_lib_path),
             "%.*s:%.*s/ext%.*s%s%s",
             last_slash - dr_path, dr_path,      /* DR path */
             lib_slash - dr_path, dr_path,       /* pre-ext path */
             last_slash - lib_slash, lib_slash,  /* libNN component */
             cur_path == NULL ? "" : ":",
             cur_path == NULL ? "" : cur_path);
    NULL_TERMINATE_BUFFER(ld_lib_path);
#ifdef MACOS
    setenv("DYLD_LIBRARY_PATH", ld_lib_path, true/*overwrite*/);
    /* XXX: why does it not work w/o the full path? */
    snprintf(ld_lib_path, BUFFER_SIZE_ELEMENTS(ld_lib_path),
             "%.*s/%s:%.*s/%s",
             last_slash - dr_path, dr_path, "libdrpreload.dylib",
             last_slash - dr_path, dr_path, "libdynamorio.dylib");
    setenv("DYLD_INSERT_LIBRARIES", ld_lib_path, true/*overwrite*/);
    /* This is required to use DYLD_INSERT_LIBRARIES on apps that use
     * two-level naming, but it can cause an app to run incorrectly.
     * Long-term we'll want a true early injector.
     */
    setenv("DYLD_FORCE_FLAT_NAMESPACE", "1", true/*overwrite*/);
#else
    setenv("LD_LIBRARY_PATH", ld_lib_path, true/*overwrite*/);
    setenv("LD_PRELOAD", "libdynamorio.so libdrpreload.so", true/*overwrite*/);
#endif
    if (verbose) {
        printf("Setting LD_USE_LOAD_BIAS for PIEs so the loader will honor "
               "DR's preferred base. (i#719)\n"
               "Set LD_USE_LOAD_BIAS=0 prior to injecting if this is a "
               "problem.\n");
    }
    setenv("LD_USE_LOAD_BIAS", "1", false/*!overwrite, let user set it*/);
}

/* Environment modifications before executing the child process for early
 * injection.
 */
static void
pre_execve_early(dr_inject_info_t *info, const char *exe)
{
    setenv(DYNAMORIO_VAR_EXE_PATH, exe, true/*overwrite*/);
    if (info->no_emulate_brk)
        setenv(DYNAMORIO_VAR_NO_EMULATE_BRK, exe, true/*overwrite*/);
}

static void
execute_exec(dr_inject_info_t *info, const char *toexec)
{
#ifdef MACOS
    if (info->spawn_32bit) {
        /* i#1643: a regular execve will always match the kernel bitwidth */
        /* XXX: use raw data structures and SYS_posix_spawn */
        posix_spawnattr_t attr;
        if (posix_spawnattr_init(&attr) == 0) {
            cpu_type_t cpu = CPU_TYPE_X86;
            size_t sz;
            if (posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC) == 0 &&
                posix_spawnattr_setbinpref_np(&attr, sizeof(cpu), &cpu, &sz) == 0) {
                posix_spawn(NULL, toexec, NULL, &attr, (char *const *) info->argv,
                            /* On Mac, shared libs don't have access to environ
                             * directly and must use this routine (if we declare
                             * environ as extern we end up looking in the wrong
                             * place at some copy of the env vars).
                             */
                            (char *const *)*_NSGetEnviron());
            }
            /* If we get here the spawn failed */
            posix_spawnattr_destroy(&attr);
        }
        return; /* don't do exec if error */
    }
#endif
    execv(toexec, (char **) info->argv);
}

static process_id_t
fork_suspended_child(const char *exe, dr_inject_info_t *info, int fds[2])
{
    process_id_t pid = fork();
    if (pid == 0) {
        /* child, suspend before exec */
        char pipe_cmd[MAXIMUM_PATH];
        ssize_t nread;
        size_t sofar = 0;
        const char *real_exe = NULL;
        const char *arg;
        close(fds[1]);  /* Close writer in child, keep reader. */
        do {
            nread = read(fds[0], pipe_cmd + sofar,
                         BUFFER_SIZE_BYTES(pipe_cmd) - sofar);
            sofar += nread;
        } while (nread > 0 && sofar < BUFFER_SIZE_BYTES(pipe_cmd)-1);
        pipe_cmd[sofar] = '\0';
        close(fds[0]);  /* Close reader before exec. */
        arg = pipe_cmd;
        /* The first token is the command and the rest is an argument. */
        while (*arg != '\0' && !isspace(*arg))
            arg++;
        while (*arg != '\0' && isspace(*arg))
            arg++;
        if (pipe_cmd[0] == '\0') {
            /* If nothing was written to the pipe, let it run natively. */
            real_exe = exe;
        } else if (strstr(pipe_cmd, "ld_preload ") == pipe_cmd) {
            pre_execve_ld_preload(arg);
            real_exe = exe;
        } else if (strcmp("ptrace", pipe_cmd) == 0) {
            /* If using ptrace, we're already attached and will walk across the
             * execv.
             */
            real_exe = exe;
        } else if (strstr(pipe_cmd, "exec_dr ") == pipe_cmd) {
            pre_execve_early(info, exe);
            real_exe = arg;
        }
        /* Trigger automated takeover in case DR is statically linked (yes
         * we blindly do this rather than try to pass in a parameter).
         */
        setenv("DYNAMORIO_TAKEOVER_IN_INIT", "1", true/*overwrite*/);
        execute_exec(info, real_exe);
        /* If execv returns, there was an error. */
        exit(-1);
    }
    return pid;
}

static void
write_pipe_cmd(int pipe_fd, const char *cmd)
{
    ssize_t towrite = strlen(cmd);
    ssize_t written = 0;
    if (verbose)
        fprintf(stderr, "writing cmd: %s\n", cmd);
    while (towrite > 0) {
        ssize_t nwrote = write(pipe_fd, cmd + written, towrite);
        if (nwrote <= 0)
            break;
        towrite -= nwrote;
        written += nwrote;
    }
}

static bool
inject_early(dr_inject_info_t *info, const char *library_path)
{
    if (info->exec_self) {
        /* exec DR with the original command line and set an environment
         * variable pointing to the real exe.
         */
        pre_execve_early(info, info->exe);
        execute_exec(info, library_path);
        return false;  /* if execv returns, there was an error */
    } else {
        /* Write the path to DR to the pipe. */
        char cmd[MAXIMUM_PATH];
        snprintf(cmd, BUFFER_SIZE_ELEMENTS(cmd), "exec_dr %s", library_path);
        NULL_TERMINATE_BUFFER(cmd);
        write_pipe_cmd(info->pipe_fd, cmd);
    }
    return true;
}

static bool
inject_ld_preload(dr_inject_info_t *info, const char *library_path)
{
    if (info->exec_self) {
        pre_execve_ld_preload(library_path);
        execute_exec(info, info->exe);
        return false;  /* if execv returns, there was an error */
    } else {
        /* Write the path to DR to the pipe. */
        char cmd[MAXIMUM_PATH];
        snprintf(cmd, BUFFER_SIZE_ELEMENTS(cmd), "ld_preload %s", library_path);
        NULL_TERMINATE_BUFFER(cmd);
        write_pipe_cmd(info->pipe_fd, cmd);
    }
    return true;
}

static dr_inject_info_t *
create_inject_info(const char *exe, const char **argv)
{
    dr_inject_info_t *info = calloc(sizeof(*info), 1);
    info->exe = exe;
    info->argv = argv;
    info->image_name = strrchr(exe, '/');
    info->image_name = (info->image_name == NULL ? exe : info->image_name + 1);
    info->exited = false;
    info->killpg = false;
    info->exitcode = -1;
    return info;
}

static platform_status_t
module_get_platform_path(const char *exe_path, dr_platform_t *platform,
                         dr_platform_t *alt_platform)
{
    file_t fd = os_open(exe_path, OS_OPEN_READ);
    platform_status_t res = PLATFORM_SUCCESS;
    if (fd == INVALID_FILE)
        res = PLATFORM_ERROR_CANNOT_OPEN;
    else {
        if (!module_get_platform(fd, platform, alt_platform)) {
            /* It may be a shell script so we try it. */
            res = PLATFORM_UNKNOWN;
            *platform = IF_X64_ELSE(DR_PLATFORM_64BIT, DR_PLATFORM_32BIT);
        }
        os_close(fd);
    }
    return res;
}

static bool
exe_is_right_bitwidth(const char *exe, int *errcode)
{
    dr_platform_t platform, alt_platform;
    if (module_get_platform_path(exe, &platform, &alt_platform) ==
        PLATFORM_ERROR_CANNOT_OPEN) {
        *errcode = errno;
        if (*errcode == 0)
            *errcode = ESRCH;
        return true;//false;
    }
    /* Check if the executable is the right bitwidth and set errcode to be
     * special code WARN_IMAGE_MACHINE_TYPE_MISMATCH_EXE if not.
     * The caller may decide what to do, e.g., terminate with error or
     * continue with cross arch executable.
     * XXX: i#1176 and DrM-i#1037, we need a long term solution to
     * support cross-arch injection.
     */
    if (platform != IF_X64_ELSE(DR_PLATFORM_64BIT, DR_PLATFORM_32BIT)
        IF_MACOS(IF_NOT_X64(&& alt_platform != DR_PLATFORM_32BIT))) {
        *errcode = WARN_IMAGE_MACHINE_TYPE_MISMATCH_EXE;
        return false;
    }
    return true;
}

/* Returns 0 on success.
 */
DR_EXPORT
int
dr_inject_process_create(const char *exe, const char **argv, void **data OUT)
{
    int r;
    int fds[2];
    dr_inject_info_t *info = create_inject_info(exe, argv);
    int errcode = 0;

#if defined(MACOS) && !defined(X64)
    dr_platform_t platform, alt_platform;
    if (module_get_platform_path(info->exe, &platform, &alt_platform) ==
        PLATFORM_ERROR_CANNOT_OPEN)
        return false; /* couldn't read header */
    if (platform == DR_PLATFORM_64BIT) {
        /* The target app is a universal binary and we're on a 64-bit kernel,
         * so we have to use posix_spawn to exec the app as 32-bit.
         */
        ASSERT(alt_platform == DR_PLATFORM_32BIT);
        info->spawn_32bit = true;
    }
#endif

    if (!exe_is_right_bitwidth(exe, &errcode) &&
        /* WARN_IMAGE_MACHINE_TYPE_MISMATCH_EXE is just a warning on Unix,
         * so we carry on but be sure to return the code.
         */
        errcode != WARN_IMAGE_MACHINE_TYPE_MISMATCH_EXE) {
        /* return here if couldn't find app */
        goto error;
    }
    /* Create a pipe to a forked child and have it block on the pipe. */
    r = pipe(fds);
    if (r != 0)
        goto error;
    info->argv = argv;
    info->pid = fork_suspended_child(exe, info, fds);
    close(fds[0]);  /* Close reader, keep writer. */
    info->pipe_fd = fds[1];
    info->exec_self = false;
    info->method = INJECT_LD_PRELOAD;

    if (info->pid == -1)
        goto error;
    *data = info;
    return errcode;

error:
    free(info);
    return errno;
}

DR_EXPORT
int
dr_inject_prepare_to_exec(const char *exe, const char **argv, void **data OUT)
{
    dr_inject_info_t *info = create_inject_info(exe, argv);
    int errcode = 0;
    *data = info;
    if (!exe_is_right_bitwidth(exe, &errcode) &&
        errcode != WARN_IMAGE_MACHINE_TYPE_MISMATCH_EXE) {
        free(info);
        return errcode;
    }
    info->pid = getpid();
    info->pipe_fd = 0;  /* No pipe. */
    info->exec_self = true;
    info->method = INJECT_LD_PRELOAD;
    /* Trigger automated takeover in case DR is statically linked. */
    setenv("DYNAMORIO_TAKEOVER_IN_INIT", "1", true/*overwrite*/);
    return errcode;
}

DR_EXPORT
bool
dr_inject_prepare_to_ptrace(void *data)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    if (data == NULL)
        return false;
    if (info->exec_self)
        return false;
    info->method = INJECT_PTRACE;
    return true;
}

DR_EXPORT
bool
dr_inject_prepare_new_process_group(void *data)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    int res;
    if (data == NULL)
        return false;
    if (info->exec_self)
        return false;
    /* Put the child in its own process group. */
    res = setpgid(info->pid, info->pid);
    if (res < 0)
        return false;
    info->killpg = true;
    return true;
}

DR_EXPORT
process_id_t
dr_inject_get_process_id(void *data)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    return info->pid;
}

DR_EXPORT
char *
dr_inject_get_image_name(void *data)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    return (char *) info->image_name;
}

/* FIXME: Use the parser in options.c.  The implementation here will find
 * options in quoted strings, like the client options string.
 */
static bool
option_present(const char *dr_ops, const char *op)
{
    size_t oplen = strlen(op);
    const char *cur = strstr(dr_ops, op);
    return (cur != NULL &&
            /* drrun now re-quotes args so we have to look for ".
             * This is not perfect: but we don't expect "-no_early_inject" to be
             * embedded into some longer string w/ literal quotes around it.
             */
            (cur[oplen] == '\0' || cur[oplen] == '\"' || isspace(cur[oplen])) &&
            (cur == dr_ops || cur[-1] == '\"' || isspace(cur[-1])));
}

DR_EXPORT
bool
dr_inject_process_inject(void *data, bool force_injection,
                         const char *library_path)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    char dr_path_buf[MAXIMUM_PATH];
    char dr_ops[MAX_OPTIONS_STRING];
    dr_platform_t platform, alt_platform;

    if (module_get_platform_path(info->exe, &platform, &alt_platform) ==
        PLATFORM_ERROR_CANNOT_OPEN)
        return false; /* couldn't read header */

#if defined(MACOS) && !defined(X64)
    if (platform == DR_PLATFORM_64BIT) {
        /* The target app is a universal binary and we're on a 64-bit kernel,
         * so we have to use posix_spawn to exec the app as 32-bit.
         */
        ASSERT(alt_platform == DR_PLATFORM_32BIT);
        platform = DR_PLATFORM_32BIT;
        info->spawn_32bit = true;
    }
#endif

    if (!get_config_val_other_app(info->image_name, info->pid, platform,
                                  DYNAMORIO_VAR_OPTIONS, dr_ops,
                                  BUFFER_SIZE_ELEMENTS(dr_ops), NULL,
                                  NULL, NULL)) {
        return false;
    }

    if (info->method == INJECT_LD_PRELOAD &&
        !option_present(dr_ops, "-no_early_inject")) {
#ifndef MACOS /* XXX i#1285: implement private loader for MacOS */
        info->method = INJECT_EARLY;
        /* i#1004: -early_inject has to decide whether to emulate the brk
         * before it can parse the options so we use an env var:
         */
        if (option_present(dr_ops, "-no_emulate_brk"))
            info->no_emulate_brk = true;
#endif
    }

#ifdef STATIC_LIBRARY
    return true;  /* Do nothing.  DR will takeover by itself. */
#endif

    /* Read the autoinject var from the config file if the caller didn't
     * override it.
     */
    if (library_path == NULL) {
        if (!get_config_val_other_app(info->image_name, info->pid, platform,
                                      DYNAMORIO_VAR_AUTOINJECT, dr_path_buf,
                                      BUFFER_SIZE_ELEMENTS(dr_path_buf), NULL,
                                      NULL, NULL)) {
            return false;
        }
        library_path = dr_path_buf;
    }

    switch (info->method) {
    case INJECT_EARLY:
        return inject_early(info, library_path);
    case INJECT_LD_PRELOAD:
        return inject_ld_preload(info, library_path);
    case INJECT_PTRACE:
#if defined(LINUX) && !defined(ANDROID) /* XXX i#1290/i#1701: NYI on MacOS/Android */
        return inject_ptrace(info, library_path);
#else
        return false;
#endif
    }

    return false;
}

/* We get the signal, we set the volatile, which is guaranteed to be signal
 * safe.  waitpid should return EINTR after we receive the signal.
 */
static void
alarm_handler(int sig)
{
    timeout_expired = true;
}

DR_EXPORT
bool
dr_inject_process_run(void *data)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    if (info->exec_self) {
        /* If we're injecting with LD_PRELOAD or STATIC_LIBRARY, we already set
         * up the environment.  If not, then let the app run natively.
         */
        execute_exec(info, info->exe);
        return false;  /* if execv returns, there was an error */
    } else {
        if (info->method == INJECT_PTRACE) {
#if defined(LINUX) && !defined(ANDROID) /* XXX i#1290/i#1701: NYI on MacOS/Android */
            our_ptrace(PTRACE_DETACH, info->pid, NULL, NULL);
#else
            return false;
#endif
        }
        /* Close the pipe. */
        close(info->pipe_fd);
        info->pipe_fd = 0;
    }
    return true;
}

DR_EXPORT
bool
dr_inject_wait_for_child(void *data, uint64 timeout_millis)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    pid_t res;

    timeout_expired = false;
    if (timeout_millis > 0) {
        /* Set a timer ala runstats. */
        struct sigaction act;
        struct itimerval timer;

        /* Set an alarm handler. */
        memset(&act, 0, sizeof(act));
        act.sa_handler = alarm_handler;
        sigaction(SIGALRM, &act, NULL);

        /* No interval, one shot only. */
        timer.it_interval.tv_sec = 0;
        timer.it_interval.tv_usec = 0;
        timer.it_value.tv_sec = timeout_millis / 1000;
        timer.it_value.tv_usec = (timeout_millis % 1000) * 1000;
        setitimer(ITIMER_REAL, &timer, NULL);
    }

    do {
        res = waitpid(info->pid, &info->exitcode, 0);
    } while (res != info->pid && res != -1 &&
             /* The signal handler sets this and makes waitpid return EINTR. */
             !timeout_expired);
    info->exited = (res == info->pid);
    return info->exited;
}

DR_EXPORT
int
dr_inject_process_exit(void *data, bool terminate)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    int status;
    if (info->exited) {
        /* If it already exited when we waited on it above, then we *cannot*
         * wait on it again or try to kill it, or we might target some new
         * process with the same pid.
         */
        status = info->exitcode;
    } else if (info->exec_self) {
        status = -1;  /* We never injected, must have been some other error. */
    } else if (terminate) {
        /* We use SIGKILL to match Windows, which doesn't provide the app a
         * chance to clean up.
         */
        if (info->killpg) {
            /* i#501: Kill app subprocesses to prevent hangs. */
            killpg(info->pid, SIGKILL);
        } else {
            kill(info->pid, SIGKILL);
        }
        /* Do a blocking wait to get the real status code.  This shouldn't take
         * long since we just sent an unblockable SIGKILL.
         */
        waitpid(info->pid, &status, 0);
    } else {
        /* Use WNOHANG to match our Windows semantics, which does not block if
         * the child hasn't exited.  The status returned is probably not useful,
         * but the caller shouldn't look at it if they haven't waited for the
         * app to terminate.
         */
        waitpid(info->pid, &status, WNOHANG);
    }
    if (info->pipe_fd != 0)
        close(info->pipe_fd);
    free(info);
    return status;
}

#if defined(LINUX) && !defined(ANDROID) /* XXX i#1290/i#1701: NYI on MacOS/Android */
/*******************************************************************************
 * ptrace injection code
 */

enum { MAX_SHELL_CODE = 4096 };

#ifdef X86
# define USER_REGS_TYPE user_regs_struct
# define REG_PC_FIELD IF_X64_ELSE(rip, eip)
# define REG_SP_FIELD IF_X64_ELSE(rsp, esp)
# define REG_RETVAL_FIELD IF_X64_ELSE(rax, eax)
#elif defined(ARM)
/* On AArch32, glibc uses user_regs instead of user_regs_struct.
 * struct user_regs {
 *   unsigned long int uregs[18];
 * };
 * - uregs[0..15] are for [r0..r15],
 * - uregs[16] is for cpsr,
 * - uregs[17] is for "orig_r0".
 */
# define USER_REGS_TYPE user_regs
# define REG_PC_FIELD uregs[15] /* r15 in user_regs */
# define REG_SP_FIELD uregs[13] /* r13 in user_regs */
/* On ARM, all reg args are also reg retvals. */
# define REG_RETVAL_FIELD uregs[0] /* r0 in user_regs */
#elif defined(AARCH64)
# define USER_REGS_TYPE user_pt_regs
# define REG_PC_FIELD pc
# define REG_SP_FIELD sp
# define REG_RETVAL_FIELD regs[0] /* x0 in user_regs_struct */
#endif

enum { REG_PC_OFFSET = offsetof(struct USER_REGS_TYPE, REG_PC_FIELD) };

#define APP  instrlist_append

static bool op_exec_gdb = false;

/* Used to pass data into the remote mapping routines. */
static dr_inject_info_t *injector_info;
static file_t injector_dr_fd;
static file_t injectee_dr_fd;

typedef struct _enum_name_pair_t {
    const int enum_val;
    const char * const enum_name;
} enum_name_pair_t;

/* Ptrace request enum name mapping.  The complete enumeration is in
 * sys/ptrace.h.
 */
static const enum_name_pair_t pt_req_map[] = {
    {PTRACE_TRACEME,        "PTRACE_TRACEME"},
    {PTRACE_PEEKTEXT,       "PTRACE_PEEKTEXT"},
    {PTRACE_PEEKDATA,       "PTRACE_PEEKDATA"},
    {PTRACE_PEEKUSER,       "PTRACE_PEEKUSER"},
    {PTRACE_POKETEXT,       "PTRACE_POKETEXT"},
    {PTRACE_POKEDATA,       "PTRACE_POKEDATA"},
    {PTRACE_POKEUSER,       "PTRACE_POKEUSER"},
    {PTRACE_CONT,           "PTRACE_CONT"},
    {PTRACE_KILL,           "PTRACE_KILL"},
    {PTRACE_SINGLESTEP,     "PTRACE_SINGLESTEP"},
#ifndef AARCH64
    {PTRACE_GETREGS,        "PTRACE_GETREGS"},
    {PTRACE_SETREGS,        "PTRACE_SETREGS"},
    {PTRACE_GETFPREGS,      "PTRACE_GETFPREGS"},
    {PTRACE_SETFPREGS,      "PTRACE_SETFPREGS"},
#endif
    {PTRACE_ATTACH,         "PTRACE_ATTACH"},
    {PTRACE_DETACH,         "PTRACE_DETACH"},
#ifndef AARCH64
    {PTRACE_GETFPXREGS,     "PTRACE_GETFPXREGS"},
    {PTRACE_SETFPXREGS,     "PTRACE_SETFPXREGS"},
#endif
    {PTRACE_SYSCALL,        "PTRACE_SYSCALL"},
    {PTRACE_SETOPTIONS,     "PTRACE_SETOPTIONS"},
    {PTRACE_GETEVENTMSG,    "PTRACE_GETEVENTMSG"},
    {PTRACE_GETSIGINFO,     "PTRACE_GETSIGINFO"},
    {PTRACE_SETSIGINFO,     "PTRACE_SETSIGINFO"},
    {0}
};

/* Ptrace syscall wrapper, for logging.
 * XXX: We could call libc's ptrace instead of using dynamorio_syscall.
 * Initially I used the raw syscall to avoid adding a libc import, but calling
 * libc from the injector process should always work.
 */
static long
our_ptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data)
{
    long r = dynamorio_syscall(SYS_ptrace, 4, request, pid, addr, data);
    if (verbose &&
        /* Don't log reads and writes. */
        request != PTRACE_POKEDATA &&
        request != PTRACE_PEEKDATA) {
        const enum_name_pair_t *pair = NULL;
        int i;
        for (i = 0; pt_req_map[i].enum_name != NULL; i++) {
             if (pt_req_map[i].enum_val == request) {
                 pair = &pt_req_map[i];
                 break;
             }
        }
        ASSERT(pair != NULL);
        fprintf(stderr, "\tptrace(%s, %d, %p, %p) -> %ld %s\n",
                pair->enum_name, (int)pid, addr, data, r, strerror(-r));
    }
    return r;
}
#define ptrace DO_NOT_USE_ptrace_USE_our_ptrace

/* We use these wrappers because PTRACE_GETREGS and PTRACE_SETREGS are not
 * present on all architectures, while the alternatives, PTRACE_GETREGSET
 * and PTRACE_SETREGSET, are present only since Linux 2.6.34.
 * Red Hat Enterprise 6.6 has Linux 2.6.32.
 */

static long
our_ptrace_getregs(pid_t pid, struct USER_REGS_TYPE *regs)
{
#ifdef AARCH64
    struct iovec iovec = { regs, sizeof(*regs) };
    return our_ptrace(PTRACE_GETREGSET, pid, (void *)NT_PRSTATUS, &iovec);
#else
    return our_ptrace(PTRACE_GETREGS, pid, NULL, regs);
#endif
}

static long
our_ptrace_setregs(pid_t pid, struct USER_REGS_TYPE *regs)
{
#ifdef AARCH64
    struct iovec iovec = { regs, sizeof(*regs) };
    return our_ptrace(PTRACE_SETREGSET, pid, (void *)NT_PRSTATUS, &iovec);
#else
    return our_ptrace(PTRACE_SETREGS, pid, NULL, regs);
#endif
}

/* Copies memory from traced process into parent.
 */
static bool
ptrace_read_memory(pid_t pid, void *dst, void *src, size_t len)
{
    uint i;
    ptr_int_t *dst_reg = dst;
    ptr_int_t *src_reg = src;
    ASSERT(len % sizeof(ptr_int_t) == 0);  /* FIXME handle */
    for (i = 0; i < len / sizeof(ptr_int_t); i++) {
        /* We use a raw syscall instead of the libc wrapper, so the value read
         * is stored in the data pointer instead of being returned in r.
         */
        long r = our_ptrace(PTRACE_PEEKDATA, pid, &src_reg[i], &dst_reg[i]);
        if (r < 0)
            return false;
    }
    return true;
}

/* Copies memory from parent into traced process.
 */
static bool
ptrace_write_memory(pid_t pid, void *dst, void *src, size_t len)
{
    uint i;
    ptr_int_t *dst_reg = dst;
    ptr_int_t *src_reg = src;
    ASSERT(len % sizeof(ptr_int_t) == 0);  /* FIXME handle */
    for (i = 0; i < len / sizeof(ptr_int_t); i++) {
        long r = our_ptrace(PTRACE_POKEDATA, pid, &dst_reg[i],
                            (void *) src_reg[i]);
        if (r < 0)
            return false;
    }
    return true;
}


/* Push a pointer to a string to the stack.  We create a fake instruction with
 * raw bytes equal to the string we want to put in the injectee.  The call will
 * pass these invalid instruction bytes, and the return address on the stack
 * will point to the string.
 */
static void
gen_push_string(void *dc, instrlist_t *ilist, const char *msg)
{
#ifdef X86
    instr_t *after_msg = INSTR_CREATE_label(dc);
    instr_t *msg_instr = instr_build_bits(dc, OP_UNDECODED, strlen(msg) + 1);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(after_msg)));
    instr_set_raw_bytes(msg_instr, (byte*)msg, strlen(msg) + 1);
    instr_set_raw_bits_valid(msg_instr, true);
    APP(ilist, msg_instr);
    APP(ilist, after_msg);
#else
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86 */
}

static void
gen_syscall(void *dc, instrlist_t *ilist, int sysnum, uint num_opnds,
            opnd_t *args)
{
#ifdef X86
    uint i;
    ASSERT(num_opnds <= MAX_SYSCALL_ARGS);
    APP(ilist, INSTR_CREATE_mov_imm
        (dc, opnd_create_reg(DR_REG_XAX), OPND_CREATE_INTPTR(sysnum)));
    for (i = 0; i < num_opnds; i++) {
        if (opnd_is_immed_int(args[i]) || opnd_is_instr(args[i])) {
            APP(ilist, INSTR_CREATE_mov_imm
                (dc, opnd_create_reg(syscall_regparms[i]), args[i]));
        } else if (opnd_is_base_disp(args[i])) {
            APP(ilist, INSTR_CREATE_mov_ld
                (dc, opnd_create_reg(syscall_regparms[i]), args[i]));
        }
    }
    /* XXX: Reuse create_syscall_instr() in emit_utils.c. */
# ifdef X64
    APP(ilist, INSTR_CREATE_syscall(dc));
# else
    APP(ilist, INSTR_CREATE_int(dc, OPND_CREATE_INT8((char)0x80)));
# endif
#else
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86 */
}

#if 0  /* Useful for debugging gen_syscall and gen_push_string. */
static void
gen_print(void *dc, instrlist_t *ilist, const char *msg)
{
    opnd_t args[MAX_SYSCALL_ARGS];
    args[0] = OPND_CREATE_INTPTR(2);
    args[1] = OPND_CREATE_MEMPTR(DR_REG_XSP, 0);  /* msg is on TOS. */
    args[2] = OPND_CREATE_INTPTR(strlen(msg));
    gen_push_string(dc, ilist, msg);
    gen_syscall(dc, ilist, SYSNUM_NO_CANCEL(SYS_write), 3, args);
}
#endif

static void
unexpected_trace_event(process_id_t pid, int sig_expected, int sig_actual)
{
    if (verbose) {
        app_pc err_pc;
        our_ptrace(PTRACE_PEEKUSER, pid, (void *)REG_PC_OFFSET, &err_pc);
        fprintf(stderr, "Unexpected trace event.  Expected %s, got signal %d "
                "at pc: %p\n", strsignal(sig_expected), sig_actual,
                err_pc);
    }
}

static bool
wait_until_signal(process_id_t pid, int sig)
{
    int status;
    int r = waitpid(pid, &status, 0);
    if (r < 0)
        return false;
    if (WIFSTOPPED(status) && WSTOPSIG(status) == sig) {
        return true;
    } else {
        unexpected_trace_event(pid, sig, WSTOPSIG(status));
        return false;
    }
}

/* Continue until the next SIGTRAP.  Returns false and prints an error message
 * if the next trap is not a breakpoint.
 */
static bool
continue_until_break(process_id_t pid)
{
    long r = our_ptrace(PTRACE_CONT, pid, NULL, NULL);
    if (r < 0)
        return false;
    return wait_until_signal(pid, SIGTRAP);
}

/* Injects the code in ilist into the injectee and runs it, returning the value
 * left in the return value register at the end of ilist execution.  Frees
 * ilist.  Returns -EUNATCH if anything fails before executing the syscall.
 */
static ptr_int_t
injectee_run_get_retval(dr_inject_info_t *info, void *dc, instrlist_t *ilist)
{
    struct USER_REGS_TYPE regs;
    byte shellcode[MAX_SHELL_CODE];
    byte orig_code[MAX_SHELL_CODE];
    app_pc end_pc;
    size_t code_size;
    ptr_int_t ret;
    app_pc pc;
    long r;
    ptr_int_t failure = -EUNATCH;  /* Unlikely to be used by most syscalls. */

    /* Get register state before executing the shellcode. */
    r = our_ptrace_getregs(info->pid, &regs);
    if (r < 0)
        return r;

    /* Use the current PC's page, since it's executable.  Our shell code is
     * always less than one page, so we won't overflow.
     */
    pc = (app_pc)ALIGN_BACKWARD(regs.REG_PC_FIELD, PAGE_SIZE);

    /* Append an int3 so we can catch the break. */
    APP(ilist, XINST_CREATE_debug_instr(dc));
    if (verbose) {
        fprintf(stderr, "injecting code:\n");
#if defined(INTERNAL) || defined(DEBUG) || defined(CLIENT_INTERFACE)
        /* XXX: This disas call aborts on our raw bytes instructions.  Can we
         * teach DR's disassembler to avoid those instrs?
         */
        instrlist_disassemble(dc, pc, ilist, STDERR);
#endif
    }

    /* Encode ilist into shellcode. */
    end_pc = instrlist_encode_to_copy(dc, ilist, shellcode, pc,
                                      &shellcode[MAX_SHELL_CODE], true/*jmp*/);
    code_size = end_pc - &shellcode[0];
    code_size = ALIGN_FORWARD(code_size, sizeof(reg_t));
    ASSERT(code_size <= MAX_SHELL_CODE);
    instrlist_clear_and_destroy(dc, ilist);

    /* Copy shell code into injectee at the current PC. */
    if (!ptrace_read_memory(info->pid, orig_code, pc, code_size) ||
        !ptrace_write_memory(info->pid, pc, shellcode, code_size))
        return failure;

    /* Run it! */
    our_ptrace(PTRACE_POKEUSER, info->pid, (void *)REG_PC_OFFSET, pc);
    if (!continue_until_break(info->pid))
        return failure;

    /* Get return value. */
    ret = failure;
    r = our_ptrace(PTRACE_PEEKUSER, info->pid,
                   (void *)offsetof(struct USER_REGS_TYPE, REG_RETVAL_FIELD), &ret);
    if (r < 0)
        return r;

    /* Put back original code and registers. */
    if (!ptrace_write_memory(info->pid, pc, orig_code, code_size))
        return failure;
    r = our_ptrace_setregs(info->pid, &regs);
    if (r < 0)
        return r;

    return ret;
}

/* Call sys_open in the child. */
static int
injectee_open(dr_inject_info_t *info, const char *path, int flags, mode_t mode)
{
    void *dc = GLOBAL_DCONTEXT;
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t args[MAX_SYSCALL_ARGS];
    int num_args = 0;
    gen_push_string(dc, ilist, path);
#ifndef SYS_open
    args[num_args++] = OPND_CREATE_INTPTR(AT_FDCWD);
#endif
    args[num_args++] = OPND_CREATE_MEMPTR(REG_XSP, 0);
    args[num_args++] = OPND_CREATE_INTPTR(flags);
    args[num_args++] = OPND_CREATE_INTPTR(mode);
    ASSERT(num_args <= MAX_SYSCALL_ARGS);
#ifdef SYS_open
    gen_syscall(dc, ilist, SYSNUM_NO_CANCEL(SYS_open), num_args, args);
#else
    gen_syscall(dc, ilist, SYSNUM_NO_CANCEL(SYS_openat), num_args, args);
#endif
    return injectee_run_get_retval(info, dc, ilist);
}

static void *
injectee_mmap(dr_inject_info_t *info, void *addr, size_t sz, int prot,
              int flags, int fd, off_t offset)
{
    void *dc = GLOBAL_DCONTEXT;
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t args[MAX_SYSCALL_ARGS];
    int num_args = 0;
    args[num_args++] = OPND_CREATE_INTPTR(addr);
    args[num_args++] = OPND_CREATE_INTPTR(sz);
    args[num_args++] = OPND_CREATE_INTPTR(prot);
    args[num_args++] = OPND_CREATE_INTPTR(flags);
    args[num_args++] = OPND_CREATE_INTPTR(fd);
    args[num_args++] = OPND_CREATE_INTPTR(IF_X64_ELSE(offset, offset >> 12));
    ASSERT(num_args <= MAX_SYSCALL_ARGS);
    /* XXX: Regular mmap gives EBADR on ia32, but mmap2 works. */
    gen_syscall(dc, ilist, IF_X64_ELSE(SYS_mmap, SYS_mmap2), num_args, args);
    return (void *) injectee_run_get_retval(info, dc, ilist);
}

/* Do an mmap syscall in the injectee, parallel to the os_map_file prototype.
 * Passed to elf_loader_map_phdrs to map DR into the injectee.  Uses the globals
 * injector_dr_fd to injectee_dr_fd to map the former to the latter.
 */
static byte *
injectee_map_file(file_t f, size_t *size INOUT, uint64 offs, app_pc addr,
                  uint prot, map_flags_t map_flags)
{
    int fd;
    int flags = 0;
    app_pc r;
    if (TEST(MAP_FILE_COPY_ON_WRITE, map_flags))
        flags |= MAP_PRIVATE;
    if (TEST(MAP_FILE_FIXED, map_flags))
        flags |= MAP_FIXED;
    /* MAP_FILE_IMAGE is a nop on Linux. */
    if (f == injector_dr_fd)
        fd = injectee_dr_fd;
    else
        fd = f;
    if (fd == -1) {
        flags |= MAP_ANONYMOUS;
    }
    r = injectee_mmap(injector_info, addr, *size, memprot_to_osprot(prot),
                      flags, fd, offs);
    if (!mmap_syscall_succeeded(r)) {
        int err = -(int)(ptr_int_t)r;
        printf("injectee_mmap(%d, %p, %p, 0x%x, 0x%lx, 0x%x) -> (%d): %s\n",
               fd, addr, (void *)*size, memprot_to_osprot(prot), (long)offs,
               flags, err, strerror(err));
        return NULL;
    }
    return r;
}

/* Do an munmap syscall in the injectee. */
static bool
injectee_unmap(byte *addr, size_t size)
{
    void *dc = GLOBAL_DCONTEXT;
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t args[MAX_SYSCALL_ARGS];
    ptr_int_t r;
    int num_args = 0;
    args[num_args++] = OPND_CREATE_INTPTR(addr);
    args[num_args++] = OPND_CREATE_INTPTR(size);
    ASSERT(num_args <= MAX_SYSCALL_ARGS);
    gen_syscall(dc, ilist, SYS_munmap, num_args, args);
    r = injectee_run_get_retval(injector_info, dc, ilist);
    if (r < 0) {
        printf("injectee_munmap(%p, %p) -> %p\n",
               addr, (void *) size, (void *)r);
        return false;
    }
    return true;
}

/* Do an mprotect syscall in the injectee. */
static bool
injectee_prot(byte *addr, size_t size, uint prot/*MEMPROT_*/)
{
    void *dc = GLOBAL_DCONTEXT;
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t args[MAX_SYSCALL_ARGS];
    ptr_int_t r;
    int num_args = 0;
    args[num_args++] = OPND_CREATE_INTPTR(addr);
    args[num_args++] = OPND_CREATE_INTPTR(size);
    args[num_args++] = OPND_CREATE_INTPTR(memprot_to_osprot(prot));
    ASSERT(num_args <= MAX_SYSCALL_ARGS);
    gen_syscall(dc, ilist, SYS_mprotect, num_args, args);
    r = injectee_run_get_retval(injector_info, dc, ilist);
    if (r < 0) {
        printf("injectee_prot(%p, %p, %x) -> %d\n",
               addr, (void *) size, prot, (int)r);
        return false;
    }
    return true;
}

/* Convert a user_regs_struct used by the ptrace API into DR's priv_mcontext_t
 * struct.
 */
static void
user_regs_to_mc(priv_mcontext_t *mc, struct USER_REGS_TYPE *regs)
{
#ifdef X86
# ifdef X64
    mc->rip = (app_pc)regs->rip;
    mc->rax = regs->rax;
    mc->rcx = regs->rcx;
    mc->rdx = regs->rdx;
    mc->rbx = regs->rbx;
    mc->rsp = regs->rsp;
    mc->rbp = regs->rbp;
    mc->rsi = regs->rsi;
    mc->rdi = regs->rdi;
    mc->r8  = regs->r8 ;
    mc->r9  = regs->r9 ;
    mc->r10 = regs->r10;
    mc->r11 = regs->r11;
    mc->r12 = regs->r12;
    mc->r13 = regs->r13;
    mc->r14 = regs->r14;
    mc->r15 = regs->r15;
# else
    mc->eip = (app_pc)regs->eip;
    mc->eax = regs->eax;
    mc->ecx = regs->ecx;
    mc->edx = regs->edx;
    mc->ebx = regs->ebx;
    mc->esp = regs->esp;
    mc->ebp = regs->ebp;
    mc->esi = regs->esi;
    mc->edi = regs->edi;
# endif
#elif defined(ARM)
    mc->r0  = regs->uregs[0];
    mc->r1  = regs->uregs[1];
    mc->r2  = regs->uregs[2];
    mc->r3  = regs->uregs[3];
    mc->r4  = regs->uregs[4];
    mc->r5  = regs->uregs[5];
    mc->r6  = regs->uregs[6];
    mc->r7  = regs->uregs[7];
    mc->r8  = regs->uregs[8];
    mc->r9  = regs->uregs[9];
    mc->r10 = regs->uregs[10];
    mc->r11 = regs->uregs[11];
    mc->r12 = regs->uregs[12];
    mc->r13 = regs->uregs[13];
    mc->r14 = regs->uregs[14];
    mc->r15 = regs->uregs[15];
    mc->cpsr = regs->uregs[16];
#elif defined(AARCH64)
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
#endif /* X86/ARM */
}

/* Detach from the injectee and re-exec ourselves as gdb with --pid.  This is
 * useful for debugging initialization in the injectee.
 * XXX: This is racy.  I have to insert os_thread_sleep(500) in takeover_ptrace()
 * in order for this to work.
 */
static void
detach_and_exec_gdb(process_id_t pid, const char *library_path)
{
    char pid_str[16];  /* long enough for a decimal string pid */
    const char *argv[20];  /* 20 is long enough for our gdb command. */
    int num_args = 0;
    char add_symfile[MAXIMUM_PATH];

    /* Get the text start, quick and dirty. */
    file_t f = os_open(library_path, OS_OPEN_READ);
    uint64 size64;
    os_get_file_size_by_handle(f, &size64);
    size_t size = (size_t) size64;
    byte *base = os_map_file(f, &size, 0, NULL, MEMPROT_READ, MAP_FILE_COPY_ON_WRITE);
    app_pc text_start = (app_pc) module_get_text_section(base, size);
    os_unmap_file(base, size);
    os_close(f);

    our_ptrace(PTRACE_DETACH, pid, NULL, NULL);
    snprintf(pid_str, BUFFER_SIZE_ELEMENTS(pid_str), "%d", pid);
    NULL_TERMINATE_BUFFER(pid_str);
    argv[num_args++] = "/usr/bin/gdb";
    argv[num_args++] = "--quiet";
    argv[num_args++] = "--pid";
    argv[num_args++] = pid_str;
    argv[num_args++] = "-ex";
    argv[num_args++] = "set confirm off";
    argv[num_args++] = "-ex";
    snprintf(add_symfile, BUFFER_SIZE_ELEMENTS(add_symfile),
             "add-symbol-file %s "PFX, library_path, text_start);
    NULL_TERMINATE_BUFFER(add_symfile);
    argv[num_args++] = add_symfile;
    argv[num_args++] = NULL;
    ASSERT(num_args < BUFFER_SIZE_ELEMENTS(argv));
    execv("/usr/bin/gdb", (char **)argv);
    ASSERT(false && "failed to exec gdb?");
}

bool
inject_ptrace(dr_inject_info_t *info, const char *library_path)
{
    long r;
    int dr_fd;
    struct USER_REGS_TYPE regs;
    ptrace_stack_args_t args;
    app_pc injected_base;
    app_pc injected_dr_start;
    elf_loader_t loader;
    int status;
    int signal;

    /* Attach to the process in question. */
    r = our_ptrace(PTRACE_ATTACH, info->pid, NULL, NULL);
    if (r < 0) {
        if (verbose) {
            fprintf(stderr, "PTRACE_ATTACH failed with error: %s\n",
                    strerror(-r));
        }
        return false;
    }
    if (!wait_until_signal(info->pid, SIGSTOP))
        return false;

    if (info->pipe_fd != 0) {
        /* For children we created, walk it across the execve call. */
        write_pipe_cmd(info->pipe_fd, "ptrace");
        close(info->pipe_fd);
        info->pipe_fd = 0;
        if (our_ptrace(PTRACE_SETOPTIONS, info->pid, NULL,
                       (void *)PTRACE_O_TRACEEXEC) < 0)
            return false;
        if (!continue_until_break(info->pid))
            return false;
    }

    /* Open libdynamorio.so as readonly in the child. */
    dr_fd = injectee_open(info, library_path, O_RDONLY, 0);
    if (dr_fd < 0) {
        if (verbose) {
            fprintf(stderr, "Unable to open libdynamorio.so in injectee (%d): "
                    "%s\n", -dr_fd, strerror(-dr_fd));
        }
        return false;
    }

    /* Call our private loader, but perform the mmaps in the child process
     * instead of the parent.
     */
    if (!elf_loader_read_headers(&loader, library_path))
        return false;
    /* XXX: Have to use globals to communicate to injectee_map_file. =/ */
    injector_info = info;
    injector_dr_fd = loader.fd;
    injectee_dr_fd = dr_fd;
    injected_base = elf_loader_map_phdrs(&loader, true/*fixed*/,
                                         injectee_map_file, injectee_unmap,
                                         injectee_prot, false/*!reachable*/);
    if (injected_base == NULL) {
        if (verbose)
            fprintf(stderr, "Unable to mmap libdynamorio.so in injectee\n");
        return false;
    }
    /* Looking up exports through ptrace is hard, so we use the e_entry from
     * the ELF header with different arguments.
     * XXX: Actually look up an export.
     */
    injected_dr_start = (app_pc) loader.ehdr->e_entry + loader.load_delta;
    elf_loader_destroy(&loader);

    our_ptrace_getregs(info->pid, &regs);

    /* Create an injection context and "push" it onto the stack of the injectee.
     * If you need to pass more info to the injected child process, this is a
     * good place to put it.
     */
    memset(&args, 0, sizeof(args));
    user_regs_to_mc(&args.mc, &regs);
    args.argc = ARGC_PTRACE_SENTINEL;

    /* We need to send the home directory over.  It's hard to find the
     * environment in the injectee, and even if we could HOME might be
     * different.
     */
    strncpy(args.home_dir, getenv("HOME"), BUFFER_SIZE_ELEMENTS(args.home_dir));
    NULL_TERMINATE_BUFFER(args.home_dir);

#if defined(X86) || defined(AARCHXX)
    regs.REG_SP_FIELD -= REDZONE_SIZE;  /* Need to preserve x64 red zone. */
    regs.REG_SP_FIELD -= sizeof(args);  /* Allocate space for args. */
    regs.REG_SP_FIELD = ALIGN_BACKWARD(regs.REG_SP_FIELD, REGPARM_END_ALIGN);
    ptrace_write_memory(info->pid, (void *)regs.REG_SP_FIELD,
                        &args, sizeof(args));
#else
# error "depends on arch stack growth direction"
#endif

    regs.REG_PC_FIELD = (ptr_int_t) injected_dr_start;
    our_ptrace_setregs(info->pid, &regs);

    if (op_exec_gdb) {
        detach_and_exec_gdb(info->pid, library_path);
        ASSERT_NOT_REACHED();
    }

    /* This should run something equivalent to dynamorio_app_init(), and then
     * return.
     * XXX: we can actually fault during dynamorio_app_init() due to safe_reads,
     * so we have to expect SIGSEGV and let it be delivered.
     */
    signal = 0;
    do {
        /* Continue or deliver pending signal from status. */
        r = our_ptrace(PTRACE_CONT, info->pid, NULL, (void *)(ptr_int_t)signal);
        if (r < 0)
            return false;
        r = waitpid(info->pid, &status, 0);
        if (r < 0 || !WIFSTOPPED(status))
            return false;
        signal = WSTOPSIG(status);
    } while (signal == SIGSEGV);

    /* When we get SIGTRAP, DR has initialized. */
    if (signal != SIGTRAP) {
        unexpected_trace_event(info->pid, SIGTRAP, signal);
        return false;
    }

    /* We've stopped the injectee prior to dynamo_start.  If we detach now, it
     * will continue into dynamo_start().
     */
    return true;
}

#endif /* LINUX && !ANDROID */
