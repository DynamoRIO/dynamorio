/* **********************************************************
 * Copyright (c) 2019-2023 Google, Inc.  All rights reserved.
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

/* XXX: We undef this b/c it's easier than getting rid of from CMake with the
 * global cflags config where all the other tests want this set.
 */
#undef DR_REG_ENUM_COMPATIBILITY

#ifdef RSEQ_TEST_ATTACH
#    include "configure.h"
#    include "dr_api.h"
#endif
#include "tools.h"
#ifdef RSEQ_TEST_ATTACH
#    include "thread.h"
#    include "condvar.h"
#    include <atomic>
#    include "drmemtrace/drmemtrace.h"
#endif
#ifndef LINUX
#    error Only Linux is supported.
#endif
/* TODO i#2350: Port this to other platforms and bitwidths.  There is a lot of
 * assembly which makes that non-trivial work.
 */
#if !defined(X64)
#    error Only x86_64 and aarch64 are supported.
#endif
#include "../../core/unix/include/syscall.h"
#ifndef HAVE_RSEQ
#    error The linux/rseq header is required.
#endif
#ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#endif
#include <sched.h>
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
#    include <sys/rseq.h>
#    define GLIBC_RSEQ 1
#else
#    define RSEQ_SIG IF_X86_ELSE(0x90909090, 0xd503201f) /* nops to disasm nicely */
#endif
#include <linux/rseq.h>
#include <errno.h>
#undef NDEBUG
#include <assert.h>

#define EXPANDSTR(x) #x
#define STRINGIFY(x) EXPANDSTR(x)

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

extern "C" void
test_rseq_native_abort_handler();

#ifdef RSEQ_TEST_ATTACH
static std::atomic<int> exit_requested;
static void *thread_ready;
#endif

static volatile int sigill_count;
static volatile bool in_rseq_migration_test;

static bool
register_rseq()
{
#ifdef GLIBC_RSEQ
    if (__rseq_size > 0) {
        /* Our glibc rseq handling in rseq_linux.c checks the following offset
         * first for the glibc-registered struct rseq. Though we do have a
         * fallback that does a wider search, it would be good to keep the
         * expected offset in sync with glibc changes.
         */
#    if __GLIBC__ == 2 && __GLIBC_MINOR__ >= 36
#        ifdef X86
#            ifdef X64
        assert(__rseq_offset == 2336);
#            else
        assert(__rseq_offset == 1184);
#            endif
#        else
        /* Did not change on glibc 2.36. */
        assert(__rseq_offset == -32);
#        endif
#    else
#        ifdef X86
#            ifdef X64
        assert(__rseq_offset == 2464);
#            else
        assert(__rseq_offset == 1312);
#            endif
#        else
        assert(__rseq_offset == -32);
#        endif
#    endif
        /* Already registered by glibc. Verify that it's there. */
        struct rseq *reg_rseq = reinterpret_cast<struct rseq *>(
            reinterpret_cast<byte *>(__builtin_thread_pointer()) + __rseq_offset);
        /* Glibc uses RSEQ_SIG as the signature. The following will return
         * EBUSY if reg_rseq is the registered struct rseq and EINVAL otherwise.
         * FTR: if we use some other value as signature (like zero),  this
         * becomes an EPERM vs EINVAL check.
         */
        int res = syscall(SYS_rseq, reg_rseq, sizeof(*reg_rseq), 0, RSEQ_SIG);
        assert(res == -1 && (errno == EBUSY || errno == ENOSYS));
    } else {
#endif
        rseq_tls.cpu_id = RSEQ_CPU_ID_UNINITIALIZED;
        int res = syscall(SYS_rseq, &rseq_tls, sizeof(rseq_tls), 0, RSEQ_SIG);
        assert(res == 0 || (res == -1 && errno == ENOSYS));
#ifdef GLIBC_RSEQ
    }
#endif
    /* Linux kernel 4.18+ is required. */
    return errno != ENOSYS;
}

static volatile struct rseq *
get_my_rseq()
{
#ifdef GLIBC_RSEQ
    if (__rseq_size > 0)
        return reinterpret_cast<struct rseq *>(
            reinterpret_cast<byte *>(__builtin_thread_pointer()) + __rseq_offset);
#endif
    return &rseq_tls;
}

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGILL)
        ++sigill_count;
}

