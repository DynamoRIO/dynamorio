/* **********************************************************
 * Copyright (c) 2021-2023 Google, LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test for checks performed by invariant_checker_t that are not tested
 * by the signal_invariants app's prefetch and handler markers.
 * This looks for precise error strings from invariant_checker.cpp: but
 * we will notice if the literals get out of sync as the test will fail.
 */

#include <iostream>
#include <vector>

#include "../tools/histogram.h"
#include "../common/memref.h"
#include "memref_gen.h"

namespace dynamorio {
namespace drmemtrace {

bool
check_cross_line()
{
    static constexpr unsigned int LINE_SIZE = 64;
    histogram_t tool(LINE_SIZE, 0, 0);
    std::vector<memref_t> memrefs = {
        gen_instr(1, 20 * LINE_SIZE),
        gen_data(1, /*load=*/true, 10 * LINE_SIZE, 8),
        gen_instr(1, 21 * LINE_SIZE),
        gen_data(1, /*load=*/true, 11 * LINE_SIZE, 8),
        gen_instr(1, 22 * LINE_SIZE),
        gen_data(1, /*load=*/true, 12 * LINE_SIZE, 8),
        // Test repeated lines: should not affect unique.
        gen_instr(1, 20 * LINE_SIZE),
        gen_data(1, /*load=*/true, 10 * LINE_SIZE, 8),
        // Test crossing a cache line.
        gen_data(1, /*load=*/false, 30 * LINE_SIZE - 4, 8),
        gen_data(1, /*load=*/false, 40 * LINE_SIZE - 4, LINE_SIZE + 5),
        gen_instr(1, 50 * LINE_SIZE - 3, 4),
    };
    for (const auto &memref : memrefs) {
        tool.process_memref(memref);
    }
    uint64_t unique_icache_lines, unique_dcache_lines;
    tool.reduce_results(&unique_icache_lines, &unique_dcache_lines);
    if (unique_icache_lines != 5 || unique_dcache_lines != 8) {
        std::cerr << "got incorrect icache " << unique_icache_lines << ", dcache "
                  << unique_dcache_lines << "\n";
        return false;
    }
    return true;
}

int
test_main(int argc, const char *argv[])
{
    if (check_cross_line()) {
        std::cerr << "histogram_test passed\n";
        return 0;
    }
    std::cerr << "histogram_test FAILED\n";
    exit(1);
}

} // namespace drmemtrace
} // namespace dynamorio
