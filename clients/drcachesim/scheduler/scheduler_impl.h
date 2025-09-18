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

/* Private implementation of drmemtrace scheduler. */

#ifndef _DRMEMTRACE_SCHEDULER_IMPL_H_
#define _DRMEMTRACE_SCHEDULER_IMPL_H_ 1

#define NOMINMAX // Avoid windows.h messing up std::max.
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "archive_istream.h"
#include "archive_ostream.h"
#include "flexible_queue.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "mutex_dbg_owned.h"
#include "reader.h"
#include "record_file_reader.h"
#include "speculator.h"
#include "trace_entry.h"
#include "utils.h"

#undef VPRINT
// We make logging available in release build to help in diagnosing issues
// and understanding scheduler behavior.
// We assume the extra branches do not add undue overhead.
#define VPRINT(obj, level, ...)                            \
    do {                                                   \
        if ((obj)->verbosity_ >= (level)) {                \
            fprintf(stderr, "%s ", (obj)->output_prefix_); \
            fprintf(stderr, __VA_ARGS__);                  \
        }                                                  \
    } while (0)
#define VDO(obj, level, statement)          \
    do {                                    \
        if ((obj)->verbosity_ >= (level)) { \
            statement                       \
        }                                   \
    } while (0)

namespace dynamorio {
namespace drmemtrace {

// We do not subclass scheduler_tmpl_t to avoid a compile-time dependence between
// them (part of the pImpl idiom) and to let us embed a dynamically chosen subclass
// of scheduler_impl_tmpl_t inside the same unchanging-type outer class created by
// the user.
template <typename RecordType, typename ReaderType> class scheduler_impl_tmpl_t {
protected:
    using sched_type_t = scheduler_tmpl_t<RecordType, ReaderType>;
    using input_ordinal_t = typename sched_type_t::input_ordinal_t;
    using output_ordinal_t = typename sched_type_t::output_ordinal_t;
    using scheduler_status_t = typename sched_type_t::scheduler_status_t;
    using stream_status_t = typename sched_type_t::stream_status_t;
    using stream_t = typename sched_type_t::stream_t;
    using input_workload_t = typename sched_type_t::input_workload_t;
    using scheduler_options_t = typename sched_type_t::scheduler_options_t;
    using input_thread_info_t = typename sched_type_t::input_thread_info_t;
    using scheduler_flags_t = typename sched_type_t::scheduler_flags_t;
    using range_t = typename sched_type_t::range_t;
    using switch_type_t = typename sched_type_t::switch_type_t;

public:
    scheduler_impl_tmpl_t() = default;
    virtual ~scheduler_impl_tmpl_t();

    virtual scheduler_status_t
    init(std::vector<input_workload_t> &workload_inputs, int output_count,
         scheduler_options_t options);

    virtual stream_t *
    get_stream(output_ordinal_t ordinal)
    {
        if (ordinal < 0 || ordinal >= static_cast<output_ordinal_t>(outputs_.size()))
            return nullptr;
        return outputs_[ordinal].stream;
    }

    virtual int
    get_input_stream_count() const
    {
        return static_cast<input_ordinal_t>(inputs_.size());
    }

    virtual memtrace_stream_t *
    get_input_stream_interface(input_ordinal_t input) const
    {
        if (input < 0 || input >= static_cast<input_ordinal_t>(inputs_.size()))
            return nullptr;
        return inputs_[input].reader.get();
    }

    virtual std::string
    get_input_stream_name(input_ordinal_t input) const
    {
        if (input < 0 || input >= static_cast<input_ordinal_t>(inputs_.size()))
            return "";
        return inputs_[input].reader->get_stream_name();
    }

    int64_t
    get_output_cpuid(output_ordinal_t output) const;

    std::string
    get_error_string() const
    {
        return error_string_;
    }

    scheduler_status_t
    write_recorded_schedule();

protected:
    typedef speculator_tmpl_t<RecordType> spec_type_t;

    struct input_info_t {
        input_info_t()
            : lock(new mutex_dbg_owned)
        {
        }
        // Returns whether the stream mixes threads (online analysis mode) yet
        // wants to treat them as separate shards (so not core-sharded-on-disk).
        bool
        is_combined_stream()
        {
            // If the tid is invalid, this is a combined stream (online analysis mode).
            return tid == INVALID_THREAD_ID;
        }
        int index = -1; // Position in inputs_ vector.
        std::unique_ptr<ReaderType> reader;
        std::unique_ptr<ReaderType> reader_end;
        // While the scheduler only hands an input to one output at a time, during
        // scheduling decisions one thread may need to access another's fields.
        // This lock controls access to fields that are modified during scheduling.
        // This must be accessed after any output lock.
        // If multiple input locks are held at once, they should be acquired in
        // increased "index" order.
        // We use a unique_ptr to make this moveable for vector storage.
        std::unique_ptr<mutex_dbg_owned> lock;
        // Index into workloads_ vector.
        // A tid can be duplicated across workloads so we need the pair of
        // workload index + tid to identify the original input.
        int workload = -1;
        // If left invalid, this is a combined stream (online analysis mode).
        memref_tid_t tid = INVALID_THREAD_ID;
        memref_pid_t pid = INVALID_PID;
        // Used for combined streams.
        memref_tid_t last_record_tid = INVALID_THREAD_ID;

        struct queued_record_t {
            queued_record_t()
            {
            }
            queued_record_t(const RecordType &record)
                : record(record)
            {
            }
            queued_record_t(const RecordType &record, uint64_t next_trace_pc)
                : record(record)
                , next_trace_pc(next_trace_pc)
                , next_trace_pc_valid(true)
            {
            }

            RecordType record;
            uint64_t next_trace_pc = 0;
            // If this is false, it means the next trace pc should be obtained
            // from the input stream get_next_trace_pc() API instead.
            bool next_trace_pc_valid = false;
        };

        // If non-empty these records should be returned before incrementing the reader.
        // This is used for read-ahead and inserting synthetic records.
        // We use a deque so we can iterate over it.
        // Records should be removed from the front of the queue using the
        // get_queued_record() helper which sets the related state automatically.
        std::deque<queued_record_t> queue;
        bool cur_from_queue;
        // Valid only if cur_from_queue is set.
        queued_record_t cur_queue_record;

        addr_t last_pc_fallthrough = 0;
        // Whether we're in the middle of returning injected syscall records.
        bool in_syscall_injection = false;

        std::set<output_ordinal_t> binding;
        int priority = 0;
        std::vector<range_t> regions_of_interest;
        // Index into regions_of_interest.
        int cur_region = 0;
        // Whether we have reached the current region proper (or are still on the
        // preceding inserted timestamp+cpuid).
        bool in_cur_region = false;
        bool has_modifier = false;
        bool needs_init = false;
        bool needs_advance = false;
        bool needs_roi = true;
        bool at_eof = false;
        // The output whose ready queue or active run slot we are in.
        output_ordinal_t containing_output = sched_type_t::INVALID_OUTPUT_ORDINAL;
        // The previous containing_output.
        output_ordinal_t prev_output = sched_type_t::INVALID_OUTPUT_ORDINAL;
        // The current output where we're actively running.
        output_ordinal_t cur_output = sched_type_t::INVALID_OUTPUT_ORDINAL;
        uintptr_t next_timestamp = 0;
        uint64_t instrs_in_quantum = 0;
        int instrs_pre_read = 0;
        // This is a per-workload value, stored in each input for convenience.
        uint64_t base_timestamp = 0;
        // This equals 'options_.deps == DEPENDENCY_TIMESTAMPS', stored here for
        // access in InputTimestampComparator which is static and has no access
        // to the schedule_t.  (An alternative would be to try to get a lambda
        // with schedule_t "this" access for the comparator to compile: it is not
        // simple to do so, however.)
        bool order_by_timestamp = false;
        // Global queue counter used to provide FIFO for same-priority inputs.
        // This value is only valid when this input is in a queue; it is set upon
        // being added to a queue.
        uint64_t queue_counter = 0;
        // Used to switch on the instruction *after* a long-latency syscall.
        bool processing_syscall = false;
        bool processing_maybe_blocking_syscall = false;
        uint64_t pre_syscall_timestamp = 0;
        // Use for special kernel features where one thread specifies a target
        // thread to replace it.
        input_ordinal_t switch_to_input = sched_type_t::INVALID_INPUT_ORDINAL;
        uint64_t syscall_timeout_arg = 0;
        // Used to switch before we've read the next instruction.
        bool switching_pre_instruction = false;
        // Used for time-based quanta.  The units are simulation time.
        uint64_t prev_time_in_quantum = 0;
        uint64_t time_spent_in_quantum = 0;
        // These fields model waiting at a blocking syscall.
        // The units are in simuilation time.
        uint64_t blocked_time = 0;
        uint64_t blocked_start_time = 0;
        // XXX i#6831: Move fields like this to be specific to subclasses by changing
        // inputs_ to a vector of unique_ptr and then subclassing input_info_t.
        // An input can be "unscheduled" and not on the ready_priority_ run queue at all
        // with an infinite timeout until directly targeted.  Such inputs are stored
        // in the unscheduled_priority_ queue.
        // This field is also set to true for inputs that are "unscheduled" but with
        // a timeout, even though that is implemented by storing them in ready_priority_
        // (because that is our mechanism for measuring timeouts).
        bool unscheduled = false;
        // Causes the next unscheduled entry to abort.
        bool skip_next_unscheduled = false;
        uint64_t last_run_time = 0;
        int to_inject_syscall = INJECT_NONE;
        bool saw_first_func_id_marker_after_syscall = false;

        // Sentinel value for to_inject_syscall.
        static constexpr int INJECT_NONE = -1;
    };

    // XXX i#6831: Should this live entirely inside the dynamic subclass?
    // We would need to make part of init() virtual with subclass overrides.
    struct workload_info_t {
        explicit workload_info_t(int output_limit, std::vector<input_ordinal_t> inputs)
            : output_limit(output_limit)
            , inputs(std::move(inputs))
        {
            live_output_count = std::unique_ptr<std::atomic<int>>(new std::atomic<int>());
            live_output_count->store(0, std::memory_order_relaxed);
        }
        const int output_limit; // No lock needed since read-only.
        // We use a unique_ptr to make this moveable for vector storage.
        std::unique_ptr<std::atomic<int>> live_output_count;
        const std::vector<input_ordinal_t> inputs; // No lock needed: read-only post-init.
    };

    // Format for recording a schedule to disk.  A separate sequence of these records
    // is stored per output stream; each output stream's sequence is in one component
    // (subfile) of an archive file.
    // All fields are little-endian.
    START_PACKED_STRUCTURE
    struct schedule_record_t {
        enum record_type_t {
            // A regular entry denoting one thread sequence between context switches.
            DEFAULT,
            // The first entry in each component must be this type.  The "key" field
            // holds a version number.
            VERSION,
            FOOTER,        // The final entry in the component.  Other fields are ignored.
            SKIP,          // Skip ahead to the next region of interest.
            SYNTHETIC_END, // A synthetic thread exit record must be supplied.
            // Indicates that the output is idle.  The value.idle_duration field holds
            // a duration in microseconds.
            IDLE,
            // Indicates that the output is idle.  The value.idle_duration field holds
            // a duration as a count of idle records.
            IDLE_BY_COUNT,
        };
        static constexpr int VERSION_CURRENT = 0;
        schedule_record_t() = default;
        schedule_record_t(record_type_t type, input_ordinal_t input, uint64_t start,
                          uint64_t stop, uint64_t time)
            : type(type)
            , key(input)
            , value(start)
            , stop_instruction(stop)
            , timestamp(time)
        {
        }
        record_type_t type;
        START_PACKED_STRUCTURE
        union key {
            key() = default;
            key(input_ordinal_t input)
                : input(input)
            {
            }
            // We assume the user will repeat the precise input workload specifications
            // (including directory ordering of thread files) and we can simply store
            // the ordinal and rely on the same ordinal on replay being the same input.
            input_ordinal_t input = -1;
            int version; // For record_type_t::VERSION.
        } END_PACKED_STRUCTURE key;
        START_PACKED_STRUCTURE
        union value {
            value() = default;
            value(uint64_t start)
                : start_instruction(start)
            {
            }
            // For record_type_t::IDLE, the duration in microseconds of the idling.
            // For record_type_t::IDLE_BY_COUNT, the duration as a count of idle records.
            uint64_t idle_duration;
            // Input stream ordinal of starting point, for non-IDLE types.
            uint64_t start_instruction = 0;
        } END_PACKED_STRUCTURE value;
        // Input stream ordinal, exclusive.  Max numeric value means continue until EOF.
        uint64_t stop_instruction = 0;
        // Timestamp in microseconds to keep context switches ordered.
        // XXX: To add more fine-grained ordering we could emit multiple entries
        // per thread segment, and update the context switching code to recognize
        // that a new entry does not always mean a context switch.
        uint64_t timestamp = 0;
    } END_PACKED_STRUCTURE;

    // Gets a queued record if one is available and sets related state in input_info_t.
    // Returns whether a queued record was indeed available and returned.
    bool
    get_queued_record(input_info_t *input, RecordType &record);

    ///////////////////////////////////////////////////////////////////////////
    // Support for ready queues for who to schedule next:

    // I tried using a lambda where we could capture "this" and so use int indices
    // in the queues instead of pointers but hit problems (weird crash while running)
    // so I'm sticking with this solution of a separate struct.
    struct InputTimestampComparator {
        bool
        operator()(input_info_t *a, input_info_t *b) const
        {
            if (a->priority != b->priority)
                return a->priority < b->priority; // Higher is better.
            if (a->order_by_timestamp &&
                (a->reader->get_last_timestamp() - a->base_timestamp) !=
                    (b->reader->get_last_timestamp() - b->base_timestamp)) {
                // Lower is better.
                return (a->reader->get_last_timestamp() - a->base_timestamp) >
                    (b->reader->get_last_timestamp() - b->base_timestamp);
            }
            // We use a counter to provide FIFO order for same-priority inputs.
            return a->queue_counter > b->queue_counter; // Lower is better.
        }
    };

    // Now that we have the lock usage narrow inside certain routines, we
    // may want to consider making this a class and having it own the add/pop
    // routines?  The complexity is popping for a different output.
    struct input_queue_t {
        explicit input_queue_t(int rand_seed = 0)
            : lock(new mutex_dbg_owned)
            , queue(rand_seed)
        {
        }
        // Protects access to this structure.
        // We use a unique_ptr to make this moveable for vector storage.
        // An output's ready_queue lock must be acquired *before* any input locks.
        // Multiple output locks should be acquired in increasing output ordinal order.
        std::unique_ptr<mutex_dbg_owned> lock;
        // Inputs ready to be scheduled, sorted by priority and then timestamp if
        // timestamp dependencies are requested.  We use the timestamp delta from the
        // first observed timestamp in each workload in order to mix inputs from different
        // workloads in the same queue.  FIFO ordering is used for same-priority entries.
        flexible_queue_t<input_info_t *, InputTimestampComparator> queue;
        // Queue counter used to provide FIFO for same-priority inputs.
        uint64_t fifo_counter = 0;
        // Tracks the count of blocked inputs.
        int num_blocked = 0;
    };

    bool
    need_output_lock();

    std::unique_lock<mutex_dbg_owned>
    acquire_scoped_output_lock_if_necessary(output_ordinal_t output);

    uint64_t
    scale_blocked_time(uint64_t initial_time) const;

    stream_status_t
    on_context_switch(output_ordinal_t output, input_ordinal_t prev_input,
                      input_ordinal_t new_input);

    void
    update_syscall_state(RecordType record, output_ordinal_t output);

    ///
    ///////////////////////////////////////////////////////////////////////////

    // We have one output_info_t per output stream, and at most one worker
    // thread owns one output, so most fields are accessed only by one thread.
    // One exception is .ready_queue which can be accessed by other threads;
    // it is protected using its internal lock.
    // Another exception is .record, which is read-only after initialization.
    // A few other fields are concurrently accessed and are of type std::atomic to allow
    // that.
    struct output_info_t {
        output_info_t(scheduler_impl_tmpl_t<RecordType, ReaderType> *scheduler_impl,
                      output_ordinal_t ordinal,
                      typename spec_type_t::speculator_flags_t speculator_flags,
                      int rand_seed, RecordType last_record_init, int verbosity = 0)
            : self_stream(scheduler_impl, ordinal, verbosity)
            , stream(&self_stream)
            , ready_queue(rand_seed)
            , speculator(speculator_flags, verbosity)
            , last_record(last_record_init)
        {
            active = std::unique_ptr<std::atomic<bool>>(new std::atomic<bool>());
            active->store(true, std::memory_order_relaxed);
            cur_time =
                std::unique_ptr<std::atomic<uint64_t>>(new std::atomic<uint64_t>());
            cur_time->store(0, std::memory_order_relaxed);
            initial_cur_time =
                std::unique_ptr<std::atomic<uint64_t>>(new std::atomic<uint64_t>());
            initial_cur_time->store(0, std::memory_order_relaxed);
            record_index = std::unique_ptr<std::atomic<int>>(new std::atomic<int>());
            record_index->store(0, std::memory_order_relaxed);
        }
        stream_t self_stream;
        // Normally stream points to &self_stream, but for single_lockstep_output
        // it points to a global stream shared among all outputs.
        stream_t *stream;
        // This is an index into the inputs_ vector so -1 is an invalid value.
        // This is set to >=0 for all non-empty outputs during init().
        input_ordinal_t cur_input = sched_type_t::INVALID_INPUT_ORDINAL;
        // Holds the prior non-invalid input.
        input_ordinal_t prev_input = sched_type_t::INVALID_INPUT_ORDINAL;
        // XXX i#6831: Move fields like this to be specific to subclasses by changing
        // inputs_ to a vector of unique_ptr and then subclassing output_info_t.
        // For static schedules we can populate this up front and avoid needing a
        // lock for dynamically finding the next input, keeping things parallel.
        std::vector<input_ordinal_t> input_indices;
        int input_indices_index = 0;
        // Inputs ready to be scheduled on this output.
        input_queue_t ready_queue;
        // Speculation support.
        std::stack<addr_t> speculation_stack; // Stores PC of resumption point.
        speculator_tmpl_t<RecordType> speculator;
        addr_t speculate_pc = 0;
        // Stores the value of speculate_pc before asking the speculator for the current
        // record.  So if that record was an instruction, speculate_pc holds the next PC
        // while this field holds the instruction's start PC.  The use case is for
        // queueing a read-ahead instruction record for start_speculation().
        addr_t prev_speculate_pc = 0;
        RecordType last_record; // Set to TRACE_TYPE_INVALID in constructor.
        // A list of schedule segments. During replay, this is read by other threads,
        // but it is only written at init time.
        std::vector<schedule_record_t> record;
        // This index into the .record vector is read by other threads and also written
        // during execution, so it requires atomic accesses.
        std::unique_ptr<std::atomic<int>> record_index;
        bool waiting = false; // Waiting or idling.
        // Used to limit stealing to one attempt per transition to idle.
        bool tried_to_steal_on_idle = false;
        // This is accessed by other outputs for stealing and rebalancing.
        // Indirected so we can store it in our vector.
        std::unique_ptr<std::atomic<bool>> active;
        // XXX: in_syscall_code and hit_syscall_code_end arguably are tied to an input
        // stream and must be a part of input_info_t instead. Today we do not context
        // switch in the middle of injected kernel syscall code, but if we did, this
        // state would be incorrect or lost.
        bool in_syscall_code = false;
        bool hit_syscall_code_end = false;
        bool in_context_switch_code = false;
        bool hit_switch_code_end = false;
        // Used for time-based quanta.
        // This is accessed by other outputs for stealing and rebalancing.
        // Indirected so we can store it in our vector.
        std::unique_ptr<std::atomic<uint64_t>> cur_time;
        // The first simulation time passed to this output.
        // This is accessed by other outputs for stealing and rebalancing.
        // Indirected so we can store it in our vector.
        std::unique_ptr<std::atomic<uint64_t>> initial_cur_time;
        // Used for MAP_TO_RECORDED_OUTPUT get_output_cpuid().
        int64_t as_traced_cpuid = -1;
        // Used for MAP_AS_PREVIOUSLY with live_replay_output_count_.
        bool at_eof = false;
        // Used for recording and replaying idle periods.
        int64_t idle_start_count = -1;
        // Exported statistics. Currently all integers and cast to double on export.
        std::vector<int64_t> stats =
            std::vector<int64_t>(memtrace_stream_t::SCHED_STAT_TYPE_COUNT);
        // When no simulation time is passed to us, we use the idle count plus
        // instruction count to measure time.
        uint64_t idle_count = 0;
        // The first timestamp (pre-update_next_record()) seen on the first input.
        uintptr_t base_timestamp = 0;
    };

    // Used for reading as-traced schedules.
    struct schedule_output_tracker_t {
        schedule_output_tracker_t(bool valid, input_ordinal_t input,
                                  uint64_t start_instruction, uint64_t timestamp)
            : valid(valid)
            , input(input)
            , start_instruction(start_instruction)
            , stop_instruction(0)
            , timestamp(timestamp)
        {
        }
        // To support removing later-discovered-as-redundant entries without
        // a linear erase operation we have a 'valid' flag.
        bool valid;
        input_ordinal_t input;
        uint64_t start_instruction;
        uint64_t stop_instruction;
        uint64_t timestamp;
    };
    // Used for reading as-traced schedules.
    struct schedule_input_tracker_t {
        schedule_input_tracker_t(output_ordinal_t output, uint64_t output_array_idx,
                                 uint64_t start_instruction, uint64_t timestamp)
            : output(output)
            , output_array_idx(output_array_idx)
            , start_instruction(start_instruction)
            , timestamp(timestamp)
        {
        }
        output_ordinal_t output;
        uint64_t output_array_idx;
        uint64_t start_instruction;
        uint64_t timestamp;
    };

    // Custom hash function used for switch_type_t and syscall num (int).
    template <typename IntCastable> struct custom_hash_t {
        std::size_t
        operator()(const IntCastable &st) const
        {
            return std::hash<int>()(static_cast<int>(st));
        }
    };

    // Tracks data used while opening inputs.
    struct input_reader_info_t {
        std::set<memref_tid_t> only_threads;
        std::set<input_ordinal_t> only_shards;
        // Maps each opened reader's tid to its input ordinal.
        std::unordered_map<memref_tid_t, int> tid2input;
        // Holds the original tids pre-filtering by only_*.
        std::set<memref_tid_t> unfiltered_tids;
        // The count of original pre-filtered inputs (might not match
        // unfiltered_tids.size() for core-sharded inputs with IDLE_THREAD_ID).
        uint64_t input_count = 0;
        // The index into inputs_ at which this workload's inputs begin.
        input_ordinal_t first_input_ordinal = 0;
    };

    // We assume a 2GHz clock and IPC=0.5 to match
    // scheduler_options_t.time_units_per_us.
    static constexpr uint64_t INSTRS_PER_US = 1000;

    ///////////////////////////////////////////////////////////////////////////
    /// Protected virtual methods.
    /// XXX i#6831: These interfaces between the main class the subclasses could be
    /// more clearly separated and crystalized.  One goal is to avoid conditionals
    // in scheduler_impl_tmpl_t based on options_mapping or possibly on options_
    // at all (possibly only storing options_ in the subclasses).

    // Called just once at initialization time to set the initial input-to-output
    // mappings and state for the particular mapping_t mode.
    // Should call set_cur_input() for all outputs with initial inputs.
    virtual scheduler_status_t
    set_initial_schedule() = 0;

    // When an output's input changes (whether between two valid inputs, from a valid to
    // INVALID_INPUT_ORDINAL, or vice versa), swap_out_input() is called on the outgoing
    // input (whose lock is held if "caller_holds_input_lock" is true; it will never be
    // true for MAP_TO_ANY_OUTPUT).  This is called after the input's fields (such as
    // cur_output and last_run_time) have been updated, if it was a valid input.
    // This should return STATUS_OK if there is nothing to do; errors are propagated.
    virtual stream_status_t
    swap_out_input(output_ordinal_t output, input_ordinal_t input,
                   bool caller_holds_input_lock) = 0;

    // When an output's input changes (to a valid input or to INVALID_INPUT_ORDINAL)
    // different from the previous input, swap_in_input() is called on the incoming
    // input (whose lock is always held by the caller, if a valid input).
    // This is properly ordered with respect to swap_out_input().
    // This should return STATUS_OK if there is nothing to do; errors are propagated.
    virtual stream_status_t
    swap_in_input(output_ordinal_t output, input_ordinal_t input) = 0;

    // Allow subclasses to perform custom initial marker processing during
    // get_initial_input_content().  Returns whether to keep reading.
    // The caller will stop calling when an instruction record is reached.
    // The 'record' may have TRACE_TYPE_INVALID in some calls in which case
    // the two bool parameters are what the return value should be based on.
    virtual bool
    process_next_initial_record(input_info_t &input, RecordType record,
                                bool &found_filetype, bool &found_timestamp);

    // Helper for pick_next_input() specialized by mapping_t mode.
    // This is called when check_for_input_switch() indicates a switch is needed.
    // No input_info_t lock can be held on entry.
    virtual stream_status_t
    pick_next_input_for_mode(output_ordinal_t output, uint64_t blocked_time,
                             input_ordinal_t prev_index, input_ordinal_t &index) = 0;

    // Helper for next_record() specialized by mapping_t mode: called on every record
    // before it's passed to the user.  Determines whether to switch to a new input
    // (returned in "need_new_input"; if so, whether it's a preempt is in "preempt"
    // and if this current input should be blocked then that time should be set in
    // "blocked_time").  If this returns true for "need_new_input",
    // pick_next_input_for_mode() is called.
    virtual stream_status_t
    check_for_input_switch(output_ordinal_t output, RecordType &record,
                           input_info_t *input, uint64_t cur_time, bool &need_new_input,
                           bool &preempt, uint64_t &blocked_time) = 0;

    // The external interface lets a user request that an output go inactive when
    // doing dynamic scheduling.
    virtual stream_status_t
    set_output_active(output_ordinal_t output, bool active)
    {
        // Only supported in scheduler_dynamic_tmpl_t subclass.
        return sched_type_t::STATUS_INVALID;
    }

    // mapping_t-mode specific actions when one output runs out of things to do
    // (pick_next_input_for_mode() has nothing left in this output's queue).
    // Success return values are either STATUS_IDLE (asking the user to keep
    // polling as more work may show up in the future) or STATUS_EOF.
    virtual stream_status_t
    eof_or_idle_for_mode(output_ordinal_t output, input_ordinal_t prev_input) = 0;

    ///
    ///////////////////////////////////////////////////////////////////////////

    // Assumed to only be called at initialization time.
    // Reads ahead in each input to find its filetype, and if "gather_timestamps"
    // is set, to find its first timestamp, queuing all records
    // read to feed to the user's first requests.
    scheduler_status_t
    get_initial_input_content(bool gather_timestamps);

    // Dumps the options, for diagnostics.
    void
    print_configuration();

    scheduler_status_t
    legacy_field_support();

    // Opens readers for each file in 'path', subject to the constraints in
    // 'reader_info'.  'path' may be a directory.
    // Updates the ti2dinput, unfiltered_tids, and input_count fields of 'reader_info'.
    scheduler_status_t
    open_readers(const std::string &path, input_reader_info_t &reader_info);

    // Opens a reader for the file in 'path', subject to the constraints in
    // 'reader_info'.  'path' may not be a directory.
    // Updates the ti2dinput, unfiltered_tids, and input_count fields of 'reader_info'.
    scheduler_status_t
    open_reader(const std::string &path, input_ordinal_t input_ordinal,
                input_reader_info_t &reader_info);

    // Creates a reader for the default file type we support.
    std::unique_ptr<ReaderType>
    get_default_reader();

    // Creates a reader for the specific file type at (non-directory) 'path'.
    std::unique_ptr<ReaderType>
    get_reader(const std::string &path, int verbosity);

    // Advances the 'output_ordinal'-th output stream.
    stream_status_t
    next_record(output_ordinal_t output, RecordType &record, input_info_t *&input,
                uint64_t cur_time = 0);

    // Undoes the last read.  May only be called once between next_record() calls.
    // Is not supported during speculation nor prior to speculation with queueing,
    // as documented in the stream interfaces.
    stream_status_t
    unread_last_record(output_ordinal_t output, RecordType &record, input_info_t *&input);

    // Skips ahead to the next region of interest if necessary.
    // The caller must hold the input.lock.
    // If STATUS_SKIPPED or STATUS_STOLE is returned, a new next record needs to be read.
    stream_status_t
    advance_region_of_interest(output_ordinal_t output, RecordType &record,
                               input_info_t &input);

    // Discards the contents of the input queue.  Meant to be used when skipping
    // input records.
    void
    clear_input_queue(input_info_t &input);

    // Does a direct skip, unconditionally.
    // The caller must hold the input.lock.
    stream_status_t
    skip_instructions(input_info_t &input, uint64_t skip_amount);

    // Records an input skip in the output's recorded schedule.
    // The caller must hold the input.lock.
    stream_status_t
    record_schedule_skip(output_ordinal_t output, input_ordinal_t input,
                         uint64_t start_instruction, uint64_t stop_instruction);

    scheduler_status_t
    create_regions_from_times(const std::unordered_map<memref_tid_t, int> &workload_tids,
                              input_workload_t &workload);

    // Interval time-to-instr-ord tree lookup with interpolation.
    bool
    time_tree_lookup(const std::map<uint64_t, uint64_t> &tree, uint64_t time,
                     uint64_t &ordinal);

    // Reads from the as-traced schedule file into the passed-in structures, after
    // fixing up zero-instruction sequences and working around i#6107.
    scheduler_status_t
    read_traced_schedule(std::vector<std::vector<schedule_input_tracker_t>> &input_sched,
                         std::vector<std::set<uint64_t>> &start2stop,
                         std::vector<std::vector<schedule_output_tracker_t>> &all_sched,
                         std::vector<output_ordinal_t> &disk_ord2index,
                         std::vector<uint64_t> &disk_ord2cpuid);

    // Helper for read_traced_schedule().
    scheduler_status_t
    remove_zero_instruction_segments(
        std::vector<std::vector<schedule_input_tracker_t>> &input_sched,
        std::vector<std::vector<schedule_output_tracker_t>> &all_sched);

    // Helper for read_traced_schedule().
    scheduler_status_t
    check_and_fix_modulo_problem_in_schedule(
        std::vector<std::vector<schedule_input_tracker_t>> &input_sched,
        std::vector<std::set<uint64_t>> &start2stop,
        std::vector<std::vector<schedule_output_tracker_t>> &all_sched);

    template <typename SequenceKey>
    SequenceKey
    invalid_kernel_sequence_key();

    template <typename SequenceKey>
    scheduler_status_t
    read_kernel_sequences(std::unordered_map<SequenceKey, std::vector<RecordType>,
                                             custom_hash_t<SequenceKey>> &sequence,
                          std::string trace_path, std::unique_ptr<ReaderType> reader,
                          std::unique_ptr<ReaderType> reader_end,
                          trace_marker_type_t start_marker,
                          trace_marker_type_t end_marker, std::string sequence_type);

    scheduler_status_t
    read_switch_sequences();

    scheduler_status_t
    read_syscall_sequences();

    uint64_t
    get_time_micros();

    uint64_t
    get_output_time(output_ordinal_t output);

    // The caller must hold the lock for the input unless it's not a real
    // input index (it's not real for VERSION, FOOTER, and IDLE).
    stream_status_t
    record_schedule_segment(
        output_ordinal_t output, typename schedule_record_t::record_type_t type,
        // "input" can instead be a version of type int.
        // As they are the same underlying type we cannot overload.
        input_ordinal_t input, uint64_t start_instruction,
        // Wrap max in parens to work around Visual Studio compiler issues with the
        // max macro (even despite NOMINMAX defined above).
        uint64_t stop_instruction = (std::numeric_limits<uint64_t>::max)());

    // The caller must hold the input.lock unless the record type
    // is VERSION, FOOTER, or IDLE.
    stream_status_t
    close_schedule_segment(output_ordinal_t output, input_info_t &input);

    std::string
    recorded_schedule_component_name(output_ordinal_t output);

    bool
    check_valid_input_limits(const input_workload_t &workload,
                             input_reader_info_t &reader_info);

    // The caller cannot hold the output or input lock.
    // The caller can hold the output's current input's lock but must pass
    // true for the 3rd parameter in that case; holding the lock is not
    // allowed for MAP_TO_ANY_OUTPUT.
    stream_status_t
    set_cur_input(output_ordinal_t output, input_ordinal_t input,
                  bool caller_holds_cur_input_lock = false);

    // Finds the next input stream for the 'output_ordinal'-th output stream.
    // No input_info_t lock can be held on entry.
    stream_status_t
    pick_next_input(output_ordinal_t output, uint64_t blocked_time);

    // If the given record has a thread id field, returns true and the value.
    bool
    record_type_has_tid(RecordType record, memref_tid_t &tid);

    // If the given record has a process id field, returns true and the value.
    bool
    record_type_has_pid(RecordType record, memref_pid_t &pid);

    // For trace_entry_t, only sets the tid for record types that have it.
    void
    record_type_set_tid(RecordType &record, memref_tid_t tid);

    // For trace_entry_t, only sets the pid for record types that have it.
    void
    record_type_set_pid(RecordType &record, memref_pid_t pid);

    // Returns whether the given record is an instruction.
    bool
    record_type_is_instr(RecordType record, addr_t *pc = nullptr, size_t *size = nullptr);

    // Returns whether the given record is an indirect branch. Returns in
    // has_indirect_branch_target whether the instr record type supports the indirect
    // branch target field or not, which will be true only when RecordType is memref_t.
    // When RecordType is memref_t, it also sets the indirect branch target field to the
    // given value if it's non-zero.
    bool
    record_type_is_indirect_branch_instr(RecordType &record,
                                         bool &has_indirect_branch_target,
                                         addr_t set_indirect_branch_target = 0);

    // If the given record is a marker, returns true and its fields.
    bool
    record_type_is_marker(RecordType record, trace_marker_type_t &type, uintptr_t &value);

    // Returns false for memref_t; for trace_entry_t returns true for the _HEADER,
    // _THREAD, and _PID record types.
    bool
    record_type_is_non_marker_header(RecordType record);

    // If the given record is a timestamp, returns true and its fields.
    bool
    record_type_is_timestamp(RecordType record, uintptr_t &value);

    bool
    record_type_set_marker_value(RecordType &record, uintptr_t value);

    bool
    record_type_is_invalid(RecordType record);

    bool
    record_type_is_encoding(RecordType record);

    bool
    record_type_is_instr_boundary(RecordType record, RecordType prev_record);

    bool
    record_type_is_thread_exit(RecordType record);

    // Creates the marker we insert between regions of interest.
    RecordType
    create_region_separator_marker(memref_tid_t tid, uintptr_t value);

    // Creates a thread exit record.
    RecordType
    create_thread_exit(memref_tid_t tid);

    RecordType
    create_invalid_record();

    // If necessary, inserts context switch info on the incoming pid+tid.
    // The lock for 'input' is held by the caller.
    void
    insert_switch_tid_pid(input_info_t &input);

    void
    update_next_record(output_ordinal_t output, RecordType &record);

    // Performs the actual injection of the kernel sequence.
    stream_status_t
    inject_kernel_sequence(std::vector<RecordType> &sequence, input_info_t *input);

    // Performs the actual injection of a kernel syscall sequence, using
    // inject_kernel_sequence as helper.
    stream_status_t
    inject_pending_syscall_sequence(output_ordinal_t output, input_info_t *input,
                                    RecordType &record);

    // Checks whether we're at a suitable injection point for a yet to be injected
    // syscall sequence, and performs the injection using
    // inject_pending_syscall_sequence as helper.
    stream_status_t
    maybe_inject_pending_syscall_sequence(output_ordinal_t output, input_info_t *input,
                                          RecordType &record);

    // Actions that must be taken only when we know for sure that the given record
    // is going to be the next record for some output stream.
    stream_status_t
    finalize_next_record(output_ordinal_t output, const RecordType &record,
                         input_info_t *input);

    // Used for diagnostics: prints record fields to stderr.
    void
    print_record(const RecordType &record);

    // Returns the get_stream_name() value for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    std::string
    get_input_name(output_ordinal_t output);

    // Returns the input ordinal value for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    input_ordinal_t
    get_input_ordinal(output_ordinal_t output);

    // Returns the thread identifier for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    int64_t
    get_tid(output_ordinal_t output);

    // Returns the shard index for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    int
    get_shard_index(output_ordinal_t output);

    // Returns the workload ordinal value for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    int
    get_workload_ordinal(output_ordinal_t output);

    // Returns whether the current record for the current input stream scheduled on
    // the 'output_ordinal'-th output stream is synthetic.
    bool
    is_record_synthetic(output_ordinal_t output);

    // Returns the direct handle to the current input stream interface for the
    // 'output_ordinal'-th output stream.
    memtrace_stream_t *
    get_input_stream(output_ordinal_t output);

    // Returns the record ordinal for the current input stream interface for the
    // 'output_ordinal'-th output stream.
    uint64_t
    get_input_record_ordinal(output_ordinal_t output);

    // Returns the input instruction ordinal taking into account queued records.
    // XXX: We need to clearly delineate where the input lock is needed: here
    // we read the queue which shouldn't be changed by other threads; yet this
    // routine used to claim it needed the input lock.
    uint64_t
    get_instr_ordinal(input_info_t &input);

    // Returns the first timestamp for the current input stream interface for the
    // 'output_ordinal'-th output stream.
    uint64_t
    get_input_first_timestamp(output_ordinal_t output);

    // Returns the last timestamp for the current input stream interface for the
    // 'output_ordinal'-th output stream.
    uint64_t
    get_input_last_timestamp(output_ordinal_t output);

    // Returns the next continuous pc that will be seen in the trace.
    uint64_t
    get_next_trace_pc(output_ordinal_t output);

    stream_status_t
    start_speculation(output_ordinal_t output, addr_t start_address,
                      bool queue_current_record);

    stream_status_t
    stop_speculation(output_ordinal_t output);

    // Caller must hold the input's lock.
    // The return value is STATUS_EOF if a global exit is now happening (an
    // early exit); otherwise STATUS_OK is returned on success but only a
    // local EOF.
    stream_status_t
    mark_input_eof(input_info_t &input);

    // Determines whether to exit or wait for other outputs when one output
    // runs out of things to do.  May end up scheduling new inputs.
    // If STATUS_STOLE is returned, a new input was found and its next record needs
    // to be read.
    // Never returns STATUS_OK.
    stream_status_t
    eof_or_idle(output_ordinal_t output, input_ordinal_t prev_input);

    // These statistics are not guaranteed to be accurate when replaying a
    // prior schedule.
    double
    get_statistic(output_ordinal_t output,
                  memtrace_stream_t::schedule_statistic_t stat) const;

    // Adds bits to the filetype if required by the scheduler config.
    offline_file_type_t
    adjust_filetype(offline_file_type_t orig_filetype) const;

    // This has the same value as scheduler_options_t.verbosity (for use in VPRINT).
    int verbosity_ = 0;
    const char *output_prefix_ = "[scheduler]";
    std::string error_string_;
    scheduler_options_t options_;
    std::vector<workload_info_t> workloads_;
    // Each vector element has a mutex which should be held when accessing its fields.
    std::vector<input_info_t> inputs_;
    // Each vector element is accessed only by its owning thread, except the
    // ready_queue-related plus record and record_index fields which are accessed under
    // the output's own lock.
    std::vector<output_info_t> outputs_;
    // Count of inputs not yet at eof.
    std::atomic<int> live_input_count_;
    // In replay mode, count of outputs not yet at the end of the replay sequence.
    std::atomic<int> live_replay_output_count_;
    // Map from workload,tid pair to input.
    struct workload_tid_t {
        workload_tid_t(int wl, memref_tid_t tid)
            : workload(wl)
            , tid(tid)
        {
        }
        bool
        operator==(const workload_tid_t &rhs) const
        {
            return workload == rhs.workload && tid == rhs.tid;
        }
        int workload;
        memref_tid_t tid;
    };
    struct workload_tid_hash_t {
        std::size_t
        operator()(const workload_tid_t &wt) const
        {
            return std::hash<int>()(wt.workload) ^ std::hash<memref_tid_t>()(wt.tid);
        }
    };
    std::unordered_map<workload_tid_t, input_ordinal_t, workload_tid_hash_t> tid2input_;

    std::unordered_map<switch_type_t, std::vector<RecordType>,
                       custom_hash_t<switch_type_t>>
        switch_sequence_;
    // We specify a custom hash function only to make it easier to generalize with
    // switch_sequence_ defined above.
    std::unordered_map<int, std::vector<RecordType>, custom_hash_t<int>>
        syscall_sequence_;
    // For single_lockstep_output.
    std::unique_ptr<stream_t> global_stream_;
    // For online where we currently have to map dynamically observed thread ids
    // to the 0-based shard index.
    std::unordered_map<memref_tid_t, int> tid2shard_;

    // stream_t needs access to input_info_t.
    friend class scheduler_tmpl_t<RecordType, ReaderType>::stream_t;

    // Our testing class needs access to schedule_record_t.
    friend class replay_file_checker_t;
};

typedef scheduler_impl_tmpl_t<memref_t, reader_t> scheduler_impl_t;

typedef scheduler_impl_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>
    record_scheduler_impl_t;

// Specialized code for dynamic schedules (MAP_TO_ANY_OUTPUT).
template <typename RecordType, typename ReaderType>
class scheduler_dynamic_tmpl_t : public scheduler_impl_tmpl_t<RecordType, ReaderType> {
public:
    scheduler_dynamic_tmpl_t()
    {
        last_rebalance_time_.store(0, std::memory_order_relaxed);
    }
    ~scheduler_dynamic_tmpl_t();

private:
    using sched_type_t = scheduler_tmpl_t<RecordType, ReaderType>;
    using input_ordinal_t = typename sched_type_t::input_ordinal_t;
    using output_ordinal_t = typename sched_type_t::output_ordinal_t;
    using scheduler_status_t = typename sched_type_t::scheduler_status_t;
    using stream_status_t = typename sched_type_t::stream_status_t;
    using typename scheduler_impl_tmpl_t<RecordType, ReaderType>::input_info_t;
    using typename scheduler_impl_tmpl_t<RecordType, ReaderType>::output_info_t;
    using typename scheduler_impl_tmpl_t<RecordType, ReaderType>::workload_info_t;
    using typename scheduler_impl_tmpl_t<RecordType, ReaderType>::schedule_record_t;
    using
        typename scheduler_impl_tmpl_t<RecordType, ReaderType>::InputTimestampComparator;
    using typename scheduler_impl_tmpl_t<RecordType, ReaderType>::workload_tid_t;
    using typename scheduler_impl_tmpl_t<RecordType, ReaderType>::input_queue_t;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::options_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::outputs_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::inputs_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::workloads_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::error_string_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::set_cur_input;
    using scheduler_impl_tmpl_t<RecordType,
                                ReaderType>::acquire_scoped_output_lock_if_necessary;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::get_output_time;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::scale_blocked_time;

protected:
    scheduler_status_t
    set_initial_schedule() override;

