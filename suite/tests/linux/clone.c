/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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
 * test of clone call
 */

#include <sys/types.h>   /* for wait and mmap */
#include <sys/wait.h>    /* for wait */
#include <linux/sched.h> /* for CLONE_ flags */
#include <time.h>        /* for nanosleep */
#include <sys/mman.h>    /* for mmap */
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "tools.h" /* for nolibc_* wrappers. */

/* i#762: Hard to get clone() from sched.h, so copy prototype. */
extern int
clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg, ...);

#define THREAD_STACK_SIZE (32 * 1024)

#ifdef X64
#    define APP_TLS_SEG "fs"
#else
#    define APP_TLS_SEG "gs"
#endif

/* forward declarations */
static pid_t
create_thread(int (*fcn)(void *), void *arg, void **stack, bool share_sighand);
static void
delete_thread(pid_t pid, void *stack);
int
run(void *arg);
static void *
stack_alloc(int size);
static void
stack_free(void *p, int size);

/* vars for child thread */
static pid_t child;
static void *stack;

/* these are used solely to provide deterministic output */
/* this is read by child, written by parent, tells child whether to exit */
static volatile bool child_exit;
/* this is read by parent, written by child, tells parent whether child done */
static volatile bool child_done;

static struct timespec sleeptime;

void
test_thread(bool share_sighand)
{
    /* First make a thread that shares signal handlers. */
    child_exit = false;
    child_done = false;
    child = create_thread(run, NULL, &stack, share_sighand);
    assert(child > -1);

    /* waste some time */
    nanosleep(&sleeptime, NULL);

    child_exit = true;
    /* we want deterministic printf ordering */
    while (!child_done)
        nanosleep(&sleeptime, NULL);
    delete_thread(child, stack);
}

int
main()
{
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 10 * 1000 * 1000; /* 10ms */

    /* First make a thread that shares signal handlers. */
    test_thread(true);

    /* Now test a thread that does not share (xref i#2089). */
    test_thread(false);
}

/* Procedure executed by sideline threads
 * XXX i#500: Cannot use libc routines (printf) in the child process.
 */
int
run(void *arg)
{
    int i = 0;
    nolibc_print("Sideline thread started\n");
    while (true) {
        /* do nothing for now */
        i++;
        if (i % 2500000 == 0) {
            nolibc_print("i = ");
            nolibc_print_int(i);
            nolibc_print("\n");
        }
        if (i % 25000000 == 0)
            break;
    }
    while (!child_exit)
        nolibc_nanosleep(&sleeptime);
    nolibc_print("Sideline thread finished\n");
    child_done = true;
    return 0;
}

void *p_tid, *c_tid;

/* Create a new thread. It should be passed "fcn", a function which
 * takes two arguments, (the second one is a dummy, always 4). The
 * first argument is passed in "arg". Returns the PID of the new
 * thread */
static pid_t
create_thread(int (*fcn)(void *), void *arg, void **stack, bool share_sighand)
{
    pid_t newpid;
    int flags;
    void *my_stack;

    my_stack = stack_alloc(THREAD_STACK_SIZE);

    /* need SIGCHLD so parent will get that signal when child dies,
     * else have errors doing a wait */
    /* we're not doing CLONE_THREAD => child has its own pid
     * (the thread.c test tests CLONE_THREAD)
     */
    flags = (SIGCHLD | CLONE_VM | CLONE_FS | CLONE_FILES |
             (share_sighand ? CLONE_SIGHAND : 0));
    newpid = clone(fcn, my_stack, flags, arg, &p_tid, NULL, &c_tid);

    if (newpid == -1) {
        print("smp.c: Error calling clone\n");
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
    print("Waiting for child to exit\n");
    result = waitpid(pid, NULL, 0);
    print("Child has exited\n");
    if (result == -1 || result != pid)
        perror("delete_thread waitpid");
    stack_free(stack, THREAD_STACK_SIZE);
}

/* allocate stack storage on the app's heap */
void *
stack_alloc(int size)
{
    size_t sp;
    void *q = NULL;
    void *p;

#if STACK_OVERFLOW_PROTECT
    /* allocate an extra page and mark it non-accessible to trap stack overflow */
    q = mmap(0, PAGE_SIZE, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(q);
    stack_redzone_start = (size_t)q;
#endif

    p = mmap(q, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(p);
#ifdef DEBUG
    memset(p, 0xab, size);
#endif

    /* stack grows from high to low addresses, so return a ptr to the top of the
       allocated region */
    sp = (size_t)p + size;

    return (void *)sp;
}

/* free memory-mapped stack storage */
void
stack_free(void *p, int size)
{
    size_t sp = (size_t)p - size;

#ifdef DEBUG
    memset((void *)sp, 0xcd, size);
#endif
    munmap((void *)sp, size);

#if STACK_OVERFLOW_PROTECT
    sp = sp - PAGE_SIZE;
    munmap((void *)sp, PAGE_SIZE);
#endif
}
