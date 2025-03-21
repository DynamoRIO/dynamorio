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

/* Scheduler dynamic rescheduling-specific code. */

#include "scheduler.h"
#include "scheduler_impl.h"

#include <atomic>
#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "flexible_queue.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "mutex_dbg_owned.h"
#include "reader.h"
#include "record_file_reader.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

template <typename RecordType, typename ReaderType>
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::~scheduler_dynamic_tmpl_t()
{
#ifndef NDEBUG
    VPRINT(this, 1, "%-37s: %9" PRId64 "\n", "Unscheduled queue lock acquired",
           unscheduled_priority_.lock->get_count_acquired());
    VPRINT(this, 1, "%-37s: %9" PRId64 "\n", "Unscheduled queue lock contended",
           unscheduled_priority_.lock->get_count_contended());
#endif
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::set_initial_schedule()
{
    if (options_.mapping != sched_type_t::MAP_TO_ANY_OUTPUT)
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    // Assign initial inputs.
    if (options_.deps == sched_type_t::DEPENDENCY_TIMESTAMPS) {
        // Compute the min timestamp (==base_timestamp) per workload and sort
        // all inputs by relative time from the base.
        for (int workload_idx = 0; workload_idx < static_cast<int>(workloads_.size());
             ++workload_idx) {
            uint64_t min_time = std::numeric_limits<uint64_t>::max();
            input_ordinal_t min_input = -1;
            for (int input_idx : workloads_[workload_idx].inputs) {
                if (inputs_[input_idx].next_timestamp < min_time) {
                    min_time = inputs_[input_idx].next_timestamp;
                    min_input = input_idx;
                }
            }
            if (min_input < 0)
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            for (int input_idx : workloads_[workload_idx].inputs) {
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
    // many bindings (or output limits), but we run a rebalancing afterward
    // (to construct it up front would take similar code to the rebalance so we
    // leverage that code).
    output_ordinal_t output = 0;
    while (!allq.empty()) {
        input_info_t *input = allq.top();
        allq.pop();
        output_ordinal_t target = output;
        if (!input->binding.empty())
            target = *input->binding.begin();
        else
            output = (output + 1) % outputs_.size();
        add_to_ready_queue(target, input);
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
            pop_from_ready_queue(i, i, queue_next);
        assert(status == sched_type_t::STATUS_OK || status == sched_type_t::STATUS_IDLE);
        if (queue_next == nullptr) {
            // Try to steal, as the initial round-robin layout and rebalancing ignores
            // output_limit and other factors.
            status = eof_or_idle_for_mode(i, sched_type_t::INVALID_INPUT_ORDINAL);
            if (status != sched_type_t::STATUS_STOLE)
                set_cur_input(i, sched_type_t::INVALID_INPUT_ORDINAL);
        } else
            set_cur_input(i, queue_next->index);
    }
    VPRINT(this, 2, "Initial queues:\n");
    VDO(this, 2, { this->print_queue_stats(); });

    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::swap_out_input(
    output_ordinal_t output, input_ordinal_t input, bool caller_holds_input_lock)
{
    // We disallow the caller holding the input lock as that precludes our call to
    // add_to_ready_queue().
    assert(!caller_holds_input_lock);
    if (input == sched_type_t::INVALID_INPUT_ORDINAL)
        return sched_type_t::STATUS_OK;
    bool at_eof = false;
    int workload = -1;
    {
        std::lock_guard<mutex_dbg_owned> lock(*inputs_[input].lock);
        at_eof = inputs_[input].at_eof;
        assert(inputs_[input].cur_output == sched_type_t::INVALID_OUTPUT_ORDINAL);
        workload = inputs_[input].workload;
    }
    // Now that the caller has updated the outgoing input's fields (we assert that
    // cur_output was changed above), add it to the ready queue (once on the queue others
    // can see it and pop it off).
    if (!at_eof) {
        add_to_ready_queue(output, &inputs_[input]);
    }
    if (workloads_[workload].output_limit > 0)
        workloads_[workload].live_output_count->fetch_add(-1, std::memory_order_release);
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::swap_in_input(output_ordinal_t output,
                                                                input_ordinal_t input)
{
    if (input == sched_type_t::INVALID_INPUT_ORDINAL)
        return sched_type_t::STATUS_OK;
    workload_info_t &workload = workloads_[inputs_[input].workload];
    if (workload.output_limit > 0)
        workload.live_output_count->fetch_add(1, std::memory_order_release);
    return sched_type_t::STATUS_OK;
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
            auto lock = acquire_scoped_output_lock_if_necessary(output);
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
    VDO(this, 1, {
        static int64_t global_heartbeat;
        // 10K is too frequent for simple analyzer runs: it is too noisy with
        // the new core-sharded-by-default for new users using defaults.
        // Even 50K is too frequent on the threadsig checked-in trace.
        // 500K is a reasonable compromise.
        // XXX: Add a runtime option to tweak this.
        static constexpr int64_t GLOBAL_HEARTBEAT_CADENCE = 500000;
        // We are ok with races as the cadence is approximate.
        if (++global_heartbeat % GLOBAL_HEARTBEAT_CADENCE == 0) {
            print_queue_stats();
        }
    });

    uint64_t cur_time = get_output_time(output);
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
            inputs_[prev_index].blocked_start_time = get_output_time(output);
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
                    acquire_scoped_output_lock_if_necessary(target_output);
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
        stream_status_t status = pop_from_ready_queue(output, output, queue_next);
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
        if (this->record_type_is_instr_boundary(record, outputs_[output].last_record) &&
            // We want to delay the context switch until after the injected syscall trace.
            !outputs_[output].in_syscall_code) {
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
            } else if (syscall_incurs_switch(input, blocked_time)) {
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
        !outputs_[output].in_context_switch_code) {
        ++input->instrs_in_quantum;
        if (input->instrs_in_quantum > options_.quantum_duration_instrs) {
            if (outputs_[output].in_syscall_code) {
                // XXX: Maybe this should be printed only once per-syscall-instance to
                // reduce log spam.
                VPRINT(this, 5,
                       "next_record[%d]: input %d delaying context switch "
                       "after end of instr quantum due to syscall trace\n",
                       output, input->index);

            } else {
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
            if (outputs_[output].in_syscall_code) {
                // XXX: Maybe this should be printed only once per-syscall-instance to
                // reduce log spam.
                VPRINT(this, 5,
                       "next_record[%d]: input %d delaying context switch after end of "
                       "time quantum after %" PRIu64 " due to syscall trace\n",
                       output, input->index, input->time_spent_in_quantum);
            } else {
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
    }
    // For sched_type_t::DEPENDENCY_TIMESTAMPS: enforcing asked-for
    // context switch rates is more important that honoring precise
    // trace-buffer-based timestamp inter-input dependencies so we do not end a
    // quantum early due purely to timestamps.

    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
void
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::process_marker(
    input_info_t &input, output_ordinal_t output, trace_marker_type_t marker_type,
    uintptr_t marker_value)
{
    assert(input.lock->owned_by_cur_thread());
    switch (marker_type) {
    case TRACE_MARKER_TYPE_SYSCALL:
        input.processing_syscall = true;
        input.pre_syscall_timestamp = input.reader->get_last_timestamp();
        break;
    case TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL:
        input.processing_maybe_blocking_syscall = true;
        // Generally we should already have the timestamp from a just-prior
        // syscall marker, but we support tests and other synthetic sequences
        // with just a maybe-blocking.
        input.pre_syscall_timestamp = input.reader->get_last_timestamp();
        break;
    case TRACE_MARKER_TYPE_CONTEXT_SWITCH_START:
        outputs_[output].in_context_switch_code = true;
        break;
    case TRACE_MARKER_TYPE_CONTEXT_SWITCH_END:
        // We have to delay until the next record.
        outputs_[output].hit_switch_code_end = true;
        break;
    case TRACE_MARKER_TYPE_TIMESTAMP:
        // Syscall sequences are not expected to have a timestamp.
        assert(!outputs_[output].in_syscall_code);
        break;
    case TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH: {
        if (!options_.honor_direct_switches)
            break;
        ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_DIRECT_SWITCH_ATTEMPTS];
        memref_tid_t target_tid = marker_value;
        auto it = this->tid2input_.find(workload_tid_t(input.workload, target_tid));
        if (it == this->tid2input_.end()) {
            VPRINT(this, 1, "Failed to find input for target switch thread %" PRId64 "\n",
                   target_tid);
        } else {
            input.switch_to_input = it->second;
        }
        // Trigger a switch either indefinitely or until timeout.
        if (input.skip_next_unscheduled) {
            // The underlying kernel mechanism being modeled only supports a single
            // request: they cannot accumulate.  Timing differences in the trace could
            // perhaps result in multiple lining up when the didn't in the real app;
            // but changing the scheme here could also push representatives in the
            // other direction.
            input.skip_next_unscheduled = false;
            VPRINT(this, 3,
                   "input %d unschedule request ignored due to prior schedule request "
                   "@%" PRIu64 "\n",
                   input.index, input.reader->get_last_timestamp());
            break;
        }
        input.unscheduled = true;
        if (!options_.honor_infinite_timeouts && input.syscall_timeout_arg == 0) {
            // As our scheduling is imperfect we do not risk things being blocked
            // indefinitely: we instead have a timeout, but the maximum value.
            input.syscall_timeout_arg = options_.block_time_max_us;
            if (input.syscall_timeout_arg == 0)
                input.syscall_timeout_arg = 1;
        }
        if (input.syscall_timeout_arg > 0) {
            input.blocked_time = scale_blocked_time(input.syscall_timeout_arg);
            // Clamp at 1 since 0 means an infinite timeout for unscheduled=true.
            if (input.blocked_time == 0)
                input.blocked_time = 1;
            input.blocked_start_time = get_output_time(output);
            VPRINT(this, 3, "input %d unscheduled for %" PRIu64 " @%" PRIu64 "\n",
                   input.index, input.blocked_time, input.reader->get_last_timestamp());
        } else {
            VPRINT(this, 3, "input %d unscheduled indefinitely @%" PRIu64 "\n",
                   input.index, input.reader->get_last_timestamp());
        }
        break;
    }
    case TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT:
        // This is cleared at the post-syscall instr.
        input.syscall_timeout_arg = static_cast<uint64_t>(marker_value);
        break;
    case TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE:
        if (!options_.honor_direct_switches)
            break;
        if (input.skip_next_unscheduled) {
            input.skip_next_unscheduled = false;
            VPRINT(this, 3,
                   "input %d unschedule request ignored due to prior schedule request "
                   "@%" PRIu64 "\n",
                   input.index, input.reader->get_last_timestamp());
            break;
        }
        // Trigger a switch either indefinitely or until timeout.
        input.unscheduled = true;
        if (!options_.honor_infinite_timeouts && input.syscall_timeout_arg == 0) {
            // As our scheduling is imperfect we do not risk things being blocked
            // indefinitely: we instead have a timeout, but the maximum value.
            input.syscall_timeout_arg = options_.block_time_max_us;
            if (input.syscall_timeout_arg == 0)
                input.syscall_timeout_arg = 1;
        }
        if (input.syscall_timeout_arg > 0) {
            input.blocked_time = scale_blocked_time(input.syscall_timeout_arg);
            // Clamp at 1 since 0 means an infinite timeout for unscheduled=true.
            if (input.blocked_time == 0)
                input.blocked_time = 1;
            input.blocked_start_time = get_output_time(output);
            VPRINT(this, 3, "input %d unscheduled for %" PRIu64 " @%" PRIu64 "\n",
                   input.index, input.blocked_time, input.reader->get_last_timestamp());
        } else {
            VPRINT(this, 3, "input %d unscheduled indefinitely @%" PRIu64 "\n",
                   input.index, input.reader->get_last_timestamp());
        }
        break;
    case TRACE_MARKER_TYPE_SYSCALL_SCHEDULE: {
        if (!options_.honor_direct_switches)
            break;
        memref_tid_t target_tid = marker_value;
        auto it = this->tid2input_.find(workload_tid_t(input.workload, target_tid));
        if (it == this->tid2input_.end()) {
            VPRINT(this, 1,
                   "Failed to find input for switchto::resume target tid %" PRId64 "\n",
                   target_tid);
            return;
        }
        input_ordinal_t target_idx = it->second;
        VPRINT(this, 3, "input %d re-scheduling input %d @%" PRIu64 "\n", input.index,
               target_idx, input.reader->get_last_timestamp());
        // Release the input lock before acquiring more input locks
        input.lock->unlock();
        {
            input_info_t *target = &inputs_[target_idx];
            std::unique_lock<mutex_dbg_owned> target_lock(*target->lock);
            if (target->at_eof) {
                VPRINT(this, 3, "input %d at eof ignoring re-schedule\n", target_idx);
            } else if (target->unscheduled) {
                target->unscheduled = false;
                bool on_unsched_queue = false;
                {
                    std::lock_guard<mutex_dbg_owned> unsched_lock(
                        *unscheduled_priority_.lock);
                    if (unscheduled_priority_.queue.find(target)) {
                        unscheduled_priority_.queue.erase(target);
                        on_unsched_queue = true;
                    }
                }
                // We have to give up the unsched lock before calling add_to_ready_queue
                // as it acquires the output lock.
                if (on_unsched_queue) {
                    output_ordinal_t resume_output = target->prev_output;
                    if (resume_output == sched_type_t::INVALID_OUTPUT_ORDINAL)
                        resume_output = output;
                    // We can't hold any locks when calling add_to_ready_queue.
                    // This input is no longer on any queue, so few things can happen
                    // while we don't hold the input lock: a competing _SCHEDULE will
                    // not find the output and it can't have blocked_time>0 (invariant
                    // for things on unsched q); once it's on the new queue we don't
                    // do anything further here so we're good to go.
                    target_lock.unlock();
                    add_to_ready_queue(resume_output, target);
                    target_lock.lock();
                } else {
                    // We assume blocked_time is from _ARG_TIMEOUT and is not from
                    // regularly-blocking i/o.  We assume i/o getting into the mix is
                    // rare enough or does not matter enough to try to have separate
                    // timeouts.
                    if (target->blocked_time > 0) {
                        VPRINT(this, 3,
                               "switchto::resume erasing blocked time for target "
                               "input %d\n",
                               target->index);
                        output_ordinal_t target_output = target->containing_output;
                        // There could be no output owner if we're mid-rebalance.
                        if (target_output != sched_type_t::INVALID_OUTPUT_ORDINAL) {
                            // We can't hold the input lock to acquire the output lock.
                            target_lock.unlock();
                            {
                                auto scoped_output_lock =
                                    acquire_scoped_output_lock_if_necessary(
                                        target_output);
                                output_info_t &out = outputs_[target_output];
                                if (out.ready_queue.queue.find(target)) {
                                    --out.ready_queue.num_blocked;
                                }
                                // Decrement this holding the lock to synch with
                                // pop_from_ready_queue().
                                target->blocked_time = 0;
                            }
                            target_lock.lock();
                        } else
                            target->blocked_time = 0;
                    }
                }
            } else {
                VPRINT(this, 3, "input %d will skip next unschedule\n", target_idx);
                target->skip_next_unscheduled = true;
            }
        }
        input.lock->lock();
        break;
    }
    default: // Nothing to do.
        break;
    }
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
           get_output_time(triggering_output));
    // First, update the time to avoid more threads coming here.
    last_rebalance_time_.store(get_output_time(triggering_output),
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
            auto lock = acquire_scoped_output_lock_if_necessary(i);
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
                status = pop_from_ready_queue_hold_locks(
                    i, sched_type_t::INVALID_OUTPUT_ORDINAL, queue_next,
                    /*from_back=*/true);
                if (status == sched_type_t::STATUS_OK && queue_next != nullptr) {
                    VPRINT(this, 3,
                           "Rebalance iteration %d: output %d giving up input %d\n",
                           iteration, i, queue_next->index);
                    inputs_to_add.push_back(queue_next->index);
                } else {
                    if (status == sched_type_t::STATUS_IDLE) {
                        // An IDLE result is not an error: it just means there were no
                        // unblocked inputs available.  We do not want to propagate it to
                        // the caller.
                        status = sched_type_t::STATUS_OK;
                    }
                    break;
                }
            }
            // If we hit some fatal error, bail and propagate the error.
            if (status != sched_type_t::STATUS_OK)
                break;
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
                    add_to_ready_queue_hold_locks(i, &input);
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

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::eof_or_idle_for_mode(
    output_ordinal_t output, input_ordinal_t prev_input)
{
    int live_inputs = this->live_input_count_.load(std::memory_order_acquire);
    if (live_inputs == 0) {
        return sched_type_t::STATUS_EOF;
    }
    if (live_inputs <=
        static_cast<int>(inputs_.size() * options_.exit_if_fraction_inputs_left)) {
        VPRINT(this, 1, "output %d exiting early with %d live inputs left\n", output,
               live_inputs);
        return sched_type_t::STATUS_EOF;
    }
    //  Before going idle, try to steal work from another output.
    //  We start with us+1 to avoid everyone stealing from the low-numbered outputs.
    //  We only try when we first transition to idle; we rely on rebalancing after
    //  that, to avoid repeatededly grabbing other output's locks over and over.
    if (!outputs_[output].tried_to_steal_on_idle) {
        outputs_[output].tried_to_steal_on_idle = true;
        for (unsigned int i = 1; i < outputs_.size(); ++i) {
            output_ordinal_t target = (output + i) % outputs_.size();
            assert(target != output); // Sanity check (we won't reach "output").
            input_info_t *queue_next = nullptr;
            VPRINT(this, 4,
                   "eof_or_idle: output %d trying to steal from %d's ready_queue\n",
                   output, target);
            stream_status_t status = pop_from_ready_queue(target, output, queue_next);
            if (status == sched_type_t::STATUS_OK && queue_next != nullptr) {
                set_cur_input(output, queue_next->index);
                ++outputs_[output].stats[memtrace_stream_t::SCHED_STAT_RUNQUEUE_STEALS];
                VPRINT(this, 2,
                       "eof_or_idle: output %d stole input %d from %d's ready_queue\n",
                       output, queue_next->index, target);
                return sched_type_t::STATUS_STOLE;
            }
            // We didn't find anything; loop and check another output.
        }
        VPRINT(this, 3, "eof_or_idle: output %d failed to steal from anyone\n", output);
    }
    return sched_type_t::STATUS_IDLE;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::syscall_incurs_switch(
    input_info_t *input, uint64_t &blocked_time)
{
    assert(input->lock->owned_by_cur_thread());
    uint64_t post_time = input->reader->get_last_timestamp();
    assert(input->processing_syscall || input->processing_maybe_blocking_syscall);
    if (input->reader->get_version() < TRACE_ENTRY_VERSION_FREQUENT_TIMESTAMPS) {
        // This is a legacy trace that does not have timestamps bracketing syscalls.
        // We switch on every maybe-blocking syscall in this case and have a simplified
        // blocking model.
        blocked_time = options_.blocking_switch_threshold;
        return input->processing_maybe_blocking_syscall;
    }
    assert(input->pre_syscall_timestamp > 0);
    assert(input->pre_syscall_timestamp <= post_time);
    uint64_t latency = post_time - input->pre_syscall_timestamp;
    uint64_t threshold = input->processing_maybe_blocking_syscall
        ? options_.blocking_switch_threshold
        : options_.syscall_switch_threshold;
    blocked_time = scale_blocked_time(latency);
    VPRINT(this, 3,
           "input %d %ssyscall latency %" PRIu64 " * scale %6.3f => blocked time %" PRIu64
           "\n",
           input->index,
           input->processing_maybe_blocking_syscall ? "maybe-blocking " : "", latency,
           options_.block_time_multiplier, blocked_time);
    return latency >= threshold;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::ready_queue_empty(
    output_ordinal_t output)
{
    auto lock = acquire_scoped_output_lock_if_necessary(output);
    return outputs_[output].ready_queue.queue.empty();
}

template <typename RecordType, typename ReaderType>
void
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::add_to_unscheduled_queue(
    input_info_t *input)
{
    assert(input->lock->owned_by_cur_thread());
    std::lock_guard<mutex_dbg_owned> unsched_lock(*unscheduled_priority_.lock);
    assert(input->unscheduled &&
           input->blocked_time == 0); // Else should be in regular queue.
    VPRINT(this, 4, "add_to_unscheduled_queue (pre-size %zu): input %d priority %d\n",
           unscheduled_priority_.queue.size(), input->index, input->priority);
    input->queue_counter = ++unscheduled_priority_.fifo_counter;
    unscheduled_priority_.queue.push(input);
    input->prev_output = input->containing_output;
    input->containing_output = sched_type_t::INVALID_INPUT_ORDINAL;
}

template <typename RecordType, typename ReaderType>
void
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::add_to_ready_queue_hold_locks(
    output_ordinal_t output, input_info_t *input)
{
    assert(input->lock->owned_by_cur_thread());
    assert(!this->need_output_lock() ||
           outputs_[output].ready_queue.lock->owned_by_cur_thread());
    if (input->unscheduled && input->blocked_time == 0) {
        // Ensure we get prev_output set for start-unscheduled so they won't
        // all resume on output #0 but rather on the initial round-robin assignment.
        input->containing_output = output;
        add_to_unscheduled_queue(input);
        return;
    }
    assert(input->binding.empty() || input->binding.find(output) != input->binding.end());
    VPRINT(this, 4,
           "add_to_ready_queue (pre-size %zu): input %d priority %d timestamp delta "
           "%" PRIu64 " block time %" PRIu64 " start time %" PRIu64 "\n",
           outputs_[output].ready_queue.queue.size(), input->index, input->priority,
           input->reader->get_last_timestamp() - input->base_timestamp,
           input->blocked_time, input->blocked_start_time);
    if (input->blocked_time > 0)
        ++outputs_[output].ready_queue.num_blocked;
    input->queue_counter = ++outputs_[output].ready_queue.fifo_counter;
    outputs_[output].ready_queue.queue.push(input);
    input->containing_output = output;
}

template <typename RecordType, typename ReaderType>
void
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::add_to_ready_queue(
    output_ordinal_t output, input_info_t *input)
{
    auto scoped_lock = acquire_scoped_output_lock_if_necessary(output);
    std::lock_guard<mutex_dbg_owned> input_lock(*input->lock);
    add_to_ready_queue_hold_locks(output, input);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::pop_from_ready_queue_hold_locks(
    output_ordinal_t from_output, output_ordinal_t for_output, input_info_t *&new_input,
    bool from_back)
{
    assert(!this->need_output_lock() ||
           (outputs_[from_output].ready_queue.lock->owned_by_cur_thread() &&
            (from_output == for_output ||
             for_output == sched_type_t::INVALID_OUTPUT_ORDINAL ||
             outputs_[for_output].ready_queue.lock->owned_by_cur_thread())));
    std::set<input_info_t *> skipped;
    std::set<input_info_t *> blocked;
    input_info_t *res = nullptr;
    stream_status_t status = sched_type_t::STATUS_OK;
    uint64_t cur_time = get_output_time(from_output);
    while (!outputs_[from_output].ready_queue.queue.empty()) {
        if (from_back) {
            res = outputs_[from_output].ready_queue.queue.back();
            outputs_[from_output].ready_queue.queue.erase(res);
        } else if (options_.randomize_next_input) {
            res = outputs_[from_output].ready_queue.queue.get_random_entry();
            outputs_[from_output].ready_queue.queue.erase(res);
        } else {
            res = outputs_[from_output].ready_queue.queue.top();
            outputs_[from_output].ready_queue.queue.pop();
        }
        std::lock_guard<mutex_dbg_owned> input_lock(*res->lock);
        assert(!res->unscheduled ||
               res->blocked_time > 0); // Should be in unscheduled_priority_.
        if (res->binding.empty() || for_output == sched_type_t::INVALID_OUTPUT_ORDINAL ||
            res->binding.find(for_output) != res->binding.end()) {
            // For blocked inputs, as we don't have interrupts or other regular
            // control points we only check for being unblocked when an input
            // would be chosen to run.  We thus keep blocked inputs in the ready queue.
            if (res->blocked_time > 0) {
                --outputs_[from_output].ready_queue.num_blocked;
                if (!options_.honor_infinite_timeouts) {
                    // cur_time can be 0 at initialization time.
                    if (res->blocked_start_time == 0 && cur_time > 0) {
                        // This was a start-unscheduled input: we didn't have a valid
                        // time at initialization.
                        res->blocked_start_time = cur_time;
                    }
                } else
                    assert(cur_time > 0);
            }
            if (res->blocked_time > 0 &&
                // cur_time can be 0 at initialization time.
                (cur_time == 0 ||
                 // Guard against time going backward (happens for wall-clock: i#6966).
                 cur_time < res->blocked_start_time ||
                 cur_time - res->blocked_start_time < res->blocked_time)) {
                VPRINT(this, 4, "pop queue: %d still blocked for %" PRIu64 "\n",
                       res->index,
                       res->blocked_time - (cur_time - res->blocked_start_time));
                // We keep searching for a suitable input.
                blocked.insert(res);
            } else {
                // This input is no longer blocked.
                res->blocked_time = 0;
                res->unscheduled = false;
                VPRINT(this, 4, "pop queue: %d @ %" PRIu64 " no longer blocked\n",
                       res->index, cur_time);
                bool found_candidate = false;
                // We've found a potential candidate.  Is it under its output limit?
                workload_info_t &workload = workloads_[res->workload];
                if (workload.output_limit > 0 &&
                    workload.live_output_count->load(std::memory_order_acquire) >=
                        workload.output_limit) {
                    VPRINT(this, 2, "output[%d]: not running input %d: at output limit\n",
                           for_output, res->index);
                    ++outputs_[from_output]
                          .stats[memtrace_stream_t::SCHED_STAT_HIT_OUTPUT_LIMIT];
                } else if (from_output == for_output) {
                    found_candidate = true;
                } else {
                    // One final check if this is a migration.
                    assert(cur_time > 0 || res->last_run_time == 0);
                    if (res->last_run_time == 0) {
                        // For never-executed inputs we consider their last execution
                        // to be the very first simulation time, which we can't
                        // easily initialize until here.
                        res->last_run_time = outputs_[from_output].initial_cur_time->load(
                            std::memory_order_acquire);
                    }
                    VPRINT(this, 5,
                           "migration check %d to %d: cur=%" PRIu64 " last=%" PRIu64
                           " delta=%" PRId64 " vs thresh %" PRIu64 "\n",
                           from_output, for_output, cur_time, res->last_run_time,
                           cur_time - res->last_run_time,
                           options_.migration_threshold_us);
                    // Guard against time going backward (happens for wall-clock: i#6966).
                    if (options_.migration_threshold_us == 0 ||
                        // Allow free movement for the initial load balance at init time.
                        cur_time == 0 ||
                        (cur_time > res->last_run_time &&
                         cur_time - res->last_run_time >=
                             static_cast<uint64_t>(options_.migration_threshold_us *
                                                   options_.time_units_per_us))) {
                        VPRINT(this, 2, "migrating %d to %d\n", from_output, for_output);
                        found_candidate = true;
                        // Do not count an initial rebalance as a migration.
                        if (cur_time > 0) {
                            ++outputs_[from_output]
                                  .stats[memtrace_stream_t::SCHED_STAT_MIGRATIONS];
                        }
                    }
                }
                if (found_candidate)
                    break;
                else
                    skipped.insert(res);
            }
        } else {
            // We keep searching for a suitable input.
            skipped.insert(res);
        }
        res = nullptr;
    }
    if (res == nullptr && !blocked.empty()) {
        // Do not hand out EOF thinking we're done: we still have inputs blocked
        // on i/o, so just wait and retry.
        if (for_output != sched_type_t::INVALID_OUTPUT_ORDINAL)
            ++outputs_[for_output].idle_count;
        status = sched_type_t::STATUS_IDLE;
    }
    // Re-add the ones we skipped, but without changing their counters so we preserve
    // the prior FIFO order.
    for (input_info_t *save : skipped)
        outputs_[from_output].ready_queue.queue.push(save);
    // Re-add the blocked ones to the back.
    for (input_info_t *save : blocked) {
        std::lock_guard<mutex_dbg_owned> input_lock(*save->lock);
        add_to_ready_queue_hold_locks(from_output, save);
    }
    auto res_lock = (res == nullptr) ? std::unique_lock<mutex_dbg_owned>()
                                     : std::unique_lock<mutex_dbg_owned>(*res->lock);
    VDO(this, 1, {
        static int output_heartbeat;
        // We are ok with races as the cadence is approximate.
        static constexpr int64_t OUTPUT_HEARTBEAT_CADENCE = 200000;
        if (++output_heartbeat % OUTPUT_HEARTBEAT_CADENCE == 0) {
            size_t unsched_size = 0;
            {
                std::lock_guard<mutex_dbg_owned> unsched_lock(
                    *unscheduled_priority_.lock);
                unsched_size = unscheduled_priority_.queue.size();
            }
            VPRINT(this, 1,
                   "heartbeat[%d] %zd in queue; %d blocked; %zd unscheduled => %d %d\n",
                   from_output, outputs_[from_output].ready_queue.queue.size(),
                   outputs_[from_output].ready_queue.num_blocked, unsched_size,
                   res == nullptr ? -1 : res->index, status);
        }
    });
    if (res != nullptr) {
        VPRINT(this, 4,
               "pop_from_ready_queue[%d] (post-size %zu): input %d priority %d timestamp "
               "delta %" PRIu64 "\n",
               from_output, outputs_[from_output].ready_queue.queue.size(), res->index,
               res->priority, res->reader->get_last_timestamp() - res->base_timestamp);
        res->unscheduled = false;
        res->prev_output = res->containing_output;
        res->containing_output = for_output;
    }
    new_input = res;
    return status;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::pop_from_ready_queue(
    output_ordinal_t from_output, output_ordinal_t for_output, input_info_t *&new_input)
{
    stream_status_t status = sched_type_t::STATUS_OK;
    {
        std::unique_lock<mutex_dbg_owned> from_lock;
        std::unique_lock<mutex_dbg_owned> for_lock;
        // If we need both locks, acquire in increasing output order to avoid deadlocks if
        // two outputs try to steal from each other.
        if (from_output == for_output ||
            for_output == sched_type_t::INVALID_OUTPUT_ORDINAL) {
            from_lock = acquire_scoped_output_lock_if_necessary(from_output);
        } else if (from_output < for_output) {
            from_lock = acquire_scoped_output_lock_if_necessary(from_output);
            for_lock = acquire_scoped_output_lock_if_necessary(for_output);
        } else {
            for_lock = acquire_scoped_output_lock_if_necessary(for_output);
            from_lock = acquire_scoped_output_lock_if_necessary(from_output);
        }
        status = pop_from_ready_queue_hold_locks(from_output, for_output, new_input);
    }
    return status;
}

template <typename RecordType, typename ReaderType>
void
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::print_queue_stats()
{
    size_t unsched_size = 0;
    {
        std::lock_guard<mutex_dbg_owned> unsched_lock(*unscheduled_priority_.lock);
        unsched_size = unscheduled_priority_.queue.size();
    }
    int live = this->live_input_count_.load(std::memory_order_acquire);
    // Make our multi-line output more atomic.
    std::ostringstream ostr;
    ostr << "Queue snapshot: inputs: " << live - unsched_size << " schedulable, "
         << unsched_size << " unscheduled, " << inputs_.size() - live << " eof\n";
    for (unsigned int i = 0; i < outputs_.size(); ++i) {
        auto lock = acquire_scoped_output_lock_if_necessary(i);
        uint64_t cur_time = get_output_time(i);
        ostr << "  out #" << i << " @" << cur_time << ": running #"
             << outputs_[i].cur_input << "; " << outputs_[i].ready_queue.queue.size()
             << " in queue; " << outputs_[i].ready_queue.num_blocked << " blocked\n";
        std::set<input_info_t *> readd;
        input_info_t *res = nullptr;
        while (!outputs_[i].ready_queue.queue.empty()) {
            res = outputs_[i].ready_queue.queue.top();
            readd.insert(res);
            outputs_[i].ready_queue.queue.pop();
            std::lock_guard<mutex_dbg_owned> input_lock(*res->lock);
            if (res->blocked_time > 0) {
                ostr << "    " << res->index << " still blocked for "
                     << res->blocked_time - (cur_time - res->blocked_start_time) << "\n";
            }
        }
        // Re-add the ones we skipped, but without changing their counters so we
        // preserve the prior FIFO order.
        for (input_info_t *add : readd)
            outputs_[i].ready_queue.queue.push(add);
    }
    VPRINT(this, 0, "%s\n", ostr.str().c_str());
}

template class scheduler_dynamic_tmpl_t<memref_t, reader_t>;
template class scheduler_dynamic_tmpl_t<trace_entry_t,
                                        dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
