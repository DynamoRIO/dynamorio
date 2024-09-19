/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

/* This application links in drmemtrace_static and acquires a trace during
 * a "burst" of execution in the middle of the application.  It then detaches.
 */

/* We deliberately do not include configure.h here to simulate what an
 * actual app will look like.  configure_DynamoRIO_static sets DR_APP_EXPORTS
 * for us.
 */
#include "dr_api.h"
#include <assert.h>
#include <iostream>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef UNIX
#    include <signal.h>
#    include <ucontext.h>
#else
#    include <process.h>
#endif
#include "../../../suite/tests/condvar.h"

namespace dynamorio {
namespace drmemtrace {

static const int num_threads = 8;
static const int num_idle_threads = 40;
static const int burst_owner = 4;
static bool finished[num_threads];
static void *burst_owner_starting;
static void *burst_owner_finished;
static void *idle_thread_started[num_idle_threads];
static void *idle_should_exit;

bool
my_setenv(const char *var, const char *value)
{
#ifdef UNIX
    return setenv(var, value, 1 /*override*/) == 0;
#else
    return SetEnvironmentVariable(var, value) == TRUE;
#endif
}

static int
do_some_work(int i)
{
    static int iters = 512;
    double val = (double)i;
    for (int i = 0; i < iters; ++i) {
        val += sin(val);
    }
    return (val > 0);
}

#ifdef WINDOWS
unsigned int __stdcall
#else
void *
#endif
    thread_func(void *arg)
{
    unsigned int idx = (unsigned int)(uintptr_t)arg;
    // We need the other threads to do enough work to be still executing
    // code at detach time to stress state restore events.
    int reattach_iters = (idx == burst_owner) ? 4 : 32;
    static const int outer_iters = 2048;
    /* We trace a 4-iter burst of execution. */
    static const int iter_start = outer_iters / 3;
    static const int iter_stop = iter_start + 4;

#ifdef UNIX
    /* We test sigaltstack with attach+detach to avoid bugs like i#3116 */
    stack_t sigstack;
#    define ALT_STACK_SIZE (SIGSTKSZ * 2)
    sigstack.ss_sp = (char *)malloc(ALT_STACK_SIZE);
    sigstack.ss_size = ALT_STACK_SIZE;
    sigstack.ss_flags = SS_ONSTACK;
    int res = sigaltstack(&sigstack, NULL);
    assert(res == 0);
#endif

    if (idx != burst_owner)
        wait_cond_var(burst_owner_starting);
    else
        signal_cond_var(burst_owner_starting);

    /* We use an outer loop to test re-attaching (i#2157). */
    for (int j = 0; j < reattach_iters; ++j) {
        if (idx == burst_owner) {
            std::cerr << "pre-DR init\n";
            dr_app_setup();
            assert(!dr_app_running_under_dynamorio());
        }
        for (int i = 0; i < outer_iters; ++i) {
            if (idx == burst_owner && i == iter_start) {
                std::cerr << "pre-DR start\n";
                dr_app_start();
            }
            if (idx == burst_owner) {
                if (i >= iter_start && i <= iter_stop)
                    assert(dr_app_running_under_dynamorio());
                else
                    assert(!dr_app_running_under_dynamorio());
            }
            if (do_some_work(i) < 0)
                std::cerr << "error in computation\n";
            if (idx == burst_owner && i == iter_stop) {
                std::cerr << "pre-DR detach\n";
                dr_app_stop_and_cleanup();
            }
        }
    }

#ifdef UNIX
    stack_t check_stack;
    res = sigaltstack(NULL, &check_stack);
    assert(res == 0 && check_stack.ss_sp == sigstack.ss_sp &&
           check_stack.ss_size == sigstack.ss_size);
    sigstack.ss_flags = SS_DISABLE;
    res = sigaltstack(&sigstack, NULL);
    assert(res == 0);
    free(sigstack.ss_sp);
#endif

    if (idx == burst_owner) {
        signal_cond_var(burst_owner_finished);
    } else {
        /* Avoid having < 1 thread per core in the output. */
        wait_cond_var(burst_owner_finished);
    }
    finished[idx] = true;
    return 0;
}

#ifdef WINDOWS
unsigned int __stdcall
#else
void *
#endif
    idle_thread_func(void *arg)
{
    unsigned int idx = (unsigned int)(uintptr_t)arg;
    signal_cond_var(idle_thread_started[idx]);
    wait_cond_var(idle_should_exit);
    return 0;
}

int
test_main(int argc, const char *argv[])
{
#ifdef UNIX
    pthread_t thread[num_threads];
    pthread_t idle_thread[num_idle_threads];
#else
    uintptr_t thread[num_threads];
    uintptr_t idle_thread[num_idle_threads];
#endif

    // While the start/stop thread only runs 4 iters, the other threads end up
    // running more and their trace files get up to 65MB or more, with the
    // merged result several GB's: too much for a test.  We thus cap each thread.
    // We set -disable_traces to help stress state recreation
    // in drbbdup with prefixes on every block.
    std::string ops = std::string(
        "-stderr_mask 0xc -disable_traces -client_lib ';;-offline -align_endpoints "
        "-max_trace_size 256K ");
    /* Support passing in extra tracer options. */
    for (int i = 1; i < argc; ++i)
        ops += std::string(argv[i]) + " ";
    ops += "'";
    if (!my_setenv("DYNAMORIO_OPTIONS", ops.c_str()))
        std::cerr << "failed to set env var!\n";

    // Create some threads that do nothing to test -align_endpoints omitting them.
    // On Linux, DR's attach wakes these up and the auto-restart SYS_futex runs
    // one syscall instruction which depending on end-of-attach timing may be
    // emitted and so the thread will show up.  But with enough threads we can be
    // pretty sure very few of them will be in the trace.
    idle_should_exit = create_cond_var();
    for (uint i = 0; i < num_idle_threads; i++) {
        idle_thread_started[i] = create_cond_var();
#ifdef UNIX
        pthread_create(&idle_thread[i], NULL, idle_thread_func, (void *)(uintptr_t)i);
#else
        idle_thread[i] =
            _beginthreadex(NULL, 0, idle_thread_func, (void *)(uintptr_t)i, 0, NULL);
#endif
        wait_cond_var(idle_thread_started[i]);
    }

    // Create the main thread pool.
    burst_owner_starting = create_cond_var();
    burst_owner_finished = create_cond_var();
    for (uint i = 0; i < num_threads; i++) {
#ifdef UNIX
        pthread_create(&thread[i], NULL, thread_func, (void *)(uintptr_t)i);
#else
        thread[i] = _beginthreadex(NULL, 0, thread_func, (void *)(uintptr_t)i, 0, NULL);
#endif
    }
    for (uint i = 0; i < num_threads; i++) {
#ifdef UNIX
        pthread_join(thread[i], NULL);
#else
        WaitForSingleObject((HANDLE)thread[i], INFINITE);
#endif
    }
    for (int i = 0; i < num_threads; ++i) {
        if (!finished[i])
            std::cerr << "thread " << i << " failed to finish\n";
    }
    // Exit the idle threads.
    signal_cond_var(idle_should_exit);
    for (uint i = 0; i < num_idle_threads; i++) {
#ifdef UNIX
        pthread_join(idle_thread[i], NULL);
#else
        WaitForSingleObject((HANDLE)idle_thread[i], INFINITE);
#endif
        destroy_cond_var(idle_thread_started[i]);
    }
    std::cerr << "all done\n";
    destroy_cond_var(burst_owner_starting);
    destroy_cond_var(burst_owner_finished);
    destroy_cond_var(idle_should_exit);
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
