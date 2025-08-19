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

// Unit tests for cache replacement policies
#include <iostream>
#include <assert.h>
#include <memory>
#include <random>
#include "cache_replacement_policy_unit_test.h"
#include "simulator/policy_bit_plru.h"
#include "simulator/cache.h"
#include "simulator/policy_fifo.h"
#include "simulator/policy_lfu.h"
#include "simulator/policy_lru.h"
#include "simulator/policy_rrip.h"
#include "simulator/tlb.h"
#include "test_helpers.h"

namespace dynamorio {
namespace drmemtrace {

// Indices for test address vector.
enum {
    ADDR_A,
    ADDR_B,
    ADDR_C,
    ADDR_D,
    ADDR_E,
    ADDR_F,
    ADDR_G,
    ADDR_H,
    ADDR_I,
    ADDR_J,
    ADDR_K,
    ADDR_L,
};

// Test address vector where all the addresses with the same block index and
// different tag for a 32 byte cache line.
static const std::vector<addr_t> addr_vec = { 128 * 0, 128 * 1, 128 * 2,  128 * 3,
                                              128 * 4, 128 * 5, 128 * 6,  128 * 7,
                                              128 * 8, 128 * 9, 128 * 10, 128 * 11 };

template <class T> class caching_device_policy_test_t : public T {
protected:
    int associativity_;
    int line_size_;

public:
    caching_device_policy_test_t(int associativity, int line_size)
        : associativity_(associativity)
        , line_size_(line_size)
    {
    }

    inline void
    check(const addr_t addr, const int expected_replacement_way_after_access)
    {
        assert(this->get_next_way_to_replace(this->get_block_index(addr)) ==
               expected_replacement_way_after_access);
    }

    void
    access_and_check(const addr_t addr, const int expected_replacement_way_after_access)
    {
        memref_t ref;
        ref.data.type = TRACE_TYPE_READ;
        ref.data.size = 1;
        ref.data.addr = addr;
        ref.data.pid = 1;
        this->request(ref);
        check(addr, expected_replacement_way_after_access);
    }

    void
    invalidate_and_check(const addr_t addr,
                         const int expected_replacement_way_after_access)
    {
        this->invalidate(this->compute_tag(addr), INVALIDATION_INCLUSIVE);
        check(addr, expected_replacement_way_after_access);
    }

    bool
    tags_are_different(const std::vector<addr_t> &addresses)
    {
        // Quadratic solution is ok here since we expect only very short vectors.
        for (int i = 0; i < addresses.size(); ++i) {
            for (int j = 0; j < addresses.size(); ++j) {
                if (i != j) { // Skip comparison if same element.
                    if (this->compute_tag(addresses[i]) ==
                        this->compute_tag(addresses[j])) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    bool
    block_indices_are_identical(const std::vector<addr_t> &addresses)
    {
        for (int i = 1; i < addresses.size(); ++i) {
            if (this->get_block_index(addresses[i - 1]) !=
                this->get_block_index(addresses[i])) {
                return false;
            }
        }
        return true;
    }

    // Note that caches expect size as total size, while TLBs expect size as
    // number of entries.
    void
    initialize_cache(std::unique_ptr<cache_replacement_policy_t> replacement_policy,
                     int size,
                     cache_inclusion_policy_t inclusion_policy =
                         cache_inclusion_policy_t::NON_INC_NON_EXC)
    {
        caching_device_stats_t *stats =
            new cache_stats_t(this->line_size_, /*miss_file=*/"",
                              /*warmup_enabled=*/true);
        if (!this->init(this->associativity_, this->line_size_, size, nullptr, stats,
                        std::move(replacement_policy), nullptr, inclusion_policy)) {
            std::cerr << this->get_replace_policy() << " cache failed to initialize\n";
            exit(1);
        }
    }
};

void
unit_test_cache_lru_four_way()
{
    caching_device_policy_test_t<cache_t> cache_lru_test(/*associativity=*/4,
                                                         /*line_size=*/32);
    cache_lru_test.initialize_cache(std::unique_ptr<policy_lru_t>(new policy_lru_t(
                                        /*num_blocks=*/256 / 4, /*associativity=*/4)),
                                    256);

    assert(cache_lru_test.get_replace_policy() == "LRU");
    assert(cache_lru_test.block_indices_are_identical(addr_vec));
    assert(cache_lru_test.tags_are_different(addr_vec));

    // Access the cache line in the following fashion. This sequence follows the
    // sequence shown in i#4881.
    // Lower-case letter shows the least recently used way.
    cache_lru_test.access_and_check(addr_vec[ADDR_A], 1); //     A x X X
    cache_lru_test.access_and_check(addr_vec[ADDR_B], 2); //     A B x X
    cache_lru_test.access_and_check(addr_vec[ADDR_C], 3); //     A B C x
    cache_lru_test.access_and_check(addr_vec[ADDR_D], 0); //     a B C D
    cache_lru_test.access_and_check(addr_vec[ADDR_A], 1); //     A b C D
    cache_lru_test.access_and_check(addr_vec[ADDR_A], 1); //     A b C D
    cache_lru_test.access_and_check(addr_vec[ADDR_A], 1); //     A b C D
    cache_lru_test.access_and_check(addr_vec[ADDR_E], 2); //     A E c D

    cache_lru_test.invalidate_and_check(addr_vec[ADDR_A], 0); // x E C D

    cache_lru_test.access_and_check(addr_vec[ADDR_F], 2); //     F E c D
    cache_lru_test.access_and_check(addr_vec[ADDR_A], 3); //     F E A d
    cache_lru_test.access_and_check(addr_vec[ADDR_B], 1); //     F e A B
    cache_lru_test.access_and_check(addr_vec[ADDR_C], 0); //     f C A B

    cache_lru_test.invalidate_and_check(addr_vec[ADDR_C], 1); // F x A B

    cache_lru_test.access_and_check(addr_vec[ADDR_D], 0); //     f D A B
    cache_lru_test.access_and_check(addr_vec[ADDR_C], 2); //     C D a B
    cache_lru_test.access_and_check(addr_vec[ADDR_A], 3); //     C D A b
    cache_lru_test.access_and_check(addr_vec[ADDR_E], 1); //     C d A E
}

void
unit_test_cache_lru_eight_way()
{
    caching_device_policy_test_t<cache_t> cache_lru_test(/*associativity=*/8,
                                                         /*line_size=*/64);
    cache_lru_test.initialize_cache(std::unique_ptr<policy_lru_t>(new policy_lru_t(
                                        /*num_blocks=*/1024 / 8, /*associativity=*/8)),
                                    1024);

    assert(cache_lru_test.get_replace_policy() == "LRU");
    assert(cache_lru_test.block_indices_are_identical(addr_vec));
    assert(cache_lru_test.tags_are_different(addr_vec));

    // Lower-case letter shows the way that is to be replaced after the access
    // (aka 'first way').
    cache_lru_test.access_and_check(addr_vec[ADDR_A], 1); //     A  x  X  X  X  X  X  X
    cache_lru_test.access_and_check(addr_vec[ADDR_B], 2); //     A  B  x  X  X  X  X  X
    cache_lru_test.access_and_check(addr_vec[ADDR_C], 3); //     A  B  C  x  X  X  X  X
    cache_lru_test.access_and_check(addr_vec[ADDR_D], 4); //     A  B  C  D  x  X  X  X
    cache_lru_test.access_and_check(addr_vec[ADDR_E], 5); //     A  B  C  D  E  x  X  X
    cache_lru_test.access_and_check(addr_vec[ADDR_F], 6); //     A  B  C  D  E  F  x  X
    cache_lru_test.access_and_check(addr_vec[ADDR_G], 7); //     A  B  C  D  F  F  G  x
    cache_lru_test.access_and_check(addr_vec[ADDR_H], 0); //     a  B  C  D  E  F  G  H
    cache_lru_test.access_and_check(addr_vec[ADDR_E], 0); //     a  B  C  D  E  F  G  H
    cache_lru_test.access_and_check(addr_vec[ADDR_A], 1); //     A  b  C  D  E  F  G  H
    cache_lru_test.access_and_check(addr_vec[ADDR_A], 1); //     A  b  C  D  E  F  G  H
    cache_lru_test.access_and_check(addr_vec[ADDR_I], 2); //     A  I  c  D  E  F  G  H
    cache_lru_test.access_and_check(addr_vec[ADDR_J], 3); //     A  I  J  d  E  F  G  H
    cache_lru_test.access_and_check(addr_vec[ADDR_K], 5); //     A  I  J  K  E  f  G  H
    cache_lru_test.access_and_check(addr_vec[ADDR_L], 6); //     A  I  J  K  E  L  g  H
    cache_lru_test.access_and_check(addr_vec[ADDR_L], 6); //     A  I  J  K  E  L  g  H

    cache_lru_test.invalidate_and_check(addr_vec[ADDR_G], 6); // A  I  J  K  E  L  x  H

    cache_lru_test.access_and_check(addr_vec[ADDR_B], 7); //     A  I  J  K  E  L  B  h
    cache_lru_test.access_and_check(addr_vec[ADDR_C], 4); //     A  I  J  K  e  L  B  C

    cache_lru_test.invalidate_and_check(addr_vec[ADDR_L], 5); // A  I  J  K  E  x  B  C
    cache_lru_test.invalidate_and_check(addr_vec[ADDR_J], 2); // A  I  x  K  E  X  B  C
    cache_lru_test.invalidate_and_check(addr_vec[ADDR_C], 2); // A  I  x  K  E  X  B  X

    cache_lru_test.access_and_check(addr_vec[ADDR_I], 2); //     A  I  x  K  E  X  B  X
    cache_lru_test.access_and_check(addr_vec[ADDR_C], 5); //     A  I  C  K  E  x  B  X
    cache_lru_test.access_and_check(addr_vec[ADDR_D], 7); //     A  I  C  K  E  D  B  x
    cache_lru_test.access_and_check(addr_vec[ADDR_F], 4); //     A  I  C  K  e  D  B  F
}

void
unit_test_cache_fifo_four_way()
{
    caching_device_policy_test_t<cache_t> cache_fifo_test(/*associativity=*/4,
                                                          /*line_size=*/32);
    cache_fifo_test.initialize_cache(std::unique_ptr<policy_fifo_t>(new policy_fifo_t(
                                         /*num_blocks=*/256 / 4, /*associativity=*/4)),
                                     256);

    assert(cache_fifo_test.get_replace_policy() == "FIFO");
    assert(cache_fifo_test.block_indices_are_identical(addr_vec));
    assert(cache_fifo_test.tags_are_different(addr_vec));

    // Lower-case letter shows the way that is to be replaced after the access.
    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 1); //     A x X X
    cache_fifo_test.access_and_check(addr_vec[ADDR_B], 2); //     A B x X
    cache_fifo_test.access_and_check(addr_vec[ADDR_C], 3); //     A B C x
    cache_fifo_test.access_and_check(addr_vec[ADDR_D], 0); //     a B C D
    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 0); //     a B C D
    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 0); //     a B C D
    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 0); //     a B C D
    cache_fifo_test.access_and_check(addr_vec[ADDR_E], 1); //     E b C D
    cache_fifo_test.access_and_check(addr_vec[ADDR_F], 2); //     E F c D
    cache_fifo_test.access_and_check(addr_vec[ADDR_F], 2); //     E F c D
    cache_fifo_test.access_and_check(addr_vec[ADDR_G], 3); //     E F G d
    cache_fifo_test.access_and_check(addr_vec[ADDR_G], 3); //     E F G d
    cache_fifo_test.access_and_check(addr_vec[ADDR_H], 0); //     e F G H
    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 1); //     A f G H

    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_G], 2); // A F x H

    cache_fifo_test.access_and_check(addr_vec[ADDR_G], 1); //     A f G H
    cache_fifo_test.access_and_check(addr_vec[ADDR_B], 3); //     A B G h

    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_H], 3); // A B G x
    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_A], 0); // x B G X

    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 3); //     A B G x
    cache_fifo_test.access_and_check(addr_vec[ADDR_B], 3); //     A B G x
    cache_fifo_test.access_and_check(addr_vec[ADDR_C], 2); //     A B g C
    cache_fifo_test.access_and_check(addr_vec[ADDR_D], 1); //     A b D C
}

