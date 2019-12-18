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

/* Helper exe that execs an exe with fake argv[0].  There isn't a way to do
 * this from bash, so we need this helper.
 *
 * We use it for DR early injection by invoking it like so:
 *   drloader path/to/libdynamorio.so /path/to/app <args...>
 *
 * This way, the kernel puts the original path to the app on the stack as
 * argv[0], and we don't have to shuffle the strings around to fix up
 * /proc/pid/cmdline.
 *
 * XXX i#840: Eventually this should be folded into a drinject and drdeploy C
 * implementation for Linux.
 */

#include "configure.h"      /* for UNIX */
#include "globals_shared.h" /* for DYNAMORIO_VAR_EXE_PATH */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int
main(int argc, char **argv)
{
    int r;
    struct stat st;
    char *libdr_so;
    char *app;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <libdynamorio.so> <app path> [argv...]\n", argv[0]);
        return -1;
    }

    /* Check if DR and the app exist.  It's easier to give a good error message
     * if we do this up front, rather than waiting for DR or execve to fail.
     */
    libdr_so = argv[1];
    r = stat(libdr_so, &st);
    if (r < 0) {
        fprintf(stderr, "%s: can't stat %s\n", argv[0], libdr_so);
        perror(argv[0]);
        return -1;
    }

    /* Check if the app exists. */
    app = argv[2];
    r = stat(app, &st);
    if (r < 0) {
        fprintf(stderr, "%s: can't stat %s\n", argv[0], app);
        perror(argv[0]);
        return -1;
    }

    /* The execve syscall prototype is:
     *   int execve(char *filename, char *argv[], char *envp[]);
     * We pass libdr_so as the filename.  For argv, we pass the rest of the
     * command line starting with the app's path.
     */
    setenv(DYNAMORIO_VAR_EXE_PATH, app, true /*overwrite*/);
    r = execv(libdr_so, &argv[2]);
    /* Only returns on error. */
    fprintf(stderr, "%s: can't exec %s\n", argv[0], app);
    perror(argv[0]);
    return r;
}
