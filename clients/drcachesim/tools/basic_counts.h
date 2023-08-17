/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

#include <stdint.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"

namespace dynamorio {
namespace drmemtrace {

class basic_counts_t : public analysis_tool_t {
public:
    basic_counts_t(unsigned int verbose);
    ~basic_counts_t() override;
    bool
    process_memref(const memref_t &memref) override;
    interval_state_snapshot_t *
    generate_interval_snapshot(uint64_t interval_id) override;
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
    interval_state_snapshot_t *
    generate_shard_interval_snapshot(void *shard_data, uint64_t interval_id) override;
    interval_state_snapshot_t *
    combine_interval_snapshots(
        std::vector<const interval_state_snapshot_t *> latest_shard_snapshots,
        uint64_t interval_end_timestamp) override;
    bool
    print_interval_results(
        const std::vector<interval_state_snapshot_t *> &interval_snapshots) override;
    bool
    release_interval_snapshot(interval_state_snapshot_t *snapshot) override;

    // i#3068: We use the following struct to also export the counters.
    struct counters_t {
        counters_t()
        {
        }
        counters_t &
        operator+=(const counters_t &rhs)
        {
            instrs += rhs.instrs;
            instrs_nofetch += rhs.instrs_nofetch;
            user_instrs += rhs.user_instrs;
            kernel_instrs += rhs.kernel_instrs;
            prefetches += rhs.prefetches;
            loads += rhs.loads;
            stores += rhs.stores;
            sched_markers += rhs.sched_markers;
            xfer_markers += rhs.xfer_markers;
            func_id_markers += rhs.func_id_markers;
            func_retaddr_markers += rhs.func_retaddr_markers;
            func_arg_markers += rhs.func_arg_markers;
            func_retval_markers += rhs.func_retval_markers;
            phys_addr_markers += rhs.phys_addr_markers;
            phys_unavail_markers += rhs.phys_unavail_markers;
            syscall_number_markers += rhs.syscall_number_markers;
            syscall_blocking_markers += rhs.syscall_blocking_markers;
            other_markers += rhs.other_markers;
            icache_flushes += rhs.icache_flushes;
            dcache_flushes += rhs.dcache_flushes;
            encodings += rhs.encodings;
            if (track_unique_pc_addrs) {
                for (const uint64_t addr : rhs.unique_pc_addrs) {
                    unique_pc_addrs.insert(addr);
                }
            }
            return *this;
        }
        counters_t &
        operator-=(const counters_t &rhs)
        {
            instrs -= rhs.instrs;
            instrs_nofetch -= rhs.instrs_nofetch;
            user_instrs -= rhs.user_instrs;
            kernel_instrs -= rhs.kernel_instrs;
            prefetches -= rhs.prefetches;
            loads -= rhs.loads;
            stores -= rhs.stores;
            sched_markers -= rhs.sched_markers;
            xfer_markers -= rhs.xfer_markers;
            func_id_markers -= rhs.func_id_markers;
            func_retaddr_markers -= rhs.func_retaddr_markers;
            func_arg_markers -= rhs.func_arg_markers;
            func_retval_markers -= rhs.func_retval_markers;
            phys_addr_markers -= rhs.phys_addr_markers;
            phys_unavail_markers -= rhs.phys_unavail_markers;
            syscall_number_markers -= rhs.syscall_number_markers;
            syscall_blocking_markers -= rhs.syscall_blocking_markers;
            other_markers -= rhs.other_markers;
            icache_flushes -= rhs.icache_flushes;
            dcache_flushes -= rhs.dcache_flushes;
            encodings -= rhs.encodings;
            for (const uint64_t addr : rhs.unique_pc_addrs) {
                unique_pc_addrs.erase(addr);
            }
            return *this;
        }
        bool
        operator==(const counters_t &rhs)
        {
            // memcmp doesn't work with the unordered_set member. Also,
            // cannot compare till offsetof(basic_counts_t::counters_t, unique_pc_addrs)
            // as it gives a non-standard-layout type warning on osx.
            return instrs == rhs.instrs && instrs_nofetch == rhs.instrs_nofetch &&
                user_instrs == rhs.user_instrs && kernel_instrs == rhs.kernel_instrs &&
                prefetches == rhs.prefetches && loads == rhs.loads &&
                stores == rhs.stores && sched_markers == rhs.sched_markers &&
                xfer_markers == rhs.xfer_markers &&
                func_id_markers == rhs.func_id_markers &&
                func_retaddr_markers == rhs.func_retaddr_markers &&
                func_arg_markers == rhs.func_arg_markers &&
                func_retval_markers == rhs.func_retval_markers &&
                phys_addr_markers == rhs.phys_addr_markers &&
                phys_unavail_markers == rhs.phys_unavail_markers &&
                syscall_number_markers == rhs.syscall_number_markers &&
                syscall_blocking_markers == rhs.syscall_blocking_markers &&
                other_markers == rhs.other_markers &&
                icache_flushes == rhs.icache_flushes &&
                dcache_flushes == rhs.dcache_flushes && encodings == rhs.encodings &&
                unique_pc_addrs == rhs.unique_pc_addrs;
        }
        int64_t instrs = 0;
        int64_t instrs_nofetch = 0;
        int64_t user_instrs = 0;
        int64_t kernel_instrs = 0;
        int64_t prefetches = 0;
        int64_t loads = 0;
        int64_t stores = 0;
        int64_t sched_markers = 0;
        int64_t xfer_markers = 0;
        int64_t func_id_markers = 0;
        int64_t func_retaddr_markers = 0;
        int64_t func_arg_markers = 0;
        int64_t func_retval_markers = 0;
        int64_t phys_addr_markers = 0;
        int64_t phys_unavail_markers = 0;
        int64_t syscall_number_markers = 0;
        int64_t syscall_blocking_markers = 0;
        int64_t other_markers = 0;
        int64_t icache_flushes = 0;
        int64_t dcache_flushes = 0;
        // The encoding entries aren't exposed at the memref_t level, but
        // we use encoding_is_new as a proxy.
        int64_t encodings = 0;
        std::unordered_set<uint64_t> unique_pc_addrs;