    stream_status_t
    swap_out_input(output_ordinal_t output, input_ordinal_t input,
                   bool caller_holds_input_lock) override;
    stream_status_t
    swap_in_input(output_ordinal_t output, input_ordinal_t input) override;

    stream_status_t
    pick_next_input_for_mode(output_ordinal_t output, uint64_t blocked_time,
                             input_ordinal_t prev_index, input_ordinal_t &index) override;

    stream_status_t
    check_for_input_switch(output_ordinal_t output, RecordType &record,
                           input_info_t *input, uint64_t cur_time, bool &need_new_input,
                           bool &preempt, uint64_t &blocked_time) override;

    stream_status_t
    eof_or_idle_for_mode(output_ordinal_t output, input_ordinal_t prev_input) override;

    stream_status_t
    set_output_active(output_ordinal_t output, bool active) override;

    // The input's lock must be held by the caller.
    // Returns a multiplier for how long the input should be considered blocked.
    bool
    syscall_incurs_switch(input_info_t *input, uint64_t &blocked_time);

    // Process each marker seen during next_record().
    // The input's lock must be held by the caller.
    // Virtual to allow further subclasses to customize behavior here.
    virtual void
    process_marker(input_info_t &input, output_ordinal_t output,
                   trace_marker_type_t marker_type, uintptr_t marker_value);

