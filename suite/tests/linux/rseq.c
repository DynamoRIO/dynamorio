/* **********************************************************
 * Copyright (c) 2019-2020 Google, Inc.  All rights reserved.
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
#ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#endif
#include <sched.h>
#include <linux/rseq.h>
#include <errno.h>
#include <assert.h>

#define EXPANDSTR(x) #x
#define STRINGIFY(x) EXPANDSTR(x)

#define RSEQ_SIG 0x90909090 /* nops to disasm nicely */
#ifdef RSEQ_TEST_USE_OLD_SECTION_NAME
#    define RSEQ_SECTION_NAME "__rseq_table"
#else
#    define RSEQ_SECTION_NAME "__rseq_cs"
#endif

#define RSEQ_ADD_TABLE_ENTRY(name, start, end, abort)         \
    ".pushsection " RSEQ_SECTION_NAME ", \"aw\"\n\t"          \
    ".balign 32\n\t"                                          \
    "rseq_cs_" #name ":\n\t"                                  \
    ".long 0, 0\n\t" /* version, flags */                     \
    ".quad " #start ", " #end " - " #start ", " #abort "\n\t" \
    ".popsection\n\t" RSEQ_ADD_ARRAY_ENTRY(rseq_cs_##name)

#if !defined(RSEQ_TEST_USE_OLD_SECTION_NAME) && !defined(RSEQ_TEST_USE_NO_ARRAY)
#    define RSEQ_ADD_ARRAY_ENTRY(label)                \
        ".pushsection __rseq_cs_ptr_array, \"aw\"\n\t" \
        ".quad " #label "\n\t"                         \
        ".popsection\n\t"
#else
#    define RSEQ_ADD_ARRAY_ENTRY(label) /* Nothing. */
#endif

/* This cannot be a stack-local variable, as the kernel will force SIGSEGV
 * if it can't read this struct.  And for multiple threads it should be in TLS.
 */
static __thread volatile struct rseq rseq_tls;
/* Make it harder to find rseq_tls for DR's heuristic by adding more static TLS. */
static __thread volatile struct rseq fill_up_tls[128];

#ifdef RSEQ_TEST_ATTACH
static volatile int exit_requested;
static void *thread_ready;
#endif

static volatile int sigill_count;

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGILL)
        ++sigill_count;
}

static void
test_rseq_call_once(bool force_restart_in, int *completions_out, int *restarts_out)
{
    /* We use static to avoid stack reference issues with our extra frame inside the asm.
     */
    static __u32 id = RSEQ_CPU_ID_UNINITIALIZED;
    static int completions;
    static int restarts;
    static volatile int force_restart;
    completions = 0;
    restarts = 0;
    force_restart = force_restart_in;
    sigill_count = 0;
    __asm__ __volatile__(
        RSEQ_ADD_TABLE_ENTRY(simple, 2f, 3f, 4f)

        /* In the past DR only supported an rseq sequence structured as a call-return
         * with an abort handler that always restarted.  We keep that structure here
         * as a test of that pattern, though we now support other patterns.
         */
        "call 6f\n\t"
        "jmp 5f\n\t"

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_simple(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_tls]\n\t"
        /* Test a register input to the sequence. */
        "movl %[cpu_id], %%eax\n\t"
        /* Test "falling into" the rseq region. */

        /* Restartable sequence. */
        "2:\n\t"
        "movl %%eax, %[id]\n\t"
        /* Test clobbering an input register. */
        "movl %[cpu_id_uninit], %%eax\n\t"
        /* Test a restart in the middle of the sequence via ud2a SIGILL. */
        "cmpb $0, %[force_restart]\n\t"
        "jz 7f\n\t"
        /* For -test_mode trace_invariants: expect a signal after ud2a.
         * (An alternative is to add decoding to trace_invariants but with
         * i#2007 and other problems that liimts the test.)
         */
        "prefetcht2 1\n\t"
        "ud2a\n\t"
        "7:\n\t"
        "addl $1, %[completions]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "ret\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        /* Start with jmp to avoid trace_invariants assert on return to u2da. */
        "jmp 42f\n\t"
        "42:\n\t"
        "addl $1, %[restarts]\n\t"
        "movb $0, %[force_restart_write]\n\t"
        "jmp 6b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_tls]\n\t"
        /* clang-format on */

        : [rseq_tls] "=m"(rseq_tls.rseq_cs), [id] "=m"(id),
          [completions] "=m"(completions), [restarts] "=m"(restarts),
          [force_restart_write] "=m"(force_restart)
        : [cpu_id] "m"(rseq_tls.cpu_id), [cpu_id_uninit] "i"(RSEQ_CPU_ID_UNINITIALIZED),
          [force_restart] "m"(force_restart)
        : "rax", "memory");
    assert(id != RSEQ_CPU_ID_UNINITIALIZED);
    *completions_out = completions;
    *restarts_out = restarts;
}

