/* **********************************************************
 * Copyright (c) 2023-2026 Google, Inc.  All rights reserved.
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

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "memref.h"
#include "mutex_dbg_owned.h"
#include "reader.h"
#include "record_file_reader.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::set_initial_schedule()
{
    if (options_.mapping == sched_type_t::MAP_AS_PREVIOUSLY) {
        this->live_replay_output_count_.store(static_cast<int>(outputs_.size()),
                                              std::memory_order_release);
        if (options_.schedule_replay_istream == nullptr ||
            options_.schedule_record_ostream != nullptr)
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        scheduler_status_t status = read_recorded_schedule();
        if (status != sched_type_t::STATUS_SUCCESS)
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    } else if (options_.schedule_replay_istream != nullptr) {
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    } else if (options_.mapping == sched_type_t::MAP_TO_RECORDED_OUTPUT &&
               options_.replay_as_traced_istream != nullptr) {
        // Even for just one output we honor a request to replay the schedule
        // (although it should match the analyzer serial mode so there's no big
        // benefit to reading the schedule file.  The analyzer serial mode or other
        // special cases of one output don't set the replay_as_traced_istream
        // field.)
        scheduler_status_t status = read_and_instantiate_traced_schedule();
        if (status != sched_type_t::STATUS_SUCCESS)
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        // Now leverage the regular replay code.
        options_.mapping = sched_type_t::MAP_AS_PREVIOUSLY;
    } else {
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::read_recorded_schedule()
{
    if (options_.schedule_replay_istream == nullptr)
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;

    schedule_record_t record;
    // We assume we can easily fit the whole context switch sequence in memory.
    // If that turns out not to be the case for very long traces, we deliberately
    // used an archive format so we could do parallel incremental reads.
    // (Conversely, if we want to commit to storing in memory, we could use a
    // non-archive format and store the output ordinal in the version record.)
    for (int i = 0; i < static_cast<int>(outputs_.size()); ++i) {
        std::string err = options_.schedule_replay_istream->open_component(
            this->recorded_schedule_component_name(i));
        if (!err.empty()) {
            error_string_ = "Failed to open schedule_replay_istream component " +
                this->recorded_schedule_component_name(i) + ": " + err;
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
        // XXX: This could be made more efficient if we stored the record count
        // in the version field's stop_instruction field or something so we can
        // size the vector up front.  As this only happens once we do not bother
        // and live with a few vector resizes.
        bool saw_footer = false;
        while (options_.schedule_replay_istream->read(reinterpret_cast<char *>(&record),
                                                      sizeof(record))) {
            if (record.type == schedule_record_t::VERSION) {
                if (record.key.version != schedule_record_t::VERSION_CURRENT)
                    return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            } else if (record.type == schedule_record_t::FOOTER) {
                saw_footer = true;
                break;
            } else
                outputs_[i].record.push_back(record);
        }
        if (!saw_footer) {
            error_string_ = "Record file missing footer";
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
        VPRINT(this, 1, "Read %zu recorded records for output #%d\n",
               outputs_[i].record.size(), i);
    }
    // See if there was more data in the file (we do this after reading to not
    // mis-report i/o or path errors as this error).
    std::string err = options_.schedule_replay_istream->open_component(
        this->recorded_schedule_component_name(
            static_cast<output_ordinal_t>(outputs_.size())));
    if (err.empty()) {
        error_string_ = "Not enough output streams for recorded file";
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    for (int i = 0; i < static_cast<output_ordinal_t>(outputs_.size()); ++i) {
        if (outputs_[i].record.empty()) {
            // XXX i#6630: We should auto-set the output count and avoid
            // having extra outputs; these complicate idle computations, etc.
            VPRINT(this, 1, "output %d empty: returning eof up front\n", i);
            set_cur_input(i, sched_type_t::INVALID_INPUT_ORDINAL);
            outputs_[i].at_eof = true;
        } else if (outputs_[i].record[0].type == schedule_record_t::IDLE ||
                   outputs_[i].record[0].type == schedule_record_t::IDLE_BY_COUNT) {
            set_cur_input(i, sched_type_t::INVALID_INPUT_ORDINAL);
            outputs_[i].waiting = true;
            if (outputs_[i].record[0].type == schedule_record_t::IDLE) {
                // Convert a legacy idle duration from microseconds to record counts.
                outputs_[i].record[0].value.idle_duration =
                    static_cast<uint64_t>(options_.time_units_per_us *
                                          outputs_[i].record[0].value.idle_duration);
            }
            outputs_[i].idle_start_count = -1; // Updated on first next_record().
            VPRINT(this, 3, "output %d starting out idle\n", i);
        } else {
            assert(outputs_[i].record[0].type == schedule_record_t::DEFAULT);
            set_cur_input(i, outputs_[i].record[0].key.input);
        }
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::read_and_instantiate_traced_schedule()
{
    std::vector<std::set<uint64_t>> start2stop(inputs_.size());
    // We also want to collapse same-cpu consecutive records so we start with
    // a temporary local vector.
    std::vector<std::vector<schedule_output_tracker_t>> all_sched(outputs_.size());
    // Work around i#6107 by tracking counts sorted by timestamp for each input.
    std::vector<std::vector<schedule_input_tracker_t>> input_sched(inputs_.size());
    // These hold entries added in the on-disk (unsorted) order.
    std::vector<output_ordinal_t> disk_ord2index; // Initially [i] holds i.
    std::vector<uint64_t> disk_ord2cpuid;         // [i] holds cpuid for entry i.
    scheduler_status_t res = this->read_traced_schedule(
        input_sched, start2stop, all_sched, disk_ord2index, disk_ord2cpuid);
    if (res != sched_type_t::STATUS_SUCCESS)
        return res;
    // Sort by cpuid to get a more natural ordering.
    // Probably raw2trace should do this in the first place, but we have many
    // schedule files already out there so we still need a sort here.
    // If we didn't have cross-indices pointing at all_sched from input_sched, we
    // would just sort all_sched: but instead we have to construct a separate
    // ordering structure.
    std::sort(disk_ord2index.begin(), disk_ord2index.end(),
              [disk_ord2cpuid](const output_ordinal_t &l, const output_ordinal_t &r) {
                  return disk_ord2cpuid[l] < disk_ord2cpuid[r];
              });
    // disk_ord2index[i] used to hold i; now after sorting it holds the ordinal in
    // the disk file that has the ith largest cpuid.  We need to turn that into
    // the output_idx ordinal for the cpu at ith ordinal in the disk file, for
    // which we use a new vector disk_ord2output.
    // E.g., if the original file was in this order disk_ord2cpuid = {6,2,3,7},
    // disk_ord2index after sorting would hold {1,2,0,3}, which we want to turn
    // into disk_ord2output = {2,0,1,3}.
    std::vector<output_ordinal_t> disk_ord2output(disk_ord2index.size());
    for (size_t i = 0; i < disk_ord2index.size(); ++i) {
        disk_ord2output[disk_ord2index[i]] = static_cast<output_ordinal_t>(i);
    }
    for (int disk_idx = 0; disk_idx < static_cast<output_ordinal_t>(outputs_.size());
         ++disk_idx) {
        if (disk_idx >= static_cast<int>(disk_ord2index.size())) {
            // XXX i#6630: We should auto-set the output count and avoid
            // having extra ouputs; these complicate idle computations, etc.
            VPRINT(this, 1, "Output %d empty: returning eof up front\n", disk_idx);
            outputs_[disk_idx].at_eof = true;
            set_cur_input(disk_idx, sched_type_t::INVALID_INPUT_ORDINAL);
            continue;
        }
        output_ordinal_t output_idx = disk_ord2output[disk_idx];
        VPRINT(this, 1, "Read %zu as-traced records for output #%d\n",
               all_sched[disk_idx].size(), output_idx);
        outputs_[output_idx].as_traced_cpuid = disk_ord2cpuid[disk_idx];
        VPRINT(this, 1, "Output #%d is as-traced CPU #%" PRId64 "\n", output_idx,
               outputs_[output_idx].as_traced_cpuid);
        // Update the stop_instruction field and collapse consecutive entries while
        // inserting into the final location.
        int start_consec = -1;
        for (int sched_idx = 0; sched_idx < static_cast<int>(all_sched[disk_idx].size());
             ++sched_idx) {
            auto &segment = all_sched[disk_idx][sched_idx];
            if (!segment.valid)
                continue;
            auto find = start2stop[segment.input].find(segment.start_instruction);
            ++find;
            if (find == start2stop[segment.input].end())
                segment.stop_instruction = std::numeric_limits<uint64_t>::max();
            else
                segment.stop_instruction = *find;
            VPRINT(this, 4,
                   "as-read segment #%d: input=%d start=%" PRId64 " stop=%" PRId64
                   " time=%" PRId64 "\n",
                   sched_idx, segment.input, segment.start_instruction,
                   segment.stop_instruction, segment.timestamp);
            if (sched_idx + 1 < static_cast<int>(all_sched[disk_idx].size()) &&
                segment.input == all_sched[disk_idx][sched_idx + 1].input &&
                segment.stop_instruction >
                    all_sched[disk_idx][sched_idx + 1].start_instruction) {
                // A second sanity check.
                error_string_ = "Invalid decreasing start field in schedule file";
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            } else if (sched_idx + 1 < static_cast<int>(all_sched[disk_idx].size()) &&
                       segment.input == all_sched[disk_idx][sched_idx + 1].input &&
                       segment.stop_instruction ==
                           all_sched[disk_idx][sched_idx + 1].start_instruction) {
                // Collapse into next.
                if (start_consec == -1)
                    start_consec = sched_idx;
            } else {
                schedule_output_tracker_t &toadd = start_consec >= 0
                    ? all_sched[disk_idx][start_consec]
                    : all_sched[disk_idx][sched_idx];
                outputs_[output_idx].record.emplace_back(
                    schedule_record_t::DEFAULT, toadd.input, toadd.start_instruction,
                    all_sched[disk_idx][sched_idx].stop_instruction, toadd.timestamp);
                start_consec = -1;
                VDO(this, 3, {
                    auto &added = outputs_[output_idx].record.back();
                    VPRINT(this, 3,
                           "segment #%zu: input=%d start=%" PRId64 " stop=%" PRId64
                           " time=%" PRId64 "\n",
                           outputs_[output_idx].record.size() - 1, added.key.input,
                           added.value.start_instruction, added.stop_instruction,
                           added.timestamp);
                });
            }
        }
        VPRINT(this, 1, "Collapsed duplicates for %zu as-traced records for output #%d\n",
               outputs_[output_idx].record.size(), output_idx);
        if (outputs_[output_idx].record.empty()) {
            error_string_ = "Empty as-traced schedule";
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        }
        if (outputs_[output_idx].record[0].value.start_instruction != 0) {
            VPRINT(this, 1, "Initial input for output #%d is: wait state\n", output_idx);
            set_cur_input(output_idx, sched_type_t::INVALID_INPUT_ORDINAL);
            outputs_[output_idx].waiting = true;
            outputs_[output_idx].record_index->store(-1, std::memory_order_release);
        } else {
            VPRINT(this, 1, "Initial input for output #%d is %d\n", output_idx,
                   outputs_[output_idx].record[0].key.input);
            set_cur_input(output_idx, outputs_[output_idx].record[0].key.input);
        }
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::swap_out_input(
    output_ordinal_t output, input_ordinal_t input, bool caller_holds_input_lock)
{
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::swap_in_input(output_ordinal_t output,
                                                               input_ordinal_t input)
{
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::pick_next_input_for_mode(
    output_ordinal_t output, uint64_t blocked_time, input_ordinal_t prev_index,
    input_ordinal_t &index)
{
    // Our own index is only modified by us so we can cache it here.
    int record_index = outputs_[output].record_index->load(std::memory_order_acquire);
    if (record_index + 1 >= static_cast<int>(outputs_[output].record.size())) {
        if (!outputs_[output].at_eof) {
            outputs_[output].at_eof = true;
            this->live_replay_output_count_.fetch_add(-1, std::memory_order_release);
        }
        return this->eof_or_idle(output, outputs_[output].cur_input);
    }
    schedule_record_t &segment = outputs_[output].record[record_index + 1];
    if (segment.type == schedule_record_t::IDLE ||
        segment.type == schedule_record_t::IDLE_BY_COUNT) {
        outputs_[output].waiting = true;
        if (segment.type == schedule_record_t::IDLE) {
            // Convert a legacy idle duration from microseconds to record counts.
            segment.value.idle_duration = static_cast<uint64_t>(
                options_.time_units_per_us * segment.value.idle_duration);
        }
        outputs_[output].idle_start_count = outputs_[output].idle_count;
        outputs_[output].record_index->fetch_add(1, std::memory_order_release);
        ++outputs_[output].idle_count;
        VPRINT(this, 5, "%s[%d]: next replay segment idle for %" PRIu64 "\n",
               __FUNCTION__, output, segment.value.idle_duration);
        return sched_type_t::STATUS_IDLE;
    }
    index = segment.key.input;
    VPRINT(this, 5,
           "%s[%d]: next replay segment in=%d (@%" PRId64 ") type=%d start=%" PRId64
           " end=%" PRId64 "\n",
           __FUNCTION__, output, index, this->get_instr_ordinal(inputs_[index]),
           segment.type, segment.value.start_instruction, segment.stop_instruction);
    {
        std::lock_guard<mutex_dbg_owned> lock(*inputs_[index].lock);
        if (this->get_instr_ordinal(inputs_[index]) > segment.value.start_instruction) {
            VPRINT(this, 1,
                   "WARNING: next_record[%d]: input %d wants instr #%" PRId64
                   " but it is already at #%" PRId64 "\n",
                   output, index, segment.value.start_instruction,
                   this->get_instr_ordinal(inputs_[index]));
        }
        if (this->get_instr_ordinal(inputs_[index]) < segment.value.start_instruction &&
            // Don't wait for an ROI that starts at the beginning.
            segment.value.start_instruction > 1 &&
            // The output may have begun in the wait state.
            (record_index == -1 ||
             // When we skip our separator+timestamp markers are at the
             // prior instr ord so do not wait for that.
             (outputs_[output].record[record_index].type != schedule_record_t::SKIP &&
              // Don't wait if we're at the end and just need the end record.
              segment.type != schedule_record_t::SYNTHETIC_END))) {
            // If the input is at eof it's an error: maybe the inputs are not identical
            // to the recording or something.
            if (inputs_[index].at_eof) {
                VPRINT(this, 1,
                       "next_record[%d]: want input %d instr #%" PRId64
                       " but input is at EOF\n",
                       output, index, segment.value.start_instruction);
                return sched_type_t::STATUS_INVALID;
            }
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
            set_cur_input(output, sched_type_t::INVALID_INPUT_ORDINAL,
                          // Avoid livelock if prev input == cur input which happens
                          // with back-to-back segments with the same input.
                          index == outputs_[output].cur_input);
            outputs_[output].waiting = true;
            return sched_type_t::STATUS_WAIT;
        }
    }
    // Also wait if this segment is ahead of the next-up segment on another
    // output.  We only have a timestamp per context switch so we can't
    // enforce finer-grained timing replay.
    if (options_.deps == sched_type_t::DEPENDENCY_TIMESTAMPS) {
        for (int i = 0; i < static_cast<output_ordinal_t>(outputs_.size()); ++i) {
            if (i == output)
                continue;
            // Do an atomic load once and use it to de-reference if it's not at the end.
            // This is safe because if the target advances to the end concurrently it
            // will only cause an extra wait that will just come back here and then
            // continue.
            int other_index = outputs_[i].record_index->load(std::memory_order_acquire);
            if (other_index + 1 < static_cast<int>(outputs_[i].record.size()) &&
                segment.timestamp > outputs_[i].record[other_index + 1].timestamp) {
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
                set_cur_input(output, sched_type_t::INVALID_INPUT_ORDINAL);
                outputs_[output].waiting = true;
                return sched_type_t::STATUS_WAIT;
            }
        }
    }
    if (segment.type == schedule_record_t::SYNTHETIC_END) {
        std::lock_guard<mutex_dbg_owned> lock(*inputs_[index].lock);
        // We're past the final region of interest and we need to insert
        // a synthetic thread exit record.  We need to first throw out the
        // queued candidate record, if any.
        this->clear_input_queue(inputs_[index]);
        inputs_[index].queue.emplace_back(this->create_thread_exit(inputs_[index].tid));
        VPRINT(this, 2, "early end for input %d\n", index);
        // We're done with this entry but we need the queued record to be read,
        // so we do not move past the entry.
        outputs_[output].record_index->fetch_add(1, std::memory_order_release);
        stream_status_t status = this->mark_input_eof(inputs_[index]);
        if (status != sched_type_t::STATUS_OK)
            return status;
        return sched_type_t::STATUS_SKIPPED;
    } else if (segment.type == schedule_record_t::SKIP) {
        std::lock_guard<mutex_dbg_owned> lock(*inputs_[index].lock);
        uint64_t cur_reader_instr = inputs_[index].reader->get_instruction_ordinal();
        VPRINT(this, 2,
               "next_record[%d]: skipping from %" PRId64 " to %" PRId64
               " in %d for schedule\n",
               output, cur_reader_instr, segment.stop_instruction, index);
        auto status = this->skip_instructions(inputs_[index],
                                              segment.stop_instruction -
                                                  cur_reader_instr - 1 /*exclusive*/);
        // Increment the region to get window id markers with ordinals.
        inputs_[index].cur_region++;
        if (status != sched_type_t::STATUS_SKIPPED)
            return sched_type_t::STATUS_INVALID;
        // We're done with the skip so move to and past it.
        outputs_[output].record_index->fetch_add(2, std::memory_order_release);
        return sched_type_t::STATUS_SKIPPED;
    } else {
        VPRINT(this, 2, "next_record[%d]: advancing to input %d instr #%" PRId64 "\n",
               output, index, segment.value.start_instruction);
    }
    outputs_[output].record_index->fetch_add(1, std::memory_order_release);
    VDO(this, 2, {
        // Our own index is only modified by us so we can cache it here.
        int local_index = outputs_[output].record_index->load(std::memory_order_acquire);
        if (local_index >= 0 &&
            local_index < static_cast<int>(outputs_[output].record.size())) {
            const schedule_record_t &local_segment = outputs_[output].record[local_index];
            int input = local_segment.key.input;
            VPRINT(this, 2,
                   "next_record[%d]: replay segment in=%d (@%" PRId64
                   ") type=%d start=%" PRId64 " end=%" PRId64 "\n",
                   output, input, this->get_instr_ordinal(inputs_[input]),
                   local_segment.type, local_segment.value.start_instruction,
                   local_segment.stop_instruction);
        }
    });
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::check_for_input_switch(
    output_ordinal_t output, RecordType &record, input_info_t *input, uint64_t cur_time,
    bool &need_new_input, bool &preempt, uint64_t &blocked_time)
{
    // Our own index is only modified by us so we can cache it here.
    int record_index = outputs_[output].record_index->load(std::memory_order_acquire);
    assert(record_index >= 0);
    if (record_index >= static_cast<int>(outputs_[output].record.size())) {
        // We're on the last record.
        VPRINT(this, 4, "next_record[%d]: on last record\n", output);
    } else if (outputs_[output].record[record_index].type == schedule_record_t::SKIP) {
        VPRINT(this, 5, "next_record[%d]: need new input after skip\n", output);
        need_new_input = true;
    } else if (outputs_[output].record[record_index].type ==
               schedule_record_t::SYNTHETIC_END) {
        VPRINT(this, 5, "next_record[%d]: at synthetic end\n", output);
    } else {
        const schedule_record_t &segment = outputs_[output].record[record_index];
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

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_replay_tmpl_t<RecordType, ReaderType>::eof_or_idle_for_mode(
    output_ordinal_t output, input_ordinal_t prev_input)
{
    if (this->live_input_count_.load(std::memory_order_acquire) == 0 ||
        // While a full schedule recorded should have each input hit either its
        // EOF or ROI end, we have a fallback to avoid hangs for possible recorded
        // schedules that end an input early deliberately without an ROI.
        (options_.mapping == sched_type_t::MAP_AS_PREVIOUSLY &&
         this->live_replay_output_count_.load(std::memory_order_acquire) == 0)) {
        assert(options_.mapping != sched_type_t::MAP_AS_PREVIOUSLY ||
               outputs_[output].at_eof);
        return sched_type_t::STATUS_EOF;
    }
    return sched_type_t::STATUS_IDLE;
}

template class scheduler_replay_tmpl_t<memref_t, reader_t>;
template class scheduler_replay_tmpl_t<trace_entry_t,
                                       dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