void
unit_test_cache_fifo_eight_way()
{
    caching_device_policy_test_t<cache_t> cache_fifo_test(/*associativity=*/8,
                                                          /*line_size=*/64);
    cache_fifo_test.initialize_cache(std::unique_ptr<policy_fifo_t>(new policy_fifo_t(
                                         /*num_blocks=*/1024 / 8, /*associativity=*/8)),
                                     1024);

    assert(cache_fifo_test.get_replace_policy() == "FIFO");
    assert(cache_fifo_test.block_indices_are_identical(addr_vec));
    assert(cache_fifo_test.tags_are_different(addr_vec));

    // Lower-case letter shows the way that is to be replaced after the access
    // (aka 'first way').
    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 1); //     A  x  X  X  X  X  X  X
    cache_fifo_test.access_and_check(addr_vec[ADDR_B], 2); //     A  B  x  X  X  X  X  X
    cache_fifo_test.access_and_check(addr_vec[ADDR_C], 3); //     A  B  C  x  X  X  X  X
    cache_fifo_test.access_and_check(addr_vec[ADDR_D], 4); //     A  B  C  D  x  X  X  X
    cache_fifo_test.access_and_check(addr_vec[ADDR_E], 5); //     A  B  C  D  E  x  X  X
    cache_fifo_test.access_and_check(addr_vec[ADDR_F], 6); //     A  B  C  D  E  F  x  X
    cache_fifo_test.access_and_check(addr_vec[ADDR_G], 7); //     A  B  C  D  F  F  G  x
    cache_fifo_test.access_and_check(addr_vec[ADDR_H], 0); //     a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_E], 0); //     a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 0); //     a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 0); //     a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_I], 1); //     I  b  C  D  E  F  G  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_J], 2); //     I  J  c  D  E  F  G  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_K], 3); //     I  J  K  d  E  F  G  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_L], 4); //     I  J  K  L  e  F  G  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_L], 4); //     I  J  K  L  e  F  G  H

    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_G], 6); // I  J  K  L  R  F  x  H
    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_K], 2); // I  J  x  L  E  F  X  H

    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 6); //     I  J  A  L  E  F  x  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_B], 4); //     I  J  K  L  e  F  B  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_C], 5); //     i  J  K  L  C  f  B  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_D], 7); //     I  j  K  L  C  D  b  H
}

