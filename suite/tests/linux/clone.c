/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

#include <sys/types.h>   /* for wait, mmap and ulong */
#include <sys/wait.h>    /* for wait */
#include <sys/syscall.h> /* for SYS_clone3 */
#include <linux/sched.h> /* for CLONE_ flags */
#include <time.h>        /* for nanosleep */
#include <sys/mman.h>    /* for mmap */
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "tools.h" /* for nolibc_* wrappers. */

#ifdef ANDROID
typedef unsigned long ulong;
#endif

/* The first published clone_args had all fields till 'tls'. A clone3
 * syscall made by the user must have a struct of at least this size.
 */
#define CLONE_ARGS_SIZE_MIN_POSSIBLE 64

/* We define this constant so that we can try to make the clone3
 * syscall on systems where it is not available, to verify that it
 * returns an expected response.
 */
#define CLONE3_SYSCALL_NUM 435

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
static int
make_clone3_syscall(void *clone_args, ulong clone_args_size, void (*fcn)(void));
static pid_t
create_thread(int (*fcn)(void *), void *arg, void **stack, bool share_sighand,
              bool clone_vm);
#ifdef SYS_clone3
static pid_t
create_thread_clone3(void (*fcn)(void), void **stack, bool share_sighand, bool clone_vm);
#endif
static void
delete_thread(pid_t pid, void *stack);
int
run(void *arg);
void
run_with_exit(void);
static void *
stack_alloc(int size);
static void
stack_free(void *p, int size);

/* vars for child thread */
static pid_t child;
static void *stack;

void
test_thread(bool share_sighand, bool clone_vm, bool use_clone3)
{
    if (use_clone3) {
#ifdef SYS_clone3
        child = create_thread_clone3(run_with_exit, &stack, share_sighand, clone_vm);
#else
        /* If SYS_clone3 is not defined, we simply use SYS_clone instead, so that
         * the expected output is the same in both cases.
         */
        child = create_thread(run, NULL, &stack, share_sighand, clone_vm);
#endif
    } else
        child = create_thread(run, NULL, &stack, share_sighand, clone_vm);
    assert(child > -1);
    delete_thread(child, stack);
}

int
main()
{
    /* First test a thread that does not share signal handlers
     * (xref i#2089).
     */
    test_thread(false /*share_sighand*/, false /*clone_vm*/, false /*use_clone3*/);
    test_thread(false /*share_sighand*/, false /*clone_vm*/, true /*use_clone3*/);

    /* Now test a thread that does not share signal handlers, but is cloned. */
    test_thread(false /*share_sighand*/, true /*clone_vm*/, false /*use_clone3*/);
    test_thread(false /*share_sighand*/, true /*clone_vm*/, true /*use_clone3*/);

    /* Now make a thread that shares signal handlers, which also requires it to
     * be cloned.
     */
    test_thread(true /*share_sighand*/, true /*clone_vm*/, false /*use_clone3*/);
    test_thread(true /*share_sighand*/, true /*clone_vm*/, true /*use_clone3*/);

    /* Try using clone3 when it is possibly not defined. */
    int ret_failure_clone3 = make_clone3_syscall(NULL, 0, NULL);
    assert(ret_failure_clone3 == -1);
#ifdef SYS_clone3
    /* Though there's no guarantee, we assume that the kernel supports clone3 if
     * SYS_clone3 is defined.
     */
    assert(errno == EINVAL);
#else
    /* On some environments, we see that the kernel supports clone3 even though
     * SYS_clone3 is not defined by glibc.
     */
    assert(errno == ENOSYS || errno == EINVAL);
#endif
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
    nolibc_print("Sideline thread finished\n");
    return 0;
}

void
run_with_exit(void)
{
    exit(run(NULL));
}

void *p_tid, *c_tid;

/* Create a new thread. It should be passed "fcn", a function which
 * takes two arguments, (the second one is a dummy, always 4). The
 * first argument is passed in "arg". Returns the PID of the new
 * thread */
static pid_t
create_thread(int (*fcn)(void *), void *arg, void **stack, bool share_sighand,
              bool clone_vm)
{
    /* !clone_vm && share_sighand is not supported. */
    assert(clone_vm || !share_sighand);
    pid_t newpid;
    int flags;
    void *my_stack;

    my_stack = stack_alloc(THREAD_STACK_SIZE);

    /* Need SIGCHLD so parent will get that signal when child dies,
     * else have errors doing a wait */
    /* We're not doing CLONE_THREAD => child has its own pid
     * (the thread.c test tests CLONE_THREAD)
     */
    flags = (SIGCHLD | CLONE_FS | CLONE_FILES | (share_sighand ? CLONE_SIGHAND : 0) |
             (clone_vm ? CLONE_VM : 0));
    /* The stack arg should point to the stack's highest address (non-inclusive). */
    newpid = clone(fcn, (void *)((size_t)my_stack + THREAD_STACK_SIZE), flags, arg,
                   &p_tid, NULL, &c_tid);

    if (newpid == -1) {
        perror("Error calling clone\n");
        stack_free(my_stack, THREAD_STACK_SIZE);
        return -1;
    }

    *stack = my_stack;
    return newpid;
}

/* glibc does not provide a wrapper for clone3 yet. This makes it difficult
 * to create new threads in C code using syscall(), as we have to deal with
 * complexities associated with the child thread having a fresh stack
 * without any return addresses or space for local variables. So, we
 * create our own wrapper for clone3.
 * Currently, this supports a fcn that does not return and calls exit() on
 * its own.
 */