        // Metadata for the counts. These are not used for the equality, increment,
        // or decrement operation, and must be set explicitly.

        // Count of shards that were combined to produce the above counts.
        int64_t shard_count = 1;

        // Stops tracking unique_pc_addrs. Tracking unique_pc_addrs can be very
        // memory intensive. We skip it for interval state snapshots.
        void
        stop_tracking_unique_pc_addrs()
        {
            track_unique_pc_addrs = false;
            unique_pc_addrs.clear();
        }
        bool
        is_tracking_unique_pc_addrs() const
        {
            return track_unique_pc_addrs;
        }

    private:
        bool track_unique_pc_addrs = true;
    };
    counters_t
    get_total_counts();

protected:
    struct per_shard_t {
        per_shard_t()
        {
            counters.resize(1);
        }
        memref_tid_t tid = 0;
        // A vector to support windows.
        std::vector<counters_t> counters;
        std::string error;
        intptr_t last_window = -1;
        intptr_t filetype_ = -1;
        /* Indicates whether we're currently in the kernel region of the trace, which
         * means we've seen a TRACE_MARKER_TYPE_SYSCALL_TRACE_START without its matching
         * TRACE_MARKER_TYPE_SYSCALL_TRACE_STOP.
         */
        bool is_kernel = false;
    };
    // Records a snapshot of counts for a trace interval.
    struct count_snapshot_t : public interval_state_snapshot_t {
        // Cumulative counters till the current interval.
        // We could alternatively keep track of just the delta values vs
        // the last interval. But that would require us to keep track of
        // the last interval's counters in per_shard_t. So we simply track
        // the cumulative values here and compute the delta at the end in
        // print_interval_results().
        // Note that we do not track unique pc addresses for interval snapshots.
        counters_t counters;
        // TODO i#6020: Add per-window counters to the snapshot, and also
        // return interval counts separately per-window in a structured
        // way and print under a flag.
    };
    static bool
    cmp_threads(const std::pair<memref_tid_t, per_shard_t *> &l,
                const std::pair<memref_tid_t, per_shard_t *> &r);
    static void
    print_counters(const counters_t &counters, int64_t num_threads,
                   const std::string &prefix, bool for_kernel_trace = false);
    void
    compute_shard_interval_result(per_shard_t *shard, uint64_t interval_id);

    // The keys here are int for parallel, tid for serial.
    std::unordered_map<memref_tid_t, per_shard_t *> shard_map_;
    // This mutex is only needed in parallel_shard_init.  In all other accesses to
    // shard_map (process_memref, print_results) we are single-threaded.
    std::mutex shard_map_mutex_;
    unsigned int knob_verbose_;
    static const std::string TOOL_NAME;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _BASIC_COUNTS_H_ */
