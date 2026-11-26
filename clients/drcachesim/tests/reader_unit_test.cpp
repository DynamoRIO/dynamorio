/* **********************************************************
 * Copyright (c) 2026 Google, LLC  All rights reserved.
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
 * * Neither the name of Google, LLC nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "reader_unit_test.h"

#include <assert.h>

#include <vector>

#include "memref.h"
#include "mock_reader.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::drmemtrace::test_util::make_exit;
using ::dynamorio::drmemtrace::test_util::make_footer;
using ::dynamorio::drmemtrace::test_util::make_header;
using ::dynamorio::drmemtrace::test_util::make_instr;
using ::dynamorio::drmemtrace::test_util::make_marker;
using ::dynamorio::drmemtrace::test_util::make_memref;
using ::dynamorio::drmemtrace::test_util::make_pid;
using ::dynamorio::drmemtrace::test_util::make_thread;
using ::dynamorio::drmemtrace::test_util::mock_reader_t;

static constexpr memref_tid_t TID = 1;
static constexpr memref_pid_t PID = 2;
static constexpr addr_t TEST_PAGE_SIZE = 0x1000;

// Returns the next record that is not a marker.
static memref_t
next_non_marker(mock_reader_t &reader)
{
    const mock_reader_t end;
    do {
        ++reader;
        assert(reader != end);
    } while ((*reader).marker.type == TRACE_TYPE_MARKER);
    return *reader;
}

static void
test_no_v2p_info()
{
    std::vector<trace_entry_t> trace = {
        make_header(TRACE_ENTRY_VERSION),
        make_thread(TID),
        make_pid(PID),
        make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, TEST_PAGE_SIZE),
        make_instr(0x1000, TRACE_TYPE_INSTR, 4),
        make_memref(0x2000, TRACE_TYPE_READ, 8),
        make_exit(TID),
        make_footer(),
    };
    mock_reader_t reader(trace);
    reader.init();
    memref_t instr = next_non_marker(reader);
    assert(instr.instr.type == TRACE_TYPE_INSTR);
    assert(instr.instr.num_pages == 0 && instr.instr.phys_rest == nullptr);
    memref_t data = next_non_marker(reader);
    assert(data.data.type == TRACE_TYPE_READ);
    assert(data.data.num_pages == 0 && data.data.phys_rest == nullptr);
}

static void
test_v2p_markers()
{
    std::vector<trace_entry_t> trace = {
        make_header(TRACE_ENTRY_VERSION),
        make_thread(TID),
        make_pid(PID),
        make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, TEST_PAGE_SIZE),
        make_marker(TRACE_MARKER_TYPE_PHYSICAL_ADDRESS, 0x9a000),
        make_marker(TRACE_MARKER_TYPE_VIRTUAL_ADDRESS, 0x7fff1000),
        make_marker(TRACE_MARKER_TYPE_PHYSICAL_ADDRESS, 0x13000),
        make_marker(TRACE_MARKER_TYPE_VIRTUAL_ADDRESS, 0x7fff2000),
        make_marker(TRACE_MARKER_TYPE_PHYSICAL_ADDRESS_NOT_AVAILABLE, 0x7fff3000),
        // Instruction spanning pages 1 and 2. We use an unaligned 3-byte instruction
        // (as can happen on x86).
        make_instr(0x7fff1fff, TRACE_TYPE_INSTR, 3),
        // Single-page access.
        make_memref(0x7fff1010, TRACE_TYPE_READ, 8),
        // Access spanning pages 1 and 2.
        make_memref(0x7fff1ff8, TRACE_TYPE_WRITE, 16),
        // Access spanning pages 2 and 3, where 3 is not available.
        make_memref(0x7fff2ff0, TRACE_TYPE_READ, 32),
        // Access spanning 3 pages.
        make_memref(0x7fff1800, TRACE_TYPE_WRITE, 0x1900),
        // Page without any marker.
        make_memref(0x7fff5000, TRACE_TYPE_READ, 4),
        // The mapping of page 2 changes.
        make_marker(TRACE_MARKER_TYPE_PHYSICAL_ADDRESS, 0x55000),
        make_marker(TRACE_MARKER_TYPE_VIRTUAL_ADDRESS, 0x7fff2000),
        make_memref(0x7fff1ff8, TRACE_TYPE_READ, 16),
        make_exit(TID),
        make_footer(),
    };
    mock_reader_t reader(trace);
    reader.init();

    memref_t instr = next_non_marker(reader);
    assert(memref_num_pages(instr) == 2);
    assert(instr.instr.phys0.addr == 0x9afff);
    assert(instr.instr.phys0.page_size == TEST_PAGE_SIZE);
    assert(memref_phys_page(instr, 0).addr == instr.instr.phys0.addr);
    assert(memref_phys_page(instr, 0).page_size == instr.instr.phys0.page_size);
    assert(memref_phys_page(instr, 1).addr == 0x13000);
    assert(memref_phys_page(instr, 1).page_size == TEST_PAGE_SIZE);

    memref_t single = next_non_marker(reader);
    assert(single.data.num_pages == 1 && single.data.phys_rest == nullptr);
    assert(single.data.phys0.addr == 0x9a010);
    assert(single.data.phys0.page_size == TEST_PAGE_SIZE);

    memref_t cross = next_non_marker(reader);
    assert(memref_num_pages(cross) == 2);
    assert(cross.data.phys0.addr == 0x9aff8);
    assert(memref_phys_page(cross, 1).addr == 0x13000);
    // Keep a deep copy to check that it survives the reader moving on.
    std::vector<_memref_phys_t> cross_storage;
    const memref_t cross_copy = memref_deep_copy(cross, cross_storage);
    assert(cross_copy.data.phys_rest == cross_storage.data());

    memref_t third_unavail = next_non_marker(reader);
    assert(memref_num_pages(third_unavail) == 2);
    assert(third_unavail.data.phys0.addr == 0x13ff0);
    assert(memref_phys_page(third_unavail, 1).addr == MEMREF_PHYS_ADDR_UNKNOWN);

    memref_t three = next_non_marker(reader);
    assert(memref_num_pages(three) == 3);
    assert(three.data.phys0.addr == 0x9a800);
    assert(memref_phys_page(three, 1).addr == 0x13000);
    assert(memref_phys_page(three, 2).addr == MEMREF_PHYS_ADDR_UNKNOWN);

    memref_t unknown = next_non_marker(reader);
    assert(memref_num_pages(unknown) == 1);
    assert(unknown.data.phys0.addr == MEMREF_PHYS_ADDR_UNKNOWN);

    memref_t remapped = next_non_marker(reader);
    assert(memref_num_pages(remapped) == 2);
    assert(memref_phys_page(remapped, 1).addr == 0x55000);
    // The deep copy still holds the translation from when it was read.
    assert(memref_num_pages(cross_copy) == 2);
    assert(memref_phys_page(cross_copy, 0).addr == 0x9aff8);
    assert(memref_phys_page(cross_copy, 1).addr == 0x13000);
    assert(memref_phys_page(cross_copy, 1).page_size == TEST_PAGE_SIZE);
}

void
unit_test_reader()
{
    test_no_v2p_info();
    test_v2p_markers();
}

} // namespace drmemtrace
} // namespace dynamorio
