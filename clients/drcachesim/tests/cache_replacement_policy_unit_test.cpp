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

class cache_lru_test_t : public cache_lru_t {
public:
    void
    initialize_cache(int associativity, int line_size, int total_size)
    {
        caching_device_stats_t *stats = new cache_stats_t(line_size, "", true);
        if (!init(associativity, line_size, total_size, nullptr, stats, nullptr)) {
            std::cerr << "LRU cache failed to initialize"
                      << "\n";
            exit(1);
        }
    }

    void
    unit_test_replace_which_way()
    {
        // Create and initialize a 4-way set associative cache with line size of 32 and
        // total size of 256 bytes.
        initialize_cache(4, 32, 256);

        memref_t ref_1;
        ref_1.data.type = TRACE_TYPE_READ;
        ref_1.data.size = 1;
        // Access the cache line in the following fashion. This sequence follows the
        // sequence shown in https://github.com/DynamoRIO/dynamorio/issues/4881.
        // Access the first row.
        ref_1.data.addr = 0;
        request(ref_1); // This accesses "a" in issue 4881.

        ref_1.data.addr = 64;
        request(ref_1); // This accesses "b" in issue 4881.

        ref_1.data.addr = 128;
        request(ref_1); // This accesses "c" in issue 4881.

        ref_1.data.addr = 192;
        request(ref_1); // This accesses "d" in issue 4881.

        // After the above accesses, the counters for each way will be as follows:
        //  way 0 ("a" in issue 4881): 3
        //  way 1 ("b" in issue 4881): 2
        //  way 2 ("c" in issue 4881): 1
        //  way 3 ("d" in issue 4881): 0
        // At this point way 0 ("a") has the highest counter so it should be replaced by
        // the LRU policy.
        assert(replace_which_way(0) == 0);

        ref_1.data.addr = 0;
        request(ref_1); // This replaces way 0 ("a").
        // At this point way 0 has been replaced(accessed) and way 1 has the highest
        // counter.
        //  way 0 ("a" in issue 4881): 0
        //  way 1 ("b" in issue 4881): 3
        //  way 2 ("c" in issue 4881): 2
        //  way 3 ("d" in issue 4881): 1
        assert(replace_which_way(0) == 1);

        ref_1.data.addr = 64;
        request(ref_1); // This replaces way 1 ("b").
        // At this point way 1 has been replaced(accessed) and way 2 has the highest
        // counter.
        //  way 0 ("a" in issue 4881): 1
        //  way 1 ("b" in issue 4881): 0
        //  way 2 ("c" in issue 4881): 3
        //  way 3 ("d" in issue 4881): 2
        assert(replace_which_way(0) == 2);

        ref_1.data.addr = 128;
        request(ref_1); // This replaces way 2 ("c").
        // At this point way 2 has been replaced(accessed) and way 3 has the highest
        // counter.
        //  way 0 ("a" in issue 4881): 2
        //  way 1 ("b" in issue 4881): 1
        //  way 2 ("c" in issue 4881): 0
        //  way 3 ("d" in issue 4881): 3
        assert(replace_which_way(0) == 3);

        ref_1.data.addr = 192;
        request(ref_1); // This replaces way 3 ("d").
        // At this point way 3 has been replaced(accessed) and way 0 has the highest
        // counter.
        //  way 0 ("a" in issue 4881): 3
        //  way 1 ("b" in issue 4881): 2
        //  way 2 ("c" in issue 4881): 1
        //  way 3 ("d" in issue 4881): 0
        assert(replace_which_way(0) == 0);
    }
};

void
unit_test_cache_replacement_policy()
{
    cache_lru_test_t cache_lru;
    cache_lru.unit_test_replace_which_way();
}
