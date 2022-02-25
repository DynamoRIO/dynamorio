/* **********************************************************
 * Copyright (c) 2017-2020 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _BASIC_COUNTS_H_
#define _BASIC_COUNTS_H_ 1

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "analysis_tool.h"

class basic_counts_t : public analysis_tool_t {
public:
    basic_counts_t(unsigned int verbose);
    ~basic_counts_t() override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init(int shard_index, void *worker_data) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;
    std::string
    parallel_shard_error(void *shard_data) override;

protected:
    struct counters_t {
        counters_t()
        {
        }
        counters_t &
        operator+=(counters_t &rhs)
        {
            instrs += rhs.instrs;
            instrs_nofetch += rhs.instrs_nofetch;
            prefetches += rhs.prefetches;
            loads += rhs.loads;
            stores += rhs.stores;
            sched_markers += rhs.sched_markers;
            xfer_markers += rhs.xfer_markers;
            func_id_markers += rhs.func_id_markers;
            func_retaddr_markers += rhs.func_retaddr_markers;
            func_arg_markers += rhs.func_arg_markers;
            func_retval_markers += rhs.func_retval_markers;
            other_markers += rhs.other_markers;
            icache_flushes += rhs.icache_flushes;
            dcache_flushes += rhs.dcache_flushes;
            for (const uint64_t addr : rhs.unique_pc_addrs) {
                unique_pc_addrs.insert(addr);
            }
            return *this;
        }
        memref_tid_t tid = 0;
        int_least64_t instrs = 0;
        int_least64_t instrs_nofetch = 0;
        int_least64_t prefetches = 0;
        int_least64_t loads = 0;
        int_least64_t stores = 0;
        int_least64_t sched_markers = 0;
        int_least64_t xfer_markers = 0;
        int_least64_t func_id_markers = 0;
        int_least64_t func_retaddr_markers = 0;
        int_least64_t func_arg_markers = 0;
        int_least64_t func_retval_markers = 0;
        int_least64_t other_markers = 0;
        int_least64_t icache_flushes = 0;
        int_least64_t dcache_flushes = 0;
        std::unordered_set<uint64_t> unique_pc_addrs;
        std::string error;
    };
    static bool
    cmp_counters(const std::pair<memref_tid_t, counters_t *> &l,
                 const std::pair<memref_tid_t, counters_t *> &r);

    // The keys here are int for parallel, tid for serial.
    std::unordered_map<memref_tid_t, counters_t *> shard_map_;
    // This mutex is only needed in parallel_shard_init.  In all other accesses to
    // shard_map (process_memref, print_results) we are single-threaded.
    std::mutex shard_map_mutex_;
    unsigned int knob_verbose_;
    static const std::string TOOL_NAME;
};

#endif /* _BASIC_COUNTS_H_ */
