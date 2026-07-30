/* **********************************************************
 * Copyright (c) 2021-2026 Google, LLC  All rights reserved.
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
#include <optional>
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
    histogram_t tool(LINE_SIZE, /*report_top=*/0, /*verbose=*/0);
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

bool
check_parallel_reduce()
{
    static constexpr unsigned int LINE_SIZE = 64;
    histogram_t tool(LINE_SIZE, /*report_top=*/10, /*verbose=*/0);
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
    void *worker_data = tool.parallel_worker_init(0);
    void *shard_data = tool.parallel_shard_init(0, worker_data);
    for (const auto &memref : memrefs) {
        if (!tool.parallel_shard_memref(shard_data, memref)) {
            std::cerr << "parallel_shard_memref failed\n";
            return false;
        }
    }
    tool.parallel_shard_exit(shard_data);
    tool.parallel_worker_exit(worker_data);

    auto check_icache = [&tool]() {
        std::optional<std::unordered_map<addr_t, uint64_t>> icache_res =
            tool.get_icache_counts();
        if (!icache_res.has_value()) {
            std::cerr << "failed to obtain icache counts\n";
            return false;
        }
        if ((*icache_res)[20 * LINE_SIZE] != 2 || (*icache_res)[21 * LINE_SIZE] != 1) {
            std::cerr << "incorrect icache counts: got " << (*icache_res)[20 * LINE_SIZE]
                      << "," << (*icache_res)[21 * LINE_SIZE] << " instead of 2,1\n";
            return false;
        }
        return true;
    };
    if (!check_icache())
        return false;

    // Do another reduce inside print_results, testing that multiple reduces
    // do not change the result. The get_icache_counts() will also do another
    // reduce.
    // While at it, we test that printing stops before hitting the 10 top
    // results we requested when there aren't 10 total.
    std::stringstream capture;
    std::streambuf *prior = std::cerr.rdbuf(capture.rdbuf());
    tool.print_results();
    std::string res = capture.str();
    std::cerr.rdbuf(prior);
    const char *expect = R"DELIM(Cache line histogram tool results:
icache: 5 unique cache lines
dcache: 8 unique cache lines
icache top 10
             0x500: 2
             0x540: 1
             0xc40: 1
             0xc80: 1
             0x580: 1
dcache top 10
             0x280: 2
             0x2c0: 1
             0xa40: 1
             0x780: 1
             0x740: 1
             0x9c0: 1
             0x300: 1
             0xa00: 1
)DELIM";
    if (res != expect) {
        std::cerr << "print_results output |" << res << "| did not match expected |"
                  << expect << "\n";
        return false;
    }

    if (!check_icache())
        return false;
    return true;
}

int
test_main(int argc, const char *argv[])
{
    if (check_cross_line() && check_parallel_reduce()) {
        std::cerr << "histogram_test passed\n";
        return 0;
    }
    std::cerr << "histogram_test FAILED\n";
    exit(1);
}

} // namespace drmemtrace
} // namespace dynamorio
