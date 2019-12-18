/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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
 * test exiting one thread group while another remains
 */

#include <unistd.h>
#include <sys/types.h>   /* for wait and mmap */
#include <sys/wait.h>    /* for wait */
#include <linux/sched.h> /* for clone and CLONE_ flags */
#include <time.h>        /* for nanosleep */
#include <sys/mman.h>    /* for mmap */
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/syscall.h> /* for SYS_* */

#include "tools.h" /* for nolibc_* wrappers. */

/* i#762: Hard to get clone() from sched.h, so copy prototype. */
extern int
clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg, ...);

#define THREAD_STACK_SIZE (32 * 1024)

#define NUM_THREADS 8

/* forward declarations */
static pid_t
create_thread(int (*fcn)(void *), void *arg, void **stack, bool same_group);
static void
delete_thread(pid_t pid, void *stack);
int
run(void *arg);
static void *
stack_alloc(int size);
static void
stack_free(void *p, int size);

/* vars for child thread group */
static pid_t child[NUM_THREADS];
static void *stack[NUM_THREADS];

/* these are used solely to provide deterministic output */
/* this is read by child, written by parent, tells child whether to exit */
static volatile bool child_exit[NUM_THREADS];
/* this is read by parent, written by child, tells parent whether child started */
static volatile bool child_started[NUM_THREADS];
/* this is read by parent, written by child, tells parent whether child done */
static volatile bool child_done[NUM_THREADS];

static struct timespec sleeptime;

int
main()
{
    int i;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 10 * 1000 * 1000; /* 10ms */

    /* parent remains in own group. creates child who creates a thread group
     * and then exits them all
     */
    for (i = 0; i < NUM_THREADS; i++) {
        child_started[i] = false;
        child_exit[i] = false;
    }
    child[0] = create_thread(run, (void *)(long)0, &stack[0], false);
    assert(child[0] > -1);

    /* wait for child to start rest of threads */
    for (i = 0; i < NUM_THREADS; i++) {
        while (!child_started[i]) {
            /* waste some time: FIXME: should use futex */
            nanosleep(&sleeptime, NULL);
        }
    }

    child_exit[0] = true;
    while (!child_done[0])
        nanosleep(&sleeptime, NULL);
    delete_thread(child[0], stack[0]);
    for (i = 1; i < NUM_THREADS; i++)
        stack_free(stack[i], THREAD_STACK_SIZE);
}

/* Procedure executed by sideline threads
 * XXX i#500: Cannot use libc routines (printf) in the child process.
 */
int
run(void *arg)
{
    int threadnum = (int)(long)arg;
    int i = 0;
    /* for CLONE_CHILD_CLEARTID for signaling parent.  if we used raw
     * clone system call we could get kernel to do this for us.
     */
    child[threadnum] = dynamorio_syscall(SYS_gettid, 0);
    dynamorio_syscall(SYS_set_tid_address, 1, &child[threadnum]);

    if (threadnum == 0) {
        /* first child creates rest of group */
        int j;
        for (j = 1; j < NUM_THREADS; j++) {
            child[j] = create_thread(run, (void *)(long)j, &stack[j], true);
            if (child[j] < 0)
                nolibc_print("failed to create child thread\n");
        }
    }

    child_started[threadnum] = true;
    nolibc_print("Sideline thread started\n");

    while (true) {
        /* do nothing for now */
        i++;
        if (i % 25000000 == 0)
            break;
    }
    while (!child_exit[threadnum])
        nolibc_nanosleep(&sleeptime);
    nolibc_print("Sideline thread finished, exiting whole group\n");
    child_done[threadnum] = true;
    /* We deliberately bring down the whole group.  Note that this is
     * the default on x64 on returning for some reason which seems
     * like a bug in _clone() (xref i#94).
     */
    dynamorio_syscall(SYS_exit_group, 1, 0);
    return 0;
}

/* Create a new thread. It should be passed "fcn", a function which
 * takes two arguments, (the second one is a dummy, always 4). The
 * first argument is passed in "arg". Returns the TID of the new
 * thread */
static pid_t
create_thread(int (*fcn)(void *), void *arg, void **stack, bool same_group)
{
    pid_t newpid;
    int flags;
    void *my_stack;

    my_stack = stack_alloc(THREAD_STACK_SIZE);
    /* need SIGCHLD so parent will get that signal when child dies,
     * else have errors doing a wait */
    flags = SIGCHLD | CLONE_VM |
        /* CLONE_THREAD => no signal to parent on termination; have to use
         * CLONE_CHILD_CLEARTID to get that.  Since we're using library call
         * instead of raw system call we don't have child_tidptr argument,
         * so we set the location in the child itself via set_tid_address(). */
        CLONE_CHILD_CLEARTID | CLONE_FS | CLONE_FILES | CLONE_SIGHAND;
    if (same_group)
        flags |= CLONE_THREAD;
    /* XXX: Using libc clone in the child here really worries me, but it seems
     * to work.  My theory is that the parent has to call clone, which invokes
     * the loader to fill in the PLT entry, so when the child calls clone it
     * doesn't go into the loader and avoiding the races like we saw in i#500.
     */
    newpid = clone(fcn, my_stack, flags, arg);
    /* this is really a tid if we passed CLONE_THREAD: child has same pid as us */

    if (newpid == -1) {
        nolibc_print("smp.c: Error calling clone\n");
        stack_free(my_stack, THREAD_STACK_SIZE);
        return -1;
    }

    *stack = my_stack;
    return newpid;
}

static void
delete_thread(pid_t pid, void *stack)
{
    pid_t result;
    /* do not print out pids to make diff easy */
    nolibc_print("Waiting for child to exit\n");
    result = waitpid(pid, NULL, 0);
    nolibc_print("Child has exited\n");
    if (result == -1 || result != pid) {
        /* somehow getting errors: ignoring for now since works */
#if VERBOSE
        perror("delete_thread waitpid");
#endif
    }
    stack_free(stack, THREAD_STACK_SIZE);
}

/* allocate stack storage on the app's heap */
static void *
stack_alloc(int size)
{
    size_t sp;
    void *q = NULL;
    void *p;

#if STACK_OVERFLOW_PROTECT
    /* allocate an extra page and mark it non-accessible to trap stack overflow */
    q = nolibc_mmap(0, PAGE_SIZE, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (q == NULL || q == MAP_FAILED)
        nolibc_print("mmap failed\n");
    stack_redzone_start = (size_t)q;
#endif

    p = nolibc_mmap(q, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (p == NULL || p == MAP_FAILED)
        nolibc_print("mmap failed\n");
#ifdef DEBUG
    nolibc_memset(p, 0xab, size);
#endif

    /* stack grows from high to low addresses, so return a ptr to the top of the
       allocated region */
    sp = (size_t)p + size;

    return (void *)sp;
}

/* free memory-mapped stack storage */
static void
stack_free(void *p, int size)
{
    size_t sp = (size_t)p - size;

#ifdef DEBUG
    nolibc_memset((void *)sp, 0xcd, size);
#endif
    nolibc_munmap((void *)sp, size);

#if STACK_OVERFLOW_PROTECT
    sp = sp - PAGE_SIZE;
    nolibc_munmap((void *)sp, PAGE_SIZE);
#endif
}
