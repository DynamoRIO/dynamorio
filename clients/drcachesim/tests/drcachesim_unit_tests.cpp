/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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
#include <random>
#include <regex>

#include <assert.h>
#include "config_reader_unit_test.h"
#include "v2p_reader_unit_test.h"
#include "tlb_simulator_unit_test.h"
#include "cache_replacement_policy_unit_test.h"
#include "simulator/cache.h"
#include "simulator/cache_simulator.h"
#include "simulator/policy_lfu.h"
#include "simulator/policy_lru.h"
#include "simulator/prefetcher.h"
#include "../common/memref.h"
#include "../common/utils.h"
#include "test_helpers.h"

namespace dynamorio {
namespace drmemtrace {

// Helper macro to make test failures more verbose.
#define TEST_EQ(A, B)                                             \
    {                                                             \
        auto aa = A;                                              \
        auto bb = B;                                              \
        if (aa != bb) {                                           \
            std::cerr << "ERROR: " << #A << " != " << #B << "\n"; \
            std::cerr << "  " << #A << " = " << aa << "\n";       \
            std::cerr << "  " << #B << " = " << bb << "\n";       \
        }                                                         \
        assert(aa == bb);                                         \
    }

#define MY_TID 1

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

memref_t
make_memref(addr_t address, trace_type_t type = TRACE_TYPE_READ, int size = 4)
{
    memref_t ref = {};
    ref.data.type = type;
    ref.data.size = size;
    ref.data.addr = address;
    ref.data.tid = MY_TID;
    return ref;
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
        ref.data.tid = MY_TID;
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_warmup_fraction failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }

