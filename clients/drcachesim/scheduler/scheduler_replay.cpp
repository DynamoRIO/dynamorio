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

/* Scheduler replay code. */

#include "scheduler.h"
#include "scheduler_impl.h"

#include <cinttypes>

namespace dynamorio {
namespace drmemtrace {

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::pick_next_input_for_mode(
    output_ordinal_t output, uint64_t blocked_time, input_ordinal_t prev_index,
    input_ordinal_t &index)
{
    // Our own index is only modified by us so we can cache it here.
    int record_index =
        this->outputs_[output].record_index->load(std::memory_order_acquire);
    if (record_index + 1 >= static_cast<int>(this->outputs_[output].record.size())) {
        if (!this->outputs_[output].at_eof) {
            this->outputs_[output].at_eof = true;
            this->live_replay_output_count_.fetch_add(-1, std::memory_order_release);
        }
        return this->eof_or_idle(output, this->outputs_[output].cur_input);
    }
    schedule_record_t &segment = this->outputs_[output].record[record_index + 1];
    if (segment.type == schedule_record_t::IDLE ||
        segment.type == schedule_record_t::IDLE_BY_COUNT) {
        this->outputs_[output].waiting = true;
        if (segment.type == schedule_record_t::IDLE) {
            // Convert a legacy idle duration from microseconds to record counts.
            segment.value.idle_duration = static_cast<uint64_t>(
                this->options_.time_units_per_us * segment.value.idle_duration);
        }
        this->outputs_[output].idle_start_count = this->outputs_[output].idle_count;
        this->outputs_[output].record_index->fetch_add(1, std::memory_order_release);
        ++this->outputs_[output].idle_count;
        VPRINT(this, 5, "%s[%d]: next replay segment idle for %" PRIu64 "\n",
               __FUNCTION__, output, segment.value.idle_duration);
        return sched_type_t::STATUS_IDLE;
    }
    index = segment.key.input;
    VPRINT(this, 5,
           "%s[%d]: next replay segment in=%d (@%" PRId64 ") type=%d start=%" PRId64
           " end=%" PRId64 "\n",
           __FUNCTION__, output, index, this->get_instr_ordinal(this->inputs_[index]),
           segment.type, segment.value.start_instruction, segment.stop_instruction);
    {
        std::lock_guard<mutex_dbg_owned> lock(*this->inputs_[index].lock);
        if (this->get_instr_ordinal(this->inputs_[index]) >
            segment.value.start_instruction) {
            VPRINT(this, 1,
                   "WARNING: next_record[%d]: input %d wants instr #%" PRId64
                   " but it is already at #%" PRId64 "\n",
                   output, index, segment.value.start_instruction,
                   this->get_instr_ordinal(this->inputs_[index]));
        }
        if (this->get_instr_ordinal(this->inputs_[index]) <
                segment.value.start_instruction &&
            // Don't wait for an ROI that starts at the beginning.
            segment.value.start_instruction > 1 &&
            // The output may have begun in the wait state.
            (record_index == -1 ||
             // When we skip our separator+timestamp markers are at the
             // prior instr ord so do not wait for that.
             (this->outputs_[output].record[record_index].type !=
                  schedule_record_t::SKIP &&
              // Don't wait if we're at the end and just need the end record.
              segment.type != schedule_record_t::SYNTHETIC_END))) {
            // Some other output stream has not advanced far enough, and we do
            // not support multiple positions in one input stream: we wait.
            // XXX i#5843: We may want to provide a kernel-mediated wait
            // feature so a multi-threaded simulator doesn't have to do a
            // spinning poll loop.
            // XXX i#5843: For replaying a schedule as it was traced with
            // sched_type_t::MAP_TO_RECORDED_OUTPUT there may have been true idle periods
            // during tracing where some other process than the traced workload was
            // scheduled on a core.  If we could identify those, we should return
            // sched_type_t::STATUS_IDLE rather than sched_type_t::STATUS_WAIT.
            VPRINT(this, 3, "next_record[%d]: waiting for input %d instr #%" PRId64 "\n",
                   output, index, segment.value.start_instruction);
            // Give up this input and go into a wait state.
            // We'll come back here on the next next_record() call.
            this->set_cur_input(output, sched_type_t::INVALID_INPUT_ORDINAL,
                                // Avoid livelock if prev input == cur input which happens
                                // with back-to-back segments with the same input.
                                index == this->outputs_[output].cur_input);
            this->outputs_[output].waiting = true;
            return sched_type_t::STATUS_WAIT;
        }
    }
    // Also wait if this segment is ahead of the next-up segment on another
    // output.  We only have a timestamp per context switch so we can't
    // enforce finer-grained timing replay.
    if (this->options_.deps == sched_type_t::DEPENDENCY_TIMESTAMPS) {
        for (int i = 0; i < static_cast<output_ordinal_t>(this->outputs_.size()); ++i) {
            if (i == output)
                continue;
            // Do an atomic load once and use it to de-reference if it's not at the end.
            // This is safe because if the target advances to the end concurrently it
            // will only cause an extra wait that will just come back here and then
            // continue.
            int other_index =
                this->outputs_[i].record_index->load(std::memory_order_acquire);
            if (other_index + 1 < static_cast<int>(this->outputs_[i].record.size()) &&
                segment.timestamp > this->outputs_[i].record[other_index + 1].timestamp) {
                VPRINT(this, 3,
                       "next_record[%d]: waiting because timestamp %" PRIu64
                       " is ahead of output %d\n",
                       output, segment.timestamp, i);
                // Give up this input and go into a wait state.
                // We'll come back here on the next next_record() call.
                // XXX: We should add a timeout just in case some timestamps are out of
                // order due to using prior values, to avoid hanging.  We try to avoid
                // this by using wall-clock time in record_schedule_segment() rather than
                // the stored output time.
                this->set_cur_input(output, sched_type_t::INVALID_INPUT_ORDINAL);
                this->outputs_[output].waiting = true;
                return sched_type_t::STATUS_WAIT;
            }
        }
    }
    if (segment.type == schedule_record_t::SYNTHETIC_END) {
        std::lock_guard<mutex_dbg_owned> lock(*this->inputs_[index].lock);
        // We're past the final region of interest and we need to insert
        // a synthetic thread exit record.  We need to first throw out the
        // queued candidate record, if any.
        this->clear_input_queue(this->inputs_[index]);
        this->inputs_[index].queue.push_back(
            this->create_thread_exit(this->inputs_[index].tid));
        VPRINT(this, 2, "early end for input %d\n", index);
        // We're done with this entry but we need the queued record to be read,
        // so we do not move past the entry.
        this->outputs_[output].record_index->fetch_add(1, std::memory_order_release);
        stream_status_t status = this->mark_input_eof(this->inputs_[index]);
        if (status != sched_type_t::STATUS_OK)
            return status;
        return sched_type_t::STATUS_SKIPPED;
    } else if (segment.type == schedule_record_t::SKIP) {
        std::lock_guard<mutex_dbg_owned> lock(*this->inputs_[index].lock);
        uint64_t cur_reader_instr =
            this->inputs_[index].reader->get_instruction_ordinal();
        VPRINT(this, 2,
               "next_record[%d]: skipping from %" PRId64 " to %" PRId64
               " in %d for schedule\n",
               output, cur_reader_instr, segment.stop_instruction, index);
        auto status = this->skip_instructions(this->inputs_[index],
                                              segment.stop_instruction -
                                                  cur_reader_instr - 1 /*exclusive*/);
        // Increment the region to get window id markers with ordinals.
        this->inputs_[index].cur_region++;
        if (status != sched_type_t::STATUS_SKIPPED)
            return sched_type_t::STATUS_INVALID;
        // We're done with the skip so move to and past it.
        this->outputs_[output].record_index->fetch_add(2, std::memory_order_release);
        return sched_type_t::STATUS_SKIPPED;
    } else {
        VPRINT(this, 2, "next_record[%d]: advancing to input %d instr #%" PRId64 "\n",
               output, index, segment.value.start_instruction);
    }
    this->outputs_[output].record_index->fetch_add(1, std::memory_order_release);
    VDO(this, 2, {
        // Our own index is only modified by us so we can cache it here.
        int local_index =
            this->outputs_[output].record_index->load(std::memory_order_acquire);
        if (local_index >= 0 &&
            local_index < static_cast<int>(this->outputs_[output].record.size())) {
            const schedule_record_t &local_segment =
                this->outputs_[output].record[local_index];
            int input = local_segment.key.input;
            VPRINT(this, 2,
                   "next_record[%d]: replay segment in=%d (@%" PRId64
                   ") type=%d start=%" PRId64 " end=%" PRId64 "\n",
                   output, input, this->get_instr_ordinal(this->inputs_[input]),
                   local_segment.type, local_segment.value.start_instruction,
                   local_segment.stop_instruction);
        }
    });
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::process_next_record_candidate(
    output_ordinal_t output, RecordType &record, input_info_t *input, uint64_t cur_time,
    bool &need_new_input, bool &preempt, uint64_t &blocked_time)
{
    // Our own index is only modified by us so we can cache it here.
    int record_index =
        this->outputs_[output].record_index->load(std::memory_order_acquire);
    assert(record_index >= 0);
    if (record_index >= static_cast<int>(this->outputs_[output].record.size())) {
        // We're on the last record.
        VPRINT(this, 4, "next_record[%d]: on last record\n", output);
    } else if (this->outputs_[output].record[record_index].type ==
               schedule_record_t::SKIP) {
        VPRINT(this, 5, "next_record[%d]: need new input after skip\n", output);
        need_new_input = true;
    } else if (this->outputs_[output].record[record_index].type ==
               schedule_record_t::SYNTHETIC_END) {
        VPRINT(this, 5, "next_record[%d]: at synthetic end\n", output);
    } else {
        const schedule_record_t &segment = this->outputs_[output].record[record_index];
        assert(segment.type == schedule_record_t::DEFAULT);
        uint64_t start = segment.value.start_instruction;
        uint64_t stop = segment.stop_instruction;
        // The stop is exclusive.  0 does mean to do nothing (easiest
        // to have an empty record to share the next-entry for a start skip
        // or other cases).
        // Only check for stop when we've exhausted the queue, or we have
        // a starter schedule with a 0,0 entry prior to a first skip entry
        // (as just mentioned, it is easier to have a seemingly-redundant entry
        // to get into the trace reading loop and then do something like a skip
        // from the start rather than adding logic into the setup code).
        if (this->get_instr_ordinal(*input) >= stop &&
            (!input->cur_from_queue || (start == 0 && stop == 0))) {
            VPRINT(this, 5,
                   "next_record[%d]: need new input: at end of segment in=%d "
                   "stop=%" PRId64 "\n",
                   output, input->index, stop);
            need_new_input = true;
        }
    }
    return sched_type_t::STATUS_OK;
}

template class scheduler_replay_tmpl_t<memref_t, reader_t>;
template class scheduler_replay_tmpl_t<trace_entry_t,
                                       dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
