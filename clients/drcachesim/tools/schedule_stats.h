/* **********************************************************
 * Copyright (c) 2023-2024 Google, Inc.  All rights reserved.
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
#include <iostream>
#include <map>
#include <memory>
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

    // Histogram interface for instrs-per-switch distribution.
    class histogram_interface_t {
    public:
        virtual ~histogram_interface_t() = default;
        virtual void
        add(int64_t value) = 0;
        virtual void
        merge(const histogram_interface_t *rhs) = 0;
        virtual void
        print() const = 0;
    };

    // Simple binning histogram for instrs-per-switch distribution.
    class histogram_t : public histogram_interface_t {
    public:
        histogram_t() = default;

        void
        add(int64_t value) override
        {
            // XXX: Add dynamic bin size changing.
            // For now with relatively known data ranges we just stick
            // with unchanging bin sizes.
            uint64_t bin = value - (value % kInitialBinSize);
            ++bin2count_[bin];
        }

        void
        merge(const histogram_interface_t *rhs) override
        {
            const histogram_t *rhs_hist = dynamic_cast<const histogram_t *>(rhs);
            for (const auto &keyval : rhs_hist->bin2count_) {
                bin2count_[keyval.first] += keyval.second;
            }
        }

        void
        print() const override
        {
            for (const auto &keyval : bin2count_) {
                std::cerr << std::setw(12) << keyval.first << ".." << std::setw(8)
                          << keyval.first + kInitialBinSize << " " << std::setw(5)
                          << keyval.second << "\n";
            }
        }

    protected:
        static constexpr uint64_t kInitialBinSize = 50000;

        // Key is the inclusive lower bound of the bin.
        std::map<uint64_t, uint64_t> bin2count_;
    };

    struct counters_t {
        counters_t()
        {
            instrs_per_switch = std::unique_ptr<histogram_interface_t>(new histogram_t);
        }
        counters_t &
        operator+=(const counters_t &rhs)
        {
            switches_input_to_input += rhs.switches_input_to_input;
            switches_input_to_idle += rhs.switches_input_to_idle;
            switches_idle_to_input += rhs.switches_idle_to_input;
            switches_nop += rhs.switches_nop;
            quantum_preempts += rhs.quantum_preempts;
            migrations += rhs.migrations;
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
            wait_microseconds += rhs.wait_microseconds;
            for (const memref_tid_t tid : rhs.threads) {
                threads.insert(tid);
            }
            instrs_per_switch->merge(rhs.instrs_per_switch.get());
            return *this;
        }
        // Statistics provided by scheduler.
        int64_t switches_input_to_input = 0;
        int64_t switches_input_to_idle = 0;
        int64_t switches_idle_to_input = 0;
        int64_t switches_nop = 0;
        int64_t quantum_preempts = 0;
        int64_t migrations = 0;
        // Our own statistics.
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
        uint64_t wait_microseconds = 0;
        std::unordered_set<memref_tid_t> threads;
        std::unique_ptr<histogram_interface_t> instrs_per_switch;
    };
    counters_t
    get_total_counts();

protected:
    // We're in one of 3 states.
    typedef enum { STATE_CPU, STATE_IDLE, STATE_WAIT } state_t;

    static constexpr char THREAD_LETTER_INITIAL_START = 'A';
    static constexpr char THREAD_LETTER_SUBSEQUENT_START = 'a';
    static constexpr char WAIT_SYMBOL = '-';
    static constexpr char IDLE_SYMBOL = '_';

    struct per_shard_t {
        std::string error;
        memtrace_stream_t *stream = nullptr;
        int64_t core = 0; // We target core-sharded.
        counters_t counters;
        int64_t prev_workload_id = -1;
        int64_t prev_tid = INVALID_THREAD_ID;
        // These are cleared when an instruction is seen.
        bool saw_syscall = false;
        memref_tid_t direct_switch_target = INVALID_THREAD_ID;
        bool saw_exit = false;
        // A representation of the thread interleavings.
        std::string thread_sequence;
        // The instruction count for the current activity (an active input or a wait
        // or idle state) on this shard, since the last context switch or reset due
        // to knob_print_every_: the time period between switches or resets we call
        // a "segment".
        uint64_t cur_segment_instrs = 0;
        state_t cur_state = STATE_CPU;
        // Computing %-idle.
        uint64_t segment_start_microseconds = 0;
        intptr_t filetype = 0;
        uint64_t switch_start_instrs = 0;
    };

    void
    print_percentage(double numerator, double denominator, const std::string &label);

    void
    print_counters(const counters_t &counters);

    virtual uint64_t
    get_current_microseconds();

    bool
    update_state_time(per_shard_t *shard, state_t state);

    void
    record_context_switch(per_shard_t *shard, int64_t tid, int64_t input_id,
                          int64_t letter_ord);

    virtual void
    aggregate_results(counters_t &total);

    void
    get_scheduler_stats(memtrace_stream_t *stream, counters_t &counters);

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
