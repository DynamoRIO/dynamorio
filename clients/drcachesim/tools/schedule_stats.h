/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

#ifndef _SCHEDULE_STATS_H_
#define _SCHEDULE_STATS_H_ 1

#include <stdint.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"

namespace dynamorio {
namespace drmemtrace {

class schedule_stats_t : public analysis_tool_t {
public:
    schedule_stats_t(uint64_t print_every, unsigned int verbose = 0);
    ~schedule_stats_t() override;
    std::string
    initialize_stream(memtrace_stream_t *serial_stream) override;
    std::string
    initialize_shard_type(shard_type_t shard_type) override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init_stream(int shard_index, void *worker_data,
                               memtrace_stream_t *stream) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;
    std::string
    parallel_shard_error(void *shard_data) override;

    struct counters_t {
        counters_t()
        {
        }
        counters_t &
        operator+=(const counters_t &rhs)
        {
            instrs += rhs.instrs;
            total_switches += rhs.total_switches;
            voluntary_switches += rhs.voluntary_switches;
            direct_switches += rhs.direct_switches;
            syscalls += rhs.syscalls;
            maybe_blocking_syscalls += rhs.maybe_blocking_syscalls;
            direct_switch_requests += rhs.direct_switch_requests;
            waits += rhs.waits;
            idles += rhs.idles;
            idle_microseconds += rhs.idle_microseconds;
            idle_micros_at_last_instr += rhs.idle_micros_at_last_instr;
            cpu_microseconds += rhs.cpu_microseconds;
            for (const memref_tid_t tid : rhs.threads) {
                threads.insert(tid);
            }
            return *this;
        }
        int64_t instrs = 0;
        int64_t total_switches = 0;
        int64_t voluntary_switches = 0;
        int64_t direct_switches = 0; // Subset of voluntary_switches.
        int64_t syscalls = 0;
        int64_t maybe_blocking_syscalls = 0;
        int64_t direct_switch_requests = 0;
        int64_t waits = 0;
        int64_t idles = 0;
        uint64_t idle_microseconds = 0;
        uint64_t idle_micros_at_last_instr = 0;
        uint64_t cpu_microseconds = 0;
        std::unordered_set<memref_tid_t> threads;
    };
    counters_t
    get_total_counts();

protected:
    struct per_shard_t {
        std::string error;
        memtrace_stream_t *stream = nullptr;
        int64_t core = 0; // We target core-sharded.
        counters_t counters;
        int64_t prev_input = -1;
        // These are cleared when an instruction is seen.
        bool saw_syscall = false;
        memref_tid_t direct_switch_target = INVALID_THREAD_ID;
        bool saw_exit = false;
        // A representation of the thread interleavings.
        std::string thread_sequence;
        uint64_t cur_segment_instrs = 0;
        bool prev_was_wait = false;
        bool prev_was_idle = false;
        // Computing %-idle.
        uint64_t segment_start_microseconds = 0;
    };

    void
    print_percentage(double numerator, double denominator, const std::string &label);

    void
    print_counters(const counters_t &counters);

    uint64_t
    get_current_microseconds();

    uint64_t knob_print_every_ = 0;
    unsigned int knob_verbose_ = 0;
    // We use an ordered map to get our output in order.  This table is not
    // used on the hot path so its performance does not matter.
    std::map<int64_t, per_shard_t *> shard_map_;
    // This mutex is only needed in parallel_shard_init.  In all other accesses to
    // shard_map (in print_results) we are single-threaded.
    std::mutex shard_map_mutex_;
    static const std::string TOOL_NAME;
    memtrace_stream_t *serial_stream_ = nullptr;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _SCHEDULE_STATS_H_ */
