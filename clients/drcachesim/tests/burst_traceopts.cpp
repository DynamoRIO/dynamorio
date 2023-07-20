/* **********************************************************
 * Copyright (c) 2019-2023 Google, Inc.  All rights reserved.
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

/* Tests offline trace recording optimizations and ensures that elided information
 * is accurately reconstructed in post-processing by collecting two traces of an
 * identical code region, one with and one without optimizations.  These two
 * traces are post-processed and compared, all within this test.
 */

#ifndef ASM_CODE_ONLY /* C code */
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

/* Asm routines. */
extern "C" {
void
test_disp_elision();
void
test_base_elision();
};

namespace dynamorio {
namespace drmemtrace {

/* Unit tests for classes used for trace optimizations. */
void
reg_id_set_unit_tests();

std::string
compare_memref(void *dcontext, int64 entry_count, const memref_t &memref1,
               const memref_t &memref2)
{
    if (memref1.instr.type != memref2.instr.type) {
        return "Trace types do not match: " + std::to_string(memref1.instr.type) +
            " vs " + std::to_string(memref2.instr.type);
    }
    // We check the details of fields which trace optimizations affect: the core
    // instruction and data fetch entries. The current optimizations have no impact
    // on other entries, and many other entries have variable values such as
    // timestamps from run to run.
    if (type_is_instr(memref1.instr.type) ||
        memref1.instr.type == TRACE_TYPE_INSTR_NO_FETCH) {
        if (memref1.instr.addr != memref2.instr.addr ||
            memref1.instr.size != memref2.instr.size) {
            std::cerr << "#" << entry_count << ": instr1=0x" << std::hex
                      << memref1.instr.addr << " x" << memref1.instr.size
                      << " vs instr2=0x" << memref2.instr.addr << " x"
                      << memref2.instr.size << "\n";
            disassemble_with_info(dcontext, reinterpret_cast<byte *>(memref1.instr.addr),
                                  STDERR, true, true);
            if (memref1.instr.addr != memref2.instr.addr) {
                disassemble_with_info(dcontext,
                                      reinterpret_cast<byte *>(memref2.instr.addr),
                                      STDERR, true, true);
            }
            return "Instr fields do not match";
        }
    } else if (memref1.data.type == TRACE_TYPE_READ ||
               memref1.data.type == TRACE_TYPE_WRITE) {
        if (memref1.data.addr != memref2.data.addr ||
            memref1.data.size != memref2.data.size ||
            memref1.data.pc != memref2.data.pc) {
            std::cerr << "#" << entry_count << ": addr1=0x" << std::hex
                      << memref1.data.addr << " x" << memref1.data.size << " @0x"
                      << memref1.data.pc << " vs addr2=0x" << memref2.data.addr << " x"
                      << memref2.data.size << " @0x" << memref2.data.pc << "\n";
            disassemble_with_info(dcontext, reinterpret_cast<byte *>(memref1.data.pc),
                                  STDERR, true, true);
            if (memref1.data.pc != memref2.data.pc) {
                disassemble_with_info(dcontext, reinterpret_cast<byte *>(memref2.data.pc),
                                      STDERR, true, true);
            }
            return "Data fields do not match";
        }
    }
    return "";
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

static void
test_arrays()
{
    // We need repeatable addresses, so no heap allocation.
    constexpr int size = 16;
    static int array[size];
    // The current elision only operates on base-reg-only memrefs.
    // If we use "array[i]" the compiler generates a base+index memref,
    // so we use a pointer de-ref instead and only add immediates to it.
    // Even so it is not easy to generate the elision cases from higher-level
    // code for non-frame-pointer registers for unoptimized builds (our primary
    // test suite builds).  The code below does trigger i#4403 on my machine
    // and contains further patterns which hopefully gives more confidence to
    // our optimizations.
    register int *ptr = array + 1;
    for (int i = 1; i < size - 1; ++i) {
        *ptr += *(ptr - 1) + *(ptr + 1);
        ptr++;
        *ptr += 4;
    }
    assert(ptr - array <= size);
}

static void
do_some_work()
{
    test_disp_elision();
    test_base_elision();
    test_arrays();
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
                              /* FIXME i#3983: Creating threads in standalone mode
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
    reg_id_set_unit_tests();

    std::string dir_opt = gather_trace("", "opt");
    std::string dir_noopt = gather_trace("-disable_optimizations", "noopt");

    // Now compare the two traces using external iterators and a custom tool.
    void *dr_context = dr_standalone_init();

    scheduler_t scheduler_opt;
    std::vector<scheduler_t::input_workload_t> sched_opt_inputs;
    sched_opt_inputs.emplace_back(dir_opt);
    if (scheduler_opt.init(sched_opt_inputs, 1,
                           scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        std::cerr << "Failed to initialize scheduler " << scheduler_opt.get_error_string()
                  << "\n";
    }

    scheduler_t scheduler_noopt;
    std::vector<scheduler_t::input_workload_t> sched_noopt_inputs;
    sched_noopt_inputs.emplace_back(dir_noopt);
    if (scheduler_noopt.init(sched_noopt_inputs, 1,
                             scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        std::cerr << "Failed to initialize scheduler "
                  << scheduler_noopt.get_error_string() << "\n";
    }

    auto *stream_opt = scheduler_opt.get_stream(0);
    auto *stream_noopt = scheduler_noopt.get_stream(0);
    int64 entry_count = 0;
    while (true) {
        memref_t memref_opt;
        memref_t memref_noopt;
        scheduler_t::stream_status_t status_opt = stream_opt->next_record(memref_opt);
        scheduler_t::stream_status_t status_noopt =
            stream_noopt->next_record(memref_noopt);
        if (status_opt == scheduler_t::STATUS_EOF) {
            assert(status_noopt == scheduler_t::STATUS_EOF);
            break;
        }
        assert(status_opt == scheduler_t::STATUS_OK &&
               status_noopt == scheduler_t::STATUS_OK);
        if (memref_opt.marker.type != memref_noopt.marker.type) {
            // Allow a header from a trace buffer filling up at a different
            // point in optimized vs unoptimized.
            while (memref_opt.marker.type == TRACE_TYPE_MARKER) {
                status_opt = stream_opt->next_record(memref_opt);
                assert(status_opt == scheduler_t::STATUS_OK);
            }
            while (memref_noopt.marker.type == TRACE_TYPE_MARKER) {
                status_noopt = stream_noopt->next_record(memref_noopt);
                assert(status_noopt == scheduler_t::STATUS_OK);
            }
        }
        std::string error =
            compare_memref(dr_context, entry_count, memref_opt, memref_noopt);
        if (!error.empty()) {
            std::cerr << "Trace mismatch found: " << error << "\n";
            break;
        }
        ++entry_count;
    }
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

#define FUNCNAME test_disp_elision
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
# if defined(X86)
        mov      REG_XAX, REG_XSP
        mov      REG_XDX, PTRSZ [REG_XAX + 8]
        add      REG_XAX, HEX(0) // Block addr elision to test just disp.
        mov      REG_XDX, PTRSZ [REG_XAX + 16]
        add      REG_XAX, HEX(0) // Block addr elision to test just disp.
        mov      REG_XDX, PTRSZ [REG_XAX + 32]
        ret
# elif defined(ARM)
        /* TODO(i#1551): Not tested because of missing ARM start/stop support. */
        ldr      r1, [sp, #16]
        mov      r0, sp
        ldr      r1, [r0, #8]
        add      r1, r1, #0 // Block addr elision to test just disp.
        ldr      r1, [r0, #16]
        add      r1, r1, #0 // Block addr elision to test just disp.
        ldr      r1, [r0, #32]
        bx       lr
# elif defined(AARCH64)
        ldr      x1, [sp, #16]
        mov      x0, sp
        ldr      x1, [x0, #8]
        add      x1, x1, #0 // Block addr elision to test just disp.
        ldr      x1, [x0, #16]
        add      x1, x1, #0 // Block addr elision to test just disp.
        ldr      x1, [x0, #32]
        ret
# else
#  error NYI
# endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_base_elision
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
# if defined(X86)
        mov      REG_XAX, REG_XSP
#  ifdef X64
        // Test rip-relative.
        mov      REG_XDX, PTRSZ SYMREF(mysym)
#  endif
mysym:
       // Test elision.
        mov      REG_XDX, PTRSZ [REG_XAX + 8]
        mov      REG_XDX, PTRSZ [REG_XSP + 64]
        mov      REG_XDX, PTRSZ [REG_XAX + 16]
        mov      REG_XDX, PTRSZ [REG_XAX + 32]
        // Test a conditional which should not be elided.
        cmovne   REG_XDX, PTRSZ [REG_XAX + 32]
        jmp      newblock
newblock:
        // Test modified bases which should not be elided.
        mov      REG_XDX, PTRSZ [REG_XSP + 8]
        push     REG_XAX
        mov      REG_XAX, REG_XSP
        mov      REG_XDX, PTRSZ [REG_XAX + 8]
        // Modify via load.
        mov      REG_XAX, PTRSZ [REG_XAX]
        mov      REG_XDX, PTRSZ [REG_XAX + 16]
        // Modify via non-memref.
        add      REG_XAX, 8
        mov      REG_XDX, PTRSZ [REG_XAX + 16]
        pop      REG_XAX
        ret
# elif defined(ARM)
        /* TODO(i#1551): Not tested because of missing ARM start/stop support. */
        // Test pc-relative
        ldr      r0, mysym
mysym:
        bx       lr
        // Test conditional/predicate loads/stores.
        ldrne    r1, [r0, #8]
        ldrne    r1, [r0, #16]
        ldrne    r1, [r0, #32]
        // Test modified bases which should not be elided.
        mov      r0, sp
        str      r0, [sp, #-8]
        ldr      r1, [r0, #16]
        // Modify via load.
        ldr      r0, [sp, #-8]
        ldr      r1, [r0, #32]
        ldr      r1, [r0, #16]
        // Modify via non-memref.
        add      r0, #8
        ldr      r1, [r0, #32]
# elif defined(AARCH64)
        // Test pc-relative
        ldr      x0, mysym
mysym:
        // Test modified bases which should not be elided.
        mov      x0, sp
        str      x0, [sp, #-8]
        ldr      x1, [x0, #16]
        // Modify via load.
        ldr      x0, [sp, #-8]
        ldr      x1, [x0, #32]
        ldr      x1, [x0, #16]
        // Modify via non-memref.
        add      x0, x0, #8
        ldr      x1, [x0, #32]
        // There are no conditional/predicate loads/stores.
        ret
# else
#  error NYI
# endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE
/* clang-format on */
#endif
