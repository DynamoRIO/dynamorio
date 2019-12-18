/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

#include "tools.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Tests i#907: /proc/self/exe transparency with early injection. */

int
main(int argc, char *argv[])
{
    /* Test reading the symlink via readlink */
    char proc[64];
    char buf[512];
    ssize_t res;
    snprintf(proc, BUFFER_SIZE_ELEMENTS(proc), "/proc/%d/exe", getpid());
    NULL_TERMINATE_BUFFER(proc);
    res = readlink(proc, buf, BUFFER_SIZE_ELEMENTS(buf));
    NULL_TERMINATE_BUFFER(buf);
    if (res > 0 && strrchr(buf, '/') != NULL)
        print("/proc/pid/exe points to %s\n", strrchr(buf, '/') + 1);
    else {
        perror("readlink failed");
        return 1;
    }
    /* XXX: another good test would be to make a thread and use /proc/tid/exe */

    /* Test executing the symlink via execve.
     * We invoked ourselves initially with an arg, to avoid repeated execs.
     */
    if (argc > 1) {
        pid_t child = fork();
        if (child < 0) {
            perror("fork failed");
        } else if (child > 0) {
            pid_t result = waitpid(child, NULL, 0);
            assert(result == child);
            print("child has exited\n");
        } else {
            const char *arg[2] = { proc, NULL };
            /* Update for child's pid */
            snprintf(proc, BUFFER_SIZE_ELEMENTS(proc), "/proc/%d/exe", getpid());
            NULL_TERMINATE_BUFFER(proc);
            res = execve(proc, (char **)arg, NULL /*env*/);
            if (res < 0)
                perror("execve failed");
        }
    }
    return 0;
}
