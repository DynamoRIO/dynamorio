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

#include <cinttypes>

namespace dynamorio {
namespace drmemtrace {

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::pick_next_input_for_mode(
    output_ordinal_t output, uint64_t blocked_time, input_ordinal_t prev_index,
    input_ordinal_t &index)
{
    uint64_t cur_time = this->get_output_time(output);
    uint64_t last_time = this->last_rebalance_time_.load(std::memory_order_acquire);
    if (last_time == 0) {
        // Initialize.
        this->last_rebalance_time_.store(cur_time, std::memory_order_release);
    } else {
        // Guard against time going backward, which happens: i#6966.
        if (cur_time > last_time &&
            cur_time - last_time >=
                static_cast<uint64_t>(this->options_.rebalance_period_us *
                                      this->options_.time_units_per_us) &&
            this->rebalancer_.load(std::memory_order_acquire) == std::thread::id()) {
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
        std::lock_guard<mutex_dbg_owned> lock(*this->inputs_[prev_index].lock);
        if (this->inputs_[prev_index].blocked_time == 0) {
            VPRINT(this, 2, "next_record[%d]: blocked time %" PRIu64 "\n", output,
                   blocked_time);
            this->inputs_[prev_index].blocked_time = blocked_time;
            this->inputs_[prev_index].blocked_start_time = this->get_output_time(output);
        }
    }
    if (prev_index != sched_type_t::INVALID_INPUT_ORDINAL &&
        this->inputs_[prev_index].switch_to_input !=
            sched_type_t::INVALID_INPUT_ORDINAL) {
        input_info_t *target = &this->inputs_[this->inputs_[prev_index].switch_to_input];
        this->inputs_[prev_index].switch_to_input = sched_type_t::INVALID_INPUT_ORDINAL;
        std::unique_lock<mutex_dbg_owned> target_input_lock(*target->lock);
        // XXX i#5843: Add an invariant check that the next timestamp of the
        // target is later than the pre-switch-syscall timestamp?
        if (target->containing_output != sched_type_t::INVALID_OUTPUT_ORDINAL) {
            output_ordinal_t target_output = target->containing_output;
            output_info_t &out = this->outputs_[target->containing_output];
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
                           this->inputs_[prev_index].reader->get_last_timestamp());
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
                        ++this->outputs_[output]
                              .stats[memtrace_stream_t::SCHED_STAT_MIGRATIONS];
                    }
                    ++this->outputs_[output]
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
                   this->inputs_[prev_index].reader->get_last_timestamp());
            if (target->prev_output != sched_type_t::INVALID_OUTPUT_ORDINAL &&
                target->prev_output != output) {
                ++this->outputs_[output].stats[memtrace_stream_t::SCHED_STAT_MIGRATIONS];
            }
            ++this->outputs_[output]
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
                   this->inputs_[prev_index].reader->get_last_timestamp());
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
            index = this->outputs_[output].cur_input;
            return sched_type_t::STATUS_OK;
        } else {
            auto lock =
                std::unique_lock<mutex_dbg_owned>(*this->inputs_[prev_index].lock);
            // If we can't go back to the current input because it's EOF
            // or unscheduled indefinitely (we already checked blocked_time
            // above: it's 0 here), this output is either idle or EOF.
            if (this->inputs_[prev_index].at_eof ||
                this->inputs_[prev_index].unscheduled) {
                lock.unlock();
                stream_status_t status = this->eof_or_idle(output, prev_index);
                if (status != sched_type_t::STATUS_STOLE)
                    return status;
                index = this->outputs_[output].cur_input;
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
        this->set_cur_input(output, sched_type_t::INVALID_INPUT_ORDINAL);
        input_info_t *queue_next = nullptr;
        stream_status_t status = this->pop_from_ready_queue(output, output, queue_next);
        if (status != sched_type_t::STATUS_OK) {
            if (status == sched_type_t::STATUS_IDLE) {
                this->outputs_[output].waiting = true;
                if (this->options_.schedule_record_ostream != nullptr) {
                    stream_status_t record_status = this->record_schedule_segment(
                        output, schedule_record_t::IDLE_BY_COUNT, 0,
                        // Start prior to this idle.
                        this->outputs_[output].idle_count - 1, 0);
                    if (record_status != sched_type_t::STATUS_OK)
                        return record_status;
                }
                if (prev_index != sched_type_t::INVALID_INPUT_ORDINAL) {
                    ++this->outputs_[output]
                          .stats[memtrace_stream_t::SCHED_STAT_SWITCH_INPUT_TO_IDLE];
                }
            }
            return status;
        }
        if (queue_next == nullptr) {
            status = this->eof_or_idle(output, prev_index);
            if (status != sched_type_t::STATUS_STOLE)
                return status;
            index = this->outputs_[output].cur_input;
            return sched_type_t::STATUS_OK;
        } else
            index = queue_next->index;
    }
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_dynamic_tmpl_t<RecordType, ReaderType>::process_next_record_candidate(
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
        if (this->record_type_is_instr_boundary(record,
                                                this->outputs_[output].last_record)) {
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
    if (this->outputs_[output].hit_switch_code_end) {
        // We have to delay so the end marker is still in_context_switch_code.
        this->outputs_[output].in_context_switch_code = false;
        this->outputs_[output].hit_switch_code_end = false;
        // We're now back "on the clock".
        if (this->options_.quantum_unit == sched_type_t::QUANTUM_TIME)
            input->prev_time_in_quantum = cur_time;
        // XXX: If we add a skip feature triggered on the output stream,
        // we'll want to make sure skipping while in these switch and kernel
        // sequences is handled correctly.
    }
    if (this->record_type_is_marker(record, marker_type, marker_value)) {
        this->process_marker(*input, output, marker_type, marker_value);
    }
    if (this->options_.quantum_unit == sched_type_t::QUANTUM_INSTRUCTIONS &&
        this->record_type_is_instr_boundary(record, this->outputs_[output].last_record) &&
        !this->outputs_[output].in_kernel_code) {
        ++input->instrs_in_quantum;
        if (input->instrs_in_quantum > this->options_.quantum_duration_instrs) {
            // We again prefer to switch to another input even if the current
            // input has the oldest timestamp, prioritizing context switches
            // over timestamp ordering.
            VPRINT(this, 4, "next_record[%d]: input %d hit end of instr quantum\n",
                   output, input->index);
            preempt = true;
            need_new_input = true;
            input->instrs_in_quantum = 0;
            ++this->outputs_[output]
                  .stats[memtrace_stream_t::SCHED_STAT_QUANTUM_PREEMPTS];
        }
    } else if (this->options_.quantum_unit == sched_type_t::QUANTUM_TIME) {
        if (cur_time == 0 || cur_time < input->prev_time_in_quantum) {
            VPRINT(this, 1,
                   "next_record[%d]: invalid time %" PRIu64 " vs start %" PRIu64 "\n",
                   output, cur_time, input->prev_time_in_quantum);
            return sched_type_t::STATUS_INVALID;
        }
        input->time_spent_in_quantum += cur_time - input->prev_time_in_quantum;
        input->prev_time_in_quantum = cur_time;
        double elapsed_micros = static_cast<double>(input->time_spent_in_quantum) /
            this->options_.time_units_per_us;
        if (elapsed_micros >= this->options_.quantum_duration_us &&
            // We only switch on instruction boundaries.  We could possibly switch
            // in between (e.g., scatter/gather long sequence of reads/writes) by
            // setting input->switching_pre_instruction.
            this->record_type_is_instr_boundary(record,
                                                this->outputs_[output].last_record)) {
            VPRINT(this, 4,
                   "next_record[%d]: input %d hit end of time quantum after %" PRIu64
                   "\n",
                   output, input->index, input->time_spent_in_quantum);
            preempt = true;
            need_new_input = true;
            input->time_spent_in_quantum = 0;
            ++this->outputs_[output]
                  .stats[memtrace_stream_t::SCHED_STAT_QUANTUM_PREEMPTS];
        }
    }
    // For sched_type_t::DEPENDENCY_TIMESTAMPS: enforcing asked-for
    // context switch rates is more important that honoring precise
    // trace-buffer-based timestamp inter-input dependencies so we do not end a
    // quantum early due purely to timestamps.

    return sched_type_t::STATUS_OK;
}

template class scheduler_dynamic_tmpl_t<memref_t, reader_t>;
template class scheduler_dynamic_tmpl_t<trace_entry_t,
                                        dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
