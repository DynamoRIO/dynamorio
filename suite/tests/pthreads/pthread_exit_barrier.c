/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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

/* Like pthreads_exit.c, this stresses exiting, but uses a barrier to ensure
 * contention on the initstack_mutex in cleanup_and_terminate.
 * This is based on the reproducer at
 * https://github.com/DynamoRIO/dynamorio/issues/7939#issuecomment-4686861688.
 */

#include <pthread.h>
#include <stdio.h>

/* We use a barrier to exit in lockstep. */
static pthread_barrier_t barrier;

static void *
thread_func(void *arg)
{
    pthread_barrier_wait(&barrier);
    return arg;
}

int
main(void)
{
#define NUM_THREADS 16
#ifdef REDUCED_ITERS
    /* The test is too slow in debug build for 1000 iterations. Debug build doesn't
     * reproduce the i#7939 crash in any case.
     */
    const int NUM_RUNS = 50;
#else
    const int NUM_RUNS = 1000;
#endif
    for (int run = 0; run < NUM_RUNS; run++) {
        pthread_t thread[NUM_THREADS];
        pthread_barrier_init(&barrier, NULL, NUM_THREADS);
        for (int i = 0; i < NUM_THREADS; i++)
            pthread_create(&thread[i], NULL, thread_func, NULL);
        for (int i = 0; i < NUM_THREADS; i++)
            pthread_join(thread[i], NULL);
        pthread_barrier_destroy(&barrier);
    }
    fprintf(stderr, "all done\n");
    return 0;
}
