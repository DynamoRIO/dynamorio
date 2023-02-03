/* **********************************************************
 * Copyright (c) 2016-2021 Google, Inc.  All rights reserved.
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
#undef NDEBUG
#include <assert.h>
#include "cache_replacement_policy_unit_test.h"
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

void
unit_test_metrics_API()
{
    cache_simulator_knobs_t knobs = make_test_knobs();
    cache_simulator_t cache_sim(knobs);

    memref_t ref;
    ref.data.type = TRACE_TYPE_WRITE;
    ref.data.addr = 0;
    ref.data.size = 8;

    // Currently invalidates are not counted properly in the configuration of
    // cache_simulator_t with cache_simulator_knobs_t.
    // TODO i#5031: Test invalidates metric when the issue is solved.
    for (int i = 0; i < 4; i++) {
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_metrics_API failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }
    assert(cache_sim.get_cache_metric(metric_name_t::MISSES, 1, 0, cache_split_t::DATA) ==
           1);
    assert(cache_sim.get_cache_metric(metric_name_t::HITS, 1, 0, cache_split_t::DATA) ==
           3);

    ref.data.type = TRACE_TYPE_INSTR;

    for (int i = 0; i < 4; i++) {
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_metrics_API failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }
    assert(cache_sim.get_cache_metric(metric_name_t::MISSES, 1, 0,
                                      cache_split_t::INSTRUCTION) == 1);
    assert(cache_sim.get_cache_metric(metric_name_t::HITS, 1, 0,
                                      cache_split_t::INSTRUCTION) == 3);

    assert(cache_sim.get_cache_metric(metric_name_t::MISSES, 2) == 1);
    assert(cache_sim.get_cache_metric(metric_name_t::HITS, 2) == 1);

    ref.data.type = TRACE_TYPE_PREFETCH;
    ref.data.addr += 64;

    for (int i = 0; i < 4; i++) {
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_metrics_API failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }
    assert(cache_sim.get_cache_metric(metric_name_t::PREFETCH_MISSES, 1, 0,
                                      cache_split_t::DATA) == 1);
    assert(cache_sim.get_cache_metric(metric_name_t::PREFETCH_HITS, 1, 0,
                                      cache_split_t::DATA) == 3);

    ref.data.type = TRACE_TYPE_DATA_FLUSH;

    for (int i = 0; i < 4; i++) {
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_metrics_API failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }
    assert(cache_sim.get_cache_metric(metric_name_t::FLUSHES, 2) == 4);
}

void
unit_test_compulsory_misses()
{
    cache_simulator_knobs_t knobs = make_test_knobs();
    knobs.L1I_size = 4 * 64;
    knobs.L1I_assoc = 4;
    cache_simulator_t cache_sim(knobs);

    memref_t ref;
    ref.data.type = TRACE_TYPE_INSTR;
    ref.data.size = 8;

    for (int i = 0; i < 5; i++) {
        ref.data.addr = i * 64;
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_compulsory_misses failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }
    ref.data.addr = 0;
    cache_sim.process_memref(ref);

    assert(cache_sim.get_cache_metric(metric_name_t::COMPULSORY_MISSES, 1, 0,
                                      cache_split_t::INSTRUCTION) == 5);
    assert(cache_sim.get_cache_metric(metric_name_t::MISSES, 1, 0,
                                      cache_split_t::INSTRUCTION) == 6);
}

void
unit_test_child_hits()
{
    // Ensure child hits include all lower levels.
    std::string config = R"MYCONFIG(// 3-level simple config.
num_cores       1
line_size       64
L1I {
  type            instruction
  core            0
  size            256
  assoc           4
  prefetcher      none
  parent          L2
}
L1D {
  type            data
  core            0
  size            256
  assoc           4
  prefetcher      none
  parent          L2
}
L2 {
  size            8K
  assoc           8
  inclusive       true
  prefetcher      none
  parent          LLC
}
LLC {
  size            1M
  assoc           8
  inclusive       true
  prefetcher      none
  parent          memory
}
)MYCONFIG";
    std::istringstream config_in(config);
    cache_simulator_t cache_sim(&config_in);

    memref_t ref;
    ref.data.type = TRACE_TYPE_READ;
    ref.data.size = 1;

    // We perform a bunch of accesses to the same cache line to ensure they hit.
    const int num_accesses = 16;
    for (int i = 0; i < num_accesses; i++) {
        ref.data.addr = 64 + i;
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_child_hits failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }
    assert(cache_sim.get_cache_metric(metric_name_t::CHILD_HITS, 1, 0,
                                      cache_split_t::DATA) == 0);
    assert(cache_sim.get_cache_metric(metric_name_t::MISSES, 1, 0, cache_split_t::DATA) ==
           1);
    assert(cache_sim.get_cache_metric(metric_name_t::HITS, 1, 0, cache_split_t::DATA) ==
           num_accesses - 1);
    assert(cache_sim.get_cache_metric(metric_name_t::MISSES, 2, 0) == 1);
    assert(cache_sim.get_cache_metric(metric_name_t::HITS, 2, 0) == 0);
    assert(cache_sim.get_cache_metric(metric_name_t::CHILD_HITS, 2, 0) ==
           num_accesses - 1);
    assert(cache_sim.get_cache_metric(metric_name_t::CHILD_HITS, 3, 0) ==
           num_accesses - 1);
}

static cache_simulator_knobs_t
gen_cache_knobs(int line_size,
                int l1_sets,
                int l1_ways,  // Associativity
                int ll_sets,
                int ll_ways) {
    cache_simulator_knobs_t knobs;
    knobs.num_cores = 1;
    knobs.line_size = line_size;
    knobs.L1I_size = line_size * l1_sets * l1_ways;
    knobs.L1D_size = line_size * l1_sets * l1_ways;
    knobs.L1I_assoc = l1_ways;
    knobs.L1D_assoc = l1_ways;
    knobs.LL_size = line_size * ll_sets * ll_ways;
    knobs.LL_assoc = ll_ways;
    knobs.data_prefetcher = "none";
    // knobs.warmup_fraction = 0.0;
    return knobs;
}

static int
generate_1D_accesses(cache_simulator_t &cache_sim, addr_t start_address, int step_size,
                     int step_count, int loop_count = 1)
{
    int access_count = 0;
    for (int i = 0; i < loop_count; ++i) {
        memref_t ref;
        ref.data.type = TRACE_TYPE_READ;
        ref.data.size = 8;
        addr_t end_address = start_address + step_size * step_count;
        for (addr_t addr = start_address; addr < end_address; addr += step_size) {
            ref.data.addr = addr;
            if (!cache_sim.process_memref(ref)) {
                std::cerr << "generate_1D_accesses failed: "
                          << cache_sim.get_error_string() << "\n";
                exit(1);
            }
            ++access_count;
        }
    }
    return access_count;
}

struct CStats {
  int_least64_t hits;
  int_least64_t misses;
  int_least64_t child_hits;
};

static CStats
get_cache_stats(cache_simulator_t &cache_sim, int level)
{
  CStats stats;
  stats.hits = cache_sim.get_cache_metric(metric_name_t::HITS, level, 0);
  stats.misses = cache_sim.get_cache_metric(metric_name_t::MISSES, level, 0);
  stats.child_hits = cache_sim.get_cache_metric(metric_name_t::CHILD_HITS, level, 0);
  return stats;
}

void
unit_test_cache_size()
{
    constexpr int kLineSize = 64;
    constexpr int kL1Sets = 32;
    constexpr int kL1Ways = 8;
    constexpr int kL1Size = kLineSize * kL1Sets * kL1Ways;
    constexpr int kLLSets = 128;
    constexpr int kLLWays = 4;
    constexpr int kLLSize = kLineSize * kLLSets * kLLWays;

    cache_simulator_knobs_t knobs =
        gen_cache_knobs(kLineSize, kL1Sets, kL1Ways, kLLSets, kLLWays);

    // Access a buffer of increasing size, make sure hits + misses are expected.
    constexpr int kNumLoops = 4;
    for (uint32_t bufsize = kL1Size / 4; bufsize < kLLSize * 4; bufsize *= 2) {
        cache_simulator_t cache_sim(knobs);
        auto read_count =
            generate_1D_accesses(cache_sim, 0, kLineSize, bufsize / kLineSize, kNumLoops);
        CStats l1stats = get_cache_stats(cache_sim, 1);
        auto hits = l1stats.hits;
        auto misses = l1stats.misses;

        int expected_misses =
            (bufsize <= kL1Size) ? bufsize / kLineSize : bufsize * kNumLoops / kLineSize;
        int expected_hits = read_count - expected_misses;
        std::cerr << "L1:"
                  << " bufsize=" << bufsize << " L1size=" << kL1Size
                  << " num_accesses=" << read_count << " hits=" << hits
                  << " expected_hits=" << expected_hits << " misses=" << misses
                  << " expected_misses=" << expected_misses << "\n";
        assert(hits == expected_hits);
        assert(misses == expected_misses);

        CStats llstats = get_cache_stats(cache_sim, 2);
        auto llhits = llstats.hits;
        auto llmisses = llstats.misses;

        int expected_llmisses =
            (bufsize <= kLLSize) ? bufsize / kLineSize : bufsize * kNumLoops / kLineSize;
        int expected_llhits = read_count - expected_hits - expected_llmisses;
        std::cerr << "LL:"
                  << " bufsize=" << bufsize << " LLsize=" << kLLSize
                  << " num_accesses=" << expected_misses << " hits=" << llhits
                  << " expected_hits=" << expected_llhits << " misses=" << llmisses
                  << " expected_misses=" << expected_llmisses
                  << " child_hits=" << llstats.child_hits
                  << "\n";
        assert(llhits == expected_llhits);
        assert(llmisses == expected_llmisses);
    }

    /*
    assert(cache_sim.get_cache_metric(metric_name_t::HITS, 1, 0, cache_split_t::DATA) ==
           11);
    assert(cache_sim.get_cache_metric(metric_name_t::MISSES, 2, 0) == 1);
    */
}