void
unit_test_cache_lfu_four_way()
{
    caching_device_policy_test_t<cache_t> cache_lfu_test(/*associativity=*/4,
                                                         /*line_size=*/32);
    cache_lfu_test.initialize_cache(std::unique_ptr<policy_lfu_t>(new policy_lfu_t(
                                        /*num_blocks=*/256 / 4, /*associativity=*/4)),
                                    256);

    assert(cache_lfu_test.get_replace_policy() == "LFU");
    assert(cache_lfu_test.block_indices_are_identical(addr_vec));
    assert(cache_lfu_test.tags_are_different(addr_vec));

    cache_lfu_test.access_and_check(addr_vec[ADDR_A], 1); //     A x X X
    cache_lfu_test.access_and_check(addr_vec[ADDR_B], 2); //     A B x X
    cache_lfu_test.access_and_check(addr_vec[ADDR_C], 3); //     A B C x
    cache_lfu_test.access_and_check(addr_vec[ADDR_D], 0); //     a B C D
    cache_lfu_test.access_and_check(addr_vec[ADDR_A], 1); //     A b C D
    cache_lfu_test.access_and_check(addr_vec[ADDR_D], 1); //     A b C D
    cache_lfu_test.access_and_check(addr_vec[ADDR_D], 1); //     A b C D
    cache_lfu_test.access_and_check(addr_vec[ADDR_B], 2); //     A B c D
    cache_lfu_test.access_and_check(addr_vec[ADDR_C], 0); //     a B C D
    cache_lfu_test.access_and_check(addr_vec[ADDR_B], 0); //     a B C D
    cache_lfu_test.access_and_check(addr_vec[ADDR_A], 2); //     A B c D
    cache_lfu_test.access_and_check(addr_vec[ADDR_C], 0); //     a B C D

    cache_lfu_test.invalidate_and_check(addr_vec[ADDR_C], 2); // A B x D

    cache_lfu_test.access_and_check(addr_vec[ADDR_E], 2); //     A B e D
    cache_lfu_test.access_and_check(addr_vec[ADDR_E], 2); //     A B e D

    cache_lfu_test.invalidate_and_check(addr_vec[ADDR_B], 1); // A x E D

    cache_lfu_test.access_and_check(addr_vec[ADDR_F], 1); //     A f E D
    cache_lfu_test.access_and_check(addr_vec[ADDR_G], 1); //     A g E D
    cache_lfu_test.access_and_check(addr_vec[ADDR_G], 1); //     A g E D
    cache_lfu_test.access_and_check(addr_vec[ADDR_G], 2); //     A G e D
    cache_lfu_test.access_and_check(addr_vec[ADDR_E], 0); //     a G E D
    cache_lfu_test.access_and_check(addr_vec[ADDR_B], 0); //     b G E D
}

