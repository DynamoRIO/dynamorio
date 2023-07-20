/* **********************************************************
 * Copyright (c) 2016-2022 Google, Inc.  All rights reserved.
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
#undef NDEBUG
#include <assert.h>
#include "cache_replacement_policy_unit_test.h"
#include "simulator/cache_fifo.h"
#include "simulator/cache_lru.h"

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

template <class T> class cache_policy_test_t : public T {
    int associativity_;
    int line_size_;
    int total_size_;

public:
    cache_policy_test_t(int associativity, int line_size, int total_size)
    {
        associativity_ = associativity;
        line_size_ = line_size;
        total_size_ = total_size;
    }
    void
    initialize_cache()
    {
        caching_device_stats_t *stats = new cache_stats_t(line_size_, /*miss_file=*/"",
                                                          /*warmup_enabled=*/true);
        if (!this->init(associativity_, line_size_, total_size_, nullptr, stats,
                        nullptr)) {
            std::cerr << this->get_replace_policy() << " cache failed to initialize\n";
            exit(1);
        }
    }

    void
    access_and_check(const addr_t addr, const int expected_replacement_way_after_access)
    {
        memref_t ref;
        ref.data.type = TRACE_TYPE_READ;
        ref.data.size = 1;
        ref.data.addr = addr;
        this->request(ref);
        assert(this->get_next_way_to_replace(this->get_block_index(addr)) ==
               expected_replacement_way_after_access);
    }

    void
    invalidate_and_check(const addr_t addr,
                         const int expected_replacement_way_after_access)
    {
        this->invalidate(this->compute_tag(addr), INVALIDATION_INCLUSIVE);
        assert(this->get_next_way_to_replace(this->get_block_index(addr)) ==
               expected_replacement_way_after_access);
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
};

void
unit_test_cache_lru_four_way()
{
    cache_policy_test_t<cache_lru_t> cache_lru_test(/*associativity=*/4,
                                                    /*line_size=*/32,
                                                    /*total_size=*/256);
    cache_lru_test.initialize_cache();

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
    cache_policy_test_t<cache_lru_t> cache_lru_test(/*associativity=*/8,
                                                    /*line_size=*/64,
                                                    /*total_size=*/1024);
    cache_lru_test.initialize_cache();

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
    cache_policy_test_t<cache_fifo_t> cache_fifo_test(/*associativity=*/4,
                                                      /*line_size=*/32,
                                                      /*total_size=*/256);
    cache_fifo_test.initialize_cache();

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

    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_G], 1); // A f X H

    cache_fifo_test.access_and_check(addr_vec[ADDR_G], 2); //     A G x H
    cache_fifo_test.access_and_check(addr_vec[ADDR_B], 3); //     A G B h

    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_H], 3); // A G B x
    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_A], 3); // X G B x

    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 0); //     x G B A
    cache_fifo_test.access_and_check(addr_vec[ADDR_B], 0); //     x G B A
    cache_fifo_test.access_and_check(addr_vec[ADDR_C], 1); //     C g B A
    cache_fifo_test.access_and_check(addr_vec[ADDR_D], 2); //     C D B A
}

void
unit_test_cache_fifo_eight_way()
{
    cache_policy_test_t<cache_fifo_t> cache_fifo_test(/*associativity=*/8,
                                                      /*line_size=*/64,
                                                      /*total_size=*/1024);
    cache_fifo_test.initialize_cache();

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

    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_G], 4); // I  J  K  L  e  F  X  H
    cache_fifo_test.invalidate_and_check(addr_vec[ADDR_K], 4); // I  J  X  L  e  F  X  H

    cache_fifo_test.access_and_check(addr_vec[ADDR_A], 5); //     I  J  K  L  A  f  G  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_B], 6); //     I  J  K  L  A  B  g  H
    cache_fifo_test.access_and_check(addr_vec[ADDR_C], 7); //     I  J  K  L  A  B  C  h
    cache_fifo_test.access_and_check(addr_vec[ADDR_D], 0); //     i  J  K  L  A  B  C  D
}

void
unit_test_cache_lfu_four_way()
{
    cache_policy_test_t<cache_t> cache_lfu_test(/*associativity=*/4,
                                                /*line_size=*/32,
                                                /*total_size=*/256);
    cache_lfu_test.initialize_cache();

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
    cache_policy_test_t<cache_t> cache_lfu_test(/*associativity=*/8,
                                                /*line_size=*/64,
                                                /*total_size=*/1024);
    cache_lfu_test.initialize_cache();

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
unit_test_cache_replacement_policy()
{
    unit_test_cache_lru_four_way();
    unit_test_cache_lru_eight_way();
    unit_test_cache_fifo_four_way();
    unit_test_cache_fifo_eight_way();
    unit_test_cache_lfu_four_way();
    unit_test_cache_lfu_eight_way();
    // XXX i#4842: Add more test sequences.
}

} // namespace drmemtrace
} // namespace dynamorio
