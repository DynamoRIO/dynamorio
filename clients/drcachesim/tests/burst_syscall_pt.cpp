/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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

// This application links in drmemtrace_static and acquires a trace during
// a "burst" of execution that includes some system call traces collected
// using Intel-PT.

// This is set globally in CMake for other tests so easier to undef here.
#undef DR_REG_ENUM_COMPATIBILITY

#include "test_helpers.h"
#include "analyzer.h"
#include "tools/basic_counts.h"
#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "drmemtrace/raw2trace.h"
#include "mock_reader.h"
#include "raw2trace_directory.h"
#include "scheduler.h"

#include <cassert>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <linux/futex.h>
#include <pthread.h>
#include <string>
#include <sys/syscall.h>

namespace dynamorio {
namespace drmemtrace {

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

/* The futex the child waits at initially. */
static uint32_t futex_var = 0xf00d;
/* The futex the child is transferred to. */
static uint32_t futex_var_other = 0x8bad;

static void *
child_futex_wait(void *)
{
    long res = syscall(SYS_futex, &futex_var, FUTEX_WAIT, /*#val=*/0xf00d,
                       /*timeout=*/nullptr, /*uaddr2=*/nullptr, /*val3=*/0);
    assert(res == 0);
    std::cerr << "Child released from futex\n";
    return NULL;
}

static void
parent_futex_wake()
{
    /* The child would be waiting at the other futex by now.
     * i#7034: Note that the child thread undergoes detach while it is waiting
     * on futex_var_other. There is a bug at this point due to a possible
     * transparency violation in DR. When the child thread restarts futex after
     * being interrupted by DR's detach signal, it is found to resume
     * waiting at the original futex_var instead of futex_var_other.
     * If we modify this app to do detach after parent_futex_wake returns, then
     * the child is found to be waiting at futex_var_other as expected.
     */
    uint32_t *child_waiting_at_futex = &futex_var;
    long res = syscall(SYS_futex, child_waiting_at_futex, FUTEX_WAKE, /*#wakeup=*/1,
                       /*timeout=*/nullptr, /*uaddr2=*/nullptr, /*val3=*/0);
    assert(res == 1);
}

static void
parent_futex_reque()
{
    long res;
    do {
        /* Repeat until the child is surely waiting at the futex. We'll know this
         * when the following call returns a 1, which means the child was
         * transferred to futex_var_other. This is to ensure that the child thread
         * is inside the futex syscall when DR detaches.
         */
        res = syscall(SYS_futex, &futex_var, FUTEX_CMP_REQUEUE, /*#wakeup_max=*/0,
                      /*#requeue_max=*/1, /*uaddr2=*/&futex_var_other, /*val3=*/0xf00d);
        assert(res == 0 || res == 1);
    } while (res == 0);
}

static int
do_some_syscalls()
{
    getpid();
    gettid();
    return 1;
}

static std::string
postprocess(void *dr_context)
{
    std::cerr << "Post-processing the trace\n";
    // Get path to write the final trace to.
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string outdir = std::string(raw_dir) + DIRSEP + "post_processed";

    const char *kcore_path;
    drmemtrace_status_t kcore_res = drmemtrace_get_kcore_path(&kcore_path);
    assert(kcore_res == DRMEMTRACE_SUCCESS);

    raw2trace_directory_t dir;
    if (!dr_create_dir(outdir.c_str()))
        FATAL_ERROR("Failed to create output dir.");
    std::string dir_err = dir.initialize(raw_dir, outdir, DEFAULT_TRACE_COMPRESSION_TYPE,
                                         /*syscall_template_file=*/"");
    assert(dir_err.empty());
    raw2trace_t raw2trace(dir.modfile_bytes_, dir.in_files_, dir.out_files_,
                          dir.out_archives_, dir.encoding_file_,
                          dir.serial_schedule_file_, dir.cpu_schedule_file_, dr_context,
                          /*verbosity=*/0, /*worker_count=*/-1,
                          /*alt_module_dir=*/"",
                          /*chunk_instr_count=*/10 * 1000 * 1000, dir.in_kfiles_map_,
                          dir.kcoredir_, /*kallsyms_path=*/"",
                          /*syscall_template_file=*/nullptr,
                          // We want to fail if any error is encountered.
                          /*pt2ir_best_effort=*/false);
    std::string error = raw2trace.do_conversion();
    if (!error.empty())
        FATAL_ERROR("raw2trace failed: %s\n", error.c_str());
    uint64 decoded_syscall_count =
        raw2trace.get_statistic(RAW2TRACE_STAT_SYSCALL_TRACES_CONVERTED);
    // We should see atleast the getpid, gettid, and futex syscalls made by the parent.
    if (decoded_syscall_count <= 2) {
        std::cerr << "Incorrect decoded syscall count (found: " << decoded_syscall_count
                  << " vs expected > 2)\n";
    }
    return outdir;
}

// Trace analysis tool that allows us to verify properties of the generated PT trace.
class pt_analysis_tool_t : public analysis_tool_t {
public:
    pt_analysis_tool_t()
    {
    }
    bool
    process_memref(const memref_t &memref) override
    {
        FATAL_ERROR("Expected to use sharded mode");
        return true;
    }
    bool
    parallel_shard_supported() override
    {
        return true;
    }
    void *
    parallel_shard_init(int shard_index, void *worker_data) override
    {
        auto per_shard = new per_shard_t;
        return reinterpret_cast<void *>(per_shard);
    }
    bool
    parallel_shard_exit(void *shard_data) override
    {
        std::lock_guard<std::mutex> guard(shard_exit_mutex_);
        per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
        if (shard->syscall_count == 0)
            return true;
        // In case the child has just the one futex syscall which was skipped
        // from the trace.
        if (shard->syscall_count > 1 && !shard->any_syscall_had_trace) {
            std::cerr << "No syscall had a trace\n";
        }
        if (shard->prev_was_futex_marker && !shard->prev_syscall_had_trace) {
            found_final_futex_without_trace_ = true;
        }
        if (shard->kernel_instr_count > 0) {
            found_some_kernel_instrs_ = true;
        }
        return true;
    }
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override
    {
        per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
        if (memref.marker.type == TRACE_TYPE_MARKER) {
            switch (memref.marker.marker_type) {
            case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
                shard->in_syscall_trace = true;
                break;
            case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
                shard->in_syscall_trace = false;
                shard->prev_syscall_had_trace = true;
                shard->any_syscall_had_trace = true;
                break;
            case TRACE_MARKER_TYPE_SYSCALL:
                ++shard->syscall_count;
                shard->prev_syscall_had_trace = false;
                if (memref.marker.marker_value == SYS_futex) {
                    shard->prev_was_futex_marker = true;
                }
                break;
            }
        }
        if (!type_is_instr(memref.data.type))
            return true;
        if (shard->in_syscall_trace) {
            ++shard->kernel_instr_count;
            return true;
        }
        shard->prev_was_futex_marker = false;
        shard->prev_syscall_had_trace = false;
        return true;
    }
    bool
    print_results() override
    {
        if (!found_final_futex_without_trace_) {
            std::cerr
                << "Did not find any thread trace with final futex without PT trace\n";
        } else {
            std::cerr << "Found matching signature in a thread\n";
        }
        if (!found_some_kernel_instrs_) {
            std::cerr << "Did not find any kernel instrs\n";
        }
        return true;
    }

private:
    // Data tracked per shard.
    struct per_shard_t {
        bool prev_was_futex_marker = false;
        bool prev_syscall_had_trace = false;
        bool any_syscall_had_trace = false;
        int syscall_count = 0;
        bool in_syscall_trace = false;
        int kernel_instr_count = 0;
    };