static void
test_rseq_call_once(bool force_restart_in, int *completions_out, int *restarts_out)
{
    volatile struct rseq *reg_rseq = get_my_rseq();
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
#ifdef X86
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
        "movq %%rax, %[rseq_cs]\n\t"
        /* Test register inputs to the sequence. */
        "movl %[cpu_id], %%eax\n\t"
        "movq %%rsp, %%rcx\n\t"
        "movq %%rsp, %%rdx\n\t"
        "movq %%rsp, %%rbx\n\t"
        "movq %%rsp, %%rdi\n\t"
        "movq %%rsp, %%rsi\n\t"
        /* Test "falling into" the rseq region. */

        /* Restartable sequence. */
        "2:\n\t"
        "movl %%eax, %[id]\n\t"
        /* Test clobbering an input register. */
        "movl %[cpu_id_uninit], %%eax\n\t"
        /* Test reading many registers to stress drreg (e.g., i#5668).
         * This is easier in x86 where we can exhaust drreg's choices.
         * We do not attempt on aarch64 for simplicity.
         */
        "movq (%%rcx), %%rax\n\t"
        "movq (%%rdx), %%rax\n\t"
        "movq (%%rbx), %%rax\n\t"
        "movq (%%rdi), %%rax\n\t"
        "movq (%%rsi), %%rax\n\t"
        /* Test a restart in the middle of the sequence via ud2 SIGILL. */
        "cmpb $0, %[force_restart]\n\t"
        "jz 7f\n\t"
        /* For -test_mode invariant_checker: expect a signal after ud2.
         * (An alternative is to add decoding to invariant_checker.)
         */
        "prefetcht2 1\n\t"
        "ud2\n\t"
        "7:\n\t"
        "addl $1, %[completions]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "ret\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        /* Start with jmp to avoid invariant_checker assert on return to u2da. */
        "jmp 42f\n\t"
        "42:\n\t"
        "addl $1, %[restarts]\n\t"
        "movb $0, %[force_restart_write]\n\t"
        "jmp 6b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_cs]\n\t"
        /* clang-format on */
        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [id] "=m"(id),
          [completions] "=m"(completions), [restarts] "=m"(restarts),
          [force_restart_write] "=m"(force_restart)
        : [cpu_id] "m"(reg_rseq->cpu_id), [cpu_id_uninit] "i"(RSEQ_CPU_ID_UNINITIALIZED),
          [force_restart] "m"(force_restart)
        : "rax", "rcx", "rdx", "rbx", "rdi", "rsi", "memory");
#elif defined(AARCH64)
    __asm__ __volatile__(
        RSEQ_ADD_TABLE_ENTRY(simple, 2f, 3f, 4f)

        /* See the x86 notes: test call-return pattern. */
        "bl 6f\n\t"
        "b 5f\n\t"

        "6:\n\t"
        /* Store the entry into the ptr. */
        "adrp x0, rseq_cs_simple\n\t"
        "add x0, x0, :lo12:rseq_cs_simple\n\t"
        /* We ask the compiler to load these references into registers
         * before this asm sequence for us.  Thus there are a bunch of
         * inputs but that's simpler than manually computing all these
         * addresses inside the sequence.
         */
        "str x0, %[rseq_cs]\n\t"
        /* Test a register input to the sequence. */
        "ldr x0, %[cpu_id]\n\t"
        /* Test "falling into" the rseq region. */

        /* Restartable sequence. */
        "2:\n\t"
        "str x0, %[id]\n\t"
        /* Test clobbering an input register. */
        "mov x0, #%[cpu_id_uninit]\n\t"
        /* Test clobbering a non-regular-GPR. */
        "mov x1, sp\n\t"
        "mov sp, x0\n\t"
        "mov sp, x1\n\t"
        /* Test a restart in the middle of the sequence via udf SIGILL. */
        "ldrb w0, %[force_restart]\n\t"
        "cbz x0, 7f\n\t"
        "mov x0, #1\n\t"
        "prfm pldl3keep, [x0]\n\t" /* See above: annotation for invariant_checker. */
        ".word 0\n\t"              /* udf */
        "7:\n\t"
        "ldr x1, %[completions]\n\t"
        "add x1, x1, #1\n\t"
        "str x1, %[completions]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "ret\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        /* Start with jmp to avoid invariant_checker assert on return to udf. */
        "b 42f\n\t"
        "42:\n\t"
        "ldr x1, %[restarts]\n\t"
        "add x1, x1, #1\n\t"
        "str x1, %[restarts]\n\t"
        "str xzr, %[force_restart_write]\n\t"
        "b 6b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "str xzr, %[rseq_cs]\n\t"
        /* clang-format on */
        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [id] "=m"(id),
          [completions] "=m"(completions), [restarts] "=m"(restarts),
          [force_restart_write] "=m"(force_restart)
        : [cpu_id] "m"(reg_rseq->cpu_id), [cpu_id_uninit] "i"(RSEQ_CPU_ID_UNINITIALIZED),
          [force_restart] "m"(force_restart)
        : "x0", "x1", "memory");
