/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

// Unit tests for snoop filter.
#include <iostream>
#undef NDEBUG
#include <assert.h>
#include "snoop_filter_test.h"
#include "simulator/cache_lru.h"
#include "snoop_filter.h"
#include "cache.h"
#include <algorithm>
#include <memory>

enum {
    CPU_0,
    CPU_1,
    CPU_2,
    CPU_3,
};

static const addr_t ADDR_A = 128;
static const int SNOOPED_FILTER_LINE_SIZE = 32;

class snoop_filter_test_t {

public:
    snoop_filter_test_t(const int num_cores)
        : num_cores_(num_cores)
    {
    }
    ~snoop_filter_test_t()
    {
        if (snooped_caches_ != NULL) {
            delete[] snooped_caches_;
        }
        if (snoop_filter_ != NULL) {
            delete snoop_filter_;
        }
    }

    void
    initialize_caches_and_snoop_filter()
    {
        cache_t *llc = new cache_lru_t;
        if (!llc->init(/*associatibity=*/8, /*line_size=*/64,
                       /*total_size=*/1024, nullptr,
                       new cache_stats_t((int)64, "", true))) {
            std::string error_string =
                "Usage error: failed to initialize LL cache.  Ensure sizes and "
                "associativity are powers of 2, that the total size is a multiple "
                "of the line size, and that any miss file path is writable.";
            std::cerr << error_string << "\n";
            return;
        }

        unsigned int total_snooped_caches = 1 * num_cores_;
        snooped_caches_ = new cache_t *[total_snooped_caches];
        for (int i = 0; i < num_cores_; i++) {
            snooped_caches_[i] = new cache_lru_t;
            snooped_caches_[i]->init(
                /*associativity=*/4, SNOOPED_FILTER_LINE_SIZE,
                /*total_size=*/256, llc,
                new cache_stats_t((int)SNOOPED_FILTER_LINE_SIZE, "", true, true),
                /*prefetcher=*/nullptr,
                /*inclusive=*/true, /*coherence_cache=*/true, i, snoop_filter_);
        }

        if (!snoop_filter_->init(snooped_caches_, num_cores_)) {
            std::cerr << "Usage error: failed to initialize snoop filter.\n";
            return;
        }
    }

    void
    request_and_check_state(const int cache_id, const addr_t addr,
                            const trace_type_t type, int expected_num_writes,
                            int expected_num_invalidates, int expected_num_writebacks,
                            int expected_num_sharers, bool block_is_dirty)
    {
        memref_t ref;
        ref.data.size = 1;
        ref.data.addr = addr;
        ref.data.type = type;
        snooped_caches_[cache_id]->request(ref);
        snoop_filter_stats_t stats = snoop_filter_->get_coherence_stats();
        assert(stats.num_writes == expected_num_writes);
        assert(stats.num_invalidates == expected_num_invalidates);
        assert(stats.num_writebacks == expected_num_writebacks);
        addr_t tag = addr >> compute_log2(SNOOPED_FILTER_LINE_SIZE);
        coherence_table_entry_t *coherence_table_entry = &stats.coherence_table[tag];
        auto num_sharers = std::count(coherence_table_entry->sharers.begin(),
                                      coherence_table_entry->sharers.end(), true);
        assert(num_sharers == expected_num_sharers);
        assert(block_is_dirty == coherence_table_entry->dirty);
        snoop_filter_->print_stats();
    }

private:
    int num_cores_;
    snoop_filter_t *snoop_filter_ = new snoop_filter_t;
    cache_t **snooped_caches_;
};

void
unit_test_snoop_filter_two_cores(void)
{
    snoop_filter_test_t snoop_filter_test(2);
    snoop_filter_test.initialize_caches_and_snoop_filter();
    snoop_filter_test.request_and_check_state(CPU_0, ADDR_A, TRACE_TYPE_READ,
                                              /*num_writes*/ 0,
                                              /*num_invalidates*/ 0,
                                              /*num_writebacks*/ 0,
                                              /*num_sharers*/ 0,
                                              /*block_is_dirty*/ false);
    snoop_filter_test.request_and_check_state(CPU_1, ADDR_A, TRACE_TYPE_READ,
                                              /*num_writes*/ 0,
                                              /*num_invalidates*/ 0,
                                              /*num_writebacks*/ 0,
                                              /*num_sharers*/ 1, false);
    snoop_filter_test.request_and_check_state(CPU_0, ADDR_A, TRACE_TYPE_WRITE,
                                              /*num_writes*/ 1,
                                              /*num_invalidates*/ 1,
                                              /*num_writebacks*/ 0,
                                              /*num_sharers*/ 2, false);
    snoop_filter_test.request_and_check_state(CPU_1, ADDR_A, TRACE_TYPE_READ,
                                              /*num_writes*/ 1,
                                              /*num_invalidates*/ 1,
                                              /*num_writebacks*/ 1,
                                              /*num_sharers*/ 1, true);
}

void
unit_test_snoop_filter_four_cores(void)
{
    snoop_filter_test_t snoop_filter_test(4);
    snoop_filter_test.initialize_caches_and_snoop_filter();
    snoop_filter_test.request_and_check_state(CPU_0, ADDR_A, TRACE_TYPE_READ,
                                              /*num_writes*/ 0,
                                              /*num_invalidates*/ 0,
                                              /*num_writebacks*/ 0,
                                              /*num_sharers*/ 0,
                                              /*block_is_dirty*/ false);
    snoop_filter_test.request_and_check_state(CPU_1, ADDR_A, TRACE_TYPE_READ,
                                              /*num_writes*/ 0,
                                              /*num_invalidates*/ 0,
                                              /*num_writebacks*/ 0,
                                              /*num_sharers*/ 1, false);
    snoop_filter_test.request_and_check_state(CPU_2, ADDR_A, TRACE_TYPE_WRITE,
                                              /*num_writes*/ 1,
                                              /*num_invalidates*/ 2,
                                              /*num_writebacks*/ 0,
                                              /*num_sharers*/ 2, false);
    snoop_filter_test.request_and_check_state(CPU_3, ADDR_A, TRACE_TYPE_READ,
                                              /*num_writes*/ 1,
                                              /*num_invalidates*/ 2,
                                              /*num_writebacks*/ 1,
                                              /*num_sharers*/ 1, true);
    snoop_filter_test.request_and_check_state(CPU_0, ADDR_A, TRACE_TYPE_READ,
                                              /*num_writes*/ 1,
                                              /*num_invalidates*/ 2,
                                              /*num_writebacks*/ 1,
                                              /*num_sharers*/ 2, false);
    snoop_filter_test.request_and_check_state(CPU_1, ADDR_A, TRACE_TYPE_WRITE,
                                              /*num_writes*/ 2,
                                              /*num_invalidates*/ 5,
                                              /*num_writebacks*/ 1,
                                              /*num_sharers*/ 3, false);
}

void
unit_test_snoop_filter(void)
{
    unit_test_snoop_filter_two_cores();
    unit_test_snoop_filter_four_cores();
}
