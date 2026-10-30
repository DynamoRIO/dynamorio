/* **********************************************************
 * Copyright (c) 2019-2025 Google, Inc.  All rights reserved.
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

// Tests timeout scaling during tracing.

// Enable asserts in release build testing too.
#undef NDEBUG

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include <chrono>
#include <iostream>
#include <string>

#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "drcovlib.h"
#include "analysis_tool.h"
#include "scheduler.h"
#include "tracer/raw2trace.h"
#include "tracer/raw2trace_directory.h"
#include "../../core/unix/include/syscall_target.h"

#ifndef LINUX
#    error Only Linux supported for this test.
#endif

namespace dynamorio {
namespace drmemtrace {

bool
my_setenv(const char *var, const char *value)
{
    return setenv(var, value, 1 /*override*/) == 0;
}

/****************************************************************************
 * Code that gets traced.
 */

static constexpr int NUM_SLEEPS = 5;

static int64
measure_sleep_time()
{
    struct timespec sleeptime = { 0, 10000000 };
    auto start = std::chrono::steady_clock::now();

    // Warm the clock and sleep paths before measuring a fixed sleep workload.
    for (int i = 0; i <= NUM_SLEEPS; ++i) {
        if (i == 1)
            start = std::chrono::steady_clock::now();
        int res = nanosleep(&sleeptime, nullptr);
        assert(res == 0);
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now() - start)
        .count();
}

/****************************************************************************
 * Trace processing code.
 */

static std::string
post_process(const std::string &out_subdir)
{
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string outdir = std::string(raw_dir) + DIRSEP + out_subdir;
    void *dr_context = dr_standalone_init();
    /* Now write a final trace to a location that the drcachesim -indir step
     * run by the outer test harness will find (TRACE_FILENAME).
     * Use a new scope to free raw2trace_directory_t before dr_standalone_exit().
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
                              dr_context, 0);
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
gather_trace(const std::string &tracer_ops, const std::string &out_subdir, int64 *elapsed)
{
    std::string dr_ops("-stderr_mask 0xc -client_lib ';;-offline " + tracer_ops + "'");
    if (!my_setenv("DYNAMORIO_OPTIONS", dr_ops.c_str()))
        std::cerr << "failed to set env var!\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    dr_app_start();
    assert(dr_app_running_under_dynamorio());
    *elapsed = measure_sleep_time();
    assert(*elapsed > 0);
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());

    return post_process(out_subdir);
}

static int
count_sleeps(const std::string &dir)
{
    int count = 0;
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(dir);
    if (scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        std::cerr << "Failed to initialize scheduler " << scheduler.get_error_string()
                  << "\n";
    }
    auto *stream = scheduler.get_stream(0);
    while (true) {
        memref_t memref;
        scheduler_t::stream_status_t status = stream->next_record(memref);
        if (status == scheduler_t::STATUS_EOF) {
            break;
        }
        assert(status == scheduler_t::STATUS_OK);
        if (memref.marker.type == TRACE_TYPE_MARKER &&
            memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL &&
            (memref.marker.marker_value == SYS_nanosleep ||
             memref.marker.marker_value == SYS_clock_nanosleep)) {
            ++count;
        }
    }
    return count;
}

int
test_main(int argc, const char *argv[])
{
    int64 elapsed_default, elapsed_scale;
    std::string dir_default = gather_trace("", "default", &elapsed_default);
    std::string dir_scale = gather_trace("-scale_timeouts 20", "scale", &elapsed_scale);

    void *dr_context = dr_standalone_init();

    int sleeps_default = count_sleeps(dir_default);
    int sleeps_scale = count_sleeps(dir_scale);
    std::cerr << "sleeps default=" << sleeps_default << " scale=" << sleeps_scale << "\n";
    assert(sleeps_default == NUM_SLEEPS + 1);
    assert(sleeps_scale == NUM_SLEEPS + 1);
    std::cerr << "sleep time (ns) default=" << elapsed_default
              << " scale=" << elapsed_scale << "\n";
    // A 20x timeout scale must yield at least a 2x duration despite scheduler noise.
    assert(elapsed_scale > 2 * elapsed_default);

    dr_standalone_exit();
    std::cerr << "all done\n";
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