    if (!cache_sim.is_warmed_up()) {
        std::cerr << "drcachesim unit_test_warmup_fraction failed\n";
        exit(1);
    }
}

void
unit_test_warmup_refs()
{
    cache_simulator_knobs_t knobs = make_test_knobs();
    constexpr int WARMUP_REFS = 16;
    knobs.warmup_refs = WARMUP_REFS;
    cache_simulator_t cache_sim(knobs);

    // Feed it some memrefs, warmup refs = 16 where the capacity at
    // each level is 32 lines each. The first 16 memrefs warm up the cache.
    std::string error;
    constexpr int MARKER_COUNT = 4;
    for (int i = 0; i < MARKER_COUNT + WARMUP_REFS; ++i) {
        if (cache_sim.is_warmed_up()) {
            std::cerr << "drcachesim unit_test_warmup_refs warmed up too early\n";
            exit(1);
        }
        memref_t ref;
        if (i < MARKER_COUNT) {
            // Make the first couple markers, to ensure warmup_refs skips markers
            // (xref i#7230).
            ref.marker.type = TRACE_TYPE_MARKER;
            ref.marker.marker_type = TRACE_MARKER_TYPE_CACHE_LINE_SIZE;
            ref.marker.marker_value = 64;
        } else {
            ref.data.type = TRACE_TYPE_READ;
            ref.data.size = 8;
            ref.data.addr = i * 128;
        }
        ref.data.tid = MY_TID;
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_warmup_fraction failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }

    if (!cache_sim.is_warmed_up()) {
        std::cerr << "drcachesim unit_test_warmup_refs failed\n";
        exit(1);
    }
}

void
unit_test_sim_refs()
{
    cache_simulator_knobs_t knobs = make_test_knobs();
    constexpr int REF_LIMIT = 8;
    knobs.sim_refs = REF_LIMIT;
    cache_simulator_t cache_sim(knobs);

    std::string error;
    constexpr int MARKER_COUNT = 3;
    // Go beyond and ensure we stop before then.
    int i;
    for (i = 0; i < MARKER_COUNT + REF_LIMIT + 100; ++i) {
        memref_t ref;
        if (i < MARKER_COUNT) {
            // Make the first couple markers, to ensure sim_refs skips markers
            // (xref i#7230).
            ref.marker.type = TRACE_TYPE_MARKER;
            ref.marker.marker_type = TRACE_MARKER_TYPE_CACHE_LINE_SIZE;
            ref.marker.marker_value = 64;
        } else {
            ref.data.type = TRACE_TYPE_READ;
            ref.data.size = 8;
            ref.data.addr = i * 128;
        }
        ref.data.tid = MY_TID;
        if (!cache_sim.process_memref(ref)) {
            // Check we don't exit before our markers + sim_refs.
            if (!cache_sim.get_error_string().empty()) {
                std::cerr << "drcachesim unit_test_sim_refs failed: "
                          << cache_sim.get_error_string() << "\n";
                exit(1);
            }
            break;
        }
    }
    // The exit is on the subsequent memref, so allow ==.
    if (i > MARKER_COUNT + REF_LIMIT) {
        std::cerr << "drcachesim unit_test_sim_refs failed: went too far\n";
        exit(1);
    }
    if (cache_sim.remaining_sim_refs() != 0) {
        std::cerr << "drcachesim unit_test_sim_refs failed: has remaining refs\n";
        exit(1);
    }
}

void
unit_test_skip_refs()
{
    cache_simulator_knobs_t knobs = make_test_knobs();
    constexpr int SKIP_REFS = 16;
    constexpr int WARMUP_REFS = 16;
    knobs.skip_refs = SKIP_REFS;
    knobs.warmup_refs = WARMUP_REFS;
    cache_simulator_t cache_sim(knobs);

    // Feed it some memrefs, warmup refs = 16 where the capacity at
    // each level is 32 lines each. The first 16 memrefs warm up the cache.
    std::string error;
    constexpr int MARKER_COUNT = 4;
    for (int i = 0; i < MARKER_COUNT + SKIP_REFS + WARMUP_REFS; ++i) {
        if (cache_sim.is_warmed_up()) {
            std::cerr << "drcachesim unit_test_warmup_refs warmed up too early\n";
            exit(1);
        }
        memref_t ref;
        if (i < MARKER_COUNT) {
            // Make the first couple markers, to ensure skip_refs skips markers
            // (xref i#7230).
            ref.marker.type = TRACE_TYPE_MARKER;
            ref.marker.marker_type = TRACE_MARKER_TYPE_CACHE_LINE_SIZE;
            ref.marker.marker_value = 64;
        } else {
            ref.data.type = TRACE_TYPE_READ;
            ref.data.size = 8;
            ref.data.addr = i * 128;
        }
        ref.data.tid = MY_TID;
        if (!cache_sim.process_memref(ref)) {
            std::cerr << "drcachesim unit_test_warmup_fraction failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }

    if (!cache_sim.is_warmed_up()) {
        std::cerr << "drcachesim unit_test_warmup_refs failed\n";
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
    ref.data.tid = MY_TID;

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
    ref.data.tid = MY_TID;

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
unit_test_nextline_prefetcher()
{
    const int LINE_SIZE = 64;
    const int TEST_ACCESSES = 6;
    const int EXPECTED_MISSES_NO_PREFETCHER = TEST_ACCESSES;
    const int EXPECTED_MISSES_NEXTLINE_PREFETCHER = TEST_ACCESSES / 2;

    // Test all misses without a prefetcher.
    cache_simulator_knobs_t knobs = make_test_knobs();
    cache_simulator_t cache_sim(knobs);

    for (int i = 0; i < TEST_ACCESSES; i++) {
        if (!cache_sim.process_memref(make_memref(i * LINE_SIZE))) {
            std::cerr << "drcachesim unit_test_nextline_prefetcher failed: "
                      << cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }

    assert(cache_sim.get_cache_metric(metric_name_t::MISSES, 1, 0) ==
           EXPECTED_MISSES_NO_PREFETCHER);

    // Test that every other miss is prevented with a nextline prefetcher.
    knobs.data_prefetcher = "nextline";
    cache_simulator_t nextline_cache_sim(knobs);
    for (int i = 0; i < TEST_ACCESSES; i++) {
        if (!nextline_cache_sim.process_memref(make_memref(i * LINE_SIZE))) {
            std::cerr << "drcachesim unit_test_nextline_prefetcher failed: "
                      << nextline_cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }

    assert(nextline_cache_sim.get_cache_metric(metric_name_t::MISSES, 1, 0) ==
           EXPECTED_MISSES_NEXTLINE_PREFETCHER);
}

class next2line_prefetcher_factory_t : public prefetcher_factory_t {
public:
    class next2line_prefetcher_t : public prefetcher_t {
    public:
        next2line_prefetcher_t(int block_size)
            : prefetcher_t(block_size)
        {
        }
        void
        prefetch(caching_device_t *cache, const memref_t &memref_in, const bool missed)
        {
            // We implement a simple 2 next-line prefetcher.
            // We also track whether inputs are hits or misses for testing.
            if (missed) {
                misses_++;
                memref_t memref = memref_in;
                memref.data.addr += block_size_;
                memref.data.type = TRACE_TYPE_HARDWARE_PREFETCH;
                cache->request(memref);
                memref.data.addr += block_size_;
                cache->request(memref);
            } else {
                hits_++;
            }
        }
        int hits_ = 0;
        int misses_ = 0;
    };

    next2line_prefetcher_t *
    create_prefetcher(int block_size) override
    {
        prefetcher_ = new next2line_prefetcher_t(block_size);
        return prefetcher_;
    }
    next2line_prefetcher_t *prefetcher_;
};

void
unit_test_custom_prefetcher()
{
    const int LINE_SIZE = 64;
    const int TEST_ACCESSES = 6;
    const int EXPECTED_MISSES_NEXT2LINE_PREFETCHER = TEST_ACCESSES / 3;
    cache_simulator_knobs_t knobs = make_test_knobs();
    knobs.data_prefetcher = "custom";
    next2line_prefetcher_factory_t next2line_prefetcher_factory;
    cache_simulator_t nextline_cache_sim(knobs, &next2line_prefetcher_factory);
    // Test that every 2/3 misses are prevented with a next2line prefetcher.
    for (int i = 0; i < TEST_ACCESSES; i++) {
        if (!nextline_cache_sim.process_memref(make_memref(i * LINE_SIZE))) {
            std::cerr << "drcachesim unit_test_custom_prefetcher failed: "
                      << nextline_cache_sim.get_error_string() << "\n";
            exit(1);
        }
    }

    assert(nextline_cache_sim.get_cache_metric(metric_name_t::MISSES, 1, 0) ==
           EXPECTED_MISSES_NEXT2LINE_PREFETCHER);
    assert(next2line_prefetcher_factory.prefetcher_->hits_ == 4);
    assert(next2line_prefetcher_factory.prefetcher_->misses_ == 2);
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
    ref.data.tid = MY_TID;

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

// cache_simulator_t wrapper to make cache objects accessible by name.
class test_cache_simulator_t : public cache_simulator_t {
public:
    test_cache_simulator_t(const cache_simulator_knobs_t &knobs)
        : cache_simulator_t(knobs) {};
    test_cache_simulator_t(std::istream *config_file)
        : cache_simulator_t(config_file) {};
    // Returns cache_t* for named cache if it exists, else faults.
    cache_t *
    get_named_cache(std::string name)
    {
        return all_caches_.at(name);
    }
};

void
unit_test_set_parent()
{
    cache_t child_1;
    cache_t child_2;
    cache_t parent;
    assert(child_1.init(1, 64, 1024, nullptr, new cache_stats_t(64, "", false, false),
                        std::unique_ptr<policy_lru_t>(new policy_lru_t(1024, 1))));
    assert(child_2.init(1, 64, 1024, nullptr, new cache_stats_t(64, "", false, false),
                        std::unique_ptr<policy_lru_t>(new policy_lru_t(1024, 1))));
    assert(parent.init(1, 64, 1024, nullptr, new cache_stats_t(64, "", false, false),
                       std::unique_ptr<policy_lru_t>(new policy_lru_t(1024, 1))));
    // Test setting parent.
    child_1.set_parent(&parent);
    assert(child_1.get_parent() == &parent);
    assert(parent.get_parent() == nullptr);
    assert(parent.get_children() == std::vector<caching_device_t *> { &child_1 });
    assert(child_1.get_children().empty());
    // Test removing parent.
    child_1.set_parent(nullptr);
    assert(parent.get_parent() == nullptr);
    assert(child_1.get_parent() == nullptr);
    assert(parent.get_children().empty());
    assert(child_1.get_children().empty());
    // Test multiple children.
    child_1.set_parent(&parent);
    child_2.set_parent(&parent);
    assert(child_1.get_parent() == &parent);
    assert(child_2.get_parent() == &parent);
    assert((parent.get_children() ==
            std::vector<caching_device_t *> { &child_1, &child_2 }));
    // Test existing child.
    child_2.set_parent(&parent);
    assert(child_1.get_parent() == &parent);
    assert(child_2.get_parent() == &parent);
    assert((parent.get_children() ==
            std::vector<caching_device_t *> { &child_1, &child_2 }));
}

void
unit_test_exclusive_cache_policy()
{
    // Exclusive caches exercise some unique code paths related to line
    // replacement.  This test was developed to track down an observed
    // bug.  The subsequent randomized test takes a more shotgun approach
    // to try to cover any cases this test misses.
    std::cerr << "\n** EXCLUSIVE POLICY TEST ***\n";

    // Create simple 2-level cache with exclusive LLC.
    std::string config = R"MYCONFIG(// 2-level with exclusive LLC.
num_cores       1
line_size       64
coherent        false

L1 {
  type            unified
  core            0
  size            1K
  assoc           1
  prefetcher      none
  parent          LLC
}
LLC {
  size            4K
  assoc           4
  exclusive       true
  prefetcher      none
  replace_policy  LRU
  parent          memory
}
)MYCONFIG";
    std::istringstream config_in(config);
    test_cache_simulator_t cache_sim(&config_in);

    // The cache config specified no coherence.
    TEST_EQ(cache_sim.get_num_snooped_caches(), 0);

    cache_t *l1 = cache_sim.get_named_cache("L1");
    cache_t *llc = cache_sim.get_named_cache("LLC");
    assert(l1 != nullptr);
    assert(llc != nullptr);
    assert(l1 != llc);
    TEST_EQ(l1->get_parent(), llc);

    // L1 is 1-way 1KB, while LLC is 4-way 4KB LRU exclusive.
    //
    // Together they should behave like a 5KB 5-way LRU cache.
    //
    // If we loop through up to 5 conflicting addresses, they should all fit in
    // the caches.  But beyond 5, there should be misses to memory.
    //
    // Furthermore, once lines start getting evicted, LRU should keep the
    // recent lines in the cache and only evict old lines.

    constexpr int NUM_LOOPS = 10;
    const int ADDR_STRIDE = llc->get_size_bytes(); // Guaranteed to conflict.

    // Helper routines to grab cache stats as if the full hierarchy were a
    // single cache.  So this means HITS are summed, but only LLC misses count.
    auto get_hits = [&](void) {
        int64_t l1_hits = cache_sim.get_cache_metric(metric_name_t::HITS, /*level=*/1,
                                                     /*core=*/0, cache_split_t::DATA);
        int64_t l2_hits = cache_sim.get_cache_metric(metric_name_t::HITS, /*level=*/2,
                                                     /*core=*/0, cache_split_t::DATA);
        return l1_hits + l2_hits;
    };
    auto get_misses = [&](void) {
        int64_t l2_misses = cache_sim.get_cache_metric(metric_name_t::MISSES, /*level=*/2,
                                                       /*core=*/0, cache_split_t::DATA);
        return l2_misses;
    };

    // Helper routine to loop through a series of conflicting lines.
    // The actual addresses are line index * address stride to make sure
    // all lines are conflicting.
    auto process_test_lines = [&](int loops, const std::vector<int> &lines) {
        for (int i = 0; i < loops; ++i) {
            for (const int line : lines) {
                addr_t maddr = ADDR_STRIDE * line;
                if (!cache_sim.process_memref(make_memref(maddr))) {
                    std::cerr << "drcachesim failed: " << cache_sim.get_error_string()
                              << "\n";
                    exit(1);
                }
            }
        }
    };

    // First, test a sequence of lines that will fit within the cache
    // associativity.  First five accesses miss, the rest hit.
    //         expectation ----->  M  M  M  M  M  H  H  H  H  H
    std::vector<int> test_lines1 { 1, 2, 3, 4, 5, 1, 2, 3, 4, 5 };

    process_test_lines(NUM_LOOPS, test_lines1);
    constexpr int EXP1_MISSES = 5;
    const int exp1_hits = test_lines1.size() * NUM_LOOPS - EXP1_MISSES;
    TEST_EQ(get_misses(), EXP1_MISSES);
    TEST_EQ(get_hits(), exp1_hits);

    // Next, access more lines than fit in the cache, which should cause a few
    // misses and replacements.  Note lines 3 and 6 are accessed frequently to keep
    // them recently-accessed and thus not evicted.
    //         expectation ----->  H  M  H  H  M  H  M  H  H  M  M  H  H  H
    //    evicted line ordinal ->     1        2     5        4  7
    std::vector<int> test_lines2 { 5, 6, 4, 3, 7, 6, 2, 6, 3, 1, 5, 6, 2, 3 };
    process_test_lines(1, test_lines2);
    constexpr int EXP2_MISSES = 5;
    int exp2_hits = test_lines2.size() - EXP2_MISSES;
    TEST_EQ(get_misses(), EXP1_MISSES + EXP2_MISSES);
    TEST_EQ(get_hits(), exp1_hits + exp2_hits);
}

void
unit_test_exclusive_cache_policy_rand()
{
    // A more extensive test of the exclusive cache logic using the property
    // of an exclusive LRU cache to "extend" the associativity of its child
    // LRU cache.
    std::cerr << "\n** EXCLUSIVE POLICY TEST w/ RANDOM ***\n";

    // Create 2-level cache with 3-way L1 and 5-way exclusive LLC, which
    // should behave the same as a 1-level 8-way cache in terms of hits and
    // misses.
    std::string config_exc = R"MYCONFIG(// 2-level with exclusive LLC.
num_cores       1
line_size       64
coherent        false

L1 {
  type            unified
  core            0
  size            3K
  assoc           3
  prefetcher      none
  replace_policy  LRU
  parent          LLC
}
LLC {
  size            5K
  assoc           5
  exclusive       true
  prefetcher      none
  replace_policy  LRU
  parent          memory
}
)MYCONFIG";

    // Create the reference 1-level 8-way equivalent LRU cache.
    std::string config_8way = R"MYCONFIG(// 1-level
num_cores       1
line_size       64
coherent        false

L1 {
  type            unified
  core            0
  size            8K
  assoc           8
  prefetcher      none
  replace_policy  LRU
  parent          memory
}
)MYCONFIG";

    // Create the two cache simulators.
    std::istringstream config_in_exc(config_exc);
    test_cache_simulator_t cache_sim_exc(&config_in_exc);

    std::istringstream config_in_8way(config_8way);
    test_cache_simulator_t cache_sim_8way(&config_in_8way);

    // Verify the cache configs specified no coherence.
    TEST_EQ(cache_sim_exc.get_num_snooped_caches(), 0);
    TEST_EQ(cache_sim_8way.get_num_snooped_caches(), 0);

    // Helper routines to grab cache stats as if the full hierarchy were a
    // single cache.  So this means HITS are summed, but only LLC misses count.
    auto get_hits_exc = [&](void) {
        int64_t l1_hits = cache_sim_exc.get_cache_metric(metric_name_t::HITS, /*level=*/1,
                                                         /*core=*/0, cache_split_t::DATA);
        int64_t l2_hits = cache_sim_exc.get_cache_metric(metric_name_t::HITS, /*level=*/2,
                                                         /*core=*/0, cache_split_t::DATA);
        return l1_hits + l2_hits;
    };
    auto get_misses_exc = [&](void) {
        int64_t l2_misses =
            cache_sim_exc.get_cache_metric(metric_name_t::MISSES, /*level=*/2,
                                           /*core=*/0, cache_split_t::DATA);
        return l2_misses;
    };

    // Similar to the above, but for the 1-level 8-way cache.
    auto get_hits_8way = [&](void) {
        int64_t l1_hits =
            cache_sim_8way.get_cache_metric(metric_name_t::HITS, /*level=*/1,
                                            /*core=*/0, cache_split_t::DATA);
        return l1_hits;
    };
    auto get_misses_8way = [&](void) {
        int64_t l1_misses =
            cache_sim_8way.get_cache_metric(metric_name_t::MISSES, /*level=*/1,
                                            /*core=*/0, cache_split_t::DATA);
        return l1_misses;
    };

    // Generate a random sequence of integers that will be converted to
    // conflicting cacheline addresses, run them through both caches, and
    // verify the caches have identical hit rates.
    // Use a geometric distribution to get clustering of similar addresses,
    // thus favoring hits (compared to a uniform distribution).
    std::random_device rd;
    std::mt19937 gen(rd());
    // The dist parameter was chosen to get a long tail of misses.
    // Higher values cause more clustering of the distribution,
    // e.g. more hits and fewer misses.
    std::geometric_distribution<> dist(0.25);

    // Run a bunch of random conflicting cache addresses through both caches
    // to give the replacement logic a workout.
    constexpr int NUM_LINES = 10000;
    // Pick a large multiple of the cache size as our stride, to ensure all
    // generated addresses conflict.
    const int ADDR_STRIDE = cache_sim_8way.get_named_cache("L1")->get_size_bytes() * 4;
    for (int i = 0; i < NUM_LINES; ++i) {
        // Generate a random address that will hit set 0.
        const int line_number = dist(gen);
        const addr_t maddr = ADDR_STRIDE * line_number;
        memref_t memref = make_memref(maddr);
        if (!cache_sim_exc.process_memref(memref)) {
            std::cerr << "drcachesim failed: " << cache_sim_exc.get_error_string()
                      << "\n";
            exit(1);
        }
        if (!cache_sim_8way.process_memref(memref)) {
            std::cerr << "drcachesim failed: " << cache_sim_8way.get_error_string()
                      << "\n";
            exit(1);
        }
        TEST_EQ(get_misses_8way(), get_misses_exc());
    }
    std::cerr << "8way cache had " << get_hits_8way() << " hits and " << get_misses_8way()
              << " misses.\n";

    // Make sure both recorded the same number of hits and misses, and that
    // there were more hits than misses.
    assert(get_misses_8way() > 1);
    assert(get_hits_8way() > get_misses_8way());

    TEST_EQ(get_hits_8way() + get_misses_8way(), NUM_LINES);
    TEST_EQ(get_hits_8way(), get_hits_exc());
    TEST_EQ(get_misses_8way(), get_misses_exc());
}

void
unit_test_exclusive_cache()
{
    // Create simple 3-level cache with exclusive LLC.
    std::string config = R"MYCONFIG(// 3-level with exclusive LLC.
num_cores       1
line_size       64
coherent        true

L1I {
  type            instruction
  core            0
  size            256
  assoc           1
  prefetcher      none
  parent          L2
}
L1D {
  type            data
  core            0
  size            256
  assoc           1
  prefetcher      none
  parent          L2
}
L2 {
  size            4K
  assoc           4
  inclusive       true
  prefetcher      none
  parent          LLC
}
LLC {
  size            64K
  assoc           4
  exclusive       true
  prefetcher      none
  parent          memory
}
)MYCONFIG";
    std::istringstream config_in(config);
    test_cache_simulator_t cache_sim(&config_in);

    // The cache config specified coherence, and the only level with
    // multiple caches is L1.  So there should be 2 snooped caches.
    TEST_EQ(cache_sim.get_num_snooped_caches(), 2);

    // L1s are 1-way, while L2 and LLC are both 4-way.
    // If we cycle through 4 conflicting lines multiple times, the L2 will
    // hold all four lines and never evict anything to LLC: we expect all
    // misses in L1, 4 misses and many hits in L2, 4 misses and no hits
    // in LLC.

    // Test 4 conflicting lines.
    const int NUM_LOOPS = 16;
    const int L2_ASSOC = 4;
    const int LLC_ASSOC = 4;
    const int LLC_SIZE = 64 * 1024;
    const int ADDR_STRIDE = LLC_SIZE; // Maximize conflicts.
    const int CONFLICTING_ADDRESSES = 4;
    for (int n = 0; n < NUM_LOOPS; ++n) {
        for (int i = 0; i < CONFLICTING_ADDRESSES; ++i) {
            if (!cache_sim.process_memref(make_memref(ADDR_STRIDE * i))) {
                std::cerr << "drcachesim failed: " << cache_sim.get_error_string()
                          << "\n";
                exit(1);
            }
        }
    }

    TEST_EQ(cache_sim.get_named_cache("L2")->get_associativity(), L2_ASSOC);
    TEST_EQ(cache_sim.get_named_cache("L2")->get_replace_policy(), "LRU");
    TEST_EQ(cache_sim.get_named_cache("LLC")->get_associativity(), LLC_ASSOC);
    TEST_EQ(cache_sim.get_named_cache("LLC")->get_size_bytes(), LLC_SIZE);
    TEST_EQ(cache_sim.get_named_cache("LLC")->get_replace_policy(), "LRU");

    // Define stats helper functions that are specific to this test config.
    auto get_l2_metric = [&](metric_name_t metric) {
        return cache_sim.get_cache_metric(metric, /*level=*/2, /*core=*/0,
                                          cache_split_t::DATA);
    };
    auto get_llc_metric = [&](metric_name_t metric) {
        return cache_sim.get_cache_metric(metric, /*level=*/3, /*core=*/0,
                                          cache_split_t::DATA);
    };

    int l2_misses = get_l2_metric(metric_name_t::MISSES);
    int l2_hits = get_l2_metric(metric_name_t::HITS);
    int llc_misses = get_llc_metric(metric_name_t::MISSES);
    int llc_hits = get_llc_metric(metric_name_t::HITS);

    TEST_EQ(l2_misses, CONFLICTING_ADDRESSES);
    TEST_EQ(l2_hits, (NUM_LOOPS - 1) * CONFLICTING_ADDRESSES);
    TEST_EQ(llc_misses, l2_misses);
    TEST_EQ(llc_hits, 0);

    // Increasing to 8 conflicting lines means no single cache can hold all
    // of the lines, but as a victim cache the LLC is additive and should hold
    // L2's evictions:  we expect 4 hits (from prior test) and the rest
    // misses in L2, but 4 (new) misses and the rest hits in LLC.
    // Since the L2 is inclusive, the L1 does NOT hold any lines not in the L2.
    const int MORE_CONFLICTING_ADDRESSES = 8;
    for (int n = 0; n < NUM_LOOPS; ++n) {
        for (int i = 0; i < MORE_CONFLICTING_ADDRESSES; ++i) {
            if (!cache_sim.process_memref(make_memref(ADDR_STRIDE * i))) {
                std::cerr << "drcachesim unit_test_child_hits failed: "
                          << cache_sim.get_error_string() << "\n";
                exit(1);
            }
        }
    }

    int new_l2_misses = get_l2_metric(metric_name_t::MISSES);
    int new_l2_hits = get_l2_metric(metric_name_t::HITS);
    int new_llc_misses = get_llc_metric(metric_name_t::MISSES);
    int new_llc_hits = get_llc_metric(metric_name_t::HITS);

    // Subtract out the counts from the prior accesses.
    TEST_EQ(new_l2_hits - l2_hits, CONFLICTING_ADDRESSES);
    TEST_EQ(new_l2_misses - l2_misses,
            NUM_LOOPS * MORE_CONFLICTING_ADDRESSES - CONFLICTING_ADDRESSES);

    TEST_EQ(new_llc_misses - llc_misses,
            MORE_CONFLICTING_ADDRESSES - CONFLICTING_ADDRESSES);
    TEST_EQ(new_llc_hits - llc_hits, (NUM_LOOPS - 1) * MORE_CONFLICTING_ADDRESSES);
}

// Generate a sequence of read accesses to a cache in a 2-D access pattern.
// Loop A is the outer loop, while loop B is the inner, fastest-changing
// loop.  The whole 2D access pattern is repeated <loop_count> times.
// Each access is to <start_address> + <A index> * <step_size_a>
//                                   + <B index> * <step_size_b>
// Returns the total number of accesses performed.
static int
generate_2D_accesses(cache_t &cache, addr_t start_address, int step_size_a,
                     int step_count_a, int step_size_b, int step_count_b,
                     int loop_count = 1)
{
    int access_count = 0;
    for (int i = 0; i < loop_count; ++i) {
        memref_t ref;
        ref.data.type = TRACE_TYPE_READ;
        ref.data.size = 4;
        ref.data.tid = MY_TID;
        for (int step_a = 0; step_a < step_count_a; ++step_a) {
            for (int step_b = 0; step_b < step_count_b; ++step_b) {
                addr_t addr = start_address + step_a * step_size_a + step_b * step_size_b;
                ref.data.addr = addr;
                cache.request(ref);
                ++access_count;
            }
        }
    }
    return access_count;
}

// Convenience wrapper for a linear access pattern.
static int
generate_1D_accesses(cache_t &cache, addr_t start_address, int step_size, int step_count,
                     int loop_count = 1)
{
    return generate_2D_accesses(cache, start_address, step_size, step_count, 0, 1,
                                loop_count);
}

// Helper code to grab a snapshot of cache stats.
struct cache_stats_snapshot_t {
    int64_t hits;
    int64_t misses;
    int64_t child_hits;
};

static cache_stats_snapshot_t
get_cache_stats(caching_device_stats_t &stats)
{
    cache_stats_snapshot_t cs;
    cs.hits = stats.get_metric(metric_name_t::HITS);
    cs.misses = stats.get_metric(metric_name_t::MISSES);
    cs.child_hits = stats.get_metric(metric_name_t::CHILD_HITS);
    return cs;
}

// Creates and tests LRU caches in a range of assocativities, verifying
// the associativity works as expected.
void
unit_test_cache_associativity()
{
    // Range of associativities to be tested.
    static constexpr int MIN_ASSOC = 1;
    static constexpr int MAX_ASSOC = 16;

    static constexpr int LINE_SIZE = 32;
    static constexpr int BLOCKS_PER_WAY = 16;

    // Test all associativities.
    for (int assoc = MIN_ASSOC; assoc <= MAX_ASSOC; ++assoc) {
        int total_size = LINE_SIZE * BLOCKS_PER_WAY * assoc;
        // Test access patterns that stress increasing associativity.
        for (uint32_t test_assoc = 1; test_assoc <= 2 * assoc; ++test_assoc) {
            cache_t cache;
            caching_device_stats_t stats(/*miss_file=*/"", LINE_SIZE);
            bool initialized =
                cache.init(assoc, LINE_SIZE, total_size, /*parent=*/nullptr, &stats,
                           std::unique_ptr<policy_lru_t>(
                               new policy_lru_t(total_size / assoc, assoc)));
            assert(initialized);
            assert(cache.get_associativity() == assoc);
            // Test start address is arbitrary.
            addr_t start_address = test_assoc * total_size;

            static constexpr int NUM_LOOPS = 3; // Anything >=2 should work.
            auto read_count =
                generate_2D_accesses(cache, start_address, LINE_SIZE, BLOCKS_PER_WAY,
                                     total_size, test_assoc, NUM_LOOPS);
            cache_stats_snapshot_t c_stats = get_cache_stats(stats);
            assert(read_count == NUM_LOOPS * BLOCKS_PER_WAY * test_assoc);

            // This is an LRU cache, so once the buffer size exceeds the cache
            // size there will be no hits.
            int expected_misses =
                (test_assoc <= assoc) ? BLOCKS_PER_WAY * test_assoc : read_count;
            int expected_hits = read_count - expected_misses;
            if (expected_hits != c_stats.hits || expected_misses != c_stats.misses) {
                std::cerr << "ERROR in unit_test_cache_associativity():"
                          << " test_assoc=" << test_assoc << " read_count=" << read_count
                          << " expected_hits=" << expected_hits
                          << " actual_hits=" << c_stats.hits
                          << " expected_misses=" << expected_misses
                          << " actual_misses=" << c_stats.misses << "\n";
            }
            assert(expected_hits == c_stats.hits);
            assert(expected_misses == c_stats.misses);
        }
    }
}

// Tests an LRU cache to verify it behaves as its size requires.
void
unit_test_cache_size()
{
    // Range of cache sizes to test, including some non-power-of-two.
    static const int TEST_SIZES_KB[] = { 16, 32, 48, 256, 768, 2048 };
    static constexpr int LINE_SIZE = 64;

    for (int cache_size_kb : TEST_SIZES_KB) {
        int cache_size = cache_size_kb * 1024;
        int associativity = IS_POWER_OF_2(cache_size_kb) ? 2 : 3;
        // Access a buffer of increasing size, make sure hits + misses are expected.
        for (int buffer_size = cache_size / 2; buffer_size < cache_size * 2;
             buffer_size *= 2) {
            cache_t cache;
            caching_device_stats_t stats(/*miss_file=*/"", LINE_SIZE);
            bool initialized =
                cache.init(associativity, LINE_SIZE, cache_size,
                           /*parent=*/nullptr, &stats,
                           std::unique_ptr<policy_lru_t>(new policy_lru_t(
                               cache_size / associativity, associativity)));
            assert(initialized);
            assert(cache.get_size_bytes() == cache_size);
            static constexpr int NUM_LOOPS = 3; // Anything >=2 should work.
            auto read_count = generate_1D_accesses(cache, 0, LINE_SIZE,
                                                   buffer_size / LINE_SIZE, NUM_LOOPS);
            cache_stats_snapshot_t c_stats = get_cache_stats(stats);

            int expected_misses = (buffer_size <= cache_size)
                ? buffer_size / LINE_SIZE
                : buffer_size * NUM_LOOPS / LINE_SIZE;
            int expected_hits = read_count - expected_misses;

            if (expected_hits != c_stats.hits || expected_misses != c_stats.misses) {
                std::cerr << "ERROR in unit_test_cache_size():"
                          << " cache_size=" << cache_size
                          << " test_buffer_size=" << buffer_size
                          << " num_accesses=" << read_count << " hits=" << c_stats.hits
                          << " expected_hits=" << expected_hits
                          << " misses=" << c_stats.misses
                          << " expected_misses=" << expected_misses << "\n";
            }
            assert(c_stats.hits == expected_hits);
            assert(c_stats.misses == expected_misses);
        }
    }
}

// Tests a cache to verify its line size works as expected.
void
unit_test_cache_line_size()
{
    // Range of line sizes to test.
    static constexpr int MIN_LINE_SIZE = 16;
    static constexpr int MAX_LINE_SIZE = 256;

    static constexpr int BLOCKS_PER_WAY = 16;
    static constexpr int ASSOCIATIVITY = 2;

    for (int line_size = MIN_LINE_SIZE; line_size <= MAX_LINE_SIZE; line_size *= 2) {
        // Stride through the cache at a test line size.  If test line size is
        // less than actual line size, there will be cache hits.  If test line
        // size is larger than actual line size, there will be fewer misses than
        // lines in the cache.
        for (uint32_t stride = line_size / 2; stride < line_size * 2; stride *= 2) {
            int cache_line_count = BLOCKS_PER_WAY * ASSOCIATIVITY;
            int total_cache_size = line_size * cache_line_count;
            cache_t cache;
            caching_device_stats_t stats(/*miss_file=*/"", line_size);
            bool initialized =
                cache.init(ASSOCIATIVITY, line_size, total_cache_size,
                           /*parent=*/nullptr, &stats,
                           std::unique_ptr<policy_lfu_t>(new policy_lfu_t(
                               total_cache_size / ASSOCIATIVITY, ASSOCIATIVITY)));
            assert(initialized);
            auto read_count =
                generate_1D_accesses(cache, 0, stride, total_cache_size / stride);
            cache_stats_snapshot_t c_stats = get_cache_stats(stats);

            int expected_misses =
                (stride <= line_size) ? cache_line_count : total_cache_size / stride;
            int expected_hits = read_count - expected_misses;
            assert(read_count == total_cache_size / stride);

            if (expected_hits != c_stats.hits || expected_misses != c_stats.misses) {
                std::cerr << "ERROR in unit_test_cache_line_size():"
                          << " line_size=" << line_size << " stride=" << stride
                          << " read_count=" << read_count << " hits=" << c_stats.hits
                          << " expected_hits=" << expected_hits
                          << " misses=" << c_stats.misses
                          << " expected_misses=" << expected_misses << "\n";
            }
            assert(c_stats.hits == expected_hits);
            assert(c_stats.misses == expected_misses);
        }
    }
}

// Verify that illegal cache configurations are rejected.
void
unit_test_cache_bad_configs()
{
    // Safe values we aren't testing.
    static constexpr int SAFE_ASSOC = 1;
    static constexpr int SAFE_LINE_SIZE = 32;
    static constexpr int SAFE_CACHE_SIZE = 1024;

    // Setup the cache to test.
    cache_t cache;
    caching_device_stats_t stats(/*miss_file=*/"", SAFE_LINE_SIZE);

    // 0 values are bad for any of these parameters.
    std::cerr << "Testing 0 parameters.\n";

    assert(!cache.init(0, SAFE_LINE_SIZE, SAFE_CACHE_SIZE, /*parent=*/nullptr, &stats,
                       std::unique_ptr<policy_lru_t>(
                           new policy_lru_t(SAFE_CACHE_SIZE / SAFE_ASSOC, SAFE_ASSOC))));
    assert(!cache.init(SAFE_ASSOC, 0, SAFE_CACHE_SIZE, /*parent=*/nullptr, &stats,
                       std::unique_ptr<policy_lru_t>(
                           new policy_lru_t(SAFE_CACHE_SIZE / SAFE_ASSOC, SAFE_ASSOC))));
    assert(!cache.init(SAFE_ASSOC, SAFE_LINE_SIZE, 0, /*parent=*/nullptr, &stats,
                       std::unique_ptr<policy_lru_t>(
                           new policy_lru_t(SAFE_CACHE_SIZE / SAFE_ASSOC, SAFE_ASSOC))));
    assert(!cache.init(SAFE_ASSOC, SAFE_LINE_SIZE, SAFE_CACHE_SIZE, /*parent=*/nullptr,
                       &stats, nullptr));

    // Test other bad line sizes: <4 and/or non-power-of-two.
    std::cerr << "Testing bad line size parameters.\n";
    assert(!cache.init(SAFE_ASSOC, 1, SAFE_CACHE_SIZE, /*parent=*/nullptr, &stats,
                       std::unique_ptr<policy_lru_t>(
                           new policy_lru_t(SAFE_CACHE_SIZE / SAFE_ASSOC, SAFE_ASSOC))));
    assert(!cache.init(SAFE_ASSOC, 2, SAFE_CACHE_SIZE, /*parent=*/nullptr, &stats,
                       std::unique_ptr<policy_lru_t>(
                           new policy_lru_t(SAFE_CACHE_SIZE / SAFE_ASSOC, SAFE_ASSOC))));
    assert(!cache.init(SAFE_ASSOC, 7, SAFE_CACHE_SIZE, /*parent=*/nullptr, &stats,
                       std::unique_ptr<policy_lru_t>(
                           new policy_lru_t(SAFE_CACHE_SIZE / SAFE_ASSOC, SAFE_ASSOC))));
    assert(!cache.init(SAFE_ASSOC, 65, SAFE_CACHE_SIZE, /*parent=*/nullptr, &stats,
                       std::unique_ptr<policy_lru_t>(
                           new policy_lru_t(SAFE_CACHE_SIZE / SAFE_ASSOC, SAFE_ASSOC))));

    // Size, associativity, and line_size are related.  The requirement is that
    // size/associativity is a power-of-two, and >= line_size, so try some
    // combinations that should fail.
    std::cerr << "Testing bad associativity and size combinations.\n";
    struct {
        int assoc;
        int size;
    } bad_combinations[] = {
        { 3, 1024 }, { 4, 768 }, { 64, 64 }, { 16, 8 * SAFE_LINE_SIZE }
    };
    for (const auto &combo : bad_combinations) {
        assert(!cache.init(combo.assoc, SAFE_LINE_SIZE, combo.size, /*parent=*/nullptr,
                           &stats,
                           std::unique_ptr<policy_lru_t>(new policy_lru_t(
                               SAFE_CACHE_SIZE / SAFE_ASSOC, SAFE_ASSOC))));
    }
}

// Tests cache attribute accessors.
void
unit_test_cache_accessors()
{
    static const int TEST_ASSOCIATIVITIES[] = { 1, 7, 16 };
    static const int TEST_SET_COUNTS[] = { 16, 128, 512 }; // Must be PO2.
    static const int TEST_LINE_SIZES[] = { 16, 64, 256 };  // Must be PO2.

    int loop_count = 0;
    for (int associativity : TEST_ASSOCIATIVITIES) {
        for (int set_count : TEST_SET_COUNTS) {
            for (int line_size : TEST_LINE_SIZES) {
                // Just cycle through these combinations.  No need to be exhaustive.
                bool coherent = TESTANY(0x1, loop_count);
                bool inclusive = TESTANY(0x2, loop_count);
                bool exclusive = !inclusive && TESTANY(0x4, loop_count);
                cache_inclusion_policy_t policy = inclusive
                    ? cache_inclusion_policy_t::INCLUSIVE
                    : exclusive ? cache_inclusion_policy_t::EXCLUSIVE
                                : cache_inclusion_policy_t::NON_INC_NON_EXC;
                ++loop_count;

                int total_size = associativity * set_count * line_size;
                std::string cache_name = "Test" + std::to_string(total_size);
                caching_device_stats_t stats(/*miss_file=*/"", line_size);
                // Only test LRU here.  Other replacement policy accessors are
                // tested in the cache_replacement_policy_unit_test.
                cache_t cache(cache_name);
                bool initialized =
                    cache.init(associativity, line_size, total_size,
                               /*parent=*/nullptr, &stats,
                               /*replacement_policy=*/
                               std::unique_ptr<policy_lru_t>(new policy_lru_t(
                                   total_size / associativity, associativity)),
                               /*prefetcher=*/nullptr, policy, coherent);
                assert(initialized);
                assert(cache.get_stats() == &stats);
                assert(stats.get_caching_device() == &cache);
                assert(cache.get_name() == cache_name);
                assert(cache.get_replace_policy() == "LRU");
                assert(cache.get_associativity() == associativity);
                assert(cache.get_size_bytes() == total_size);
                assert(cache.get_block_size() == line_size);
                assert(cache.get_num_blocks() == total_size / line_size);
                assert(cache.is_inclusive() == inclusive);
                assert(cache.is_exclusive() == exclusive);
                assert(cache.is_coherent() == coherent);
            }
        }
    }
}

void
unit_test_core_sharded()
{
    {
        // Test invalid cpu_scheduling + core-sharded combo.
        cache_simulator_knobs_t knobs = make_test_knobs();
        knobs.cpu_scheduling = true;
        cache_simulator_t sim(knobs);
        std::string error = sim.initialize_shard_type(SHARD_BY_CORE);
        assert(!error.empty());
    }
    {
        // Test cpu to core mapping by passing larger integers as cpus.
        cache_simulator_knobs_t knobs = make_test_knobs();
        knobs.num_cores = 2;
        cache_simulator_t sim(knobs);
        default_memtrace_stream_t stream;
        sim.initialize_stream(&stream);
        std::string error = sim.initialize_shard_type(SHARD_BY_CORE);
        assert(error.empty());
        memref_t ref = make_memref(42);
        stream.set_shard_index(0);
        stream.set_output_cpuid(123400);
        bool res = sim.process_memref(ref);
        assert(res);
        stream.set_shard_index(1);
        stream.set_output_cpuid(567800);
        res = sim.process_memref(ref);
        assert(res);
        // Capture output.
        std::stringstream output;
        std::streambuf *prev_buf = std::cerr.rdbuf(output.rdbuf());
        res = sim.print_results();
        assert(res);
        std::cerr.rdbuf(prev_buf);
        // Make sure the large cpuids are mapped to core 0 and core 1.
        // XXX: This regex causes a "regex_constants::error_complexity"
        // exception on Windows; for now we disable this part of the test there.
#ifndef WINDOWS
        assert(std::regex_search(output.str(), std::regex(R"DELIM((.|\r?\n)*
Core #0 \(traced CPU\(s\): #123400\)
(.|\r?\n)*
Core #1 \(traced CPU\(s\): #567800\)
(.|\r?\n)*
)DELIM")));
#endif
    }
    {
        // Test graceful handling of too-few cpus.
        cache_simulator_knobs_t knobs = make_test_knobs();
        knobs.num_cores = 2;
        cache_simulator_t sim(knobs);
        default_memtrace_stream_t stream;
        sim.initialize_stream(&stream);
        std::string error = sim.initialize_shard_type(SHARD_BY_CORE);
        assert(error.empty());
        memref_t ref = make_memref(42);
        stream.set_shard_index(2); // Too large for knobs.num_cores.
        stream.set_output_cpuid(1);
        bool res = sim.process_memref(ref);
        // We should see graceful failure and not a crash.
        assert(!res);
        assert(!sim.get_error_string().empty());
    }
}

int
test_main(int argc, const char *argv[])
{
    // Takes in a path to the tests/ src dir.
    assert(argc == 2);

    unit_test_exclusive_cache();
    unit_test_exclusive_cache_policy();
    unit_test_exclusive_cache_policy_rand();
    unit_test_cache_accessors();
    unit_test_config_reader(std::string(argv[1]));
    unit_test_v2p_reader(std::string(argv[1]));
    unit_test_tlb_simulator(std::string(argv[1]));
    unit_test_cache_associativity();
    unit_test_cache_size();
    unit_test_cache_line_size();
    unit_test_cache_bad_configs();
    unit_test_metrics_API();
    unit_test_compulsory_misses();
    unit_test_warmup_fraction();
    unit_test_warmup_refs();
    unit_test_sim_refs();
    unit_test_skip_refs();
    unit_test_child_hits();
    unit_test_cache_replacement_policy();
    unit_test_core_sharded();
    unit_test_nextline_prefetcher();
    unit_test_custom_prefetcher();
    unit_test_set_parent();
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
