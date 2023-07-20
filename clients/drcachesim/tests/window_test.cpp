/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY /* C code */
#    include <unistd.h>
#    include "dr_api.h"
#    include "drmemtrace/drmemtrace.h"
#    include "drcovlib.h"
#    include "analysis_tool.h"
#    include "scheduler.h"
#    include "tracer/raw2trace.h"
#    include "tracer/raw2trace_directory.h"
#    include <assert.h>
#    include <iostream>
#    include <math.h>
#    include <stdlib.h>
#    include <string.h>

namespace dynamorio {
namespace drmemtrace {

constexpr int SECONDS_TO_SLEEP = 4;

bool
my_setenv(const char *var, const char *value)
{
#    ifdef UNIX
    return setenv(var, value, 1 /*override*/) == 0;
#    else
    return SetEnvironmentVariable(var, value) == TRUE;
#    endif
}

char
noargs(void)
{
    return 'B';
}

void
noret_func(int x, int y)
{
    if (x != y)
        noret_func(x + 1, y);
}

int
fib_with_sleep(int n)
{
    std::cerr << "Sleeping for " << SECONDS_TO_SLEEP << " seconds\n";
    sleep(SECONDS_TO_SLEEP);
    if (n <= 1)
        return 1;
    noret_func(n, n + 1);
    noargs();
    return fib_with_sleep(n - 1) + fib_with_sleep(n - 2);
}

static std::string
post_process(const std::string &out_subdir)
{
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string outdir = std::string(raw_dir) + DIRSEP + "../trace";
    std::cerr << "rawdir: " << raw_dir << "\n";
    std::cerr << "outdir: " << outdir << "\n";
    void *dr_context = dr_standalone_init();
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
#    ifdef WINDOWS
                              /* TODO i#3983: Creating threads in standalone mode
                               * causes problems.  We disable the pool for now.
                               */
                              ,
                              0
#    endif
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

static std::string
gather_trace(const std::string &out_subdir)
{
    std::string dr_ops(
        "-stderr_mask 0xc -client_lib ';;-offline -offline -trace_after_instrs 10000 "
        "-trace_for_instrs 2000 -retrace_every_instrs 1000");
    std::cerr << "dr_ops: " << dr_ops << "\n";
    if (!my_setenv("DYNAMORIO_OPTIONS", dr_ops.c_str()))
        std::cerr << "failed to set env var!\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    dr_app_start();
    assert(dr_app_running_under_dynamorio());
    fib_with_sleep(6);
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());
    return post_process(out_subdir);
}

int
main(int argc, const char *argv[])
{
    std::string dir_fib = gather_trace("window.0001");
    void *dr_context = dr_standalone_init();
    scheduler_t scheduler_fib;
    std::vector<scheduler_t::input_workload_t> sched_opt_inputs;
    sched_opt_inputs.emplace_back(dir_fib);
    if (scheduler_fib.init(sched_opt_inputs, 1,
                           scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        std::cerr << "Failed to initialize scheduler " << scheduler_fib.get_error_string()
                  << "\n";
    }

    auto *stream_fib = scheduler_fib.get_stream(0);
    uint64_t first_timestamp = 0;
    while (true) {
        memref_t memref_fib;
        scheduler_t::stream_status_t status_fib = stream_fib->next_record(memref_fib);
        assert(status_fib == scheduler_t::STATUS_OK);
        if (memref_fib.marker.type == TRACE_TYPE_MARKER &&
            memref_fib.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
            if (first_timestamp != 0) {
                if ((memref_fib.marker.marker_value - first_timestamp) >
                    SECONDS_TO_SLEEP * 1000000) {
                    std::cerr << "window_test FAILED\n";
                    exit(1);
                } else {
                    std::cerr << "Did not find a window bug\n";
                }
                break;
            }
            first_timestamp = memref_fib.marker.marker_value;
        }
    }
    dr_standalone_exit();
    std::cerr << "window_test passed\n";
    return 0;
}
} // namespace drmemtrace
} // namespace dynamorio
#endif
