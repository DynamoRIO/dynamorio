/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h> /* for wait and mmap */
#include <sys/wait.h>  /* for wait */
#include <assert.h>
#include <stdio.h>

const int N = 8;

/***************************************************************************/

int
fib(int n)
{
    if (n <= 1)
        return 1;
    return fib(n - 1) + fib(n - 2);
}

#ifdef DEBUG
#    define pf printf
#else
#    define pf \
        if (0) \
        printf
#endif

int
main(int argc, char **argv)
{
    pid_t child1, child2;
    int do_vfork = 1; // argc == 1;

    int n, sum = 0;
    int result;
    char *arg[5];
    char **env = NULL;
    char carg[10], csum[10];

    if (find_dynamo_library())
        printf("rio\n");
    else
        printf("native\n");

    // printf("%d %s %s %s\n", argc, argv[0], argv[1], argv[2]);
    if (argc < 3) {    // start calculation
        if (2 == argc) // vfork-fib 10
            n = atoi(argv[1]);
        else
            n = N;

        printf("parent fib(%d)=%d\n", n, fib(n));
        sum = 0;
    } else {
        assert(argc == 3); // vfork-fib fib 10
        n = atoi(argv[2]);
        sum = 0;
    }

    pf("\tfib %d\n", n);

    if (n <= 1) { // base case
        pf("base case\n");
        _exit(1);
    }

    // now spawn two children
    arg[0] = argv[0];
    arg[1] = "fib";
    arg[3] = NULL;

    if (do_vfork) { /* default */
        pf("using vfork()\n");
        child1 = vfork();
    } else {
        pf("using fork()\n");
        child1 = fork();
    }

    if (child1 < 0) {
        perror("ERROR on fork");
    } else if (child1 == 0) {
        snprintf(carg, 10, "%d", n - 2);
        arg[2] = carg;

#if 0
        pf("execing %d %s %s=%s %s\n",
               3, arg[0], carg, arg[1], arg[2]);
#endif
        result = execve(arg[0], arg, env);
        if (result < 0)
            perror("ERROR in execve");
    } else {
        pid_t result;
        int status;
        int children = 2;

        if (do_vfork) { /* default */
            pf("second child using vfork()\n");
            child2 = vfork();
        } else {
            pf("second child using fork()\n");
            child2 = fork();
        }
        if (child2 < 0) {
            perror("ERROR on fork");
        } else if (child2 == 0) {
            snprintf(carg, 10, "%d", n - 1);
            arg[2] = carg;
            result = execve(arg[0], arg, env);
            if (result < 0)
                perror("ERROR in execve");
        }

        while (children > 0) {
            pf("parent waiting for %d children\n", children);
            result = wait(&status);

            assert(result == child2 || result == child1);
            assert(WIFEXITED(status));

            sum += WEXITSTATUS(status);

            if (children == 2 && result == child1)
                pf("first child before second\n");
            else
                pf("second child before first\n");

            children--;
        }

#if 0
        result = waitpid(child2, &status, 0);
        assert(result == child2);
        assert(WIFEXITED(status));
        pf("child2 has exited with status=%d %d\n", status, WEXITSTATUS(status));
        sum+=WEXITSTATUS(status);

        pf("parent waiting for child1 %d\n", child1);
        result = waitpid(child1, &status, 0);
        assert(result == child1);
        assert(WIFEXITED(status));
        pf("child1 has exited with status=%d %d\n", status, WEXITSTATUS(status));
        sum+=WEXITSTATUS(status);
#endif
    }

#ifdef DEBUG
    printf("\tfib(%d)=%d [%d] %s\n", n, sum, fib(n), sum == fib(n) ? "OK" : "BAD");
#else
    if (argc == 1)
        printf("\tfib(%d)=%d [%d] %s\n", n, sum, fib(n), sum == fib(n) ? "OK" : "BAD");
#endif
    _exit(sum);
}

// TODO it would be nice to have in the tests a measure for
// nondeterminism to also a guarantee that we don't introduce extra
// synchronization to stifle any parallelism
