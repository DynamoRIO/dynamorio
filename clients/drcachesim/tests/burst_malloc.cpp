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
 * a "burst" of execution and memory allocations (malloc() and free())
 * in the middle of the application. It then detaches. Later it re-attaches
 * and detaches again for several times.
 */

/* Like burst_static we deliberately do not include configure.h here */

#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "scheduler.h"
#include "tracer/raw2trace.h"
#include "tracer/raw2trace_directory.h"

#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <math.h>
#include <stdlib.h>

namespace dynamorio {
namespace drmemtrace {

int realloc_id = -1;

bool
my_setenv(const char *var, const char *value)
{
#ifdef UNIX
    return setenv(var, value, 1 /*override*/) == 0;
#else
    return SetEnvironmentVariable(var, value) == TRUE;
#endif
}

// Test recording large values that require two entries.
extern "C" ptr_uint_t
return_big_value(int arg)
{
    return (((ptr_uint_t)1 << (8 * sizeof(ptr_uint_t) - 1)) - 1) | arg;
}

#ifdef UNIX
// Test i#4451 same-PC functions with differing arg counts.
// UNIX-only to simplify the aliasing setup.
// We build with -Wno-attribute-alias to avoid warnings here.
extern "C" {
int
has_aliases(int arg1, int arg2)
{
    return arg1;
}

int
alias_1arg(int arg1) __attribute__((alias("has_aliases")));
int
alias_3args(int arg1, int arg2, int arg3) __attribute__((alias("has_aliases")));
}
#endif

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
        *val += *vals[i] + (double)return_big_value(i);
#ifdef UNIX
        *val += has_aliases(i, i);
#endif
        vals[i] = (double *)realloc(vals[i], 2 * sizeof(double));
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
    bool found_calloc = false;
    bool found_realloc = false;
    bool found_return_big_value = false;
    int found_alias_count = 0;
    while (std::getline(stream, line)) {
        assert(line.find('!') != std::string::npos);
        if (line.find("!return_big_value") != std::string::npos)
            found_return_big_value = true;
        if (line.find("!malloc") != std::string::npos)
            found_malloc = true;
        if (line.find("!calloc") != std::string::npos)
            found_calloc = true;
        if (line.find("!realloc") != std::string::npos &&
            line.find("libc.so") != std::string::npos) {
            found_realloc = true;
            std::istringstream iss(line);
            char comma;
            iss >> realloc_id >> comma;
        }
#ifdef UNIX
        if (line.find("alias") != std::string::npos) {
            ++found_alias_count;
            // The min arg count should be present.
            assert(line.find(",1,") != std::string::npos);
        }
#endif
    }
    assert(found_malloc);
    assert(found_calloc);
    assert(found_realloc);
    assert(realloc_id >= 0);
    assert(found_return_big_value);
#ifdef UNIX
    // All 3 should be in the file, even though 2 had duplicate PC's.
    assert(found_alias_count == 3);
#endif
}

/* XXX: Some of this is very similar to code in other tests like burst_traceopts
 * and burst_futex.  Maybe we can share some of it through common library.
 */
static std::string
post_process()
{
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string outdir = std::string(raw_dir) + DIRSEP + "malloc";
    void *dr_context = dr_standalone_init();
    /* Use a new scope to free raw2trace_directory_t before dr_standalone_exit().
     * We could alternatively make a scope exit template helper.
     */
    {
        raw2trace_directory_t dir;
        if (!dr_create_dir(outdir.c_str())) {
            std::cerr << "Failed to create output dir";
            assert(false);
        }
        std::string dir_err = dir.initialize(raw_dir, outdir);
        assert(dir_err.empty());
        raw2trace_t raw2trace(dir.modfile_bytes_, dir.in_files_, dir.out_files_,
                              dir.out_archives_, dir.encoding_file_,
                              dir.serial_schedule_file_, dir.cpu_schedule_file_,
                              dr_context,
                              0
#ifdef WINDOWS
                              /* FIXME i#3983: Creating threads in standalone mode
                               * causes problems.  We disable the pool for now.
                               */
                              ,
                              0
#endif
        );
        std::string error = raw2trace.do_conversion();
        if (!error.empty()) {
            std::cerr << "raw2trace failed: " << error << "\n";
            assert(false);
        }
    }
    dr_standalone_exit();
    return outdir;
}

void
walk_trace(const std::string &tracedir)
{
    // Now walk the trace and ensure it has futex markers.
    void *dr_context = dr_standalone_init();

    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(tracedir);
    if (scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        std::cerr << "Failed to initialize scheduler " << scheduler.get_error_string()
                  << "\n";
    }

    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    int arg_ord = 0;
    bool saw_realloc = false;
    bool saw_realloc_args = false;
    bool in_realloc_now = false;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        if (memref.marker.type != TRACE_TYPE_MARKER)
            continue;
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_FUNC_ID) {
            if (memref.marker.marker_value == realloc_id) {
                saw_realloc = true;
                in_realloc_now = true;
            } else
                in_realloc_now = false;
        }
        if (in_realloc_now && memref.marker.marker_type == TRACE_MARKER_TYPE_FUNC_ARG) {
            saw_realloc_args = true;
        }
        if (in_realloc_now &&
            memref.marker.marker_type == TRACE_MARKER_TYPE_FUNC_RETVAL) {
            // Should have succeeded.
            assert(memref.marker.marker_value > 0);
        }
    }
    assert(saw_realloc);
    assert(saw_realloc_args);

    dr_standalone_exit();
}

int
test_main(int argc, const char *argv[])
{
    /* We also test -rstats_to_stderr */
    if (!my_setenv("DYNAMORIO_OPTIONS",
                   "-stderr_mask 0xc -rstats_to_stderr"
                   " -client_lib ';;-offline -record_heap"
                   // Test the low-overhead-wrapping option.
                   " -record_replace_retaddr"
#ifdef UNIX
                   // Test aliases with differing arg counts.
                   " -record_function \"has_aliases|2&alias_1arg|1&alias_3args|3\""
#endif
                   // Deliberately test a duplication warning on "malloc".
                   // Test large values that require two entries.
                   " -record_function \"malloc|1&return_big_value|1\"'"))
        std::cerr << "failed to set env var!\n";

    for (int i = 0; i < 2; i++) {
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
        dr_app_stop_and_cleanup();

        if (do_some_work(i * 3) < 0)
            std::cerr << "error in computation\n";
        std::cerr << "all done\n";
    }

    std::string tracedir = post_process();
    walk_trace(tracedir);

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