void
unit_test_cache_lfu_eight_way()
{
    caching_device_policy_test_t<cache_t> cache_lfu_test(/*associativity=*/8,
                                                         /*line_size=*/64);
    cache_lfu_test.initialize_cache(std::unique_ptr<policy_lfu_t>(new policy_lfu_t(
                                        /*num_blocks=*/1024 / 8, /*associativity=*/8)),
                                    1024);

    assert(cache_lfu_test.get_replace_policy() == "LFU");
    assert(cache_lfu_test.block_indices_are_identical(addr_vec));
    assert(cache_lfu_test.tags_are_different(addr_vec));

    cache_lfu_test.access_and_check(addr_vec[ADDR_A], 1); //     A  x  X  X  X  X  X  X
    cache_lfu_test.access_and_check(addr_vec[ADDR_B], 2); //     A  B  x  X  X  X  X  X
    cache_lfu_test.access_and_check(addr_vec[ADDR_C], 3); //     A  B  C  x  X  X  X  X
    cache_lfu_test.access_and_check(addr_vec[ADDR_D], 4); //     A  B  C  D  x  X  X  X
    cache_lfu_test.access_and_check(addr_vec[ADDR_E], 5); //     A  B  C  D  E  x  X  X
    cache_lfu_test.access_and_check(addr_vec[ADDR_F], 6); //     A  B  C  D  E  F  x  X
    cache_lfu_test.access_and_check(addr_vec[ADDR_G], 7); //     A  B  C  D  F  F  G  x
    cache_lfu_test.access_and_check(addr_vec[ADDR_H], 0); //     a  B  C  D  E  F  G  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_E], 0); //     a  B  C  D  E  F  G  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_D], 0); //     a  B  C  D  E  F  G  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_C], 0); //     a  B  C  D  E  F  G  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_A], 1); //     A  b  C  D  E  F  G  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_B], 5); //     A  B  C  D  E  f  G  H

    cache_lfu_test.invalidate_and_check(addr_vec[ADDR_C], 2); // A  B  x  D  E  F  G  H

    cache_lfu_test.access_and_check(addr_vec[ADDR_I], 2); //     A  B  i  D  E  F  G  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_I], 5); //     A  B  I  D  E  f  G  H

    cache_lfu_test.invalidate_and_check(addr_vec[ADDR_G], 6); // A  B  I  D  E  F  x  H
    cache_lfu_test.invalidate_and_check(addr_vec[ADDR_B], 1); // A  x  I  D  E  F  X  H

    cache_lfu_test.access_and_check(addr_vec[ADDR_J], 6); //     A  J  I  D  E  F  x  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_K], 1); //     A  j  I  D  E  F  K  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_L], 1); //     A  l  I  D  E  F  K  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_L], 5); //     A  L  I  D  E  f  K  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_F], 6); //     A  L  I  D  E  F  k  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_A], 6); //     A  L  I  D  E  F  k  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_L], 6); //     A  L  I  D  E  F  k  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_K], 7); //     A  L  I  D  E  F  K  h
    cache_lfu_test.access_and_check(addr_vec[ADDR_H], 2); //     A  L  i  D  E  F  K  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_D], 2); //     A  L  i  D  E  F  K  H
    cache_lfu_test.access_and_check(addr_vec[ADDR_I], 4); //     A  L  i  D  E  F  K  H
}

