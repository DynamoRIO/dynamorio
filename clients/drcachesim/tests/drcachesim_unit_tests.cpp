/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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

// Unit tests for drcachesim
#include <iostream>
#include <cstdlib>
#include "simulator/cache_simulator.h"
#include "../common/memref.h"

void
unit_test_warmup_fraction() {
    cache_simulator_t * cache_sim = new cache_simulator_t(1, 64, 32*64, 32*64,
                                                          32, 32, 32*64,
                                                          32, "", "LRU", "none",
                                                          0, 0, 0.5, 1ULL << 63, 0);

    // Feed it some memrefs, warmup fraction is set to 0.5 where the capacity at
    // each level is 32 lines each. The first 16 memrefs warm up the cache and
    // the 17th allows us to check for the warmup_fraction.
    for(int i = 0; i < 16 + 1; i++) {
        memref_t ref = { .data = { .type = TRACE_TYPE_READ, .pid = 0,
                                   .tid = 0, .addr = 0, .size = 8, .pc = 0} };
        ref.data.addr = i * 128;
        cache_sim->process_memref(ref);
    }

    if (!cache_sim->check_warmed_up()) {
        std::cerr << "drcachesim unit_test_warmup_fraction failed\n";
        exit(1);
    }

    delete cache_sim;
}

void
unit_test_warmup_refs() {
    cache_simulator_t * cache_sim = new cache_simulator_t(1, 64, 32*64, 32*64,
                                                          32, 32, 32*64,
                                                          32, "", "LRU", "none",
                                                          0, 16, 0.0, 1ULL << 63, 0);

    // Feed it some memrefs, warmup refs = 16 where the capacity at
    // each level is 32 lines each. The first 16 memrefs warm up the cache and
    // the 17th allows us to check.
    for(int i = 0; i < 16 + 1; i++) {
        memref_t ref = { .data = { .type = TRACE_TYPE_READ, .pid = 0,
                                   .tid = 0, .addr = 0, .size = 8, .pc = 0} };
        ref.data.addr = i * 128;
        cache_sim->process_memref(ref);
    }

    if (!cache_sim->check_warmed_up()) {
        std::cerr << "drcachesim unit_test_warmup_refs failed\n";
        exit(1);
    }

    delete cache_sim;
}


int
main(int argc, const char *argv[]) {
    unit_test_warmup_fraction();
    unit_test_warmup_refs();
    return 0;
}
