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

class cache_lru_test_t : public cache_lru_t {
public:
    void
    initialize_cache(int associativity, int line_size, int total_size)
    {
        caching_device_stats_t *stats = new cache_stats_t(line_size, "", true);
        if (!init(associativity, line_size, total_size, nullptr, stats, nullptr)) {
            std::cerr << "LRU cache failed to initialize\n";
            exit(1);
        }
    }

    void
    access_and_check_lru(const addr_t addr,
                         const int expected_replacement_way_after_access)
    {
        memref_t ref;
        ref.data.type = TRACE_TYPE_READ;
        ref.data.size = 1;
        ref.data.addr = addr;
        request(ref);
        assert(get_next_way_to_replace(get_block_index(addr)) ==
               expected_replacement_way_after_access);
    }
};

class cache_fifo_test_t : public cache_fifo_t {
public:
    void
    initialize_cache(int associativity, int line_size, int total_size)
    {
        caching_device_stats_t *stats = new cache_stats_t(line_size, "", true);
        if (!init(associativity, line_size, total_size, nullptr, stats, nullptr)) {
            std::cerr << "FIFO cache failed to initialize\n";
            exit(1);
        }
    }

    void
    access_and_check_fifo(const addr_t addr,
                          const int expected_replacement_way_after_access)
    {
        memref_t ref;
        ref.data.type = TRACE_TYPE_READ;
        ref.data.size = 1;
        ref.data.addr = addr;
        request(ref);
        assert(get_next_way_to_replace(get_block_index(addr)) ==
               expected_replacement_way_after_access);
    }
};

void
unit_test_cache_lru_four_way()
{
    cache_lru_test_t cache_lru_test;
    cache_lru_test.initialize_cache(/*associativity=*/4, /*line_size=*/32,
                                    /*total_size=*/256);
    const addr_t ADDRESS_A = 0;
    const addr_t ADDRESS_B = 64;
    const addr_t ADDRESS_C = 128;
    const addr_t ADDRESS_D = 192;
    const addr_t ADDRESS_E = 72;

    assert(cache_lru_test.get_block_index(ADDRESS_A) ==
           cache_lru_test.get_block_index(ADDRESS_B));
    assert(cache_lru_test.get_block_index(ADDRESS_B) ==
           cache_lru_test.get_block_index(ADDRESS_C));
    assert(cache_lru_test.get_block_index(ADDRESS_C) ==
           cache_lru_test.get_block_index(ADDRESS_D));
    assert(cache_lru_test.get_block_index(ADDRESS_D) ==
           cache_lru_test.get_block_index(ADDRESS_E));

    // Access the cache line in the following fashion. This sequence follows the
    // sequence shown in i#4881.
    // Lower-case letter shows the least recently used way.
    cache_lru_test.access_and_check_lru(ADDRESS_A, 1); // A x X X
    cache_lru_test.access_and_check_lru(ADDRESS_B, 2); // A B x X
    cache_lru_test.access_and_check_lru(ADDRESS_C, 3); // A B C x
    cache_lru_test.access_and_check_lru(ADDRESS_D, 0); // a B C D
    cache_lru_test.access_and_check_lru(ADDRESS_A, 1); // A b C D
    cache_lru_test.access_and_check_lru(ADDRESS_A, 1); // A b C D
    cache_lru_test.access_and_check_lru(ADDRESS_A, 1); // A b C D
    cache_lru_test.access_and_check_lru(ADDRESS_E, 2); // A E c D
}

void
unit_test_cache_fifo_four_way()
{
    cache_fifo_test_t cache_fifo_test;
    cache_fifo_test.initialize_cache(/*associativity=*/4, /*line_size=*/32,
                                     /*total_size=*/256);
    const addr_t ADDRESS_A = 0;
    const addr_t ADDRESS_B = 64;
    const addr_t ADDRESS_C = 128;
    const addr_t ADDRESS_D = 192;
    const addr_t ADDRESS_E = 72;
    const addr_t ADDRESS_F = 130;

    assert(cache_fifo_test.get_block_index(ADDRESS_A) ==
           cache_fifo_test.get_block_index(ADDRESS_B));
    assert(cache_fifo_test.get_block_index(ADDRESS_B) ==
           cache_fifo_test.get_block_index(ADDRESS_C));
    assert(cache_fifo_test.get_block_index(ADDRESS_C) ==
           cache_fifo_test.get_block_index(ADDRESS_D));
    assert(cache_fifo_test.get_block_index(ADDRESS_D) ==
           cache_fifo_test.get_block_index(ADDRESS_E));
    assert(cache_fifo_test.get_block_index(ADDRESS_E) ==
           cache_fifo_test.get_block_index(ADDRESS_F));

    // Lower-case letter shows the way that is to be replaced after the access.
    cache_fifo_test.access_and_check_fifo(ADDRESS_A, 1); // A x X X
    cache_fifo_test.access_and_check_fifo(ADDRESS_B, 2); // A B x X
    cache_fifo_test.access_and_check_fifo(ADDRESS_C, 3); // A B C x
    cache_fifo_test.access_and_check_fifo(ADDRESS_D, 0); // a B C D
    cache_fifo_test.access_and_check_fifo(ADDRESS_A, 0); // a B C D
    cache_fifo_test.access_and_check_fifo(ADDRESS_A, 0); // a B C D
    cache_fifo_test.access_and_check_fifo(ADDRESS_A, 0); // a B C D
    cache_fifo_test.access_and_check_fifo(ADDRESS_E, 0); // a B C D
    cache_fifo_test.access_and_check_fifo(ADDRESS_A, 0); // a B C D
    cache_fifo_test.access_and_check_fifo(ADDRESS_F, 0); // a B C D
}