    bool found_final_futex_without_trace_ = false;
    bool found_some_kernel_instrs_ = false;
    std::mutex shard_exit_mutex_;
};

static bool
run_pt_analysis(const std::string &trace_dir)
{
    auto pt_analysis_tool = std::unique_ptr<pt_analysis_tool_t>(new pt_analysis_tool_t());
    std::vector<analysis_tool_t *> tools;
    tools.push_back(pt_analysis_tool.get());
    analyzer_t analyzer(trace_dir, &tools[0], static_cast<int>(tools.size()));
    if (!analyzer) {
        FATAL_ERROR("failed to initialize analyzer: %s",
                    analyzer.get_error_string().c_str());
    }
    if (!analyzer.run()) {
        FATAL_ERROR("failed to run analyzer: %s", analyzer.get_error_string().c_str());
    }
    if (!analyzer.print_stats()) {
        FATAL_ERROR("failed to print stats: %s", analyzer.get_error_string().c_str());
    }
    return true;
}

static void
gather_trace()
{
    if (setenv("DYNAMORIO_OPTIONS",
               "-stderr_mask 0xc -client_lib ';;-offline -enable_kernel_tracing",
               1 /*override*/) != 0)
        std::cerr << "failed to set env var!\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    dr_app_start();

    pthread_t child_thread;
    int res = pthread_create(&child_thread, NULL, child_futex_wait, NULL);
    assert(res == 0);

    /* Ensure that the child is waiting at a futex. */
    parent_futex_reque();

    do_some_syscalls();

    dr_app_stop_and_cleanup();

    /* Wake up the child finally. */
    parent_futex_wake();

    pthread_join(child_thread, NULL);

    return;
}

static int
test_pt_trace(void *dr_context)
{
    std::string trace_dir = postprocess(dr_context);
    if (!run_pt_analysis(trace_dir))
        return 1;
    return 0;
}

int
test_main(int argc, const char *argv[])
{
    gather_trace();
    void *dr_context = dr_standalone_init();
    if (test_pt_trace(dr_context)) {
        return 1;
    }
    dr_standalone_exit();
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
