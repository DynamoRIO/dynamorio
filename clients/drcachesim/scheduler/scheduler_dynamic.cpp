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

/* Scheduler dynamic rescheduling-specific code. */

#include "scheduler.h"
#include "scheduler_impl.h"

#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <mutex>
#include <thread>

#include "memref.h"
#include "memtrace_stream.h"
#include "mutex_dbg_owned.h"
#include "reader.h"
#include "record_file_reader.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::set_initial_schedule(
    std::unordered_map<int, std::vector<int>> &workload2inputs)
{
    if (options_.mapping != sched_type_t::MAP_TO_ANY_OUTPUT)
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    // Assign initial inputs.
    if (options_.deps == sched_type_t::DEPENDENCY_TIMESTAMPS) {
        // Compute the min timestamp (==base_timestamp) per workload and sort
        // all inputs by relative time from the base.
        for (int workload_idx = 0;
             workload_idx < static_cast<int>(workload2inputs.size()); ++workload_idx) {
            uint64_t min_time = std::numeric_limits<uint64_t>::max();
            input_ordinal_t min_input = -1;
            for (int input_idx : workload2inputs[workload_idx]) {
                if (inputs_[input_idx].next_timestamp < min_time) {
                    min_time = inputs_[input_idx].next_timestamp;
                    min_input = input_idx;
                }
            }
            if (min_input < 0)
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            for (int input_idx : workload2inputs[workload_idx]) {
                VPRINT(this, 4,
                       "workload %d: setting input %d base_timestamp to %" PRIu64
                       " vs next_timestamp %zu\n",
                       workload_idx, input_idx, min_time,
                       inputs_[input_idx].next_timestamp);
                inputs_[input_idx].base_timestamp = min_time;
                inputs_[input_idx].order_by_timestamp = true;
            }
        }
        // We'll pick the starting inputs below by sorting by relative time from
        // each workload's base_timestamp, which our queue does for us.
    }
    // First, put all inputs into a temporary queue to sort by priority and
    // time for us.
    flexible_queue_t<input_info_t *, InputTimestampComparator> allq;
    for (int i = 0; i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
        inputs_[i].queue_counter = i;
        allq.push(&inputs_[i]);
    }
    // Now assign round-robin to the outputs.  We have to obey bindings here: we
    // just take the first.  This isn't guaranteed to be perfect if there are
    // many bindings, but we run a rebalancing afterward.
    output_ordinal_t output = 0;
    while (!allq.empty()) {
        input_info_t *input = allq.top();
        allq.pop();
        output_ordinal_t target = output;
        if (!input->binding.empty())
            target = *input->binding.begin();
        else
            output = (output + 1) % outputs_.size();
        this->add_to_ready_queue(target, input);
    }
    stream_status_t status = rebalance_queues(0, {});
    if (status != sched_type_t::STATUS_OK) {
        VPRINT(this, 0, "Failed to rebalance with status %d\n", status);
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    for (int i = 0; i < static_cast<output_ordinal_t>(outputs_.size()); ++i) {
        input_info_t *queue_next;
#ifndef NDEBUG
        status =
#endif
            this->pop_from_ready_queue(i, i, queue_next);
        assert(status == sched_type_t::STATUS_OK || status == sched_type_t::STATUS_IDLE);
        if (queue_next == nullptr)
            set_cur_input(i, sched_type_t::INVALID_INPUT_ORDINAL);
        else
            set_cur_input(i, queue_next->index);
    }
    VPRINT(this, 2, "Initial queues:\n");
    VDO(this, 2, { this->print_queue_stats(); });

    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::set_output_active(
    output_ordinal_t output, bool active)
{
    if (outputs_[output].active->load(std::memory_order_acquire) == active)
        return sched_type_t::STATUS_OK;
    outputs_[output].active->store(active, std::memory_order_release);
    VPRINT(this, 2, "Output stream %d is now %s\n", output,
           active ? "active" : "inactive");
    std::vector<input_ordinal_t> ordinals;
    if (!active) {
        // Make the now-inactive output's input available for other cores.
        // This will reset its quantum too.
        // We aren't switching on a just-read instruction not passed to the consumer,
        // if the queue is empty.
        input_ordinal_t cur_input = outputs_[output].cur_input;
        if (cur_input != sched_type_t::INVALID_INPUT_ORDINAL) {
            if (inputs_[cur_input].queue.empty())
                inputs_[cur_input].switching_pre_instruction = true;
            set_cur_input(output, sched_type_t::INVALID_INPUT_ORDINAL);
        }
        // Move the ready_queue to other outputs.
        {
            auto lock = this->acquire_scoped_output_lock_if_necessary(output);
            while (!outputs_[output].ready_queue.queue.empty()) {
                input_info_t *tomove = outputs_[output].ready_queue.queue.top();
                ordinals.push_back(tomove->index);
                outputs_[output].ready_queue.queue.pop();
            }
        }
    } else {
        outputs_[output].waiting = true;
    }
    return rebalance_queues(output, ordinals);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::pick_next_input_for_mode(
    output_ordinal_t output, uint64_t blocked_time, input_ordinal_t prev_index,
    input_ordinal_t &index)
{
    uint64_t cur_time = this->get_output_time(output);
    uint64_t last_time = last_rebalance_time_.load(std::memory_order_acquire);
    if (last_time == 0) {
        // Initialize.
        last_rebalance_time_.store(cur_time, std::memory_order_release);
    } else {
        // Guard against time going backward, which happens: i#6966.
        if (cur_time > last_time &&
            cur_time - last_time >= static_cast<uint64_t>(options_.rebalance_period_us *
                                                          options_.time_units_per_us) &&
            rebalancer_.load(std::memory_order_acquire) == std::thread::id()) {
            VPRINT(this, 2,
                   "Output %d hit rebalance period @%" PRIu64 " (last rebalance @%" PRIu64
                   ")\n",
                   output, cur_time, last_time);
            stream_status_t status = this->rebalance_queues(output, {});
            if (status != sched_type_t::STATUS_OK)
                return status;
        }
    }
    if (blocked_time > 0 && prev_index != sched_type_t::INVALID_INPUT_ORDINAL) {
        std::lock_guard<mutex_dbg_owned> lock(*inputs_[prev_index].lock);
        if (inputs_[prev_index].blocked_time == 0) {
            VPRINT(this, 2, "next_record[%d]: blocked time %" PRIu64 "\n", output,
                   blocked_time);
            inputs_[prev_index].blocked_time = blocked_time;
            inputs_[prev_index].blocked_start_time = this->get_output_time(output);
        }
    }
    if (prev_index != sched_type_t::INVALID_INPUT_ORDINAL &&
        inputs_[prev_index].switch_to_input != sched_type_t::INVALID_INPUT_ORDINAL) {
        input_info_t *target = &inputs_[inputs_[prev_index].switch_to_input];
        inputs_[prev_index].switch_to_input = sched_type_t::INVALID_INPUT_ORDINAL;
        std::unique_lock<mutex_dbg_owned> target_input_lock(*target->lock);
        // XXX i#5843: Add an invariant check that the next timestamp of the
        // target is later than the pre-switch-syscall timestamp?
        if (target->containing_output != sched_type_t::INVALID_OUTPUT_ORDINAL) {
            output_ordinal_t target_output = target->containing_output;
            output_info_t &out = outputs_[target->containing_output];
            // We cannot hold an input lock when we acquire an output lock.
            target_input_lock.unlock();
            {
                auto target_output_lock =
                    this->acquire_scoped_output_lock_if_necessary(target_output);
                target_input_lock.lock();
                if (out.ready_queue.queue.find(target)) {
                    VPRINT(this, 2,
                           "next_record[%d]: direct switch from input %d to "
                           "input %d "
                           "@%" PRIu64 "\n",
                           output, prev_index, target->index,
                           inputs_[prev_index].reader->get_last_timestamp());
                    out.ready_queue.queue.erase(target);
                    index = target->index;
                    // Erase any remaining wait time for the target.
                    if (target->blocked_time > 0) {
                        VPRINT(this, 3,
                               "next_record[%d]: direct switch erasing "
                               "blocked time "
                               "for input %d\n",
                               output, target->index);
                        --out.ready_queue.num_blocked;
                        target->blocked_time = 0;
                        target->unscheduled = false;
                    }
                    if (target->containing_output != output) {
                        ++outputs_[output]
                              .stats[memtrace_stream_t::SCHED_STAT_MIGRATIONS];
                    }
                    ++outputs_[output]
                          .stats[memtrace_stream_t::SCHED_STAT_DIRECT_SWITCH_SUCCESSES];
                } // Else, actively running.
                target_input_lock.unlock();
            }
            target_input_lock.lock();
        }
        std::lock_guard<mutex_dbg_owned> unsched_lock(*this->unscheduled_priority_.lock);
        if (index == sched_type_t::INVALID_INPUT_ORDINAL &&
            this->unscheduled_priority_.queue.find(target)) {
            target->unscheduled = false;
            this->unscheduled_priority_.queue.erase(target);
            index = target->index;
            VPRINT(this, 2,
                   "next_record[%d]: direct switch from input %d to "
                   "was-unscheduled input %d "
                   "@%" PRIu64 "\n",
                   output, prev_index, target->index,
                   inputs_[prev_index].reader->get_last_timestamp());
            if (target->prev_output != sched_type_t::INVALID_OUTPUT_ORDINAL &&
                target->prev_output != output) {
                ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_MIGRATIONS];
            }
            ++outputs_[output]
                  .stats[memtrace_stream_t::SCHED_STAT_DIRECT_SWITCH_SUCCESSES];
        }
        if (index == sched_type_t::INVALID_INPUT_ORDINAL) {
            // We assume that inter-input dependencies are captured in
            // the _DIRECT_THREAD_SWITCH, _UNSCHEDULE, and _SCHEDULE markers
            // and that if a switch request targets a thread running elsewhere
            // that means there isn't a dependence and this is really a
            // dynamic switch to whoever happens to be available (and
            // different timing between tracing and analysis has caused this
            // miss).
            VPRINT(this, 2,
                   "Direct switch (from %d) target input #%d is running "
                   "elsewhere; picking a different target @%" PRIu64 "\n",
                   prev_index, target->index,
                   inputs_[prev_index].reader->get_last_timestamp());
            // We do ensure the missed target doesn't wait indefinitely.
            // XXX i#6822: It's not clear this is always the right thing to
            // do.
            target->skip_next_unscheduled = true;
        }
    }
    if (index != sched_type_t::INVALID_INPUT_ORDINAL) {
        // We found a direct switch target above.
    }
    // XXX: We're grabbing the output ready_queue lock 3x here:
    // ready_queue_empty(), set_cur_input()'s add_to_ready_queue(),
    // and pop_from_ready_queue().  We could call versions of those
    // that let the caller hold the lock: but holding it across other
    // calls in between here adds complexity.
    else if (this->ready_queue_empty(output) && blocked_time == 0) {
        // There's nothing else to run so either stick with the
        // current input or if it's invalid go idle/eof.
        if (prev_index == sched_type_t::INVALID_INPUT_ORDINAL) {
            stream_status_t status = this->eof_or_idle(output, prev_index);
            if (status != sched_type_t::STATUS_STOLE)
                return status;
            // eof_or_idle stole an input for us, now in .cur_input.
            index = outputs_[output].cur_input;
            return sched_type_t::STATUS_OK;
        } else {
            auto lock = std::unique_lock<mutex_dbg_owned>(*inputs_[prev_index].lock);
            // If we can't go back to the current input because it's EOF
            // or unscheduled indefinitely (we already checked blocked_time
            // above: it's 0 here), this output is either idle or EOF.
            if (inputs_[prev_index].at_eof || inputs_[prev_index].unscheduled) {
                lock.unlock();
                stream_status_t status = this->eof_or_idle(output, prev_index);
                if (status != sched_type_t::STATUS_STOLE)
                    return status;
                index = outputs_[output].cur_input;
                return sched_type_t::STATUS_OK;
            } else
                index = prev_index; // Go back to prior.
        }
    } else {
        // There's something else to run, or we'll soon be in the queue
        // even if it's empty now.
        // Give up the input before we go to the queue so we can add
        // ourselves to the queue.  If we're the highest priority we
        // shouldn't switch.  The queue preserves FIFO for same-priority
        // cases so we will switch if someone of equal priority is
        // waiting.
        set_cur_input(output, sched_type_t::INVALID_INPUT_ORDINAL);
        input_info_t *queue_next = nullptr;
        stream_status_t status = this->pop_from_ready_queue(output, output, queue_next);
        if (status != sched_type_t::STATUS_OK) {
            if (status == sched_type_t::STATUS_IDLE) {
                outputs_[output].waiting = true;
                if (options_.schedule_record_ostream != nullptr) {
                    stream_status_t record_status = this->record_schedule_segment(
                        output, schedule_record_t::IDLE_BY_COUNT, 0,
                        // Start prior to this idle.
                        outputs_[output].idle_count - 1, 0);
                    if (record_status != sched_type_t::STATUS_OK)
                        return record_status;
                }
                if (prev_index != sched_type_t::INVALID_INPUT_ORDINAL) {
                    ++outputs_[output]
                          .stats[memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_IDLE];
                }
            }
            return status;
        }
        if (queue_next == nullptr) {
            status = this->eof_or_idle(output, prev_index);
            if (status != sched_type_t::STATUS_STOLE)
                return status;
            index = outputs_[output].cur_input;
            return sched_type_t::STATUS_OK;
        } else
            index = queue_next->index;
    }
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::check_for_input_switch(
    output_ordinal_t output, RecordType &record, input_info_t *input, uint64_t cur_time,
    bool &need_new_input, bool &preempt, uint64_t &blocked_time)
{
    trace_marker_type_t marker_type;
    uintptr_t marker_value;
    // While regular traces typically always have a syscall marker when
    // there's a maybe-blocking marker, some tests and synthetic traces have
    // just the maybe so we check both.
    if (input->processing_syscall || input->processing_maybe_blocking_syscall) {
        // Wait until we're past all the markers associated with the syscall.
        // XXX: We may prefer to stop before the return value marker for
        // futex, or a kernel xfer marker, but our recorded format is on instr
        // boundaries so we live with those being before the switch.
        // XXX: Once we insert kernel traces, we may have to try harder
        // to stop before the post-syscall records.
        if (this->record_type_is_instr_boundary(record, outputs_[output].last_record)) {
            if (input->switch_to_input != sched_type_t::INVALID_INPUT_ORDINAL) {
                // The switch request overrides any latency threshold.
                need_new_input = true;
                VPRINT(this, 3,
                       "next_record[%d]: direct switch on low-latency "
                       "syscall in "
                       "input %d\n",
                       output, input->index);
            } else if (input->blocked_time > 0) {
                // If we've found out another way that this input should
                // block, use that time and do a switch.
                need_new_input = true;
                blocked_time = input->blocked_time;
                VPRINT(this, 3, "next_record[%d]: blocked time set for input %d\n",
                       output, input->index);
            } else if (input->unscheduled) {
                need_new_input = true;
                VPRINT(this, 3, "next_record[%d]: input %d going unscheduled\n", output,
                       input->index);
            } else if (this->syscall_incurs_switch(input, blocked_time)) {
                // Model as blocking and should switch to a different input.
                need_new_input = true;
                VPRINT(this, 3, "next_record[%d]: hit blocking syscall in input %d\n",
                       output, input->index);
            }
            input->processing_syscall = false;
            input->processing_maybe_blocking_syscall = false;
            input->pre_syscall_timestamp = 0;
            input->syscall_timeout_arg = 0;
        }
    }
    if (outputs_[output].hit_switch_code_end) {
        // We have to delay so the end marker is still in_context_switch_code.
        outputs_[output].in_context_switch_code = false;
        outputs_[output].hit_switch_code_end = false;
        // We're now back "on the clock".
        if (options_.quantum_unit == sched_type_t::QUANTUM_TIME)
            input->prev_time_in_quantum = cur_time;
        // XXX: If we add a skip feature triggered on the output stream,
        // we'll want to make sure skipping while in these switch and kernel
        // sequences is handled correctly.
    }
    if (this->record_type_is_marker(record, marker_type, marker_value)) {
        this->process_marker(*input, output, marker_type, marker_value);
    }
    if (options_.quantum_unit == sched_type_t::QUANTUM_INSTRUCTIONS &&
        this->record_type_is_instr_boundary(record, outputs_[output].last_record) &&
        !outputs_[output].in_kernel_code) {
        ++input->instrs_in_quantum;
        if (input->instrs_in_quantum > options_.quantum_duration_instrs) {
            // We again prefer to switch to another input even if the current
            // input has the oldest timestamp, prioritizing context switches
            // over timestamp ordering.
            VPRINT(this, 4, "next_record[%d]: input %d hit end of instr quantum\n",
                   output, input->index);
            preempt = true;
            need_new_input = true;
            input->instrs_in_quantum = 0;
            ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_QUANTUM_PREEMPTS];
        }
    } else if (options_.quantum_unit == sched_type_t::QUANTUM_TIME) {
        if (cur_time == 0 || cur_time < input->prev_time_in_quantum) {
            VPRINT(this, 1,
                   "next_record[%d]: invalid time %" PRIu64 " vs start %" PRIu64 "\n",
                   output, cur_time, input->prev_time_in_quantum);
            return sched_type_t::STATUS_INVALID;
        }
        input->time_spent_in_quantum += cur_time - input->prev_time_in_quantum;
        input->prev_time_in_quantum = cur_time;
        double elapsed_micros = static_cast<double>(input->time_spent_in_quantum) /
            options_.time_units_per_us;
        if (elapsed_micros >= options_.quantum_duration_us &&
            // We only switch on instruction boundaries.  We could possibly switch
            // in between (e.g., scatter/gather long sequence of reads/writes) by
            // setting input->switching_pre_instruction.
            this->record_type_is_instr_boundary(record, outputs_[output].last_record)) {
            VPRINT(this, 4,
                   "next_record[%d]: input %d hit end of time quantum after %" PRIu64
                   "\n",
                   output, input->index, input->time_spent_in_quantum);
            preempt = true;
            need_new_input = true;
            input->time_spent_in_quantum = 0;
            ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_QUANTUM_PREEMPTS];
        }
    }
    // For sched_type_t::DEPENDENCY_TIMESTAMPS: enforcing asked-for
    // context switch rates is more important that honoring precise
    // trace-buffer-based timestamp inter-input dependencies so we do not end a
    // quantum early due purely to timestamps.

    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::rebalance_queues(
    output_ordinal_t triggering_output, std::vector<input_ordinal_t> inputs_to_add)
{
    std::thread::id nobody;
    if (!rebalancer_.compare_exchange_weak(nobody, std::this_thread::get_id(),
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
        // Someone else is rebalancing.
        return sched_type_t::STATUS_OK;
    }
    stream_status_t status = sched_type_t::STATUS_OK;
    assert(options_.mapping == sched_type_t::MAP_TO_ANY_OUTPUT);
    VPRINT(this, 1, "Output %d triggered a rebalance @%" PRIu64 ":\n", triggering_output,
           this->get_output_time(triggering_output));
    // First, update the time to avoid more threads coming here.
    last_rebalance_time_.store(this->get_output_time(triggering_output),
                               std::memory_order_release);
    VPRINT(this, 2, "Before rebalance:\n");
    VDO(this, 2, { this->print_queue_stats(); });
    ++outputs_[triggering_output]
          .stats[memtrace_stream_t::SCHED_STAT_RUNQUEUE_REBALANCES];

    // Workaround to avoid hangs when _SCHEDULE and/or _DIRECT_THREAD_SWITCH
    // directives miss their targets (due to running with a subset of the
    // original threads, or other scenarios) and we end up with no scheduled
    // inputs but a set of unscheduled inputs who will never be scheduled.
    // TODO i#6959: Just exit early instead, maybe under a flag.
    // It would help to see what % of total records we've processed.
    size_t unsched_size = 0;
    {
        std::lock_guard<mutex_dbg_owned> unsched_lock(*unscheduled_priority_.lock);
        unsched_size = unscheduled_priority_.queue.size();
    }
    if (this->live_input_count_.load(std::memory_order_acquire) ==
        static_cast<int>(unsched_size)) {
        VPRINT(
            this, 1,
            "rebalancing moving entire unscheduled queue (%zu entries) to ready_queues\n",
            unsched_size);
        {
            std::lock_guard<mutex_dbg_owned> unsched_lock(*unscheduled_priority_.lock);
            while (!unscheduled_priority_.queue.empty()) {
                input_info_t *tomove = unscheduled_priority_.queue.top();
                inputs_to_add.push_back(tomove->index);
                unscheduled_priority_.queue.pop();
            }
        }
        for (input_ordinal_t input : inputs_to_add) {
            std::lock_guard<mutex_dbg_owned> lock(*inputs_[input].lock);
            inputs_[input].unscheduled = false;
        }
    }

    int live_inputs = this->live_input_count_.load(std::memory_order_acquire);
    int live_outputs = 0;
    for (unsigned int i = 0; i < outputs_.size(); ++i) {
        if (outputs_[i].active->load(std::memory_order_acquire))
            ++live_outputs;
    }
    double avg_per_output = live_inputs / static_cast<double>(live_outputs);
    unsigned int avg_ceiling = static_cast<int>(std::ceil(avg_per_output));
    unsigned int avg_floor = static_cast<int>(std::floor(avg_per_output));
    int iteration = 0;
    do {
        // Walk the outputs, filling too-short queues from inputs_to_add and
        // shrinking too-long queues into inputs_to_add.  We may need a 2nd pass
        // for this; and a 3rd pass if bindings prevent even splitting.
        VPRINT(
            this, 3,
            "Rebalance iteration %d inputs_to_add size=%zu avg_per_output=%4.1f %d-%d\n",
            iteration, inputs_to_add.size(), avg_per_output, avg_floor, avg_ceiling);
        // We're giving up the output locks as we go, so there may be some stealing
        // in the middle of our operation, but the rebalancing is approximate anyway.
        for (unsigned int i = 0; i < outputs_.size(); ++i) {
            if (!outputs_[i].active->load(std::memory_order_acquire))
                continue;
            auto lock = this->acquire_scoped_output_lock_if_necessary(i);
            // Only remove on the 1st iteration; later we can exceed due to binding
            // constraints.
            while (iteration == 0 && outputs_[i].ready_queue.queue.size() > avg_ceiling) {
                input_info_t *queue_next = nullptr;
                // We use our regular pop_from_ready_queue which means we leave
                // blocked inputs on the queue: those do not get rebalanced.
                // XXX: Should we revisit that?
                //
                // We remove from the back to avoid penalizing the next-to-run entries
                // at the front of the queue by putting them at the back of another
                // queue.
                status = this->pop_from_ready_queue_hold_locks(
                    i, sched_type_t::INVALID_OUTPUT_ORDINAL, queue_next,
                    /*from_back=*/true);
                if (status == sched_type_t::STATUS_OK && queue_next != nullptr) {
                    VPRINT(this, 3,
                           "Rebalance iteration %d: output %d giving up input %d\n",
                           iteration, i, queue_next->index);
                    inputs_to_add.push_back(queue_next->index);
                } else
                    break;
            }
            std::vector<input_ordinal_t> incompatible_inputs;
            // If we reach the 3rd iteration, we have fussy inputs with bindings.
            // Try to add them to every output.
            while (
                (outputs_[i].ready_queue.queue.size() < avg_ceiling || iteration > 1) &&
                !inputs_to_add.empty()) {
                input_ordinal_t ordinal = inputs_to_add.back();
                inputs_to_add.pop_back();
                input_info_t &input = inputs_[ordinal];
                std::lock_guard<mutex_dbg_owned> input_lock(*input.lock);
                if (input.binding.empty() ||
                    input.binding.find(i) != input.binding.end()) {
                    VPRINT(this, 3, "Rebalance iteration %d: output %d taking input %d\n",
                           iteration, i, ordinal);
                    this->add_to_ready_queue_hold_locks(i, &input);
                } else {
                    incompatible_inputs.push_back(ordinal);
                }
            }
            inputs_to_add.insert(inputs_to_add.end(), incompatible_inputs.begin(),
                                 incompatible_inputs.end());
        }
        ++iteration;
        if (iteration >= 3 && !inputs_to_add.empty()) {
            // This is possible with bindings limited to inactive outputs.
            // XXX: Rather than return an error, we could add to the unscheduled queue,
            // but do not mark the input unscheduled.  Then when an output is
            // marked active, we could walk the unscheduled queue and take
            // inputs not marked unscheduled.
            VPRINT(this, 1, "Rebalance hit impossible binding\n");
            status = sched_type_t::STATUS_IMPOSSIBLE_BINDING;
            break;
        }
    } while (!inputs_to_add.empty());
    VPRINT(this, 2, "After:\n");
    VDO(this, 2, { this->print_queue_stats(); });
    rebalancer_.store(std::thread::id(), std::memory_order_release);
    return status;
}

template class scheduler_dynamic_tmpl_t<memref_t, reader_t>;
template class scheduler_dynamic_tmpl_t<trace_entry_t,
                                        dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