    stream_status_t
    rebalance_queues(output_ordinal_t triggering_output,
                     std::vector<input_ordinal_t> inputs_to_add);

    bool
    ready_queue_empty(output_ordinal_t output);

    // If input->unscheduled is true and input->blocked_time is 0, input
    // is placed on the unscheduled_priority_ queue instead.
    // The caller cannot hold the input's lock: this routine will acquire it.
    void
    add_to_ready_queue(output_ordinal_t output, input_info_t *input);

    // Identical to add_to_ready_queue() except the output's lock must be held by the
    // caller.
    // The caller must also hold the input's lock.
    void
    add_to_ready_queue_hold_locks(output_ordinal_t output, input_info_t *input);

    // The caller must hold the input's lock.
    void
    add_to_unscheduled_queue(input_info_t *input);

    // "for_output" is which output stream is looking for a new input; only an
    // input which is able to run on that output will be selected.
    // for_output can be INVALID_OUTPUT_ORDINAL, which will ignore bindings.
    // If from_output != for_output (including for_output == INVALID_OUTPUT_ORDINAL)
    // this is a migration and only migration-ready inputs will be picked.
    stream_status_t
    pop_from_ready_queue(output_ordinal_t from_output, output_ordinal_t for_output,
                         input_info_t *&new_input);

