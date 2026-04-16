/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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

/* Unit tests for analysis of OFFLINE_FILE_TYPE_WHOLE_SYSTEM traces. */

#include "dr_api.h"
#include "tools/basic_counts.h"
#include "memref_gen.h"

#include <vector>

namespace dynamorio {
namespace drmemtrace {

static constexpr memref_tid_t TID_A = 11;
static constexpr addr_t PC = 1001;
static constexpr addr_t SOME_VAL = 1;

class local_stream_t : public default_memtrace_stream_t {
public:
    int64_t
    get_tid() const override
    {
        return TID_A;
    }
};

basic_counts_t::counters_t
run_basic_counts(const std::vector<memref_t> &memrefs)
{
    auto stream = std::unique_ptr<local_stream_t>(new local_stream_t());
    auto tool = std::unique_ptr<basic_counts_t>(new basic_counts_t(/*verbose=*/0));
    tool->initialize_stream(stream.get());
    for (const memref_t &memref : memrefs) {
        tool->process_memref(memref);
    }
    return tool->get_total_counts();
}

static bool
test_hardware_xfer_marker_counts()
{
    std::vector<memref_t> memrefs = {
        gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
        gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
        gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, SOME_VAL),
        gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, SOME_VAL),
        gen_instr(TID_A, PC),
        gen_instr(TID_A, PC + 1),
        gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 0),
        gen_marker(TID_A, TRACE_MARKER_TYPE_HARDWARE_EVENT, SOME_VAL),
        gen_marker(TID_A, TRACE_MARKER_TYPE_HARDWARE_CONTEXT_RETURN, SOME_VAL),
        gen_instr(TID_A, PC + 2),
        gen_marker(TID_A, TRACE_MARKER_TYPE_HARDWARE_CONTEXT_RETURN, SOME_VAL),
        gen_exit(TID_A),
    };
    basic_counts_t::counters_t counts = run_basic_counts(memrefs);
    int64_t expected_hardware_xfer_markers = 3;
    if (counts.hardware_xfer_markers != 3) {
        fprintf(stderr, "Expected %ld hardware xfer markers, found %ld\n",
                expected_hardware_xfer_markers, counts.hardware_xfer_markers);
        return false;
    }
    fprintf(stderr, "test_hardware_xfer_markers passed\n");
    return true;
}

int
test_main(int argc, const char *argv[])
{
    dr_standalone_init();
    if (!test_hardware_xfer_marker_counts())
        return 1;
    fprintf(stderr, "All done!\n");
    dr_standalone_exit();
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