#else
#    error Unsupported arch
#endif

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
    volatile struct rseq *reg_rseq = get_my_rseq();
    __u32 id = RSEQ_CPU_ID_UNINITIALIZED;
    int completions = 0;
    int restarts = 0;
#ifdef X86
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(branches, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_branches(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_cs]\n\t"
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
        /* Test a restart via ud2 SIGILL. */
        "cmpb $0, %[force_restart]\n\t"
        "jz 7f\n\t"
        "prefetcht2 1\n\t" /* See above: annotation for invariant_checker. */
        "ud2\n\t"
        "7:\n\t"
        "addl $1, %[completions]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "jmp 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        /* Start with jmp to avoid invariant_checker assert on return to u2da. */
        "jmp 42f\n\t"
        "42:\n\t"
        "addl $1, %[restarts]\n\t"
        "movb $0, %[force_restart_write]\n\t"
        "jmp 6b\n\t"

        /* Clear the ptr. */
        "13:\n\t"
        "12:\n\t"
        "5:\n\t"
        "movq $0, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [id] "=m"(id),
          [completions] "=m"(completions), [restarts] "=m"(restarts),
          [force_restart_write] "=m"(force_restart)
        : [cpu_id] "m"(reg_rseq->cpu_id), [force_restart] "m"(force_restart)
        : "rax", "rcx", "memory");
#elif defined(AARCH64)
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(branches, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "adrp x0, rseq_cs_branches\n\t"
        "add x0, x0, :lo12:rseq_cs_branches\n\t"
        "str x0, %[rseq_cs]\n\t"
        /* Test a register input to the sequence. */
        "ldr x0, %[cpu_id]\n\t"
        /* Test "falling into" the rseq region. */

        /* Restartable sequence.  We include control flow to test a
         * complex sequence with midpoint branches, but no exits.
         * TODO i#2350: Support for exits has not yet been added and
         * once finished separate tests will be added.
         */
        "2:\n\t"
        "str x0, %[id]\n\t"
        "mov x0, #0\n\t"
        "cbz x0, 11f\n\t"
        "mov x1, #4\n\t"
        "11:\n\t"
        "cmp x0, #1\n\t"
        "b.eq 12f\n\t"
        "cmp x0, #2\n\t"
        "b.eq 13f\n\t"
        /* Test a restart via ud2 SIGILL. */
        "ldrb w0, %[force_restart]\n\t"
        "cbz x0, 7f\n\t"
        "mov x0, #1\n\t"
        "prfm pldl3keep, [x0]\n\t" /* See above: annotation for invariant_checker. */
        ".word 0\n\t"              /* udf */
        "7:\n\t"
        "ldr x1, %[completions]\n\t"
        "add x1, x1, #1\n\t"
        "str x1, %[completions]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "b 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        /* Start with jmp to avoid invariant_checker assert on return to udf. */
        "b 42f\n\t"
        "42:\n\t"
        "ldr x1, %[restarts]\n\t"
        "add x1, x1, #1\n\t"
        "str x1, %[restarts]\n\t"
        "str xzr, %[force_restart_write]\n\t"
        "b 6b\n\t"

        /* Clear the ptr. */
        "13:\n\t"
        "12:\n\t"
        "5:\n\t"
        "str xzr, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [id] "=m"(id),
          [completions] "=m"(completions), [restarts] "=m"(restarts),
          [force_restart_write] "=m"(force_restart)
        : [cpu_id] "m"(reg_rseq->cpu_id), [force_restart] "m"(force_restart)
        : "x0", "x1", "memory");
#else
#    error Unsupported arch
#endif
    assert(id != RSEQ_CPU_ID_UNINITIALIZED);
    *completions_out = completions;
    *restarts_out = restarts;
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
 * we cheat and take advantage of DR not restoring SIMD state to have different
 * behavior in the two DR executions of the rseq code.
 */