void
unit_test_cache_srrip_four_way()
{
    std::cerr << "Testing SRRIP replacement policy" << std::endl;
    caching_device_policy_test_t<cache_t> cache_rrip_test(/*associativity=*/4,
                                                          /*line_size=*/32);
    cache_rrip_test.initialize_cache(std::unique_ptr<policy_rrip_t>(new policy_rrip_t(
                                         /*num_blocks=*/256 / 4, /*associativity=*/4,
                                         /*rrpv_bits=*/2, /*rrpv_period=*/0,
                                         /*rrpv_long_per_period=*/0)),
                                     256);

    assert(cache_rrip_test.get_replace_policy() == "RRIP");
    assert(cache_rrip_test.block_indices_are_identical(addr_vec));
    assert(cache_rrip_test.tags_are_different(addr_vec));

    // m: miss
    // h: hit
    // p: promotion (RRPV increased for all the ways)

    std::cerr << "Testing SRRIP cache reuse" << std::endl;
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 1); //     m : A2 x3 X3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2); //     m : A2 B2 x3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 3); //     m : A2 B2 C2 x3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 0); //     m : a2 B2 C2 D2
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 1); //     h : A0 b2 C2 D2
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 1); //     h : A0 b2 C2 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2); //     h : A0 B0 c2 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 0); //     h : a0 B0 C0 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 0); //     h : a0 B0 C0 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 0); //     h : a0 B0 C0 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 0); //     h : a0 B0 C0 D0

    std::cerr << "Testing SRRIP partial cache reuse" << std::endl;
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_C], 2); //     A0 B0 x3 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 2);     // m : A0 B0 e2 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 0);     // h : a0 B0 E0 D0

    std::cerr << "Testing SRRIP cache pollution with 1-block reuse" << std::endl;
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_D], 3); //     A0 B0 E0 x3
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_B], 1); //     A0 x3 E0 X3
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_E], 1); //     A0 x3 X3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 1);     // h : A0 x3 X3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2);     // m : A0 B2 x3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 3);     // m : A0 B2 C2 x3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 1);     // m : A0 b2 C2 D2
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 2);     // mp: A1 E2 c3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 2);     // h : A0 E2 c3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 3);     // m : A0 E2 B2 d3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 1);     // m : A0 e2 B2 C2
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 2);     // mp: A1 D2 b3 C3
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 3);     // m : A1 D2 E2 c3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 3);     // h : A0 D2 E2 c3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 1);     // m : A0 d2 E2 B2
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 2);     // mp: A1 C2 e3 B3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 3);     // m : A1 C2 D2 b3
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 1);     // m : A1 c2 D2 E2

    std::cerr << "Testing SRRIP cache pollution with 2-blocks reuse" << std::endl;
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_E], 3); //     A1 C2 D2 x3
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_D], 2); //     A1 C2 x3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 2);     // h : A0 C2 x3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 3);     // m : A0 C2 B2 x3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 3);     // h : A0 C0 B2 x3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 2);     // m : A0 C0 b2 D2
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 3);     // mp: A1 C1 E2 d3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 3);     // h : A0 C1 E2 d3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2);     // m : A0 C1 e2 B2
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 2);     // h : A0 C0 e2 B2
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 3);     // mp: A1 C1 D2 b3
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 2);     // m : A1 C1 d2 E2
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 2);     // h : A0 C1 d2 E2
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 3);     // mp: A1 C2 B2 e3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 3);     // h : A1 C0 B2 e3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 2);     // m : A1 C0 b2 D2
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 3);     // mp: A2 C1 E2 d3

    std::cerr << "Testing SRRIP: RRPV for new inserted value" << std::endl;
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_E], 2); //     A2 C1 x3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 2);     // h : A0 C1 x3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 2);     // h : A0 C0 x3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 2);     // h : A0 C0 x3 D0
    // Insert new value with RRPV=2. Check the way to replace not changed
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2); //     m : A0 C0 b2 D0
}