    // Identical to pop_from_ready_queue but the caller must hold both output locks.
    stream_status_t
    pop_from_ready_queue_hold_locks(output_ordinal_t from_output,
                                    output_ordinal_t for_output, input_info_t *&new_input,
                                    bool from_back = false);

    // Up to the caller to check verbosity before calling.
    void
    print_queue_stats();

    // Rebalancing coordination.
    std::atomic<std::thread::id> rebalancer_;
    std::atomic<uint64_t> last_rebalance_time_;
    // This lock protects unscheduled_priority_ and unscheduled_counter_.
    // It should be acquired *after* both output or input locks: it is narrowmost.
    mutex_dbg_owned unsched_lock_;
    // Inputs that are unscheduled indefinitely until directly targeted.
    input_queue_t unscheduled_priority_;
};

// Specialized code for replaying schedules: either a recorded dynamic schedule
// or an as-traced schedule.
template <typename RecordType, typename ReaderType>
class scheduler_replay_tmpl_t : public scheduler_impl_tmpl_t<RecordType, ReaderType> {
private:
    using sched_type_t = scheduler_tmpl_t<RecordType, ReaderType>;
    using input_ordinal_t = typename sched_type_t::input_ordinal_t;
    using output_ordinal_t = typename sched_type_t::output_ordinal_t;
    using scheduler_status_t = typename sched_type_t::scheduler_status_t;
    using stream_status_t = typename sched_type_t::stream_status_t;
    using typename scheduler_impl_tmpl_t<RecordType, ReaderType>::schedule_record_t;
    using typename scheduler_impl_tmpl_t<RecordType, ReaderType>::input_info_t;
    using
        typename scheduler_impl_tmpl_t<RecordType, ReaderType>::schedule_output_tracker_t;
    using
        typename scheduler_impl_tmpl_t<RecordType, ReaderType>::schedule_input_tracker_t;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::options_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::outputs_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::inputs_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::error_string_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::set_cur_input;

protected:
    scheduler_status_t
    set_initial_schedule() override;