static void
test_rseq_call(void)
{
    int completions, restarts;
    sigill_count = 0;
    test_rseq_call_once(false, &completions, &restarts);
    /* There *could* have been a migration restart. */
    assert(completions == 1 && sigill_count == 0);
    test_rseq_call_once(true, &completions, &restarts);
    assert(completions == 1 && restarts > 0 && sigill_count == 1);
}

static void
test_rseq_branches_once(bool force_restart, int *completions_out, int *restarts_out)
{
    __u32 id = RSEQ_CPU_ID_UNINITIALIZED;
    int completions = 0;
    int restarts = 0;
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(branches, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_branches(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_tls]\n\t"
        /* Test a register input to the sequence. */
        "movl %[cpu_id], %%eax\n\t"
        /* Test "falling into" the rseq region. */

        /* Restartable sequence.  We include control flow to test a
         * complex sequence with midpoint branches, but no exits.
         * TODO i#2350: Support for exits has not yet been added and
         * once finished separate tests will be added.
         */
        "2:\n\t"
        "movl %%eax, %[id]\n\t"
        "mov $0, %%rax\n\t"
        "cmp $0, %%rax\n\t"
        "je 11f\n\t"
        "mov $4, %%rcx\n\t"
        "11:\n\t"
        "cmp $1, %%rax\n\t"
        "je 12f\n\t"
        "cmp $2, %%rax\n\t"
        "je 13f\n\t"
        /* Test a restart via ud2a SIGILL. */
        "cmpb $0, %[force_restart]\n\t"
        "jz 7f\n\t"
        "prefetcht2 1\n\t" /* See above: annotation for trace_invariants. */
        "ud2a\n\t"
        "7:\n\t"
        "addl $1, %[completions]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "jmp 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        /* Start with jmp to avoid trace_invariants assert on return to u2da. */
        "jmp 42f\n\t"
        "42:\n\t"
        "addl $1, %[restarts]\n\t"
        "movb $0, %[force_restart_write]\n\t"
        "jmp 6b\n\t"

        /* Clear the ptr. */
        "13:\n\t"
        "12:\n\t"
        "5:\n\t"
        "movq $0, %[rseq_tls]\n\t"
        /* clang-format on */

        : [rseq_tls] "=m"(rseq_tls.rseq_cs), [id] "=m"(id),
          [completions] "=m"(completions), [restarts] "=m"(restarts),
          [force_restart_write] "=m"(force_restart)
        : [cpu_id] "m"(rseq_tls.cpu_id), [cpu_id_uninit] "i"(RSEQ_CPU_ID_UNINITIALIZED),
          [force_restart] "m"(force_restart)
        : "rax", "rcx", "rdx", "memory");
    assert(id != RSEQ_CPU_ID_UNINITIALIZED);
}

static void
test_rseq_branches(void)
{
    int completions, restarts;
    sigill_count = 0;
    test_rseq_branches_once(false, &completions, &restarts);
    /* There *could* have been a migration restart. */
    assert(completions == 1 && sigill_count == 0);
    test_rseq_branches_once(true, &completions, &restarts);
    assert(completions == 1 && restarts > 0 && sigill_count == 1);
}

/* Tests that DR handles a signal inside its native rseq copy.
 * Any synchronous signal is going to pretty much never happen for real, since
 * it would happen on the instrumentation execution and never make it to the
 * native run, but an asynchronous signal could arrive.
 * It's complicated to set up an asynchronous signal at the right spot, so
 * we cheat and take advantage of DR not restoring XMM state to have different
 * behavior in the two DR executions of the rseq code.
 */
static void
test_rseq_native_fault(void)
{
    int restarts = 0;
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(fault, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_fault(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_tls]\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "mov $1,%%rcx\n\t"
        "movq %%rcx, %%xmm1\n\t"

        /* Restartable sequence. */
        "2:\n\t"
        /* Increase xmm0 every time.  DR (currently) won't restore xmm inputs
         * to rseq sequences, nor does it detect that it needs to.
         */
        "paddq %%xmm1,%%xmm0\n\t"
        "movq %%xmm0, %%rax\n\t"
        /* Only raise the signal on the 2nd run == native run. */
        "cmp $2, %%rax\n\t"
        "jne 11f\n\t"
        /* Raise a signal on the native run. */
        "ud2a\n\t"
        "11:\n\t"
        "nop\n\t"

        /* Post-commit. */
        "3:\n\t"
        "jmp 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        /* Start with jmp to avoid trace_invariants assert on return to u2da. */
        "jmp 42f\n\t"
        "42:\n\t"
        "addl $1, %[restarts]\n\t"
        "jmp 2b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_tls]\n\t"
        /* clang-format on */

        : [rseq_tls] "=m"(rseq_tls.rseq_cs), [restarts] "=m"(restarts)
        :
        : "rax", "rcx", "rdx", "xmm0", "xmm1", "memory");
    /* This is expected to fail on a native run where restarts will be 0. */
    assert(restarts > 0);
}

