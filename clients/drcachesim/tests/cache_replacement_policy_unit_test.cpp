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
#include <assert.h>
#include "cache_replacement_policy_unit_test.h"
#include "simulator/cache_lru.h"

struct cache_config_t {
    int associativity = 4;
    int line_size = 32;
    int total_size = 256;
    caching_device_t *parent = nullptr;
    caching_device_stats_t *stats = new cache_stats_t(line_size, "", true);
    prefetcher_t *prefetcher = nullptr;
    bool inclusive = true;
    bool coherent_cache = true;
    int id = -1;
    snoop_filter_t *snoop_filter = nullptr;
};

static cache_config_t
make_test_config(int associativity, int line_size, int total_size)
{
    cache_config_t config;
    config.associativity = associativity;
    config.line_size = line_size;
    config.total_size = total_size;
    return config;
}

class cache_lru_test_t : public cache_lru_t {
public:
    void
    unit_test_replace_which_way()
    {
        // Create and initialize a 4-way set associative cache with line size of 32 and
        // total size of 128 bytes.
        // 0            32          64         96
        //  ----------------------------------------------
        // |   way 0    |   way 1   |   way 2   |  way 3  |
        // -----------------------------------------------
        cache_config_t config_1 = make_test_config(4, 32, 256);
        if (!init(config_1.associativity, config_1.line_size, config_1.total_size,
                  config_1.parent, config_1.stats, config_1.prefetcher,
                  config_1.inclusive, config_1.coherent_cache, config_1.id,
                  config_1.snoop_filter)) {
            std::cerr << "LRU cache failed to initialize"
                      << "\n";
            exit(1);
        }

        memref_t ref_1;
        ref_1.data.type = TRACE_TYPE_READ;
        ref_1.data.size = 1;
        for (int i = 0; i < 256; i++) {
            ref_1.data.addr = i;
            request(ref_1);
        }
        // Access the ways in the following fashion. This sequence follows the sequence
        // shown in https://github.com/DynamoRIO/dynamorio/issues/4881.
        // Access the ways in 3, 0, 1, 2 order.
        ref_1.data.addr = 192; // Access way 3 ("a" in issue 4881).
        request(ref_1);

        ref_1.data.addr = 0; // Access way 0 ("b" in issue 4881).
        request(ref_1);

        ref_1.data.addr = 64; // Access way 1 ("c" in issue 4881).
        request(ref_1);

        ref_1.data.addr = 128; // Access way 2 ("d" in issue 4881).
        request(ref_1);

        assert(replace_which_way(0) == 3);

        // Create and initialize an 8-way set associative cache with line size of 32 and
        // total size of 256 bytes.
        cache_config_t config_2 = make_test_config(8, 32, 256);
        if (!init(config_2.associativity, config_2.line_size, config_2.total_size,
                  config_2.parent, config_2.stats, config_2.prefetcher,
                  config_2.inclusive, config_2.coherent_cache, config_2.id,
                  config_2.snoop_filter)) {
            std::cerr << "LRU cache failed to initialize"
                      << "\n";
            exit(1);
        }

        memref_t ref_2;
        ref_2.data.type = TRACE_TYPE_READ;
        ref_2.data.size = 1;
        for (int i = 0; i < 256; i++) {
            ref_2.data.addr = i;
            request(ref_2);
        }

        // Access the ways in 1, 0, 2, 3, 4, 5, 7, 6 order.
        ref_2.data.addr = 32; // Access way 1.
        request(ref_2);

        ref_2.data.addr = 0; // Access way 0.
        request(ref_2);

        ref_2.data.addr = 64; // Access way 2.
        request(ref_2);

        ref_2.data.addr = 96; // Access way 3.
        request(ref_2);

        ref_2.data.addr = 128; // Access way 4.
        request(ref_2);

        ref_2.data.addr = 160; // Access way 5.
        request(ref_2);

        ref_2.data.addr = 224; // Access way 7.
        request(ref_2);

        ref_2.data.addr = 192; // Access way 6.
        request(ref_2);

        assert(replace_which_way(0) == 1);

        // Access the ways in 2, 1, 0, 3, 4, 5, 7, 6 order.
        ref_2.data.addr = 64; // Access way 2.
        request(ref_2);

        ref_2.data.addr = 32; // Access way 1.
        request(ref_2);

        ref_2.data.addr = 0; // Access way 0.
        request(ref_2);

        ref_2.data.addr = 96; // Access way 3.
        request(ref_2);

        ref_2.data.addr = 128; // Access way 4.
        request(ref_2);

        ref_2.data.addr = 160; // Access way 5.
        request(ref_2);

        ref_2.data.addr = 192; // Access way 6.
        request(ref_2);

        ref_2.data.addr = 224; // Access way 7.
        request(ref_2);

        assert(replace_which_way(0) == 2);
    }
};

void
unit_test_cache_replacement_policy()
{
    cache_lru_test_t cache_lru;
    cache_lru.unit_test_replace_which_way();
}