    stream_status_t
    swap_out_input(output_ordinal_t output, input_ordinal_t input,
                   bool caller_holds_input_lock) override;
    stream_status_t
    swap_in_input(output_ordinal_t output, input_ordinal_t input) override;

    stream_status_t
    pick_next_input_for_mode(output_ordinal_t output, uint64_t blocked_time,
                             input_ordinal_t prev_index, input_ordinal_t &index) override;

    stream_status_t
    check_for_input_switch(output_ordinal_t output, RecordType &record,
                           input_info_t *input, uint64_t cur_time, bool &need_new_input,
                           bool &preempt, uint64_t &blocked_time) override;

    stream_status_t
    eof_or_idle_for_mode(output_ordinal_t output, input_ordinal_t prev_input) override;

    scheduler_status_t
    read_recorded_schedule();

    scheduler_status_t
    read_and_instantiate_traced_schedule();
};

// Specialized code for fixed "schedules": typically serial or parallel analyzer
// modes.
template <typename RecordType, typename ReaderType>
class scheduler_fixed_tmpl_t : public scheduler_impl_tmpl_t<RecordType, ReaderType> {
private:
    using sched_type_t = scheduler_tmpl_t<RecordType, ReaderType>;
    using input_ordinal_t = typename sched_type_t::input_ordinal_t;
    using output_ordinal_t = typename sched_type_t::output_ordinal_t;
    using scheduler_status_t = typename sched_type_t::scheduler_status_t;
    using stream_status_t = typename sched_type_t::stream_status_t;
    using typename scheduler_impl_tmpl_t<RecordType, ReaderType>::input_info_t;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::options_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::outputs_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::inputs_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::error_string_;
    using scheduler_impl_tmpl_t<RecordType, ReaderType>::set_cur_input;

protected:
    scheduler_status_t
    set_initial_schedule() override;

    stream_status_t
    swap_out_input(output_ordinal_t output, input_ordinal_t input,
                   bool caller_holds_input_lock) override;
    stream_status_t
    swap_in_input(output_ordinal_t output, input_ordinal_t input) override;

    stream_status_t
    pick_next_input_for_mode(output_ordinal_t output, uint64_t blocked_time,
                             input_ordinal_t prev_index, input_ordinal_t &index) override;

    stream_status_t
    check_for_input_switch(output_ordinal_t output, RecordType &record,
                           input_info_t *input, uint64_t cur_time, bool &need_new_input,
                           bool &preempt, uint64_t &blocked_time) override;
    stream_status_t
    eof_or_idle_for_mode(output_ordinal_t output, input_ordinal_t prev_input) override;
};

/* For testing, where schedule_record_t is not accessible. */
class replay_file_checker_t {
public:
    std::string
    check(archive_istream_t *infile);
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DRMEMTRACE_SCHEDULER_IMPL_H_ */
