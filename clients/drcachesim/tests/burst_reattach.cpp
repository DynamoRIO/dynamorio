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
 * a "burst" of execution. It then detaches, and it later re-attaches and
 * detaches multiple times. Its purpose is to detect issues of using
 * statically linked DR with a very high number of re-attaches.
 */

/* Like burst_static we deliberately do not include configure.h here */
#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <stdlib.h>

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
    static const int iters = 100;
    double *val = new double; // libc malloc is called inside new
    *val = arg;
    for (int i = 0; i < iters; ++i) {
        *val += sin(*val);
    }
    double temp = *val;
    delete val; // libc free is called inside delete
    return (temp > 0);
}

static void
exit_cb(void *)
{
    const char *funclist_path;
    drmemtrace_status_t res = drmemtrace_get_funclist_path(&funclist_path);
    assert(res == DRMEMTRACE_SUCCESS);
    std::ifstream stream(funclist_path);
    assert(stream.good());
    std::string line;
    bool found_malloc = false;
    bool found_return_big_value = false;
    while (std::getline(stream, line)) {
        assert(line.find('!') != std::string::npos);
        if (line.find("!malloc") != std::string::npos)
            found_malloc = true;
    }
    assert(found_malloc);
}

int
test_main(int argc, const char *argv[])
{
    if (!my_setenv("DYNAMORIO_OPTIONS",
                   "-stderr_mask 0xc"
                   " -client_lib '#;;-offline"
                   // Minimize symbol lookup overheads.
                   " -record_dynsym_only"
                   // Test the low-overhead-wrapping option.
                   " -record_replace_retaddr"
                   " -record_function \"malloc|1\"'"))
        std::cerr << "failed to set env var!\n";

    for (int i = 0; i < 100; i++) {
        std::cerr << "pre-DR init\n";
        dr_app_setup();
        assert(!dr_app_running_under_dynamorio());

        drmemtrace_status_t res = drmemtrace_buffer_handoff(nullptr, exit_cb, nullptr);
        assert(res == DRMEMTRACE_SUCCESS);

        std::cerr << "pre-DR start\n";
        if (do_some_work(i * 1) < 0)
            std::cerr << "error in computation\n";

        dr_app_start();
        if (do_some_work(i * 2) < 0)
            std::cerr << "error in computation\n";
        std::cerr << "pre-DR detach\n";
        assert(dr_app_running_under_dynamorio());
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

} // namespace drmemtrace
} // namespace dynamorio
