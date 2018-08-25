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
 * a "burst" of execution and memory allocations (malloc() and free())
 * in the middle of the application. It then detaches. Later it re-attaches
 * and detaches again for several times.
 */

/* Like burst_static we deliberately do not include configure.h here */

#include "dr_api.h"
#include <assert.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>

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
    static const int iters = 1000;
    double *val = new double; // libc malloc is called inside new
    double **vals = (double **)calloc(iters, sizeof(double *));
    *val = arg;

    for (int i = 0; i < iters; ++i) {
        vals[i] = (double *)malloc(sizeof(double));
        *vals[i] = sin(*val);
        *val += *vals[i];
    }
    for (int i = 0; i < iters; i++) {
        *val += *vals[i];
    }
    for (int i = 0; i < iters; i++) {
        free(vals[i]);
    }
    free(vals);
    double temp = *val;
    delete val; // libc free is called inside delete
    return (temp > 0);
}

int
main(int argc, const char *argv[])
{
    /* We also test -rstats_to_stderr */
    if (!my_setenv("DYNAMORIO_OPTIONS",
                   "-stderr_mask 0xc -rstats_to_stderr"
                   " -client_lib ';;-offline -record_heap"
                   " -record_function \"malloc|0|1\"'"))
        std::cerr << "failed to set env var!\n";

    for (int i = 0; i < 3; i++) {
        std::cerr << "pre-DR init\n";
        dr_app_setup();
        assert(!dr_app_running_under_dynamorio());

        std::cerr << "pre-DR start\n";
        if (do_some_work(i * 1) < 0)
            std::cerr << "error in computation\n";

        dr_app_start();
        if (do_some_work(i * 2) < 0)
            std::cerr << "error in computation\n";
        std::cerr << "pre-DR detach\n";
        dr_app_stop_and_cleanup();

        if (do_some_work(i * 3) < 0)
            std::cerr << "error in computation\n";
        std::cerr << "all done\n";
    }

    return 0;
}

#if defined(UNIX) && defined(TEST_APP_DR_CLIENT_MAIN)
#    ifdef __cplusplus
extern "C" {
#    endif

/* This dr_client_main should be called instead of the one in tracer.cpp */
DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    std::cerr << "app dr_client_main\n";
    drmemtrace_client_main(id, argc, argv);
}

#    ifdef __cplusplus
}
#    endif
#endif /* UNIX && TEST_APP_DR_CLIENT_MAIN */
