/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Never actually called, but needed to link in config.c. */
const char *
get_application_short_name(void)
{
    return "";
}

/* Opaque type to users, holds our state */
typedef struct _dr_inject_info_t {
    process_id_t pid;
    const char *exe;            /* full path of executable */
    const char *image_name;     /* basename of exe */
    const char **argv;          /* array of arguments */
    int pipe_fd;
    bool exec_self;
} dr_inject_info_t;

/* libc's environment pointer. */
extern char **environ;

static process_id_t
fork_suspended_child(const char *exe, const char **argv, int fds[2])
{
    process_id_t pid = fork();
    if (pid == 0) {
        /* child, suspend before exec */
        char libdr_path[MAXIMUM_PATH];
        ssize_t nread;
        size_t sofar = 0;
        char *real_exe;
        close(fds[1]);  /* Close writer in child, keep reader. */
        do {
            nread = read(fds[0], libdr_path + sofar,
                         BUFFER_SIZE_BYTES(libdr_path) - sofar);
            sofar += nread;
        } while (nread > 0 && sofar < BUFFER_SIZE_BYTES(libdr_path)-1);
        libdr_path[sofar] = '\0';
        close(fds[0]);  /* Close reader before exec. */
        if (libdr_path[0] == '\0') {
            /* If nothing was written to the pipe, let it run natively. */
            real_exe = (char *) exe;
        } else {
            real_exe = libdr_path;
        }
        setenv(DYNAMORIO_VAR_EXE_PATH, exe, true/*overwrite*/);
#ifdef STATIC_LIBRARY
        setenv("DYNAMORIO_TAKEOVER_IN_INIT", "1", true/*overwrite*/);
#endif
        execv(real_exe, (char **) argv);
        /* If execv returns, there was an error. */
        exit(-1);
    }
    return pid;
}

static void
set_exe_and_argv(dr_inject_info_t *info, const char *exe, const char **argv)
{
    info->exe = exe;
    info->argv = argv;
    info->image_name = strrchr(exe, '/');
    info->image_name = (info->image_name == NULL ? exe : info->image_name + 1);
}

/* Returns 0 on success.
 */
DR_EXPORT
int
dr_inject_process_create(const char *exe, const char **argv, void **data OUT)
{
    int r;
    int fds[2];
    dr_inject_info_t *info = malloc(sizeof(*info));
    set_exe_and_argv(info, exe, argv);

    /* Create a pipe to a forked child and have it block on the pipe. */
    r = pipe(fds);
    if (r != 0)
        goto error;
    info->pid = fork_suspended_child(exe, argv, fds);
    close(fds[0]);  /* Close reader, keep writer. */
    info->pipe_fd = fds[1];
    info->exec_self = false;

    if (info->pid == -1)
        goto error;
    *data = info;
    return 0;

error:
    free(info);
    return errno;
}

DR_EXPORT
int
dr_inject_prepare_to_exec(const char *exe, const char **argv, void **data OUT)
{
    dr_inject_info_t *info = malloc(sizeof(*info));
    set_exe_and_argv(info, exe, argv);
    info->pid = getpid();
    info->pipe_fd = 0;  /* No pipe. */
    info->exec_self = true;
    *data = info;
#ifdef STATIC_LIBRARY
    setenv("DYNAMORIO_TAKEOVER_IN_INIT", "1", true/*overwrite*/);
#endif
    return 0;
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

DR_EXPORT
bool
dr_inject_process_inject(void *data, bool force_injection,
                         const char *library_path)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    ssize_t towrite;
    ssize_t written = 0;
    char dr_path_buf[MAXIMUM_PATH];

#ifdef STATIC_LIBRARY
    return true;  /* Do nothing.  DR will takeover by itself. */
#endif

    /* Read the autoinject var from the config file if the caller didn't
     * override it.
     */
    if (library_path == NULL) {
        if (!get_config_val_other_app(info->image_name, info->pid,
                                      DYNAMORIO_VAR_AUTOINJECT, dr_path_buf,
                                      BUFFER_SIZE_ELEMENTS(dr_path_buf), NULL,
                                      NULL, NULL)) {
            return false;
        }
        library_path = dr_path_buf;
    }

    if (info->exec_self) {
        /* exec DR with the original command line and set an environment
         * variable pointing to the real exe.
         */
        /* XXX: setenv will modify the environment on failure. */
        setenv(DYNAMORIO_VAR_EXE_PATH, info->exe, true/*overwrite*/);
        execv(library_path, (char **) info->argv);
        return false;  /* if execv returns, there was an error */
    } else {
        /* Write the path to DR to the pipe. */
        towrite = strlen(library_path);
        while (towrite > 0) {
            ssize_t nwrote = write(info->pipe_fd, library_path + written, towrite);
            if (nwrote <= 0)
                break;
            towrite -= nwrote;
            written += nwrote;
        }
    }
    return true;
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
        execv(info->exe, (char **) info->argv);
        return false;  /* if execv returns, there was an error */
    } else {
        /* Close the pipe. */
        close(info->pipe_fd);
        info->pipe_fd = 0;
    }
    return true;
}

DR_EXPORT
int
dr_inject_process_exit(void *data, bool terminate)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    int status;
    if (terminate) {
        kill(info->pid, SIGKILL);
    }
    if (info->pipe_fd != 0)
        close(info->pipe_fd);
    waitpid(info->pid, &status, 0);
    free(info);
    return status;
}
