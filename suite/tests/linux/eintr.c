/* **********************************************************
 * Copyright (c) 2013-2017 Google, Inc.  All rights reserved.
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

/*
 * test of restarted syscalls
 */

#include "tools.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>

static pthread_cond_t condvar;
static bool child_ready;
static pthread_mutex_t lock;

static void
handler(int sig)
{
    print("in handler %d\n", sig);

    /* potentially unsafe but we risk it: we should be in our read syscall */
    pthread_mutex_lock(&lock);
    child_ready = true;
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&lock);
}

static void *
thread_routine(void *arg)
{
    int fd = (int)(long)arg;
    char buf[16];
    signal(SIGUSR1, handler);

    pthread_mutex_lock(&lock);
    child_ready = true;
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&lock);

    int res = read(fd, buf, 2);
    if (res < 0)
        perror("error during read");
    else
        print("got %d bytes == %d %d\n", res, buf[0], buf[1]);
    close(fd);
    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t thread;
    void *retval;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&condvar, NULL);

    if (pthread_create(&thread, NULL, thread_routine, (void *)(long)pipefd[0]) != 0) {
        perror("failed to create thread");
        exit(1);
    }

    pthread_mutex_lock(&lock);
    while (!child_ready)
        pthread_cond_wait(&condvar, &lock);
    child_ready = false;
    pthread_mutex_unlock(&lock);

    /* Impossible to have child notify us when inside SYS_read but it should get
     * there pretty quickly after it signals condvar.
     */

    /* test signal w/ no handler but default-ignore */
    print("sending SIGURG\n");
    pthread_kill(thread, SIGURG);

    /* separate the two signals a bit */
    sleep(1);

    /* test signal w/ handler */
    print("sending SIGUSR1\n");
    pthread_kill(thread, SIGUSR1);

    pthread_mutex_lock(&lock);
    while (!child_ready)
        pthread_cond_wait(&condvar, &lock);
    child_ready = false;
    pthread_mutex_unlock(&lock);

    write(pipefd[1], "ab", 2);

    if (pthread_join(thread, &retval) != 0)
        perror("failed to join thread");

    pthread_cond_destroy(&condvar);
    pthread_mutex_destroy(&lock);

    print("all done\n");
    close(pipefd[1]);

    return 0;
}
