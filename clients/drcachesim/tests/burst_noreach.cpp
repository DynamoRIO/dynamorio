/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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
 * a "burst" of execution in the middle of the application.  Before attaching
 * it allocates a lot of heap, preventing the statically linked client from
 * being 32-bit reachable from any available space for the code cache.
 */

/* Like burst_static we deliberately do not include configure.h here. */
#include "dr_api.h"
#include <assert.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

namespace dynamorio {
namespace drmemtrace {

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
do_some_work(int arg)
{
    static int iters = 512;
    double val = (double)arg;
    for (int i = 0; i < iters; ++i) {
        val += sin(val);
    }
    return (val > 0);
}

static void
fill_up_heap()
{
    void *cur_brk = sbrk(0);
    void *new_brk = (byte *)cur_brk + 2 * 1024 * 1024 * 1024ULL;
    int res = brk(new_brk);
}

int
test_main(int argc, const char *argv[])
{
    static int outer_iters = 2048;
    /* We trace a 4-iter burst of execution. */
    static int iter_start = outer_iters / 3;
    static int iter_stop = iter_start + 4;

    fill_up_heap();

    if (!my_setenv("DYNAMORIO_OPTIONS",
                   "-stderr_mask 0xc -vm_size 512M "
                   "-no_reachable_client -client_lib ';;-offline'"))
        std::cerr << "failed to set env var!\n";

    /* We use an outer loop to test re-attaching (i#2157). */
    for (int j = 0; j < 3; ++j) {
        std::cerr << "pre-DR init\n";
        dr_app_setup();
        assert(!dr_app_running_under_dynamorio());

        for (int i = 0; i < outer_iters; ++i) {
            if (i == iter_start) {
                std::cerr << "pre-DR start\n";
                dr_app_start();
            }
            if (i >= iter_start && i <= iter_stop)
                assert(dr_app_running_under_dynamorio());
            else
                assert(!dr_app_running_under_dynamorio());
            if (do_some_work(i) < 0)
                std::cerr << "error in computation\n";
            if (i == iter_stop) {
                std::cerr << "pre-DR detach\n";
                dr_app_stop_and_cleanup();
            }
        }
        std::cerr << "all done\n";
    }
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
