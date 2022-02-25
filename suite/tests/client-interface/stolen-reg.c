/* **********************************************************
 * Copyright (c) 2020-2021 Google, Inc.  All rights reserved.
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
 * API regression test for stolen register translation.
 */

#include "tools.h"
#include "thread.h"
#include <stdatomic.h>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h> /* uintptr_t */
#include <ucontext.h>

#define STOLEN_REG_SENTINEL 42

/* We assume a single thread when these are used. */
static SIGJMP_BUF mark;
static int sigsegv_count;

static atomic_bool thread_finished;
static atomic_bool ready_for_thread;

static uintptr_t
get_stolen_reg_val(void)
{
    uintptr_t val = 0;
#if defined(AARCH64)
    __asm__ __volatile__("str x28, %0\n\t" : "=m"(val) : :);
#elif defined(ARM)
    __asm__ __volatile__("str r10, %0\n\t" : "=m"(val) : :);
#else
#    error Unsupported arch
#endif
    return val;
}

/* This can't be a regular function since the stolen reg is callee-saved. */
#if defined(AARCH64)
#    define SET_STOLEN_REG_TO_SENTINEL() \
        __asm__ __volatile__("mov x28, #" STRINGIFY(STOLEN_REG_SENTINEL) : : : "x28")
#elif defined(ARM)
#    define SET_STOLEN_REG_TO_SENTINEL() \
        __asm__ __volatile__("mov r10, #" STRINGIFY(STOLEN_REG_SENTINEL) : : : "r10")
#else
#    error Unsupported arch
#endif

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig != SIGSEGV)
        return;
    print("Got SIGSEGV\n");
    uintptr_t val = get_stolen_reg_val();
    if (val != STOLEN_REG_SENTINEL) {
        print("ERROR: Stolen register %d not preserved on handler entry: %d\n",
              STOLEN_REG_SENTINEL, val);
    }
    sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
    if (sc->IF_ARM_ELSE(SC_R10, SC_R28) != STOLEN_REG_SENTINEL) {
        print("ERROR: Stolen register %d not preserved in signal context: %d\n",
              STOLEN_REG_SENTINEL, sc->IF_ARM_ELSE(SC_R10, SC_R28));
    }
    if (sigsegv_count == 0) {
        /* Allow re-execution to no longer fault. */
        sc->SC_R0 = sc->SC_XSP;
    } else {
        SIGLONGJMP(mark, 1);
    }
}

static uintptr_t
cause_sigsegv(void)
{
    uintptr_t val = 0;
    /* Generate SIGSEGV with a sentinel in the stolen reg.
     * This precise instruction sequence is matched by the stolen-reg.dll.c client.
     */
#if defined(AARCH64)
    __asm__ __volatile__("mov x28, #" STRINGIFY(STOLEN_REG_SENTINEL) "\n\t"
                                                                     "mov x0, #0\n\t"
                                                                     "ldr x1, [x0]\n\t"
                                                                     "str x28, %0\n\t"
                         : "=m"(val)
                         :
                         : "x0", "x1", "x28");
#elif defined(ARM)
    __asm__ __volatile__("mov r10, #" STRINGIFY(STOLEN_REG_SENTINEL) "\n\t"
                                                                     "mov r0, #0\n\t"
                                                                     "ldr r1, [r0]\n\t"
                                                                     "str r10, %0\n\t"
                         : "=m"(val)
                         :
                         : "r0", "r1", "r10");
#else
#    error Unsupported arch
#endif
    return val;
}

THREAD_FUNC_RETURN_TYPE
thread_func(void *arg)
{
    while (!atomic_load_explicit(&ready_for_thread, memory_order_acquire)) {
        /* We can't use cond var helpers b/c the main thread can't make calls.
         * Thus we just spin for simplicity, but we use release-acquire ordering
         * to ensure no load-store reordering.
         */
    }
    /* The stolen-reg.dll.c client looks for this exact sequence of instructions. */
#if defined(AARCH64)
    __asm__ __volatile__("mov x28, #" STRINGIFY(STOLEN_REG_SENTINEL) "\n\t"
                                                                     "nop\n\t"
                                                                     "nop\n\t"
                         :
                         :
                         : "x28");
#elif defined(ARM)
    __asm__ __volatile__("mov r10, #" STRINGIFY(STOLEN_REG_SENTINEL) "\n\t"
                                                                     "nop\n\t"
                                                                     "nop\n\t"
                         :
                         :
                         : "r10");
#else
#    error Unsupported arch
#endif
    atomic_store_explicit(&thread_finished, true, memory_order_release);
    return THREAD_FUNC_RETURN_ZERO;
}

int
main(int argc, char **argv)
{
    intercept_signal(SIGSEGV, signal_handler, false);

    /* First, raise SIGSEGV and continue at the same context. */
    uintptr_t val = cause_sigsegv();
    if (val != STOLEN_REG_SENTINEL) {
        print("ERROR: Stolen register %d not preserved past handler: %d\n",
              STOLEN_REG_SENTINEL, val);
    }
    /* Now, raise SIGSEGV and longjmp from the handler. */
    ++sigsegv_count;
    /* We assume the stolen register doesn't change between our inlined asm and
     * later C code.  If necessary we could put the whole thing in asm but that
     * does not seem needed.
     */
    SET_STOLEN_REG_TO_SENTINEL();
    if (SIGSETJMP(mark) == 0) {
        cause_sigsegv();
    }
    val = get_stolen_reg_val();
    if (val != STOLEN_REG_SENTINEL) {
        print("ERROR: Stolen register %d not preserved past longjmp: %d\n",
              STOLEN_REG_SENTINEL, val);
    }

    /* Now test synchall from another thread (the initiating thread does not
     * hit the i#4495 issue).
     */
    thread_t thread = create_thread(thread_func, NULL);
    /* The stolen-reg.dll.c client looks for this exact sequence of instructions. */
#if defined(AARCH64)
    __asm__ __volatile__("mov x28, #" STRINGIFY(STOLEN_REG_SENTINEL) "\n\t"
                                                                     "mov x0, #0\n\t"
                                                                     "nop\n\t"
                         :
                         :
                         : "x0", "x28");
#elif defined(ARM)
    __asm__ __volatile__("mov r10, #" STRINGIFY(STOLEN_REG_SENTINEL) "\n\t"
                                                                     "mov r0, #0\n\t"
                                                                     "nop\n\t"
                         :
                         :
                         : "r0", "r10");
#else
#    error Unsupported arch
#endif
    /* Avoid making calls or anything that might save+restore the stolen reg between
     * the asm and this loop; else we risk test failure (i#4671). Thus we
     * use atomics for inlined release-acquire to ensure no load-store reordering.
     */
    atomic_store_explicit(&ready_for_thread, true, memory_order_release);
    while (!atomic_load_explicit(&thread_finished, memory_order_acquire)) {
        /* We need to ensure we're *translated* which won't always happen if we're sitting
         * at a syscall.  So we deliberately spin.
         */
    }

    join_thread(thread);

    val = get_stolen_reg_val();
    if (val != STOLEN_REG_SENTINEL) {
        print("ERROR: Stolen register %d not preserved past synchall: %d\n",
              STOLEN_REG_SENTINEL, val);
    }

    // Checking for this sequence in stolen-reg.dll.c
#if defined(AARCH64)
    __asm__ __volatile__("mov x28, #0xdead" : : : "x28");
#elif defined(ARM)
    __asm__ __volatile__("movw r10, #0xdead" : : : "r10");
#else
#    error Unsupported arch
#endif

    print("Done\n");
}