static int
make_clone3_syscall(void *clone_args, ulong clone_args_size, void (*fcn)(void))
{
#ifdef SYS_clone3
    assert(CLONE3_SYSCALL_NUM == SYS_clone3);
#endif
    long result;
#ifdef X86
#    ifdef X64
    asm volatile("mov %[sys_clone3], %%rax\n\t"
                 "mov %[clone_args], %%rdi\n\t"
                 "mov %[clone_args_size], %%rsi\n\t"
                 "mov %[fcn], %%rdx\n\t"
                 "syscall\n\t"
                 "test %%rax, %%rax\n\t"
                 "jnz parent\n\t"
                 "call *%%rdx\n\t"
                 "parent:\n\t"
                 "mov %%rax, %[result]\n\t"
                 : [ result ] "=m"(result)
                 : [ sys_clone3 ] "i"(CLONE3_SYSCALL_NUM), [ clone_args ] "m"(clone_args),
                   [ clone_args_size ] "m"(clone_args_size), [ fcn ] "m"(fcn)
                 /* syscall clobbers rcx and r11 */
                 : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory");
#    else
    asm volatile("mov %[sys_clone3], %%eax\n\t"
                 "mov %[clone_args], %%ebx\n\t"
                 "mov %[clone_args_size], %%ecx\n\t"
                 "mov %[fcn], %%edx\n\t"
                 "int $0x80\n\t"
                 "test %%eax, %%eax\n\t"
                 "jnz parent\n\t"
                 "call *%%edx\n\t"
                 "parent:\n\t"
                 "mov %%eax, %[result]\n\t"
                 : [ result ] "=m"(result)
                 : [ sys_clone3 ] "i"(CLONE3_SYSCALL_NUM), [ clone_args ] "m"(clone_args),
                   [ clone_args_size ] "m"(clone_args_size), [ fcn ] "m"(fcn)
                 : "eax", "ebx", "ecx", "edx", "memory");
#    endif
#elif defined(AARCH64)
    asm volatile("mov x8, #%[sys_clone3]\n\t"
                 "ldr x0, %[clone_args]\n\t"
                 "ldr x1, %[clone_args_size]\n\t"
                 "ldr x2, %[fcn]\n\t"
                 "svc #0\n\t"
                 "cbnz x0, parent\n\t"
                 "blr x2\n\t"
                 "parent:\n\t"
                 "str x0, %[result]\n\t"
                 : [ result ] "=m"(result)
                 : [ sys_clone3 ] "i"(CLONE3_SYSCALL_NUM), [ clone_args ] "m"(clone_args),
                   [ clone_args_size ] "m"(clone_args_size), [ fcn ] "m"(fcn)
                 : "x0", "x1", "x2", "x8", "memory");
#elif defined(ARM)
    /* XXX: Add asm wrapper for ARM.
     * Currently we do not run this test on ARM, so this missing support doesn't
     * cause any test failure.
     */
#else
#    error Unsupported architecture
#endif
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

#ifdef SYS_clone3
static pid_t
create_thread_clone3(void (*fcn)(void), void **stack, bool share_sighand, bool clone_vm)
{
    /* !clone_vm && share_sighand is not supported. */
    assert(clone_vm || !share_sighand);
    struct clone_args cl_args = { 0 };
    void *my_stack;
    my_stack = stack_alloc(THREAD_STACK_SIZE);
    /* We're not doing CLONE_THREAD => child has its own pid
     * (the thread.c test tests CLONE_THREAD)
     */
    cl_args.flags = CLONE_FS | CLONE_FILES | (share_sighand ? CLONE_SIGHAND : 0) |
        (clone_vm ? CLONE_VM : 0);
    /* Need SIGCHLD so parent will get that signal when child dies,
     * else have errors doing a wait */
    cl_args.exit_signal = SIGCHLD;
    cl_args.stack = (ptr_uint_t)my_stack;
    cl_args.stack_size = THREAD_STACK_SIZE;
    int ret = make_clone3_syscall(NULL, sizeof(struct clone_args), fcn);
    assert(errno == EFAULT);

    ret = make_clone3_syscall((void *)0x123 /* bogus address */,
                              sizeof(struct clone_args), fcn);
    assert(errno == EFAULT);

    ret = make_clone3_syscall(&cl_args, CLONE_ARGS_SIZE_MIN_POSSIBLE - 1, fcn);
    assert(errno == EINVAL);

    ret = make_clone3_syscall(&cl_args, sizeof(struct clone_args), fcn);
    /* Child threads should already have been directed to fcn. */
    assert(ret != 0);
    if (ret == -1) {
        perror("Error calling clone\n");
        stack_free(my_stack, THREAD_STACK_SIZE);
        return -1;
    } else {
        assert(ret > 0);
        /* Ensure that DR restores fields in clone_args after the syscall. */
        assert(cl_args.stack == (ptr_uint_t)my_stack &&
               cl_args.stack_size == THREAD_STACK_SIZE);
    }
    *stack = my_stack;
    return (pid_t)ret;
}
#endif

static void
delete_thread(pid_t pid, void *stack)
{
    pid_t result;
    /* do not print out pids to make diff easy */
    result = waitpid(pid, NULL, 0);
    print("Child has exited\n");
    if (result == -1 || result != pid)
        perror("delete_thread waitpid");
    stack_free(stack, THREAD_STACK_SIZE);
}

/* Allocate stack storage on the app's heap. Returns the lowest address of the
 * stack (inclusive).
 */
void *
stack_alloc(int size)
{
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
    return p;
}

/* free memory-mapped stack storage */
void
stack_free(void *p, int size)
{
#ifdef DEBUG
    memset(p, 0xcd, size);
#endif
    munmap(p, size);

#if STACK_OVERFLOW_PROTECT
    munmap((void *)((size_t)p - PAGE_SIZE), PAGE_SIZE);
#endif
}
