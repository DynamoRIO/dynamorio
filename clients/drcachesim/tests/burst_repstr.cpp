/* **********************************************************
 * Copyright (c) 2019-2026 Google, Inc.  All rights reserved.
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

/* Tests faults in the middle of repeated string operations. */

#ifndef ASM_CODE_ONLY /* C code */

#    include "test_helpers.h"
#    include "dr_api.h"
#    include "drmemtrace/drmemtrace.h"
#    include "drcovlib.h"
#    include "analysis_tool.h"
#    include "scheduler.h"
#    include "tracer/raw2trace.h"
#    include "tracer/raw2trace_directory.h"
#    include <assert.h>
#    include <iostream>
#    include <signal.h>
#    include <setjmp.h>
#    include <stdlib.h>
#    include <string.h>
#    include <sys/mman.h>
#    include <unistd.h>

#    ifndef X86
#        error X86-only
#    endif

/* Asm routines. */
extern "C" {
void
test_rep_movs(char *dst, const char *src, size_t len);
void
rep_movs_pc();
};

namespace dynamorio {
namespace drmemtrace {

static sigjmp_buf mark;
static int signal_count = 0;

void
signal_handler(int signal)
{
    assert(signal == SIGSEGV);
    signal_count++;
    siglongjmp(mark, signal_count);
    assert(false);
}

bool
my_setenv(const char *var, const char *value)
{
#    ifdef UNIX
    return setenv(var, value, 1 /*override*/) == 0;
#    else
    return SetEnvironmentVariable(var, value) == TRUE;
#    endif
}

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
#    ifdef WINDOWS
                              /* XXX i#3983: Creating threads in standalone mode
                               * causes problems.  We disable the pool for now.
                               */
                              ,
                              0
#    endif
        );
        // We rely on raw2trace unit tests checking that actual elision in raw
        // inputs is handled, as it is difficult to confirm here that elision
        // is actually happening: though a bug that fails to elide will generally
        // cause failure in raw2trace, so it would take 2 corresponding bugs to
        // get this test to pass without real elision.
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
gather_trace(const std::string &tracer_ops, const std::string &out_subdir, char *dst,
             const char *src, size_t len)
{
    std::string dr_ops("-stderr_mask 0xc -client_lib ';;-offline " + tracer_ops + "'");
    if (!my_setenv("DYNAMORIO_OPTIONS", dr_ops.c_str()))
        std::cerr << "failed to set env var!\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    dr_app_start();
    assert(dr_app_running_under_dynamorio());

    if (sigsetjmp(mark, 1) == 0) {
        test_rep_movs(dst, src, len);
    }

    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());

    return post_process(out_subdir);
}

