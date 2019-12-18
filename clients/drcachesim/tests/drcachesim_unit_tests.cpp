/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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

static cache_simulator_knobs_t
make_test_knobs()
{
    cache_simulator_knobs_t knobs;
    knobs.num_cores = 1;
    knobs.L1I_size = 32 * 64;
    knobs.L1D_size = 32 * 64;
    knobs.L1I_assoc = 32;
    knobs.L1D_assoc = 32;
    knobs.LL_size = 32 * 64;
    knobs.LL_assoc = 32;
    knobs.data_prefetcher = "none";
    return knobs;
}

void
unit_test_warmup_fraction()
{
    cache_simulator_knobs_t knobs = make_test_knobs();
    knobs.warmup_fraction = 0.5;
    cache_simulator_t cache_sim(knobs);

    // Feed it some memrefs, warmup fraction is set to 0.5 where the capacity at
    // each level is 32 lines each. The first 16 memrefs warm up the cache and
    // the 17th allows us to check for the warmup_fraction.
    std::string error;
    for (int i = 0; i < 16 + 1; i++) {
        memref_t ref;
        ref.data.type = TRACE_TYPE_READ;
        ref.data.size = 8;
        ref.data.addr = i * 128;
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_warmup_fraction failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }

    if (!cache_sim.check_warmed_up()) {
        std::cerr << "drcachesim unit_test_warmup_fraction failed\n";
        exit(1);
    }
}

void
unit_test_warmup_refs()
{
    cache_simulator_knobs_t knobs = make_test_knobs();
    knobs.warmup_refs = 16;
    cache_simulator_t cache_sim(knobs);

    // Feed it some memrefs, warmup refs = 16 where the capacity at
    // each level is 32 lines each. The first 16 memrefs warm up the cache and
    // the 17th allows us to check.
    std::string error;
    for (int i = 0; i < 16 + 1; i++) {
        memref_t ref;
        ref.data.type = TRACE_TYPE_READ;
        ref.data.size = 8;
        ref.data.addr = i * 128;
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_warmup_fraction failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }

    if (!cache_sim.check_warmed_up()) {
        std::cerr << "drcachesim unit_test_warmup_refs failed\n";
        exit(1);
    }
}

void
unit_test_sim_refs()
{
    cache_simulator_knobs_t knobs = make_test_knobs();
    knobs.sim_refs = 8;
    cache_simulator_t cache_sim(knobs);

    std::string error;
    for (int i = 0; i < 16; i++) {
        memref_t ref;
        ref.data.type = TRACE_TYPE_READ;
        ref.data.size = 8;
        ref.data.addr = i * 128;
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_sim_refs failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }

    if (cache_sim.remaining_sim_refs() != 0) {
        std::cerr << "drcachesim unit_test_sim_refs failed\n";
        exit(1);
    }
}

int
main(int argc, const char *argv[])
{
    unit_test_warmup_fraction();
    unit_test_warmup_refs();
    unit_test_sim_refs();
    return 0;
}