void
unit_test_cache_brrip_four_way()
{
    std::cerr << "Testing BRRIP replacement policy" << std::endl;
    caching_device_policy_test_t<cache_t> cache_rrip_test(/*associativity=*/4,
                                                          /*line_size=*/32);
    cache_rrip_test.initialize_cache(std::unique_ptr<policy_rrip_t>(new policy_rrip_t(
                                         /*num_blocks=*/256 / 4, /*associativity=*/4,
                                         /*rrpv_bits=*/2, /*rrpv_period=*/4,
                                         /*rrpv_long_per_period=*/1)),
                                     256);

    assert(cache_rrip_test.get_replace_policy() == "RRIP");
    assert(cache_rrip_test.block_indices_are_identical(addr_vec));
    assert(cache_rrip_test.tags_are_different(addr_vec));

    // m: miss
    // h: hit
    // p: promotion (RRPV increased for all the ways)
    // l: "long" RRPV == 2
    // d: "distant" RRPV == 3

    std::cerr << "Testing BRRIP cache reuse" << std::endl;
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 1); //     ml : A2 x3 X3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2); //     md : A2 B3 x3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 3); //     md : A2 B3 C3 x3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 1); //     md : A2 b3 C3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 1); //     h  : A0 b3 C3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 1); //     h  : A0 b3 C3 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2); //     h  : A0 B0 c3 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 0); //     h  : a0 B0 C0 D0

    std::cerr << "Testing BRRIP cache pollution" << std::endl;
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_D], 3); //      A0 B0 C0 x3
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_C], 2); //      A0 B0 x3 X3
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_B], 1); //      A0 x3 X3 X3
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_A], 0); //      x3 X3 X3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 1);     // ml : A2 x3 X3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2);     // md : A2 B3 x3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 3);     // md : A2 B3 C3 x3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 1);     // md : A2 b3 C3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 2);     // ml : A2 E2 c3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_F], 2);     // md : A2 E2 f3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_G], 2);     // md : A2 E2 g3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_H], 2);     // md : A2 E2 h3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_I], 3);     // ml : A2 E2 I2 d3
    cache_rrip_test.access_and_check(addr_vec[ADDR_J], 3);     // md : A2 E2 I2 j3
    cache_rrip_test.access_and_check(addr_vec[ADDR_K], 3);     // md : A2 E2 I2 k3
    cache_rrip_test.access_and_check(addr_vec[ADDR_L], 3);     // md : A2 E2 I2 l3

    std::cerr << "Testing BRRIP cache pollution with 1-block reuse" << std::endl;
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_L], 3); //      A0 E0 I0 x3
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_I], 2); //      A0 E0 x3 X3
    cache_rrip_test.invalidate_and_check(addr_vec[ADDR_E], 1); //      A0 x3 X3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 1);     // h  : A0 x3 X3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2);     // ml : A0 B2 x3 X3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 3);     // md : A0 B2 C3 x3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 2);     // md : A0 B2 c3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 2);     // md : A0 B2 e3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 2);     // h  : A0 B2 e3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2);     // h  : A0 B0 e3 D3
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 3);     // ml : A0 B0 C2 d3
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 2);     // h  : A0 B0 c2 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 2);     // mdp: A1 B1 e3 D1
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 2);     // h  : A0 B1 e3 D1
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2);     // h  : A0 B0 e3 D1
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 2);     // md : A0 B0 c3 D1
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 2);     // h  : A0 B0 c3 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 2);     // md : A0 B0 e3 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_A], 2);     // h  : A0 B0 e3 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_B], 2);     // h  : A0 B0 e3 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_C], 2);     // ml : A0 B0 c2 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_D], 2);     // h  : A0 B0 c2 D0
    cache_rrip_test.access_and_check(addr_vec[ADDR_E], 2);     // mdp: A1 B1 e3 D1
}