void
verify_fault(void *drcontext, const std::string &trace_dir, char *dst, char *src,
             int expected_iters)
{
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_opt_inputs;
    sched_opt_inputs.emplace_back(trace_dir);
    if (scheduler.init(sched_opt_inputs, 1,
                       scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        std::cerr << "Failed to initialize scheduler " << scheduler.get_error_string()
                  << "\n";
    }
    auto *stream = scheduler.get_stream(0);
    int64 entry_count = 0;
    int64 entry_count_at_target = 0;
    int target_read_count = 0, target_write_count = 0;
    bool found_loop = false;
    bool found_uncompleted_marker = false;
    bool verbose = false;
    while (true) {
        memref_t memref;
        scheduler_t::stream_status_t status = stream->next_record(memref);
        if (status == scheduler_t::STATUS_EOF)
            break;
        assert(status == scheduler_t::STATUS_OK);
        if (type_is_instr(memref.instr.type) ||
            memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH) {
            if (verbose) {
                std::cerr << "#" << entry_count << ": ";
                disassemble_with_info(drcontext,
                                      reinterpret_cast<byte *>(memref.instr.addr), STDERR,
                                      true, true);
            }
            if (type_is_instr(memref.instr.type)) {
                if (memref.instr.addr == reinterpret_cast<addr_t>(rep_movs_pc)) {
                    entry_count_at_target = entry_count;
                    found_loop = true;
                } else
                    entry_count_at_target = 0;
            }
        } else if (memref.marker.type == TRACE_TYPE_MARKER) {
            if (verbose) {
                std::cerr << "#" << entry_count << ": marker "
                          << memref.marker.marker_type << " 0x" << std::hex
                          << memref.marker.marker_value << std::dec << "\n";
            }
            if (memref.marker.marker_type == TRACE_MARKER_TYPE_UNCOMPLETED_INSTRUCTION) {
                constexpr addr_t REP_MOVSB_ENCODING = 0xa4f3;
                assert(memref.marker.marker_value ==
                       reinterpret_cast<addr_t>(REP_MOVSB_ENCODING));
                found_uncompleted_marker = true;
            }
        } else if (memref.data.type == TRACE_TYPE_READ ||
                   memref.data.type == TRACE_TYPE_WRITE) {
            if (verbose) {
                std::cerr << "#" << entry_count << ": data 0x" << std::hex
                          << memref.data.addr << " x" << memref.data.size << " @0x"
                          << memref.data.pc << std::dec << "\n";
            }
            if (entry_count_at_target > 0) {
                if ((entry_count - entry_count_at_target) % 3 == 1) {
                    assert(memref.data.type == TRACE_TYPE_READ);
                    assert(memref.data.addr ==
                           reinterpret_cast<addr_t>(src) +
                               (entry_count - entry_count_at_target) / 3);
                    ++target_read_count;
                } else if ((entry_count - entry_count_at_target) % 3 == 2) {
                    assert(memref.data.type == TRACE_TYPE_WRITE);
                    assert(memref.data.addr ==
                           reinterpret_cast<addr_t>(dst) +
                               (entry_count - entry_count_at_target) / 3);
                    ++target_write_count;
                } else {
                    // The 3rd is the non-fetched instr which won't come here.
                    assert(false);
                }
            }
        }
        ++entry_count;
    }
    // If the loop faulted before any iters executed, we shouldn't see it at all.
    if (expected_iters == 0) {
        assert(found_uncompleted_marker);
        assert(!found_loop);
    } else {
        assert(!found_uncompleted_marker);
        assert(found_loop);
    }
    assert(target_read_count == expected_iters);
    assert(target_write_count == expected_iters);
    std::cerr << "Case with " << expected_iters << " iters passed.\n";
}

int
test_main(int argc, const char *argv[])
{
    signal(SIGSEGV, signal_handler);

    // Create two adjacent pages, the first writable and the 2nd not.
    const size_t page_size = sysconf(_SC_PAGESIZE);
    char *map = reinterpret_cast<char *>(mmap(
        nullptr, 2 * page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
    assert(map != MAP_FAILED);
    int res = mprotect(map + page_size, page_size, PROT_READ);
    assert(res == 0);

    // Gather a trace with a loop that faults on the first iteration.
    std::string dir_start =
        gather_trace("", "burst_repstr_start", map + page_size, map, 10);
    // Gather a trace with a loop that faults after 11 iterations.
    size_t bytes_before_fault = 11;
    char *dst = map + page_size - bytes_before_fault;
    char *src = map;
    std::string dir_midloop =
        gather_trace("", "burst_repstr_mid", dst, src, bytes_before_fault * 2);

    // Check the traces.
    void *drcontext = dr_standalone_init();
    verify_fault(drcontext, dir_start, map + page_size, map, 0);
    verify_fault(drcontext, dir_midloop, dst, src, bytes_before_fault);
    dr_standalone_exit();

    std::cerr << "all done\n";
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio

#else /* asm code *************************************************************/
/* Avoid warnings from defines passed to the C++ side from configuring it as a client.
 * Is there a better way?  Reset the flags for the client?
 */
#    undef UNIX
#    undef LINUX
#    undef MACOS
#    undef WINDOWS
#    undef X86_64
#    undef X86_32
#    undef ARM_32
#    undef ARM_64
#    undef DR_APP_EXPORTS
#    undef DR_HOST_X64
#    undef DR_HOST_X86
#    undef DR_HOST_ARM
#    undef DR_HOST_AARCH64
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#define FUNCNAME test_rep_movs
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XDI, ARG1 // "dst".
        mov      REG_XSI, ARG2 // "src".
        mov      REG_XCX, ARG3 // "len".
        cld
DECLARE_GLOBAL(rep_movs_pc)
ADDRTAKEN_LABEL(rep_movs_pc:)
        rep      movsb
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE
/* clang-format on */
#endif