static void
test_rseq_native_fault(void)
{
    volatile struct rseq *reg_rseq = get_my_rseq();
    int restarts = 0;
#ifdef X86
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(fault, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_fault(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_cs]\n\t"
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
        "ud2\n\t"
        "11:\n\t"
        "nop\n\t"

        /* Post-commit. */
        "3:\n\t"
        "jmp 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        /* Start with jmp to avoid invariant_checker assert on return to u2da. */
        "jmp 42f\n\t"
        "42:\n\t"
        "addl $1, %[restarts]\n\t"
        "jmp 2b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts)
        :
        : "rax", "rcx", "xmm0", "xmm1", "memory");
#elif defined(AARCH64)
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(fault, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "adrp x0, rseq_cs_fault\n\t"
        "add x0, x0, :lo12:rseq_cs_fault\n\t"
        "str x0, %[rseq_cs]\n\t"
        "eor v0.16b, v0.16b, v0.16b\n\t"
        "mov x1, #1\n\t"
        "mov v1.D[0], x1\n\t"

        /* Restartable sequence. */
        "2:\n\t"
        /* Increase xmm0 every time.  DR (currently) won't restore xmm inputs
         * to rseq sequences, nor does it detect that it needs to.
         */
        "add d0, d0, d1\n\t"
        "mov x0, v0.D[0]\n\t"
        /* Only raise the signal on the 2nd run == native run. */
        "cmp x0, #2\n\t"
        "b.ne 11f\n\t"
        /* Raise a signal on the native run. */
        ".word 0\n\t" /* udf */
        "11:\n\t"
        "nop\n\t"

        /* Post-commit. */
        "3:\n\t"
        "b 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        /* Start with jmp to avoid invariant_checker assert on return to u2da. */
        "b 42f\n\t"
        "42:\n\t"
        "ldr x1, %[restarts]\n\t"
        "add x1, x1, #1\n\t"
        "str x1, %[restarts]\n\t"
        "b 2b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "str xzr, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts)
        :
        : "x0", "x1", "q0", "q1", "memory");
#else
#    error Unsupported arch
#endif
    /* This is expected to fail on a native run where restarts will be 0. */
    assert(restarts > 0);
}

/* Tests that DR handles an rseq abort from migration or context switch (a signal
 * is tested in test_rseq_native_fault()) in the native rseq execution.
 * We again cheat and take advantage of DR not restoring SIMD state to have different
 * behavior in the two DR executions of the rseq code.
 * The only reliable way we can force a context switch or migration is to use
 * a system call, which is officially disallowed.  We have special exceptions in
 * the code which look for the test name "linux.rseq" and are limited to DEBUG.
 */
static void
test_rseq_native_abort(void)
{
    volatile struct rseq *reg_rseq = get_my_rseq();
#ifdef DEBUG /* See above: special code in core/ is DEBUG-only> */
    in_rseq_migration_test = true;
    int restarts = 0;
#    ifdef X86
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(abort, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_abort(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_cs]\n\t"
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
        ".global test_rseq_native_abort_handler\n\t"
        "test_rseq_native_abort_handler:\n\t"
        "addl $1, %[restarts]\n\t"
        "jmp 2b\n\t"

        "sched_mask_1:\n\t"
        ".long 0x1, 0, 0, 0\n\t" /* cpu #1 */
        "sched_mask_2:\n\t"
        ".long 0x2, 0, 0, 0\n\t" /* cpu #2 */

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts)
        : [cpu_mask_size] "i"(sizeof(cpu_set_t)),
          [sysnum_setaffinity] "i"(SYS_sched_setaffinity)
        : "rax", "rcx", "rdx", "rsi", "rdi", "xmm0", "xmm1", "memory");