void
unit_test_cache_fifo_eight_way()
{
    cache_fifo_test_t cache_fifo_test;
    cache_fifo_test.initialize_cache(/*associativity=*/8, /*line_size=*/32,
                                     /*total_size=*/1024);
    const addr_t ADDRESS_A = 0;
    const addr_t ADDRESS_B = 128;
    const addr_t ADDRESS_C = 256;
    const addr_t ADDRESS_D = 384;
    const addr_t ADDRESS_E = 512;
    const addr_t ADDRESS_F = 640;
    const addr_t ADDRESS_G = 768;
    const addr_t ADDRESS_H = 896;
    const addr_t ADDRESS_I = 144;
    const addr_t ADDRESS_J = 150;
    const addr_t ADDRESS_K = 900;
    const addr_t ADDRESS_L = 780;

    assert(cache_fifo_test.get_block_index(ADDRESS_A) ==
           cache_fifo_test.get_block_index(ADDRESS_B));
    assert(cache_fifo_test.get_block_index(ADDRESS_B) ==
           cache_fifo_test.get_block_index(ADDRESS_C));
    assert(cache_fifo_test.get_block_index(ADDRESS_C) ==
           cache_fifo_test.get_block_index(ADDRESS_D));
    assert(cache_fifo_test.get_block_index(ADDRESS_D) ==
           cache_fifo_test.get_block_index(ADDRESS_E));
    assert(cache_fifo_test.get_block_index(ADDRESS_F) ==
           cache_fifo_test.get_block_index(ADDRESS_G));
    assert(cache_fifo_test.get_block_index(ADDRESS_H) ==
           cache_fifo_test.get_block_index(ADDRESS_I));
    assert(cache_fifo_test.get_block_index(ADDRESS_I) ==
           cache_fifo_test.get_block_index(ADDRESS_J));
    assert(cache_fifo_test.get_block_index(ADDRESS_J) ==
           cache_fifo_test.get_block_index(ADDRESS_K));
    assert(cache_fifo_test.get_block_index(ADDRESS_K) ==
           cache_fifo_test.get_block_index(ADDRESS_L));

    // Lower-case letter shows the way that is to be replaced after the access
    // (aka 'first way').
    cache_fifo_test.access_and_check_fifo(ADDRESS_A, 1); // A  x  X  X  X  X  X  X
    cache_fifo_test.access_and_check_fifo(ADDRESS_B, 2); // A  B  x  X  X  X  X  X
    cache_fifo_test.access_and_check_fifo(ADDRESS_C, 3); // A  B  C  x  X  X  X  X
    cache_fifo_test.access_and_check_fifo(ADDRESS_D, 4); // A  B  C  D  x  X  X  X
    cache_fifo_test.access_and_check_fifo(ADDRESS_E, 5); // A  B  C  D  E  x  X  X
    cache_fifo_test.access_and_check_fifo(ADDRESS_F, 6); // A  B  C  D  E  F  x  X
    cache_fifo_test.access_and_check_fifo(ADDRESS_G, 7); // A  B  C  D  F  F  G  x
    cache_fifo_test.access_and_check_fifo(ADDRESS_H, 0); // a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check_fifo(ADDRESS_E, 0); // a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check_fifo(ADDRESS_E, 0); // a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check_fifo(ADDRESS_E, 0); // a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check_fifo(ADDRESS_A, 0); // a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check_fifo(ADDRESS_A, 0); // a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check_fifo(ADDRESS_I, 0); // a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check_fifo(ADDRESS_J, 0); // a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check_fifo(ADDRESS_K, 0); // a  B  C  D  E  F  G  H
    cache_fifo_test.access_and_check_fifo(ADDRESS_L, 0); // a  B  C  D  E  F  G  H
}

void
unit_test_cache_replacement_policy()
{
    unit_test_cache_lru_four_way();
    unit_test_cache_fifo_four_way();
    unit_test_cache_fifo_eight_way();
    // XXX i#4842: Add more test sequences.
}
