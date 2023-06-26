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

/* This application links in drmemtrace_static and acquires a trace during
 * a "burst" of execution during which SYS_futex is called.  The resulting
 * trace is analyzed to confirm that futex parameters were included.
 */

#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "analysis_tool.h"
#include "scheduler.h"
#include "tracer/raw2trace.h"
#include "tracer/raw2trace_directory.h"

#include <assert.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <iostream>

namespace dynamorio {
namespace drmemtrace {

static uint32_t futex_var;

bool
my_setenv(const char *var, const char *value)
{
#ifdef UNIX
    return setenv(var, value, 1 /*override*/) == 0;
#else
    return SetEnvironmentVariable(var, value) == TRUE;
#endif
}

static void
do_some_work()
{
    // Make at least one futex call with values we can check in post-processing.
    long res = syscall(SYS_futex, &futex_var, FUTEX_WAKE, /*#wakeup=*/1,
                       /*timeout=*/nullptr, /*uaddr2=*/nullptr, /*val3=*/0);
}

/* XXX: Some of this is very similar to code in other tests like burst_traceopts.
 * Maybe we can share some of it through common library.
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

static std::string
gather_trace(const std::string &tracer_ops, const std::string &out_subdir)
{
    std::string dr_ops("-stderr_mask 0xc -client_lib ';;-offline " + tracer_ops + "'");
    if (!my_setenv("DYNAMORIO_OPTIONS", dr_ops.c_str()))
        std::cerr << "failed to set env var!\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    dr_app_start();
    assert(dr_app_running_under_dynamorio());
    do_some_work();
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());

    return post_process(out_subdir);
}

int
test_main(int argc, const char *argv[])
{
    std::string tracedir = gather_trace("", "futex");

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
    bool saw_maybe_blocking = false;
    bool saw_futex_marker = false;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        if (memref.marker.type != TRACE_TYPE_MARKER)
            continue;
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL) {
            saw_maybe_blocking = true;
        }
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_FUNC_ID) {
            saw_futex_marker = true;
            assert(memref.marker.marker_value ==
                   static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) +
                       IF_X64_ELSE(SYS_futex, SYS_futex & 0xffff));
        }
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_FUNC_ARG) {
            // We assume there is no futex call in any library used here.
            switch (arg_ord) {
            case 0:
                assert(memref.marker.marker_value ==
                       reinterpret_cast<uintptr_t>(&futex_var));
                break;
            case 1: assert(memref.marker.marker_value == FUTEX_WAKE); break;
            case 2: assert(memref.marker.marker_value == 1); break;
            default: assert(memref.marker.marker_value == 0); break;
            }
            ++arg_ord;
        }
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_FUNC_RETVAL) {
            // Should have succeeded.
            assert(memref.marker.marker_value == 1);
        }
    }
    assert(saw_maybe_blocking);
    assert(saw_futex_marker);
    static constexpr int FUTEX_ARG_COUNT = 6;
    assert(arg_ord == FUTEX_ARG_COUNT);

    dr_standalone_exit();
    std::cerr << "all done\n";
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