/* Tests that DR handles an rseq abort from migration or context switch (a signal
 * is tested in test_rseq_native_fault()) in the native rseq execution.
 * We again cheat and take advantage of DR not restoring XMM state to have different
 * behavior in the two DR executions of the rseq code.
 * The only reliable way we can force a context switch or migration is to use
 * a system call, which is officially disallowed.  We have special exceptions in
 * the code which look for the test name "linux.rseq" and are limited to DEBUG.
 */
static void
test_rseq_native_abort(void)
{
#ifdef DEBUG /* See above: special code in core/ is DEBUG-only> */
    int restarts = 0;
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(abort, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_abort(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_tls]\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "mov $1,%%rcx\n\t"
        "movq %%rcx, %%xmm1\n\t"

        /* Restartable sequence. */
        "2:\n\t"
        /* Increase xmm0 every time.  DR (currently) won't restore xmm inputs
         * to rseq sequences, nor does it detect that it needs to.
         */
        "paddq %%xmm1,%%xmm0\n\t"
        "movq %%xmm0, %%rax\n\t"
        /* Only raise the signal on the 2nd run == native run. */
        "cmp $2, %%rax\n\t"
        "jne 11f\n\t"
        /* Force a migration by setting the affinity mask to two different singleton
         * CPU's.
         */
        "mov $0, %%rdi\n\t"
        "mov %[cpu_mask_size], %%rsi\n\t"
        "leaq sched_mask_1(%%rip), %%rdx\n\t"
        "mov %[sysnum_setaffinity], %%eax\n\t"
        "syscall\n\t"
        "mov $0, %%rdi\n\t"
        "mov %[cpu_mask_size], %%rsi\n\t"
        "leaq sched_mask_2(%%rip), %%rdx\n\t"
        "mov %[sysnum_setaffinity], %%eax\n\t"
        "syscall\n\t"
        "11:\n\t"
        "nop\n\t"

        /* Post-commit. */
        "3:\n\t"
        "jmp 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "addl $1, %[restarts]\n\t"
        "jmp 2b\n\t"

        "sched_mask_1:\n\t"
        ".long 0x1, 0, 0, 0\n\t" /* cpu #1 */
        "sched_mask_2:\n\t"
        ".long 0x2, 0, 0, 0\n\t" /* cpu #2 */

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_tls]\n\t"
        /* clang-format on */

        : [rseq_tls] "=m"(rseq_tls.rseq_cs), [restarts] "=m"(restarts)
        : [cpu_mask_size] "i"(sizeof(cpu_set_t)),
          [sysnum_setaffinity] "i"(SYS_sched_setaffinity)
        : "rax", "rcx", "rdx", "xmm0", "xmm1", "memory");
    /* This is expected to fail on a native run where restarts will be 0. */
    assert(restarts > 0);
#endif /* DEBUG */
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
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(thread, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_thread(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_tls]\n\t"
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
        "movl $1, %[zero]\n\t"
        "jmp 2b\n\t"
        /* We can't end the sequence in a branch (DR can't handle it). */
        "nop\n\t"

        /* Post-commit. */
        "3:\n\t"
        "jmp 5f\n\t"

        /* Abort handler: if we're done, exit; else, re-enter. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "mov %[exit_requested], %%rax\n\t"
        "cmp $0, %%rax\n\t"
        "jne 3b\n\t"
        "jmp 6b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_tls]\n\t"
        /* clang-format on */

        : [rseq_tls] "=m"(rseq_tls.rseq_cs), [zero] "=m"(zero)
        : [exit_requested] "m"(exit_requested)
        : "rax", "memory");
    return NULL;
}

static void
kernel_xfer_event(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    static bool skip_print;
    if (!skip_print)
        dr_fprintf(STDERR, "%s: type %d\n", __FUNCTION__, info->type);
    /* Avoid tons of prints for the trace loop in main(). */
    if (info->type == DR_XFER_RSEQ_ABORT)
        skip_print = true;
    dr_mcontext_t mc = { sizeof(mc) };
    mc.flags = DR_MC_CONTROL;
    bool ok = dr_get_mcontext(drcontext, &mc);
    assert(ok);
    assert(mc.pc == info->target_pc);
    assert(mc.xsp == info->target_xsp);
    mc.flags = DR_MC_ALL;
    ok = dr_get_mcontext(drcontext, &mc);
    assert(ok);
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    /* Ensure DR_XFER_RSEQ_ABORT is rasied. */
    dr_register_kernel_xfer_event(kernel_xfer_event);
}
#endif /* RSEQ_TEST_ATTACH */

int
main()
{
    intercept_signal(SIGILL, signal_handler, false);
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
        test_rseq_call();
        /* Test variations inside the sequence. */
        test_rseq_branches();
        /* Test a fault in the native run. */
        test_rseq_native_fault();
        /* Test a non-fault abort in the native run. */
        test_rseq_native_abort();
        /* Test a trace. */
        int i;
        for (i = 0; i < 200; i++)
            test_rseq_branches();
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
    }
    print("All done\n");
    return 0;
}