#    elif defined(AARCH64)
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(abort, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "adrp x0, rseq_cs_abort\n\t"
        "add x0, x0, :lo12:rseq_cs_abort\n\t"
        "str x0, %[rseq_cs]\n\t"
        "eor v0.16b, v0.16b, v0.16b\n\t"
        "mov x1, #1\n\t"
        "mov v1.D[0], x1\n\t"

        /* Restartable sequence. */
        "2:\n\t"
        /* Increase xmm0 every time.  DR (currently) won't restore xmm inputs
         * to rseq sequences, nor does it detect that it needs to.
         */
        "add d0, d0, d1\n\t"
        "mov x0, v0.D[0]\n\t"
        /* Only force the abort on the 2nd run == native run. */
        "cmp x0, #2\n\t"
        "b.ne 11f\n\t"
        /* Force a migration by setting the affinity mask to two different singleton
         * CPU's.
         */
        "mov x0, #0\n\t"
        "mov w1, #%[cpu_mask_size]\n\t"
        "adrp x2, sched_mask_1\n\t"
        "add x2, x2, :lo12:sched_mask_1\n\t"
        "mov w8, #%[sysnum_setaffinity]\n\t"
        "svc #0\n\t"
        "mov x0, #0\n\t"
        "mov w1, #%[cpu_mask_size]\n\t"
        "adrp x2, sched_mask_2\n\t"
        "add x2, x2, :lo12:sched_mask_2\n\t"
        "mov w8, #%[sysnum_setaffinity]\n\t"
        "svc #0\n\t"
        "11:\n\t"
        "nop\n\t"

        /* Post-commit. */
        "3:\n\t"
        "b 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        ".global test_rseq_native_abort_handler\n\t"
        "test_rseq_native_abort_handler:\n\t"
        "ldr x1, %[restarts]\n\t"
        "add x1, x1, #1\n\t"
        "str x1, %[restarts]\n\t"
        "b 2b\n\t"

        ".balign 32\n\t"
        "sched_mask_1:\n\t"
        ".long 0x1, 0, 0, 0\n\t" /* cpu #1 */
        "sched_mask_2:\n\t"
        ".long 0x2, 0, 0, 0\n\t" /* cpu #2 */

        /* Clear the ptr. */
        "5:\n\t"
        "str xzr, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts)
        : [cpu_mask_size] "i"(sizeof(cpu_set_t)),
          [sysnum_setaffinity] "i"(SYS_sched_setaffinity)
        : "x0", "x1", "x2", "x8", "q0", "q1", "memory");
#    else
#        error Unsupported arch
#    endif
    in_rseq_migration_test = false;
    /* This is expected to fail on a native run where restarts will be 0. */
    assert(restarts > 0);
#endif /* DEBUG */
}

/* Tests that DR handles an asynch signal in the native code in the final rseq
 * fragment.  We use a system call, which is officially disallowed.  We have special
 * exceptions in the code which look for the test name "linux.rseq" and are limited to
 * DEBUG.
 */
static void
test_rseq_asynch_signal(void)
{
    volatile struct rseq *reg_rseq = get_my_rseq();
#ifdef DEBUG /* See above: special code in core/ is DEBUG-only> */
    int restarts = 0;
    int commits = 0;
#    ifdef X86
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(asynch_signal, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_abort(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_cs]\n\t"
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
        /* We raise the signal on the instrumented run and never come back. */
        /* Send ourselves a SIGALRM. */
        "mov $0, %%rdi\n\t"
        "mov %[signum_alarm], %%rsi\n\t"
        "mov %[sysnum_kill], %%eax\n\t"
        "syscall\n\t"
        "11:\n\t"
        "addl $1, %[commits]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "jmp 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "addl $1, %[restarts]\n\t"
        "jmp 5f\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts),
          [commits] "=m"(commits)
        : [signum_alarm] "i"(SIGALRM), [sysnum_kill] "i"(SYS_kill)
        : "rax", "rcx", "rdx", "rsi", "rdi", "xmm0", "xmm1", "memory");
#    elif defined(AARCH64)
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(asynch_signal, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "adrp x0, rseq_cs_abort\n\t"
        "add x0, x0, :lo12:rseq_cs_abort\n\t"
        "str x0, %[rseq_cs]\n\t"
        "eor v0.16b, v0.16b, v0.16b\n\t"
        "mov x1, #1\n\t"
        "mov v1.D[0], x1\n\t"

        /* Restartable sequence. */
        "2:\n\t"
        /* Increase xmm0 every time.  DR (currently) won't restore xmm inputs
         * to rseq sequences, nor does it detect that it needs to.
         */
        "add d0, d0, d1\n\t"
        "mov x0, v0.D[0]\n\t"
        /* We raise the signal on the instrumented run and never come back. */
        /* Send ourselves a SIGALRM. */
        "mov x0, #0\n\t"
        "mov w1, #%[signum_alarm]\n\t"
        "mov w8, #%[sysnum_kill]\n\t"
        "svc #0\n\t"
        "11:\n\t"
        "ldr x0, %[commits]\n\t"
        "add x0, x0, #1\n\t"
        "str x0, %[commits]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "b 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "ldr x1, %[restarts]\n\t"
        "add x1, x1, #1\n\t"
        "str x1, %[restarts]\n\t"
        "b 5f\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "str xzr, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts),
          [commits] "=m"(commits)
        : [signum_alarm] "i"(SIGALRM), [sysnum_kill] "i"(SYS_kill)
        : "x0", "x1", "x2", "x8", "q0", "q1", "memory");
