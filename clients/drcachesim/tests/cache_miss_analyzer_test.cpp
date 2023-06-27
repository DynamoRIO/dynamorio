/* **********************************************************
 * Copyright (c) 2015-2023 Google, LLC  All rights reserved.
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

#include <iostream>

#include "../simulator/cache_miss_analyzer.h"
#include "../simulator/cache_simulator.h"
#include "../common/memref.h"

namespace dynamorio {
namespace drmemtrace {

static memref_t
generate_mem_ref(const addr_t addr, const addr_t pc)
{
    memref_t memref;
    memref.data.type = TRACE_TYPE_READ;
    memref.data.pid = 11111;
    memref.data.tid = 22222;
    memref.data.addr = addr;
    memref.data.size = 8;
    memref.data.pc = pc;
    return memref;
}

// A test with no dominant stride.
bool
no_dominant_stride()
{
    const unsigned int kLineSize = 64;

    // Create the cache simulator knobs object.
    cache_simulator_knobs_t knobs;
    knobs.line_size = kLineSize;
    knobs.LL_size = 1024 * 1024;
    knobs.data_prefetcher = "none";

    // Create the cache miss analyzer object.
    cache_miss_analyzer_t analyzer(knobs, 1000, 0.01, 0.75);

    // Analyze a stream of memory load references with no dominant stride.
    addr_t addr = 0x1000;
    for (int i = 0; i < 50000; ++i) {
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += kLineSize;
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += (kLineSize * 3);
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += (kLineSize * 5);
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += (kLineSize * 7);
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += (kLineSize * 5);
    }

    // Generate the analyzer's result and check it.
    std::vector<prefetching_recommendation_t *> recommendations =
        analyzer.generate_recommendations();
    if (recommendations.empty()) {
        std::cout << "no_dominant_stride test passed." << std::endl;
        return true;
    } else {
        std::cerr << "no_dominant_stride test failed." << std::endl;
        return false;
    }
}

// A test with one dominant stride.
bool
one_dominant_stride()
{
    const int kStride = 7;
    const unsigned int kLineSize = 64;

    // Create the cache simulator knobs object.
    cache_simulator_knobs_t knobs;
    knobs.line_size = kLineSize;
    knobs.LL_size = 1024 * 1024;
    knobs.data_prefetcher = "none";

    // Create the cache miss analyzer object.
    cache_miss_analyzer_t analyzer(knobs, 1000, 0.01, 0.75);

    // Analyze a stream of memory load references with one dominant stride.
    addr_t addr = 0x1000;
    for (int i = 0; i < 50000; ++i) {
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += (kLineSize * kStride);
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += (kLineSize * kStride);
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += (kLineSize * kStride);
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += (kLineSize * kStride);
        analyzer.process_memref(generate_mem_ref(addr, 0xAAAA));
        addr += 1000;
    }

    // Generate the analyzer's result and check it.
    std::vector<prefetching_recommendation_t *> recommendations =
        analyzer.generate_recommendations();
    if (recommendations.size() == 1) {
        if (recommendations[0]->pc == 0xAAAA &&
            recommendations[0]->stride == (kStride * kLineSize)) {
            std::cout << "one_dominant_stride test passed." << std::endl;
            return true;
        } else {
            std::cerr << "one_dominant_stride test failed: wrong recommendation: "
                      << "pc=" << recommendations[0]->pc
                      << ", stride=" << recommendations[0]->stride << std::endl;
            return false;
        }
    } else {
        std::cerr << "one_dominant_stride test failed: number of recommendations "
                  << "should be exactly 1, but was " << recommendations.size()
                  << std::endl;
        return false;
    }
}

// A test with two dominant strides.
bool
two_dominant_strides()
{
    const int kStride1 = 3;
    const int kStride2 = 11;
    const unsigned int kLineSize = 64;

    // Create the cache simulator knobs object.
    cache_simulator_knobs_t knobs;
    knobs.line_size = kLineSize;
    knobs.LL_size = 1024 * 1024;
    knobs.data_prefetcher = "none";

    // Create the cache miss analyzer object.
    cache_miss_analyzer_t analyzer(knobs, 1000, 0.01, 0.75);

    // Analyze a stream of memory load references with two dominant strides.
    addr_t addr1 = 0x1000;
    addr_t addr2 = 0x2000;
    for (int i = 0; i < 50000; ++i) {
        analyzer.process_memref(generate_mem_ref(addr1, 0xAAAA));
        addr1 += (kLineSize * kStride1);
        analyzer.process_memref(generate_mem_ref(addr1, 0xAAAA));
        addr1 += (kLineSize * kStride1);
        analyzer.process_memref(generate_mem_ref(addr2, 0xBBBB));
        addr2 += (kLineSize * kStride2);
        analyzer.process_memref(generate_mem_ref(addr1, 0xAAAA));
        addr1 += (kLineSize * kStride1);
        analyzer.process_memref(generate_mem_ref(addr2, 0xBBBB));
        addr2 += (kLineSize * kStride2);
        analyzer.process_memref(generate_mem_ref(addr2, 0xBBBB));
        addr2 += (kLineSize * kStride2);
    }

    // Generate the analyzer's result and check it.
    std::vector<prefetching_recommendation_t *> recommendations =
        analyzer.generate_recommendations();
    if (recommendations.size() == 2) {
        if ((recommendations[0]->pc == 0xAAAA &&
             recommendations[0]->stride == (kStride1 * kLineSize) &&
             recommendations[1]->pc == 0xBBBB &&
             recommendations[1]->stride == (kStride2 * kLineSize)) ||
            (recommendations[1]->pc == 0xAAAA &&
             recommendations[1]->stride == (kStride1 * kLineSize) &&
             recommendations[0]->pc == 0xBBBB &&
             recommendations[0]->stride == (kStride2 * kLineSize))) {
            std::cout << "two_dominant_strides test passed." << std::endl;
            return true;
        } else {
            std::cerr << "two_dominant_strides test failed: wrong recommendations."
                      << std::endl;
            return false;
        }
    } else {
        std::cerr << "two_dominant_strides test failed: number of recommendations "
                  << "should be exactly 2, but was " << recommendations.size()
                  << std::endl;
        return false;
    }
}

int
test_main(int argc, const char *argv[])
{
    if (no_dominant_stride() && one_dominant_stride() && two_dominant_strides()) {
        return 0;
    } else {
        std::cerr << "cache_miss_analyzer_test failed" << std::endl;
        exit(1);
    }
}

} // namespace drmemtrace
} // namespace dynamorio
