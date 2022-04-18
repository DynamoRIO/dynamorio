/* **********************************************************
 * Copyright (c) 2012-2015 Google, Inc.  All rights reserved.
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

#include "tools.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h> /* for fork */

/* FIXME i#26: We can't reliably synch with threads that have just been created.
 * Raising NUM_FORK_THREADS above 1 means we spawn threads and fork at the same
 * time.  Raise NUM_FORK_THREADS when i#26 is fixed.
 */

#define NUM_MPROTECT_THREADS 2
#define NUM_FORK_THREADS 1
#define NUM_FORK_LEVELS 2
#define RET_OPCODE 0xc2

/* This global limits us to running one set of mprotect threads per process. */
volatile int keep_running;

static void
use_fork_and_threads(int level);

/* Flip some page protections.  Should trigger DR to acquire the vmareas lock a
 * fair amount.
 */
static void *
mprotect_thread(void *page)
{
    while (keep_running) {
        mprotect(page, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC);
        /* If we were adventurous, we'd execute some code off this RWX page to
         * really try and contend the vmareas lock.  However, Linux tends to
         * merge these pages into adjacent regions in /proc/pid/maps, which DR
         * doesn't like, so we don't execute from page.
         */
        mprotect(page, PAGE_SIZE, PROT_READ);
    }
    return NULL;
}

/* Forks a child and waits for it in the parent.  The child will recursively
 * spawn threads and processes until level hits 1.
 */
static void *
do_fork(int level)
{
    pid_t pid = fork();
    if (pid == 0) {
        fprintf(stderr, "child process\n");
        /* Make sure the child can also fork and make threads. */
        if (level > 1) {
            use_fork_and_threads(level - 1);
        }
        exit(0);
    } else {
        int status = -1;
        pid_t waited_pid;
        do {
            waited_pid = waitpid(pid, &status, 0);
        } while (waited_pid != pid);
        if (status != 0) {
            fprintf(stderr, "child %d exited non-zero: %x\n", pid, status);
            fprintf(stderr, "signalled: %d, signal: %d, exited: %d, exit: %d\n",
                    WIFSIGNALED(status), WTERMSIG(status), WIFEXITED(status),
                    WEXITSTATUS(status));
        }
    }
    return NULL;
}

static void
use_fork_and_threads(int level)
{
    void *pages[NUM_MPROTECT_THREADS];
    pthread_t threads[NUM_MPROTECT_THREADS];
    pthread_t fork_threads[NUM_FORK_THREADS];
    int i;

    /* Spawn a few threads that try to acquire DR's locks. */
    keep_running = 1;
    for (i = 0; i < NUM_MPROTECT_THREADS; i++) {
        pages[i] = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (pages[i] == MAP_FAILED) {
            fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        }
        pthread_create(&threads[i], NULL, mprotect_thread, pages[i]);
    }

    /* Spawn a few threads and have them fork concurrently. */
    for (i = 0; i < NUM_FORK_THREADS; i++) {
        pthread_create(&fork_threads[i], NULL, (void *(*)(void *))do_fork,
                       (void *)(long)level);
    }

    /* Wait for everything we spawned and clean up. */
    for (i = 0; i < NUM_FORK_THREADS; i++) {
        pthread_join(fork_threads[i], NULL);
    }
    keep_running = 0;
    for (i = 0; i < NUM_MPROTECT_THREADS; i++) {
        pthread_join(threads[i], NULL);
        munmap(pages[i], PAGE_SIZE);
    }
}

int
main(int argc, char **argv)
{
    use_fork_and_threads(NUM_FORK_LEVELS);

    fprintf(stderr, "all done\n");
    return 0;
}