void
unit_test_cache_srrip_exclusive()
{
    std::cerr << "Testing SRRIP replacement policy for exclusive cache" << std::endl;
    caching_device_policy_test_t<cache_t> l1_test(/*associativity=*/4,
                                                  /*line_size=*/32);
    caching_device_policy_test_t<cache_t> l2_test(/*associativity=*/4,
                                                  /*line_size=*/32);
    l1_test.initialize_cache(std::unique_ptr<policy_rrip_t>(new policy_rrip_t(
                                 /*num_blocks=*/256 / 4, /*associativity=*/4,
                                 /*rrpv_bits=*/2, /*rrpv_period=*/0,
                                 /*rrpv_long_per_period=*/0)),
                             256, cache_inclusion_policy_t::INCLUSIVE);
    l2_test.initialize_cache(std::unique_ptr<policy_rrip_t>(new policy_rrip_t(
                                 /*num_blocks=*/256 / 4, /*associativity=*/4,
                                 /*rrpv_bits=*/2, /*rrpv_period=*/0,
                                 /*rrpv_long_per_period=*/0)),
                             256, cache_inclusion_policy_t::EXCLUSIVE);
    l1_test.set_parent(&l2_test);
    assert(l2_test.get_replace_policy() == "RRIP");
    assert(l2_test.block_indices_are_identical(addr_vec));
    assert(l2_test.tags_are_different(addr_vec));
    assert(l2_test.get_children().size() == 1);
    assert(l2_test.get_children().front() == &l1_test);
    assert(l1_test.get_parent() == &l2_test);

    // m: miss
    // h: hit
    // p: promotion (RRPV increased for all the ways)

    std::cerr << "Testing SRRIP L1 cache initialization" << std::endl;
    l1_test.access_and_check(addr_vec[ADDR_A], 1); //     m : A2 x3 X3 X3
    l2_test.check(addr_vec[ADDR_A], 0);            //                       x3 X3 X3 X3
    l1_test.access_and_check(addr_vec[ADDR_B], 2); //     m : A2 B2 x3 X3
    l2_test.check(addr_vec[ADDR_B], 0);            //                       x3 X3 X3 X3
    l1_test.access_and_check(addr_vec[ADDR_C], 3); //     m : A2 B2 C2 x3
    l2_test.check(addr_vec[ADDR_C], 0);            //                       x3 X3 X3 X3
    l1_test.access_and_check(addr_vec[ADDR_D], 0); //     m : a2 B2 C2 D2
    l2_test.check(addr_vec[ADDR_D], 0);            //                       x3 X3 X3 X3

    std::cerr << "Testing SRRIP cache eviction from L1 to exclusive L2" << std::endl;
    l1_test.access_and_check(addr_vec[ADDR_E], 1); //     mp: E2 b3 C3 D3
    l2_test.check(addr_vec[ADDR_E], 1);            //     m :               A2 x3 X3 X3
    l1_test.access_and_check(addr_vec[ADDR_F], 2); //     m : E2 F2 c3 D3
    l2_test.check(addr_vec[ADDR_F], 2);            //     m :               A2 B2 x3 X3
    l1_test.access_and_check(addr_vec[ADDR_G], 3); //     m : E2 F2 G2 d3
    l2_test.check(addr_vec[ADDR_G], 3);            //     m :               A2 B2 C2 x3
    l1_test.access_and_check(addr_vec[ADDR_H], 0); //     m : e2 F2 G2 H2
    l2_test.check(addr_vec[ADDR_H], 0);            //     m :               a2 B2 C2 D2

    std::cerr << "Testing SRRIP cache eviction from exclusive L2" << std::endl;
    l1_test.access_and_check(addr_vec[ADDR_I], 1); //     mp: I2 f3 G3 H3
    l2_test.check(addr_vec[ADDR_I], 1);            //     mp:               E2 b3 C3 D3
    l1_test.access_and_check(addr_vec[ADDR_J], 2); //     m : I2 J2 g3 H3
    l2_test.check(addr_vec[ADDR_J], 2);            //     m :               E2 F2 c3 D3
    l1_test.access_and_check(addr_vec[ADDR_K], 3); //     m : I2 J2 K2 h3
    l2_test.check(addr_vec[ADDR_K], 3);            //     m :               E2 F2 G2 d3
    l1_test.access_and_check(addr_vec[ADDR_L], 0); //     m : i2 J2 K2 L2
    l2_test.check(addr_vec[ADDR_L], 0);            //     m :               e2 F2 G2 H2
    // Reuse the cache line I (from L1)
    l1_test.access_and_check(addr_vec[ADDR_I], 1); //     m : I0 j2 K2 L2
    l2_test.check(addr_vec[ADDR_I], 0);            //     m :               e2 F2 G2 H2

    std::cerr << "Testing SRRIP exclusive cache L2 reuse for new block" << std::endl;
    // Eviction from L1 to exclusive L2 previously not serviced cache line J
    // Access the cache line E (from L2)
    // Expected behavior: E moved from L2 to L1, victim J moved from L1 to L2
    // J previously not serviced in the L2 => access to J treated as L2 cache miss
    l1_test.access_and_check(addr_vec[ADDR_E], 2); //     mp: I1 E2 k3 L3
    l2_test.check(addr_vec[ADDR_E], 0);            //     m :               j2 F2 G2 H2
    // Same for F and K
    l1_test.access_and_check(addr_vec[ADDR_F], 3); //     m : I1 E2 F2 l3
    l2_test.check(addr_vec[ADDR_F], 0);            //     m :               j2 K2 G2 H2

    std::cerr << "Testing SRRIP exclusive cache L2 reuse for serviced block" << std::endl;
    // Eviction from L1 to exclusive L2 previously serviced cache line E
    // Prepare victim E: Reuse the cache lines F and L (from L1)
    l1_test.access_and_check(addr_vec[ADDR_F], 3); //     m : I1 E2 F0 l3
    l2_test.check(addr_vec[ADDR_F], 0);            //     m :               j2 K2 G2 H2
    l1_test.access_and_check(addr_vec[ADDR_L], 1); //     m : I1 e2 F0 L0
    l2_test.check(addr_vec[ADDR_E], 0);            //     m :               j2 K2 G2 H2
    // Reuse the cache line J (from L2)
    // Expected behavior: J moved from L2 to L1, victim E moved from L1 to L2
    // E was serviced in the L2, access to K treated as L2 cache hit
    l1_test.access_and_check(addr_vec[ADDR_J], 0); //     mp: i2 J2 F1 L1
    l2_test.check(addr_vec[ADDR_J], 1);            //     m :               E0 k2 G2 H2
}

