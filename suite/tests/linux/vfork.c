/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2003 VMware, Inc.  All rights reserved.
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
 * test of fork
 */

#include "tools.h"
#ifdef LINUX
#    include "thread_clone.h"
#endif

#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h> /* for wait and mmap */
#include <sys/wait.h>  /* for wait */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/***************************************************************************/

static bool verbose = false;

static void
do_execve(const char *path)
{
    int result;
    const char *arg[3];
    char **env = NULL;
    arg[0] = path;
    arg[1] = "/fake/path/it_worked";
    arg[2] = NULL;
    if (find_dynamo_library())
        print("child is running under DynamoRIO\n");
    else
        print("child is running natively\n");
    /* test i#237 resource cleanup by invoking execve */
    result = execve(path, (char **)arg, env);
    if (result < 0)
        perror("ERROR in execve");
}

static int
run_child(void *arg)
{
    /* i#500: Avoid libc in the child. */
    nolibc_print("child thread running\n");
    return 0;
}

int
main(int argc, char **argv)
{
    pid_t child;
    void *stack;

    if (argc < 2)
        return 1;
    if (find_dynamo_library())
        print("parent is running under DynamoRIO\n");
    else
        print("parent is running natively\n");

    print("trying vfork() #1\n");
    child = vfork();
    if (child < 0) {
        perror("ERROR on fork");
    } else if (child > 0) {
        pid_t result;
        if (verbose)
            print("parent waiting for child\n");
        result = waitpid(child, NULL, 0);
        assert(result == child);
        print("child has exited\n");
    } else {
        do_execve(argv[1]);
    }

    /* do 2 in a row to test i#237/PR 498284 */
    print("trying vfork() #2\n");
    child = vfork();
    if (child < 0) {
        perror("ERROR on fork");
    } else if (child > 0) {
        pid_t result;
        int err;
        if (verbose)
            print("parent waiting for child\n");
        /* linux kernel will (incorrectly) return ECHILD sometimes
         * if the child has already exited
         */
        result = waitpid(child, NULL, 0);
        err = errno;
        assert(result == child || (result == -1 && err == ECHILD));
        print("child has exited\n");
    } else {
        do_execve(argv[1]);
    }

    /* i#1010: clone() after vfork reuses our private fds.  Have to run this
     * manually with -loglevel N to trigger this.
     */
    print("trying clone() after vfork()\n");
#ifdef LINUX
    stack = NULL;
    child = create_thread(run_child, NULL, &stack);
    if (child < 0) {
        perror("ERROR on create_thread");
    }
    delete_thread(child, stack);
#else
    print("child thread running"); /* match output */
#endif
    print("child has exited\n");

    return 0;
}