#    else
#        error Unsupported arch
#    endif
    /* This is expected to fail on a native run where restarts will be 0. */
    assert(restarts > 0);
    assert(commits == 0);
#endif /* DEBUG */
}

/* Tests that DR and drmemtrace handle a side exit in the native sequence. */
static void
test_rseq_side_exit(void)
{
    volatile struct rseq *reg_rseq = get_my_rseq();
    int restarts = 0;
    int commits = 0;
#ifdef X86
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(side, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_side(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_cs]\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "mov $1,%%rax\n\t"
        "movq %%rax, %%xmm1\n\t"

        /* Restartable sequence. */
        "2:\n\t"
        /* Increase xmm0 every time.  DR (currently) won't restore xmm inputs
         * to rseq sequences, nor does it detect that it needs to.
         */
        "paddq %%xmm1,%%xmm0\n\t"
        "movq %%xmm0, %%rax\n\t"
        /* Take an inverted side exit in the 2nd run == native run. */
        "cmp $2, %%rax\n\t"
        "jne 7f\n\t"
        "jmp 5f\n\t"
        "7:\n\t"
        "addl $1, %[commits]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "jmp 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "addl $1, %[restarts]\n\t"
        "jmp 2b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts),
          [commits] "=m"(commits)
        :
        : "rax", "xmm0", "xmm1", "memory");
#elif defined(AARCH64)
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(side, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "adrp x0, rseq_cs_side\n\t"
        "add x0, x0, :lo12:rseq_cs_side\n\t"
        "str x0, %[rseq_cs]\n\t"
        "eor v0.16b, v0.16b, v0.16b\n\t"
        "mov x0, #1\n\t"
        "mov v1.D[0], x0\n\t"

        /* Restartable sequence. */
        "2:\n\t"
        /* Increase xmm0 every time.  DR (currently) won't restore xmm inputs
         * to rseq sequences, nor does it detect that it needs to.
         */
        "add d0, d0, d1\n\t"
        "mov x0, v0.D[0]\n\t"
        /* Take an inverted side exit in the 2nd run == native run. */
        "cmp x0, #2\n\t"
        "b.ne 7f\n\t"
        "b 5f\n\t"
        "7:\n\t"
        "ldr x0, %[commits]\n\t"
        "add x0, x0, #1\n\t"
        "str x0, %[commits]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "b 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "ldr x1, %[restarts]\n\t"
        "add x1, x1, #1\n\t"
        "str x1, %[restarts]\n\t"
        "b 2b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "str xzr, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts),
          [commits] "=m"(commits)
        :
        : "x0", "q0", "q1", "memory");
#else
#    error Unsupported arch
#endif
    assert(restarts == 0);
    assert(commits == 0);
}

/* Tests that DR and drmemtrace handle a side exit in the instrumented sequence. */
static void
test_rseq_instru_side_exit(void)
{
    volatile struct rseq *reg_rseq = get_my_rseq();
    int restarts = 0;
    int commits = 0;
#ifdef X86
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(side_instru, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_side(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_cs]\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "mov $1,%%rax\n\t"
        "movq %%rax, %%xmm1\n\t"

        /* Restartable sequence. */
        "2:\n\t"
        /* Increase xmm0 every time.  DR (currently) won't restore xmm inputs
         * to rseq sequences, nor does it detect that it needs to.
         */
        "paddq %%xmm1,%%xmm0\n\t"
        "movq %%xmm0, %%rax\n\t"
        /* Take a side exit in the 1st run == instrumented run. */
        "cmp $1, %%rax\n\t"
        "je 5f\n\t"
        "7:\n\t"
        "addl $1, %[commits]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "jmp 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "addl $1, %[restarts]\n\t"
        "jmp 2b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "movq $0, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts),
          [commits] "=m"(commits)
        :
        : "rax", "xmm0", "xmm1", "memory");