void
unit_test_cache_assoc()
{
    constexpr int kLineSize = 64;
    constexpr int kL1Sets = 32;
    constexpr int kL1Ways = 8;
    constexpr int kL1Size = kLineSize * kL1Sets * kL1Ways;
    constexpr int kLLSets = 128;
    constexpr int kLLWays = 4;
    constexpr int kLLSize = kLineSize * kLLSets * kLLWays;

    cache_simulator_knobs_t knobs =
        gen_cache_knobs(kLineSize, kL1Sets, kL1Ways, kLLSets, kLLWays);

    // Access a buffer of increasing size, make sure hits + misses are expected.
    constexpr int kNumLoops = 4;
    for (uint32_t bufsize = kL1Size / 4; bufsize < kLLSize * 4; bufsize *= 2) {
      std::cerr << "\n**** bufsize=" << bufsize << " *****\n";
        for (int assoc = 1; assoc <= 32; assoc *= 2) {
            cache_simulator_t cache_sim(knobs);
            auto way_size = bufsize / assoc;
            std::cerr << "\n**** assoc=" << assoc << " way_size=" << way_size
                      << " *****\n";
            int read_count = 0;
            for (int lp = 0; lp < kNumLoops; ++lp) {
                for (int i = 0; i < assoc; ++i) {
                    read_count += generate_1D_accesses(cache_sim, i * bufsize, kLineSize,
                                                       way_size / kLineSize, 1);
                }
            }
            CStats l1stats = get_cache_stats(cache_sim, 1);
            auto hits = l1stats.hits;
            auto misses = l1stats.misses;

            int expected_misses = (bufsize <= kL1Size && assoc <= kL1Ways)
                ? bufsize / kLineSize
                : bufsize * kNumLoops / kLineSize;
            int expected_hits = read_count - expected_misses;
            std::cerr << "L1:"
                      << " bufsize=" << bufsize << " L1size=" << kL1Size
                      << " num_accesses=" << read_count << " hits=" << hits
                      << " expected_hits=" << expected_hits << " misses=" << misses
                      << " expected_misses=" << expected_misses << "\n";
            assert(hits == expected_hits);
            assert(misses == expected_misses);

            CStats llstats = get_cache_stats(cache_sim, 2);
            auto llhits = llstats.hits;
            auto llmisses = llstats.misses;

            int expected_llmisses = (bufsize <= kLLSize && assoc <= kLLWays)
                ? bufsize / kLineSize
                : expected_misses;  // bufsize * kNumLoops / kLineSize;
            int expected_llhits = read_count - expected_hits - expected_llmisses;
            std::cerr << "LL:"
                      << " bufsize=" << bufsize << " LLsize=" << kLLSize
                      << " num_accesses=" << expected_misses << " hits=" << llhits
                      << " expected_hits=" << expected_llhits << " misses=" << llmisses
                      << " expected_misses=" << expected_llmisses
                      << " child_hits=" << llstats.child_hits << "\n";
            assert(llhits == expected_llhits);
            assert(llmisses == expected_llmisses);
        }
    }

    /*
    assert(cache_sim.get_cache_metric(metric_name_t::HITS, 1, 0, cache_split_t::DATA) ==
           11);
    assert(cache_sim.get_cache_metric(metric_name_t::MISSES, 2, 0) == 1);
    */
}

int
main(int argc, const char *argv[])
{
    unit_test_cache_size();
    unit_test_cache_assoc();
    unit_test_metrics_API();
    unit_test_compulsory_misses();
    unit_test_warmup_fraction();
    unit_test_warmup_refs();
    unit_test_sim_refs();
    unit_test_child_hits();
    unit_test_cache_replacement_policy();
    return 0;
}
