/* **********************************************************
 * Copyright (c) 2023-2025 Google, Inc.  All rights reserved.
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
#include <unordered_map>
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
    shard_type_t
    preferred_shard_type() override
    {
        return SHARD_BY_CORE;
    }
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
        virtual std::string
        to_string() const = 0;
        virtual void
        print() const = 0;
        virtual bool
        empty() const = 0;
    };

    // Simple binning histogram for instrs-per-switch distribution.
    class histogram_t : public histogram_interface_t {
    public:
        explicit histogram_t(uint64_t bin_size)
            : bin_size_(bin_size)
        {
        }

        void
        add(int64_t value) override
        {
            // XXX: Add dynamic bin size changing.
            // For now with relatively known data ranges we just stick
            // with unchanging bin sizes.
            uint64_t bin = value - (value % bin_size_);
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

        std::string
        to_string() const override
        {
            std::ostringstream stream;
            for (const auto &keyval : bin2count_) {
                stream << std::setw(12) << keyval.first << ".." << std::setw(8)
                       << keyval.first + bin_size_ << " " << std::setw(5) << keyval.second
                       << "\n";
            }
            return stream.str();
        }

        void
        print() const override
        {
            std::cerr << to_string();
        }

        bool
        empty() const override
        {
            return bin2count_.empty();
        }

    protected:
        uint64_t bin_size_;

        // Key is the inclusive lower bound of the bin.
        std::map<uint64_t, uint64_t> bin2count_;
    };

    struct workload_tid_t {
        workload_tid_t(int64_t workload, int64_t thread)
            : workload_id(workload)
            , tid(thread)
        {
        }
        bool
        operator==(const workload_tid_t &rhs) const
        {
            return workload_id == rhs.workload_id && tid == rhs.tid;
        }

        bool
        operator!=(const workload_tid_t &rhs) const
        {
            return !(*this == rhs);
        }
        int64_t workload_id;
        int64_t tid;
    };

    struct workload_tid_hash_t {
        std::size_t
        operator()(const workload_tid_t &wt) const
        {
            // Ensure {workload_id=X, tid=Y} doesn't always hash the same as
            // {workload_id=Y, tid=X} by avoiding a simple symmetric wid^tid.
            return std::hash<size_t>()(static_cast<size_t>(wt.workload_id ^ wt.tid)) ^
                std::hash<memref_tid_t>()(wt.tid);
        }
    };

    struct counters_t {
        explicit counters_t(schedule_stats_t *analyzer)
            : analyzer(analyzer)
        {
            static constexpr uint64_t kSwitchBinSize = 50000;
            static constexpr uint64_t kCoresBinSize = 1;
            instrs_per_switch = analyzer->create_histogram(kSwitchBinSize);
            cores_per_thread = analyzer->create_histogram(kCoresBinSize);
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
            steals += rhs.steals;
            rebalances += rhs.rebalances;
            at_output_limit += rhs.at_output_limit;
            instrs += rhs.instrs;
            total_switches += rhs.total_switches;
            voluntary_switches += rhs.voluntary_switches;
            direct_switches += rhs.direct_switches;
            syscalls += rhs.syscalls;
            maybe_blocking_syscalls += rhs.maybe_blocking_syscalls;
            direct_switch_requests += rhs.direct_switch_requests;
            switch_sequence_injections += rhs.switch_sequence_injections;
            syscall_sequence_injections += rhs.syscall_sequence_injections;
            observed_migrations += rhs.observed_migrations;
            waits += rhs.waits;
            idles += rhs.idles;
            idle_microseconds += rhs.idle_microseconds;
            idle_micros_at_last_instr += rhs.idle_micros_at_last_instr;
            cpu_microseconds += rhs.cpu_microseconds;
            wait_microseconds += rhs.wait_microseconds;
            for (const workload_tid_t tid : rhs.threads) {
                threads.insert(tid);
            }
            instrs_per_switch->merge(rhs.instrs_per_switch.get());
            // We do not track this incrementally but for completeness we include
            // aggregation code for it.
            cores_per_thread->merge(rhs.cores_per_thread.get());
            for (const auto &it : rhs.sysnum_switch_latency) {
                histogram_interface_t *hist_ptr = analyzer->find_or_add_histogram(
                    sysnum_switch_latency, it.first, kSysnumLatencyBinSize);
                hist_ptr->merge(it.second.get());
            }
            for (const auto &it : rhs.sysnum_noswitch_latency) {
                histogram_interface_t *hist_ptr = analyzer->find_or_add_histogram(
                    sysnum_noswitch_latency, it.first, kSysnumLatencyBinSize);
                hist_ptr->merge(it.second.get());
            }
            return *this;
        }
        schedule_stats_t *analyzer;
        // Statistics provided by scheduler.
        // XXX: Should we change all of these to uint64_t? Never negative: but signed
        // ints are generally recommended as better for the compiler.
        int64_t switches_input_to_input = 0;
        int64_t switches_input_to_idle = 0;
        int64_t switches_idle_to_input = 0;
        int64_t switches_nop = 0;
        int64_t quantum_preempts = 0;
        int64_t migrations = 0;
        int64_t steals = 0;
        int64_t rebalances = 0;
        int64_t at_output_limit = 0;
        // Our own statistics.
        int64_t instrs = 0;
        int64_t total_switches = 0;
        int64_t voluntary_switches = 0;
        int64_t direct_switches = 0; // Subset of voluntary_switches.
        int64_t syscalls = 0;
        int64_t maybe_blocking_syscalls = 0;
        int64_t direct_switch_requests = 0;
        int64_t switch_sequence_injections = 0;
        int64_t syscall_sequence_injections = 0;
        // Our observed migrations will be <= the scheduler's reported migrations
        // for a dynamic schedule as we don't know the initial runqueue allocation
        // and so can't see the migration of an input that didn't execute in the
        // trace yet. For replayed (including core-sharded-on-disk) the scheduler
        // does not provide any data and so this stat is required there.
        int64_t observed_migrations = 0;
        int64_t waits = 0;
        int64_t idles = 0;
        uint64_t idle_microseconds = 0;
        uint64_t idle_micros_at_last_instr = 0;
        uint64_t cpu_microseconds = 0;
        uint64_t wait_microseconds = 0;
        std::unordered_set<workload_tid_t, workload_tid_hash_t> threads;
        std::unique_ptr<histogram_interface_t> instrs_per_switch;
        // CPU footprint of each thread. This is computable during aggregation from
        // the .threads field above so we don't bother to track this incrementally.
        // We still store it inside counters_t as this structure is assumed in
        // several places to hold all aggregated statistics.
        std::unique_ptr<histogram_interface_t> cores_per_thread;
        // Breakdown of system calls by number (key of map) and latency (in
        // microseconds; stored as a histogram) and whether a context switch was
        // incurred (separate map for each).
        std::unordered_map<int, std::unique_ptr<histogram_interface_t>>
            sysnum_switch_latency;
        std::unordered_map<int, std::unique_ptr<histogram_interface_t>>
            sysnum_noswitch_latency;
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
    static constexpr uint64_t kSysnumLatencyBinSize = 5;

    struct per_shard_t {
        per_shard_t(schedule_stats_t *analyzer)
            : counters(analyzer)
        {
        }
        // Provide a virtual destructor to allow subclassing.
        virtual ~per_shard_t() = default;
        std::string error;
        memtrace_stream_t *stream = nullptr;
        int64_t core = 0; // We target core-sharded.
        counters_t counters;
        int64_t prev_workload_id = -1;
        int64_t prev_tid = INVALID_THREAD_ID;
        // These are cleared when an instruction is seen.
        bool saw_syscall = false;
        int last_syscall_number = -1;
        uint64_t pre_syscall_timestamp = 0;
        uint64_t post_syscall_timestamp = 0;
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
        bool in_syscall_trace = false;
    };

    virtual std::unique_ptr<histogram_interface_t>
    create_histogram(uint64_t bin_size)
    {
        return std::unique_ptr<histogram_interface_t>(new histogram_t(bin_size));
    }

    histogram_interface_t *
    find_or_add_histogram(
        std::unordered_map<int, std::unique_ptr<histogram_interface_t>> &map, int key,
        int bin_size = 1)
    {
        auto find_it = map.find(key);
        if (find_it != map.end())
            return find_it->second.get();
        auto new_hist = create_histogram(bin_size);
        histogram_interface_t *hist_ptr = new_hist.get();
        map.insert({ key, std::move(new_hist) });
        return hist_ptr;
    }

    void
    print_percentage(double numerator, double denominator, const std::string &label);

    void
    print_counters(const counters_t &counters);

    virtual uint64_t
    get_current_microseconds();

    bool
    update_state_time(per_shard_t *shard, state_t state);

    // shard->prev_workload_id and shard->prev_tid are cleared when this is called,
    // so we pass in the preserved values so there's no confusion.
    virtual void
    record_context_switch(per_shard_t *shard, int64_t prev_workload_id, int64_t prev_tid,
                          int64_t workload_id, int64_t tid, int64_t input_id,
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
    // To track migrations we unfortunately need a global synchronized map to
    // remember the last core for each input.
    std::unordered_map<workload_tid_t, int64_t, workload_tid_hash_t> prev_core_;
    std::mutex prev_core_mutex_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _SCHEDULE_STATS_H_ */
