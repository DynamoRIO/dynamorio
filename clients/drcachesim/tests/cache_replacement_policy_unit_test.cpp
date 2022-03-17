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
make_test_configs(int associativity, int line_size, int total_size)
{
    cache_config_t config;
    config.associativity = associativity;
    config.line_size = line_size;
    config.total_size = total_size;
    return config;
}

void
cache_lru_test_t::unit_test_replace_which_way()
{
    // Create and initialize an 8-way set associative cache with line size of 32 and
    // total size of 256 bytes.
    cache_config_t config_two = make_test_configs(8, 32, 256);
    if (!init(config_two.associativity, config_two.line_size, config_two.total_size,
              config_two.parent, config_two.stats, config_two.prefetcher,
              config_two.inclusive, config_two.coherent_cache, config_two.id,
              config_two.snoop_filter)) {
        std::cerr << "LRU cache failed to initialize"
                  << "\n";
        exit(1);
    }

    memref_t ref;
    ref.data.type = TRACE_TYPE_READ;
    ref.data.size = 1;
    for (int i = 0; i < 256; i++) {
        ref.data.addr = i;
        request(ref);
    }

    assert(get_caching_device_block(0, 0).counter_ == 7);
    assert(get_caching_device_block(0, 1).counter_ == 6);
    assert(get_caching_device_block(0, 2).counter_ == 5);
    assert(get_caching_device_block(0, 3).counter_ == 4);
    assert(get_caching_device_block(0, 4).counter_ == 3);
    assert(get_caching_device_block(0, 5).counter_ == 2);
    assert(get_caching_device_block(0, 6).counter_ == 1);
    assert(get_caching_device_block(0, 7).counter_ == 0);

    assert(replace_which_way(0) == 0);
}

void
unit_test_cache_replacement_policy()
{
    cache_lru_test_t cache_lru;
    cache_lru.unit_test_replace_which_way();
}
