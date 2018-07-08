/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2004-2007 VMware, Inc.  All rights reserved.
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

#ifndef THREAD_CLONE_H
#define THREAD_CLONE_H

#ifndef LINUX
#    error Only LINUX is supported
#endif

#define WINAPI

#include <sys/types.h>   /* for wait and mmap */
#include <sys/wait.h>    /* for wait */
#include <linux/sched.h> /* for clone and CLONE_ flags */
#include <sys/mman.h>    /* for mmap */

/* i#762: Hard to get clone() from sched.h, so copy prototype. */
extern int
clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg, ...);

typedef pid_t thread_t;

#define THREAD_STACK_SIZE (32 * 1024)

/* allocate stack storage on the app's heap */
static void *
stack_alloc(int size)
{
    void *q, *p;

#if STACK_OVERFLOW_PROTECT
    /* allocate an extra page and mark it non-accessible to trap stack overflow */
    q = mmap(0, PAGE_SIZE, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(q);
    stack_redzone_start = (size_t)q;
#endif

    p = mmap(q, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(p);
    /* stack grows from high to low addresses, so return a ptr to the top of the
       allocated region */
    return (void *)((size_t)p + size);
}

/* free memory-mapped stack storage */
static void
stack_free(void *p, int size)
{
    void *sp = (void *)((size_t)p - size);
    munmap(sp, size);
#if STACK_OVERFLOW_PROTECT
    sp = sp - PAGE_SIZE;
    munmap(sp, PAGE_SIZE);
#endif
}

/* Create a new thread. It should be passed "run_func", a function which
 * takes one argument ("arg"), for the thread to execute.
 * If *stack == NULL a new stack is allocated.
 * Returns the tid of the new thread.
 */
static thread_t
create_thread(int (*run_func)(void *), void *arg, void **stack)
{
    thread_t newpid;
    int flags;
    void *my_stack;

    if (*stack == NULL)
        my_stack = stack_alloc(THREAD_STACK_SIZE);
    else
        my_stack = *stack;
    /* need SIGCHLD so parent will get that signal when child dies,
     * else have errors doing a wait */
    flags = SIGCHLD | CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND;
    newpid = clone(run_func, my_stack, flags, arg);

    if (newpid == -1) {
        print("smp.c: Error calling clone\n");
        stack_free(my_stack, THREAD_STACK_SIZE);
        return -1;
    }

    if (*stack == NULL)
        *stack = my_stack;
    assert(newpid > -1);
    return newpid;
}

static void
delete_thread(thread_t pid, void *stack)
{
    thread_t result;
    /* do not print out pids to make diff easy */
    VERBOSE_PRINT("Waiting for child to exit\n");
    result = waitpid(pid, NULL, 0);
    VERBOSE_PRINT("Child has exited\n");
    if (result == -1 || result != pid)
        perror("delete_thread waitpid");
    stack_free(stack, THREAD_STACK_SIZE);
}

#endif /* THREAD_CLONE_H */