#elif defined(AARCH64)
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(side_instru, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "adrp x0, rseq_cs_side\n\t"
        "add x0, x0, :lo12:rseq_cs_side\n\t"
        "str x0, %[rseq_cs]\n\t"
        "eor v0.16b, v0.16b, v0.16b\n\t"
        "mov x0, #1\n\t"
        "mov v1.D[0], x0\n\t"

        /* Restartable sequence. */
        "2:\n\t"
        /* Increase xmm0 every time.  DR (currently) won't restore xmm inputs
         * to rseq sequences, nor does it detect that it needs to.
         */
        "add d0, d0, d1\n\t"
        "mov x0, v0.D[0]\n\t"
        /* Take a side exit in the 1st run == instrumented run. */
        "cmp x0, #1\n\t"
        "b.eq 5f\n\t"
        "7:\n\t"
        "ldr x0, %[commits]\n\t"
        "add x0, x0, #1\n\t"
        "str x0, %[commits]\n\t"

        /* Post-commit. */
        "3:\n\t"
        "b 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "ldr x1, %[restarts]\n\t"
        "add x1, x1, #1\n\t"
        "str x1, %[restarts]\n\t"
        "b 2b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "str xzr, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [restarts] "=m"(restarts),
          [commits] "=m"(commits)
        :
        : "x0", "q0", "q1", "memory");
#else
#    error Unsupported arch
#endif
    assert(restarts == 0);
    assert(commits == 0);
}

