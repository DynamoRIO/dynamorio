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
    char image_name[MAXIMUM_PATH];
    int pipe_fd;
} dr_inject_info_t;


static process_id_t
fork_suspended_child(const char *exe, const char **argv, int fds[2])
{
    process_id_t pid = fork();
    if (pid == 0) {
        /* child, suspend before exec */
        char libdr_path[MAXIMUM_PATH];
        ssize_t nread;
        size_t sofar = 0;
        close(fds[1]);  /* Close writer in child, keep reader. */
        do {
            nread = read(fds[0], libdr_path + sofar,
                         BUFFER_SIZE_BYTES(libdr_path) - sofar);
            sofar += nread;
        } while (nread > 0 && sofar < BUFFER_SIZE_BYTES(libdr_path)-1);
        libdr_path[sofar] = '\0';
        close(fds[0]);  /* Close reader before exec. */
        if (libdr_path[0] == '\0') {
            execv((char *) exe, (char **) argv);
        } else {
            /* FIXME i#908: Allow the caller to pass an arbitrary argv[0] when
             * using early injection.  We can probably pass the absolute path of
             * the exe in an environment variable.
             */
            argv[0] = exe;  /* Make argv[0] absolute for DR's benefit. */
            execv(libdr_path, (char **) argv);
        }
        /* If execv returns, there was an error. */
        exit(-1);
    }
    return pid;
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
    const char *basename = strrchr(exe, '/');
    if (basename == NULL) {
        return -1;  /* exe should be absolute. */
    }
    basename++;
    strncpy(info->image_name, basename, BUFFER_SIZE_ELEMENTS(info->image_name));
    NULL_TERMINATE_BUFFER(info->image_name);

    /* Create a pipe to a forked child and have it block on the pipe. */
    r = pipe(fds);
    if (r != 0)
        goto error;
    info->pid = fork_suspended_child(exe, argv, fds);
    close(fds[0]);  /* Close reader, keep writer. */
    info->pipe_fd = fds[1];

    if (info->pid == -1)
        goto error;
    *data = info;
    return 0;

error:
    free(info);
    return errno;
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
    return info->image_name;
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

    /* Write the path to DR to the pipe. */
    towrite = strlen(library_path);
    while (towrite > 0) {
        ssize_t nwrote = write(info->pipe_fd, library_path + written, towrite);
        if (nwrote <= 0)
            break;
        towrite -= nwrote;
        written += nwrote;
    }
    close(info->pipe_fd);
    info->pipe_fd = 0;
    return true;
}

DR_EXPORT
bool
dr_inject_process_run(void *data)
{
    dr_inject_info_t *info = (dr_inject_info_t *) data;
    /* Close the pipe without writing anything.  This will cause the app to run
     * natively.
     */
    close(info->pipe_fd);
    info->pipe_fd = 0;
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
