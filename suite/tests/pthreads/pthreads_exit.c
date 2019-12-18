/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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

/* similar to pthreads.c but starts 10 threads and then the main thread
 * exits while the others are still running: a good test of races
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

volatile double pi = 0.0;  /* Approximation to pi (shared) */
pthread_mutex_t pi_lock;   /* Lock for above */
volatile double intervals; /* How many intervals? */

void *
process(void *arg)
{
    register double width, localsum;
    register int i;
    register int iproc;

#if VERBOSE
    fprintf(stderr, "\tthread %d starting\n", id);
#endif
    iproc = (int)(long)arg;

    /* Set width */
    width = 1.0 / intervals;

    /* Do the local computations */
    localsum = 0;
    for (i = iproc; i < intervals; i += 2) {
        register double x = (i + 0.5) * width;
        localsum += 4.0 / (1.0 + x * x);
        /* Make a system call to trigger DR operations that might
         * crash in a race (PR 470957)
         */
        sigprocmask(SIG_BLOCK, NULL, NULL);
    }
    localsum *= width;

    /* Lock pi for update, update it, and unlock */
    pthread_mutex_lock(&pi_lock);
    pi += localsum;
    pthread_mutex_unlock(&pi_lock);

#if VERBOSE
    fprintf(stderr, "\tthread %d exiting\n", id);
#endif
    return (NULL);
}

#define NUM_THREADS 10

int
main(int argc, char **argv)
{
    pthread_t thread[NUM_THREADS];
    int i;

    /* now make a lot of threads and then just exit while they're still
     * running to test exit races (PR 470957)
     */
    intervals = 10000000;
    for (i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&thread[i], NULL, process, (void *)(long)i)) {
            fprintf(stderr, "%s: cannot make thread\n", argv[0]);
            exit(1);
        }
    }

    return 0;
}
