/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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
#ifndef UNIX
# include <process.h>
#endif

static const int num_threads = 8;
static const int burst_owner = 4;
static bool finished[num_threads];

bool
my_setenv(const char *var, const char *value)
{
#ifdef UNIX
    return setenv(var, value, 1/*override*/) == 0;
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
    static const int outer_iters = 2048;
    /* We trace a 4-iter burst of execution. */
    static const int iter_start = outer_iters/3;
    static const int iter_stop = iter_start + 4;

    /* We use an outer loop to test re-attaching (i#2157), except
     * there is an unfixed bug i#2175.
     * XXX i#2175: up the iter count once we fix the bug.
     */
    for (int j = 0; j < 1; ++j) {
        if (j > 0 && idx == burst_owner)
            dr_app_setup();
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
    finished[idx] = true;
    return 0;
}

int
main(int argc, const char *argv[])
{
#ifdef UNIX
    pthread_t thread[num_threads];
#else
    uintptr_t thread[num_threads];
#endif

    /* While the start/stop thread only runs 4 iters, the other threads end up
     * running more and their trace files get up to 65MB or more, with the
     * merged result several GB's: too much for a test.  We thus cap each thread.
     */
    if (!my_setenv("DYNAMORIO_OPTIONS", "-stderr_mask 0xc -client_lib ';;-offline -max_trace_size 256K'"))
        std::cerr << "failed to set env var!\n";

    std::cerr << "pre-DR init\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());

    for (uint i = 0; i < num_threads; i++) {
#ifdef UNIX
        pthread_create(&thread[i], NULL, thread_func, (void*)(uintptr_t)i);
#else
        thread[i] = _beginthreadex(NULL, 0, thread_func, (void*)(uintptr_t)i, 0, NULL);
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
    return 0;
}
