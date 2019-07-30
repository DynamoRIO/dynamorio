/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

#ifdef RSEQ_TEST_ATTACH
#    include "configure.h"
#    include "dr_api.h"
#endif
#include "tools.h"
#ifdef RSEQ_TEST_ATTACH
#    include "thread.h"
#    include "condvar.h"
#endif
#ifndef LINUX
#    error Only Linux is supported.
#endif
/* TODO i#2350: Port this to other platforms and bitwidths.  There is a lot of
 * assembly which makes that non-trivial work.
 */
#if !defined(X86) || !defined(X64)
#    error Only x86_64 is supported.
#endif
#include "../../core/unix/include/syscall.h"
#ifndef HAVE_RSEQ
#    error The linux/rseq header is required.
#endif
#include <linux/rseq.h>
#include <errno.h>
#include <assert.h>

#define EXPANDSTR(x) #x
#define STRINGIFY(x) EXPANDSTR(x)

#define RSEQ_SIG 0x90909090 /* nops to disasm nicely */

/* This cannot be a stack-local variable, as the kernel will force SIGSEGV on a syscall
 * if it can't read this struct.  And for multiple threads it should be in TLS.
 */
static __thread volatile struct rseq rseq_tls;

#ifdef RSEQ_TEST_ATTACH
static volatile int exit_requested;
static void *thread_ready;
#endif

int
test_rseq(void)
{
    /* We use static to avoid stack reference issues with our extra frame inside the asm.
     */
    static __u32 id = RSEQ_CPU_ID_UNINITIALIZED;
    static int restarts = 0;
    __asm__ __volatile__(
#ifdef RSEQ_TEST_USE_OLD_SECTION_NAME
        /* Add a table entry. */
        ".pushsection __rseq_table, \"aw\"\n\t"
#else
        ".pushsection __rseq_cs, \"aw\"\n\t"
#endif
        ".balign 32\n\t"
        "1:\n\t"
        ".long 0, 0\n\t"          /* version, flags */
        ".quad 2f, 3f-2f, 4f\n\t" /* start_ip, post_commit_offset, abort_ip */
        ".popsection\n\t"
#if !defined(RSEQ_TEST_USE_OLD_SECTION_NAME) && !defined(RSEQ_TEST_USE_NO_ARRAY)
        /* Add an array entry. */
        ".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"
        ".quad 1b\n\t"
        ".popsection\n\t"
#endif

        /* Although our abort handler has to handle being called (that's all DR
         * supports), we structure the code to allow directly calling past it, to
         * count restart_count.
         */
        "call 6f\n\t"
        "jmp 5f\n\t"

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq 1b(%%rip), %%rax\n\t"
        "movq %%rax, %0\n\t"
        /* Test "falling into" the rseq region. */

        /* Restartable sequence.
         * If I pause in gdb in here, often the thread is migrated and the abort
         * handler invoked: a simple way to test a restart natively.
         */
        "2:\n\t"
        "mov %3, %%rax\n\t"
        "mov %%rax, %1\n\t"

        /* Post-commit. */
        "3:\n\t"
        "ret\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "addl $1, %2\n\t"
        "jmp 6b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "leaq 1b(%%rip), %%rax\n\t"
        "movq $0, %0\n\t"
        /* clang-format on */

        : "=m"(rseq_tls.rseq_cs), "=m"(id), "=m"(restarts)
        : "m"(rseq_tls.cpu_id)
        : "rax", "memory");
    assert(id != RSEQ_CPU_ID_UNINITIALIZED);
    return restarts;
}

#ifdef RSEQ_TEST_ATTACH
void *
rseq_thread_loop(void *arg)
{
    /* We don't try to signal inside the rseq code.  Just having the thread scheduled
     * in this function is close enough: the test already has non-determinism.
     */
    signal_cond_var(thread_ready);
    rseq_tls.cpu_id = RSEQ_CPU_ID_UNINITIALIZED;
    int res = syscall(SYS_rseq, &rseq_tls, sizeof(rseq_tls), 0, RSEQ_SIG);
    if (res != 0)
        return NULL;
    static int zero;
    __asm__ __volatile__(
        /* Add a table entry. */
        ".pushsection __rseq_cs, \"aw\"\n\t"
        ".balign 32\n\t"
        "1:\n\t"
        ".long 0, 0\n\t"          /* version, flags */
        ".quad 2f, 3f-2f, 4f\n\t" /* start_ip, post_commit_offset, abort_ip */
        ".popsection\n\t"
        /* Add an array entry. */
        ".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"
        ".quad 1b\n\t"
        ".popsection\n\t"

        /* Although our abort handler has to handle being called (that's all DR
         * supports), we structure the code to allow directly calling past it, to
         * count restart_count.
         */
        "call 6f\n\t"
        "jmp 5f\n\t"

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq 1b(%%rip), %%rax\n\t"
        "movq %%rax, %0\n\t"
        /* Test "falling into" the rseq region. */

        /* Restartable sequence.  We loop to ensure we're in the region on
         * detach.  If DR fails to translate this thread to the abort handler
         * on detach, it will loop forever and the test will timeout and fail.
         * Note that this breaks DR's assumptions: the instrumented run
         * never exits the loop, and thus never reaches the "commit point" of the
         * nop, and thus never invokes the handler natively.  However, we don't
         * care: we just want to test detach.
         */
        "2:\n\t"
        /* I was going to assert that zero==0 at the end, but that requires more
         * synch to not reach here natively before DR attaches.  Decided against it.
         */
        "movl $1, %1\n\t"
        "jmp 2b\n\t"
        /* We can't end the sequence in a branch (DR can't handle it). */
        "nop\n\t"

        /* Post-commit. */
        "3:\n\t"
        "ret\n\t"

        /* Abort handler: if we're done, exit; else, re-enter. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "mov %2, %%rax\n\t"
        "cmp $0, %%rax\n\t"
        "jne 3b\n\t"
        "jmp 6b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "leaq 1b(%%rip), %%rax\n\t"
        "movq $0, %0\n\t"
        /* clang-format on */

        : "=m"(rseq_tls.rseq_cs), "=m"(zero)
        : "m"(exit_requested)
        : "rax", "memory");
    return NULL;
}
#endif /* RSEQ_TEST_ATTACH */

int
main()
{
    int restart_count = 0;
    rseq_tls.cpu_id = RSEQ_CPU_ID_UNINITIALIZED;
    int res = syscall(SYS_rseq, &rseq_tls, sizeof(rseq_tls), 0, RSEQ_SIG);
    if (res == 0) {
#ifdef RSEQ_TEST_ATTACH
        /* Create a thread that sits in the rseq region, to test attaching and detaching
         * from inside the region.
         */
        thread_ready = create_cond_var();
        thread_t mythread = create_thread(rseq_thread_loop, NULL);
        wait_cond_var(thread_ready);
        dr_app_setup_and_start();
#endif
        restart_count = test_rseq();
#ifdef RSEQ_TEST_ATTACH
        /* Detach while the thread is in its rseq region loop. */
        exit_requested = 1; /* atomic on x86; ARM will need more. */
        dr_app_stop_and_cleanup();
        join_thread(mythread);
        destroy_cond_var(thread_ready);
#endif
    } else {
        /* Linux kernel 4.18+ is required. */
        assert(errno == ENOSYS);
        /* Make the test pass. */
        restart_count = 1;
    }
    /* We expect 0 restart_count natively (ok, tiny chance of >0), and 1 under DR. */
    print("Saw %s restarts\n", restart_count > 0 ? "some" : "no");
    print("All done\n");
    return 0;
}
