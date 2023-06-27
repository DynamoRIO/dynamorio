/* **********************************************************
 * Copyright (c) 2020-2023 Google, Inc.  All rights reserved.
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
 * a "burst" of execution in the middle of the application.  It then detaches.
 * It then post-processes the acquired trace and confirms various assertions.
 */
#include "scheduler.h"
#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "tracer/instru.h"

#include "tracer/raw2trace.h"
#include "tracer/raw2trace_directory.h"
#include <assert.h>
#include <iostream>
#include <signal.h>
#include <setjmp.h>

namespace dynamorio {
namespace drmemtrace {

#define ASSERT_MSG(x, msg)                                                           \
    ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s (%s)", __FILE__, \
                                 __LINE__, #x, msg),                                 \
                      dr_abort(), 0)                                                 \
                   : 0))
#define ASSERT_NOT_REACHED() ASSERT_MSG(false, "Shouldn't be reached")
#define ALIGNED(x, alignment) ((((ptr_uint_t)x) & ((alignment)-1)) == 0)

/*******************************************************************************
 * Begin application code.
 */

static sigjmp_buf mark;
static int handled_sigill_count = 0;

#define TO_BE_ZEROED_ARR_SIZE 512
char to_be_zeroed[TO_BE_ZEROED_ARR_SIZE];

void
sigill_handler(int signal)
{
    handled_sigill_count++;
    siglongjmp(mark, handled_sigill_count);
    ASSERT_NOT_REACHED();
}

static void
dc_zva()
{
    int n = proc_get_cache_line_size() / sizeof(char);
    assert(n * 3 < TO_BE_ZEROED_ARR_SIZE);
    // Exactly n elements make up a cache line.
    // We issue a DC ZVA operation for each offset in a cache line.
    // We use the region [&to_be_zeroed[n], &to_be_zeroed[2*n]) to make sure that
    // the DC ZVA operation zeroes data only belonging to this array.
    for (int i = n; i < 2 * n; i++) {
        __asm__ __volatile__("dc zva, %0" : : "r"(&to_be_zeroed[i]));
    }
}

/* Attempts to execute the privileged 'dc ivac' instruction.
 * This will raise a SIGILL. Caller must register a SIGILL handler before
 * invoking this function.
 */
static void
dc_ivac()
{
    int d = 0;
    // Expected to raise SIGILL.
    // Control will be transferred to sigill_handler.
    __asm__ __volatile__("dc ivac, %0" : : "r"(&d));
}

static void
dc_unprivileged_flush()
{
    int d = 0;
    __asm__ __volatile__("dc civac, %0" : : "r"(&d));
    __asm__ __volatile__("dc cvac , %0" : : "r"(&d));
    __asm__ __volatile__("dc cvau , %0" : : "r"(&d));
}

static void
ic_unprivileged_flush()
{
    __asm__ __volatile__("ic ivau , %0" : : "r"(ic_unprivileged_flush));
}

static void
do_some_work()
{
    dc_zva();
    dc_unprivileged_flush();
    ic_unprivileged_flush();

    // Testing privileged instructions requires handling SIGILL.
    // We use sigsetjmp/siglongjmp to resume execution after handling the
    // signal.
    handled_sigill_count = 0;
    int i = sigsetjmp(mark, 1);
    switch (i) {
    case 0:
        dc_ivac();
        // TODO i#4406: Test other privileged dc and ic flush instructions too.
    }
    return;
}

/*******************************************************************************
 * End application code.
 */

bool
my_setenv(const char *var, const char *value)
{
    return setenv(var, value, 1 /*override*/) == 0;
}

static std::string
post_process()
{
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string outdir = std::string(raw_dir) + DIRSEP + "post_processed";
    void *dr_context = dr_standalone_init();
    // Use a new scope to free raw2trace_directory_t before dr_standalone_exit().
    {
        raw2trace_directory_t dir;
        if (!dr_create_dir(outdir.c_str())) {
            std::cerr << "Failed to create output dir";
            ASSERT_NOT_REACHED();
        }
        std::string dir_err = dir.initialize(raw_dir, outdir);
        assert(dir_err.empty());
        raw2trace_t raw2trace(dir.modfile_bytes_, dir.in_files_, dir.out_files_,
                              dir.out_archives_, dir.encoding_file_,
                              dir.serial_schedule_file_, dir.cpu_schedule_file_,
                              dr_context);
        std::string error = raw2trace.do_conversion();
        if (!error.empty()) {
            std::cerr << "raw2trace failed: " << error << "\n";
            ASSERT_NOT_REACHED();
        }
    }
    dr_standalone_exit();
    return outdir;
}

static std::string
gather_trace()
{
    if (!my_setenv("DYNAMORIO_OPTIONS", "-stderr_mask 0xc -client_lib ';;-offline'"))
        std::cerr << "failed to set env var!\n";

    std::cerr << "pre-DR init\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    std::cerr << "pre-DR start\n";
    dr_app_start();
    assert(dr_app_running_under_dynamorio());
    do_some_work();
    dr_app_stop_and_cleanup();
    std::cerr << "all done\n";
    assert(!dr_app_running_under_dynamorio());
    return post_process();
}

bool
is_dc_zva_instr(void *dr_context, memref_t memref)
{
    if (!type_is_instr(memref.instr.type))
        return false;
    app_pc pc = (app_pc)memref.instr.addr;
    instr_t instr;
    instr_init(dr_context, &instr);
    auto *ret = decode(dr_context, pc, &instr);
    assert(ret != NULL && instr_valid(&instr));
    bool is_dc_zva = instru_t::is_aarch64_dc_zva_instr(&instr);
    instr_free(dr_context, &instr);
    return is_dc_zva;
}

int
test_main(int argc, const char *argv[])
{
    // App setup.
    signal(SIGILL, sigill_handler);

    // Gather app trace.
    std::string trace_dir = gather_trace();

    void *dr_context = dr_standalone_init();
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(trace_dir);
    if (scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        std::cerr << "Failed to initialize scheduler " << scheduler.get_error_string()
                  << "\n";
    }
    bool found_cache_line_size_marker = false;
    bool found_page_size_marker = false;
    int dc_zva_instr_count = 0;
    int dc_zva_memref_count = 0;
    addr_t last_dc_zva_pc = 0;
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        if (memref.marker.type == TRACE_TYPE_MARKER &&
            memref.marker.marker_type == TRACE_MARKER_TYPE_CACHE_LINE_SIZE) {
            found_cache_line_size_marker = true;
            assert(memref.marker.marker_value == proc_get_cache_line_size());
        }
        if (memref.marker.type == TRACE_TYPE_MARKER &&
            memref.marker.marker_type == TRACE_MARKER_TYPE_PAGE_SIZE) {
            found_page_size_marker = true;
            assert(memref.marker.marker_value == dr_page_size());
        }
        if (is_dc_zva_instr(dr_context, memref)) {
            dc_zva_instr_count++;
            last_dc_zva_pc = memref.instr.addr;
            continue;
        }
        // Look for _memref_data_t entries.
        if ((memref.data.type == TRACE_TYPE_READ ||
             memref.data.type == TRACE_TYPE_WRITE || type_is_prefetch(memref.data.type))
            // Look for memrefs for last seen dc zva pc.
            && (last_dc_zva_pc != 0 && memref.data.pc == last_dc_zva_pc)) {
            assert(memref.data.type == TRACE_TYPE_WRITE);
            dc_zva_memref_count++;
            assert(ALIGNED(memref.data.addr, proc_get_cache_line_size()));
            assert(memref.data.size == proc_get_cache_line_size());
        }
    }
    std::cerr << "DC ZVA count: " << dc_zva_instr_count << "\n";
    std::cerr << "DC ZVA memref count: " << dc_zva_memref_count << "\n";
    std::cerr << "All DC ZVA memrefs are cache-line shaped stores!\n";
    assert(dc_zva_memref_count != 0);
    assert(dc_zva_instr_count == dc_zva_memref_count);
    assert(found_cache_line_size_marker);
    assert(found_page_size_marker);
    dr_standalone_exit();

    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