void
unit_test_tlb_plru_four_way()
{
    caching_device_policy_test_t<tlb_t> tlb_plru_test(/*associativity=*/4,
                                                      /*line_size=*/64);
    tlb_plru_test.initialize_cache(
        std::unique_ptr<policy_bit_plru_t>(
            new policy_bit_plru_t(1 /*num_blocks*/, 4 /*associativity*/, 0 /*seed*/)),
        4);
    assert(tlb_plru_test.get_replace_policy() == "BIT_PLRU");
    assert(tlb_plru_test.block_indices_are_identical(addr_vec));
    assert(tlb_plru_test.tags_are_different(addr_vec));
    tlb_plru_test.access_and_check(addr_vec[ADDR_A], 1); //     A x x x
    tlb_plru_test.access_and_check(addr_vec[ADDR_B], 2); //     A B x x
    tlb_plru_test.access_and_check(addr_vec[ADDR_C], 3); //     A B C x
    tlb_plru_test.access_and_check(addr_vec[ADDR_D], 2); //     a b c D
    // Next replacement chosen randomly from zero bits, the test runs with const seed.
    tlb_plru_test.access_and_check(addr_vec[ADDR_A], 2); //     A b c D
    tlb_plru_test.access_and_check(addr_vec[ADDR_B], 2); //     A B c D
    tlb_plru_test.access_and_check(addr_vec[ADDR_E], 1); //     a b E d
    tlb_plru_test.access_and_check(addr_vec[ADDR_A], 3); //     A b E d
    tlb_plru_test.access_and_check(addr_vec[ADDR_B], 3); //     A B E d
}

void
unit_test_tlb_lfu_four_way()
{
    caching_device_policy_test_t<tlb_t> tlb_lfu_test(/*associativity=*/4,
                                                     /*line_size=*/64);
    tlb_lfu_test.initialize_cache(std::unique_ptr<policy_lfu_t>(new policy_lfu_t(
                                      1 /*num_blocks*/, 4 /*associativity*/)),
                                  4);
    assert(tlb_lfu_test.get_replace_policy() == "LFU");
    assert(tlb_lfu_test.block_indices_are_identical(addr_vec));
    assert(tlb_lfu_test.tags_are_different(addr_vec));
    tlb_lfu_test.access_and_check(addr_vec[ADDR_A], 1); //     A1 x  x  x
    tlb_lfu_test.access_and_check(addr_vec[ADDR_B], 2); //     A1 B1 x  x
    tlb_lfu_test.access_and_check(addr_vec[ADDR_C], 3); //     A1 B1 C1 x
    tlb_lfu_test.access_and_check(addr_vec[ADDR_D], 0); //     A1 B1 C1 D1
    tlb_lfu_test.access_and_check(addr_vec[ADDR_A], 1); //     A2 B1 C1 D1
    tlb_lfu_test.access_and_check(addr_vec[ADDR_C], 1); //     A2 B1 C2 D1
    tlb_lfu_test.access_and_check(addr_vec[ADDR_B], 3); //     A2 B2 C2 D1
    tlb_lfu_test.access_and_check(addr_vec[ADDR_E], 3); //     A2 B2 C2 E1
    tlb_lfu_test.access_and_check(addr_vec[ADDR_D], 3); //     A2 B2 C2 D1
}

void
unit_test_cache_replacement_policy()
{
    unit_test_cache_lru_four_way();
    unit_test_cache_lru_eight_way();
    unit_test_cache_fifo_four_way();
    unit_test_cache_fifo_eight_way();
    unit_test_cache_lfu_four_way();
    unit_test_cache_lfu_eight_way();
    unit_test_cache_srrip_four_way();
    unit_test_cache_brrip_four_way();
    unit_test_cache_srrip_exclusive();
    unit_test_tlb_plru_four_way();
    unit_test_tlb_lfu_four_way();
    // XXX i#4842: Add more test sequences.
}

} // namespace drmemtrace
} // namespace dynamorio
