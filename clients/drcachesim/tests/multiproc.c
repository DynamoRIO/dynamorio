/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Multi-process test */

#include "tools.h"

#ifdef UNIX
#    include <unistd.h>
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <assert.h>
#    include <stdio.h>
#endif

#define LINE_SIZE 64
typedef struct {
    union {
        char fill[LINE_SIZE];
        int val;
    } u;
} line_t;

#define NUM_LINES 512 * 1024
static line_t many_lines[NUM_LINES];

static void
lots_of_misses(void)
{
    int i;
    for (i = 0; i < NUM_LINES; i++)
        many_lines[i].u.val = i;
}

static void
lots_of_hits(void)
{
    int i;
    for (i = 0; i < NUM_LINES; i++)
        many_lines[0].u.val = i;
}

int
main(int argc, char **argv)
{
#ifdef UNIX
    pid_t child;
    child = fork();
    if (child < 0) {
        perror("error on fork");
    } else if (child > 0) {
        /* parent process */
        pid_t result;
        lots_of_hits();
        result = waitpid(child, NULL, 0);
        assert(result == child);
    }
#else
    if (argc == 2) { /* user must pass path in */
        /* parent process */
        STARTUPINFO si = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION pi;
        if (!CreateProcess(argv[1], argv[1], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            assert(0);
        lots_of_hits();
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            assert(0);
    }
#endif
    else {
        /* child process */
        lots_of_misses();
        exit(0);
    }
    print("all done\n");
    return 0;
}
