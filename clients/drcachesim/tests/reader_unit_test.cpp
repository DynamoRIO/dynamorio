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

#include <cstdlib>
#include <iostream>
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

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::cerr << "CHECK failed at line " << __LINE__ << ": " #cond "\n"; \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

static constexpr memref_tid_t TID = 1;
static constexpr memref_pid_t PID = 2;
static constexpr addr_t TEST_PAGE_SIZE = 0x1000;

// Returns the next record that is not a marker.
static memref_t
next_non_marker(mock_reader_t &reader)
{
    do {
        ++reader;
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
    CHECK(instr.instr.num_pages == 0 && instr.instr.phys_rest == nullptr);
    memref_t data = next_non_marker(reader);
    CHECK(data.data.num_pages == 0 && data.data.phys_rest == nullptr);
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
        // Instruction straddling pages 1 and 2.
        make_instr(0x7fff1ffe, TRACE_TYPE_INSTR, 4),
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
    CHECK(memref_num_pages(instr) == 2);
    CHECK(instr.instr.phys0.addr == 0x9affe);
    CHECK(instr.instr.phys0.page_size == TEST_PAGE_SIZE);
    CHECK(memref_phys_page(instr, 1).addr == 0x13000);
    CHECK(memref_phys_page(instr, 1).page_size == TEST_PAGE_SIZE);

    memref_t single = next_non_marker(reader);
    CHECK(single.data.num_pages == 1 && single.data.phys_rest == nullptr);
    CHECK(single.data.phys0.addr == 0x9a010);

    memref_t cross = next_non_marker(reader);
    CHECK(memref_num_pages(cross) == 2);
    CHECK(cross.data.phys0.addr == 0x9aff8);
    CHECK(memref_phys_page(cross, 1).addr == 0x13000);
    // Accesses spanning the same pages share storage.
    CHECK(cross.data.phys_rest == instr.instr.phys_rest);

    memref_t unavail = next_non_marker(reader);
    CHECK(memref_num_pages(unavail) == 2);
    CHECK(unavail.data.phys0.addr == 0x13ff0);
    CHECK(memref_phys_page(unavail, 1).addr == MEMREF_PHYS_ADDR_UNKNOWN);

    memref_t three = next_non_marker(reader);
    CHECK(memref_num_pages(three) == 3);
    CHECK(three.data.phys0.addr == 0x9a800);
    CHECK(memref_phys_page(three, 1).addr == 0x13000);
    CHECK(memref_phys_page(three, 2).addr == MEMREF_PHYS_ADDR_UNKNOWN);

    memref_t unknown = next_non_marker(reader);
    CHECK(memref_num_pages(unknown) == 1);
    CHECK(unknown.data.phys0.addr == MEMREF_PHYS_ADDR_UNKNOWN);

    memref_t remapped = next_non_marker(reader);
    CHECK(memref_num_pages(remapped) == 2);
    CHECK(memref_phys_page(remapped, 1).addr == 0x55000);
    // Earlier records are unaffected by the mapping change.
    CHECK(remapped.data.phys_rest != cross.data.phys_rest);
    CHECK(memref_phys_page(cross, 1).addr == 0x13000);
}

void
unit_test_reader()
{
    test_no_v2p_info();
    test_v2p_markers();
}

} // namespace drmemtrace
} // namespace dynamorio
