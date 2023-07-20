/* **********************************************************
 * Copyright (c) 2018-2023 Google, Inc.  All rights reserved.
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
 * It tests the thread filtering feature.
 */

/* We deliberately do not include configure.h here to simulate what an
 * actual app will look like.  configure_DynamoRIO_static sets DR_APP_EXPORTS
 * for us.
 */
#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include <assert.h>
#include <iostream>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#ifndef UNIX
#    include <process.h>
#endif
#include "../../../suite/tests/condvar.h"

namespace dynamorio {
namespace drmemtrace {

static const int num_threads = 8;
static const int burst_owner = 4;
static bool finished[num_threads];
static uint tid[num_threads];
#ifdef UNIX
static int filter_call_count;
#endif
static void *burst_owner_finished;

bool
my_setenv(const char *var, const char *value)
{
#ifdef UNIX
    return setenv(var, value, 1 /*override*/) == 0;
#else
    return SetEnvironmentVariable(var, value) == TRUE;
#endif
}

static file_t
open_nothing(const char *fname, uint mode_flags)
{
    return (file_t)(1UL);
}

static void
close_nothing(file_t file)
{
    /* nothing */
}

static bool
create_no_dir(const char *dir)
{
    return true;
}

static ssize_t
write_nothing(file_t file, const void *data, size_t size)
{
    return size;
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

static bool
should_trace_thread_cb(thread_id_t thread_id, void *user_data)
{
    /* Test user_data across reattach (see setup below). */
    if (*static_cast<int *>(user_data) == 0)
        std::cerr << "invalid user_data (likely reattach error)\n";
#ifdef UNIX
    /* We have no simple way to get the id (short of letting each thread get
     * its own and using synch to wait for them all) so we just take the 1st N.
     * We assume this is called exactly once per thread.  We are fine with races.
     */
    return filter_call_count++ < num_threads / 2;
#else
    for (uint i = 0; i < num_threads; i++) {
        if (thread_id == tid[i])
            return i % 2 == 0;
    }
    return true;
#endif
}

#ifdef WINDOWS
unsigned int __stdcall
#else
void *
#endif
    thread_func(void *arg)
{
    unsigned int idx = (unsigned int)(uintptr_t)arg;
    static const int reattach_iters = 4;
    static const int outer_iters = 2048;
    /* We trace a 4-iter burst of execution. */
    static const int iter_start = outer_iters / 3;
    static const int iter_stop = iter_start + 4;
    int *cb_arg[reattach_iters];

    /* We use an outer loop to test re-attaching (i#2157). */
    for (int j = 0; j < reattach_iters; ++j) {
        if (idx == burst_owner) {
            std::cerr << "pre-DR init\n";
            assert(!dr_app_running_under_dynamorio());
            /* We test reattach state clearing by passing a memory location as
             * user data that we clear on reattach, and we do not filter on one
             * of the runs.
             */
            cb_arg[j] = new int;
            *cb_arg[j] = 1;
            if (j == 1) {
                /* Not filtering on this run, so we don't want output to avoid
                 * messing up the template.
                 */
                drmemtrace_status_t res = drmemtrace_replace_file_ops(
                    open_nothing, nullptr, write_nothing, close_nothing, create_no_dir);
                assert(res == DRMEMTRACE_SUCCESS);
            } else {
#ifdef UNIX
                filter_call_count = 0;
#endif
                drmemtrace_status_t res =
                    drmemtrace_filter_threads(should_trace_thread_cb, cb_arg[j]);
                assert(res == DRMEMTRACE_SUCCESS);
            }
            dr_app_setup();
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
                *cb_arg[j] = 0;
            }
        }
    }
    if (idx == burst_owner) {
        for (int j = 0; j < reattach_iters; ++j)
            delete cb_arg[j];
        signal_cond_var(burst_owner_finished);
    } else {
        /* Avoid having < num_threads/2 threads in our filter. */
        wait_cond_var(burst_owner_finished);
    }
    finished[idx] = true;
    return 0;
}

int
test_main(int argc, const char *argv[])
{
#ifdef UNIX
    pthread_t thread[num_threads];
#else
    uintptr_t thread[num_threads];
#endif

    /* While the start/stop thread only runs 4 iters, the other threads end up
     * running more and their trace files get up to 65MB or more, with the
     * merged result several GB's: too much for a test.  We thus cap each thread.
     * We run with -record_heap to ensure we test that combination.
     */
    std::string ops = std::string(
        "-stderr_mask 0xc -client_lib ';;-offline -record_heap -max_trace_size 256K ");
    /* Support passing in extra tracer options. */
    for (int i = 1; i < argc; ++i)
        ops += std::string(argv[i]) + " ";
    ops += "'";
    if (!my_setenv("DYNAMORIO_OPTIONS", ops.c_str()))
        std::cerr << "failed to set env var!\n";

    burst_owner_finished = create_cond_var();
    for (uint i = 0; i < num_threads; i++) {
#ifdef UNIX
        pthread_create(&thread[i], NULL, thread_func, (void *)(uintptr_t)i);
#else
        thread[i] =
            _beginthreadex(NULL, 0, thread_func, (void *)(uintptr_t)i, 0, &tid[i]);
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
    std::cerr << "all done\n";
    destroy_cond_var(burst_owner_finished);
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