#ifdef AARCH64
static void
test_rseq_writeback_store(void)
{
    volatile struct rseq *reg_rseq = get_my_rseq();
    __u32 id = RSEQ_CPU_ID_UNINITIALIZED;
    int array[2] = { 0 };
    int *index = array;
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(writeback, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "adrp x0, rseq_cs_writeback\n\t"
        "add x0, x0, :lo12:rseq_cs_writeback\n\t"
        "str x0, %[rseq_cs]\n\t"
        /* Test a register input to the sequence. */
        "ldr x0, %[cpu_id]\n\t"
        /* Test "falling into" the rseq region. */

        "2:\n\t"
        "str x0, %[id]\n\t"
        /* Test writeback stores on both executions by having a loop
         * that won't terminate otherwise.
         * Further stress corner cases by using the default stolen register.
         */
        "mov x28, %[index]\n\t"
        "add x2, x28, #8\n\t"
        "mov w1, #42\n\t"
        "loop:\n\t"
        "str w1, [x28, 4]!\n\t"
        "str w1, [x28], 4\n\t"
        "cmp x28, x2\n\t"
        "b.ne loop\n\t"
        "nop\n\t"

        /* Post-commit. */
        "3:\n\t"
        "b 5f\n\t"

        /* Abort handler. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "b 6b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "str xzr, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [id] "=m"(id), [index] "=g"(index)
        : [cpu_id] "m"(reg_rseq->cpu_id)
        : "x0", "x1", "x2", "x28", "memory");
    assert(id != RSEQ_CPU_ID_UNINITIALIZED);
}
#endif

#ifdef RSEQ_TEST_ATTACH
void *
rseq_thread_loop(void *arg)
{
    /* We don't try to signal inside the rseq code.  Just having the thread scheduled
     * in this function is close enough: the test already has non-determinism.
     */
    signal_cond_var(thread_ready);
    bool res = register_rseq();
    assert(res);
    volatile struct rseq *reg_rseq = get_my_rseq();
    static int zero;
#    ifdef X86
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(thread, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "leaq rseq_cs_thread(%%rip), %%rax\n\t"
        "movq %%rax, %[rseq_cs]\n\t"
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
        "movq $0, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [zero] "=m"(zero)
        : [exit_requested] "m"(exit_requested)
        : "rax", "memory");
#    elif defined(AARCH64)
    __asm__ __volatile__(
        /* clang-format off */ /* (avoid indenting next few lines) */
        RSEQ_ADD_TABLE_ENTRY(thread, 2f, 3f, 4f)
        /* clang-format on */

        "6:\n\t"
        /* Store the entry into the ptr. */
        "adrp x0, rseq_cs_thread\n\t"
        "add x0, x0, :lo12:rseq_cs_thread\n\t"
        "str x0, %[rseq_cs]\n\t"
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
        "mov x1, #1\n\t"
        "str x1, %[zero]\n\t"
        "b 2b\n\t"
        /* We can't end the sequence in a branch (DR can't handle it). */
        "nop\n\t"

        /* Post-commit. */
        "3:\n\t"
        "b 5f\n\t"

        /* Abort handler: if we're done, exit; else, re-enter. */
        /* clang-format off */ /* (avoid indenting next few lines) */
        ".long " STRINGIFY(RSEQ_SIG) "\n\t"
        "4:\n\t"
        "ldar w0, %[exit_requested]\n\t"
        "cbnz x0, 3b\n\t"
        "b 6b\n\t"

        /* Clear the ptr. */
        "5:\n\t"
        "str xzr, %[rseq_cs]\n\t"
        /* clang-format on */

        : [rseq_cs] "=m"(reg_rseq->rseq_cs), [zero] "=m"(zero)
        : [exit_requested] "m"(exit_requested)
        : "x0", "x1", "memory");
#    else
#        error Unsupported arch
#    endif
    return NULL;
}

static void
kernel_xfer_event(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    static bool skip_print;
    static int count;
    if (!skip_print)
        dr_fprintf(STDERR, "%s: type %d\n", __FUNCTION__, info->type);
    /* Avoid tons of prints for the trace loop in main(). */
    if (++count > 13)
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
    /* All transfer cases in this test should have source info. */
    assert(info->source_mcontext != NULL);
    /* For a migration, the context should be identical.
     * For a signal it won't be as signal handler arguments will be set in
     * registers and a sigaltstack may be set.
     */
    if (info->type == DR_XFER_RSEQ_ABORT && in_rseq_migration_test) {
        /* The interrupted context should be identical, including the pc now
         * that DR passes the abort handler PC just like the kernel does.
         */
#    ifdef X86
        /* This compares the GPR's plus flags and PC. */
        assert(memcmp(&info->source_mcontext->xdi, &mc.xdi,
                      (byte *)&mc.padding - (byte *)&mc.xdi) == 0);
#    elif defined(AARCH64)
        assert(info->source_mcontext->pc == mc.pc);
        assert(memcmp(&info->source_mcontext->r0, &mc.r0, sizeof(mc.r0) * 32) == 0);
#    else
#        error Unsupported arch
#    endif
#    ifdef DEBUG /* See above: special code in core/ is DEBUG-only> */
        /* Check that the interrupted PC for the true abort case is the handler.
         * We could check this for every test if we wrote the expected handler
         * into a global or something: does not seem worth the extra code.
         */
        assert(info->source_mcontext->pc == (app_pc)test_rseq_native_abort_handler);
#    endif
    }
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    /* Ensure DR_XFER_RSEQ_ABORT is raised. */
    dr_register_kernel_xfer_event(kernel_xfer_event);
    dynamorio::drmemtrace::drmemtrace_client_main(id, argc, argv);
}
#endif /* RSEQ_TEST_ATTACH */

int
main()
{
    intercept_signal(SIGILL, signal_handler, false);
    intercept_signal(SIGALRM, signal_handler, false);
    if (register_rseq()) {
#ifdef RSEQ_TEST_ATTACH
        /* Set -offline to avoid trying to open a pipe to a missing reader. */
        if (setenv("DYNAMORIO_OPTIONS",
                   "-stderr_mask 0xc -client_lib ';;-offline -max_trace_size 256K'",
                   1 /*override*/) != 0)
            return 1;
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
        /* Test a (non-fault) signal. */
        test_rseq_asynch_signal();
        /* Test a side exit in the native run. */
        test_rseq_side_exit();
        /* Test a side exit in the instrumented run. */
        test_rseq_instru_side_exit();
        /* Test a trace. */
        int i;
        for (i = 0; i < 200; i++)
            test_rseq_branches();
#ifdef AARCH64
        /* Test nop-ing stores with side effects. */
        test_rseq_writeback_store();
#endif
#ifdef RSEQ_TEST_ATTACH
        /* Detach while the thread is in its rseq region loop. */
        exit_requested.store(1, std::memory_order_release);
        dr_app_stop_and_cleanup();
        join_thread(mythread);
        destroy_cond_var(thread_ready);
#endif
    }
    print("All done\n");
    return 0;
}
