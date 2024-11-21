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

/* Scheduler fixed-schedule-specific code. */

#include "scheduler.h"
#include "scheduler_impl.h"

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
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
scheduler_fixed_tmpl_t<RecordType, ReaderType>::set_initial_schedule()
{
    if (options_.mapping == sched_type_t::MAP_TO_CONSISTENT_OUTPUT) {
        // Assign the inputs up front to avoid locks once we're in parallel mode.
        // We use a simple round-robin static assignment for now.
        for (int i = 0; i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
            size_t index = i % outputs_.size();
            if (outputs_[index].input_indices.empty())
                set_cur_input(static_cast<input_ordinal_t>(index), i);
            outputs_[index].input_indices.push_back(i);
            VPRINT(this, 2, "Assigning input #%d to output #%zd\n", i, index);
        }
    } else if (options_.mapping == sched_type_t::MAP_TO_RECORDED_OUTPUT) {
        if (options_.replay_as_traced_istream != nullptr) {
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        } else if (outputs_.size() > 1) {
            return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
        } else if (inputs_.size() == 1) {
            set_cur_input(0, 0);
        } else {
            // The old file_reader_t interleaving would output the top headers for every
            // thread first and then pick the oldest timestamp once it reached a
            // timestamp. We instead queue those headers so we can start directly with the
            // oldest timestamp's thread.
            uint64_t min_time = std::numeric_limits<uint64_t>::max();
            input_ordinal_t min_input = -1;
            for (int i = 0; i < static_cast<input_ordinal_t>(inputs_.size()); ++i) {
                if (inputs_[i].next_timestamp < min_time) {
                    min_time = inputs_[i].next_timestamp;
                    min_input = i;
                }
            }
            if (min_input < 0)
                return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
            set_cur_input(0, static_cast<input_ordinal_t>(min_input));
        }
    } else {
        return sched_type_t::STATUS_ERROR_INVALID_PARAMETER;
    }
    return sched_type_t::STATUS_SUCCESS;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_fixed_tmpl_t<RecordType, ReaderType>::swap_out_input(
    output_ordinal_t output, input_ordinal_t input, bool caller_holds_input_lock)
{
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_fixed_tmpl_t<RecordType, ReaderType>::swap_in_input(output_ordinal_t output,
                                                              input_ordinal_t input)
{
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_fixed_tmpl_t<RecordType, ReaderType>::pick_next_input_for_mode(
    output_ordinal_t output, uint64_t blocked_time, input_ordinal_t prev_index,
    input_ordinal_t &index)
{
    if (options_.deps == sched_type_t::DEPENDENCY_TIMESTAMPS) {
        uint64_t min_time = std::numeric_limits<uint64_t>::max();
        for (size_t i = 0; i < inputs_.size(); ++i) {
            std::lock_guard<mutex_dbg_owned> lock(*inputs_[i].lock);
            if (!inputs_[i].at_eof && inputs_[i].next_timestamp > 0 &&
                inputs_[i].next_timestamp < min_time) {
                min_time = inputs_[i].next_timestamp;
                index = static_cast<int>(i);
            }
        }
        if (index < 0) {
            stream_status_t status = this->eof_or_idle(output, prev_index);
            if (status != sched_type_t::STATUS_STOLE)
                return status;
            index = outputs_[output].cur_input;
            return sched_type_t::STATUS_OK;
        }
        VPRINT(this, 2,
               "next_record[%d]: advancing to timestamp %" PRIu64 " == input #%d\n",
               output, min_time, index);
    } else if (options_.mapping == sched_type_t::MAP_TO_CONSISTENT_OUTPUT) {
        // We're done with the prior thread; take the next one that was
        // pre-allocated to this output (pre-allocated to avoid locks). Invariant:
        // the same output will not be accessed by two different threads
        // simultaneously in this mode, allowing us to support a lock-free
        // parallel-friendly increment here.
        int indices_index = ++outputs_[output].input_indices_index;
        if (indices_index >= static_cast<int>(outputs_[output].input_indices.size())) {
            VPRINT(this, 2, "next_record[%d]: all at eof\n", output);
            return sched_type_t::STATUS_EOF;
        }
        index = outputs_[output].input_indices[indices_index];
        VPRINT(this, 2, "next_record[%d]: advancing to local index %d == input #%d\n",
               output, indices_index, index);
    } else
        return sched_type_t::STATUS_INVALID;

    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_fixed_tmpl_t<RecordType, ReaderType>::check_for_input_switch(
    output_ordinal_t output, RecordType &record, input_info_t *input, uint64_t cur_time,
    bool &need_new_input, bool &preempt, uint64_t &blocked_time)
{
    if (options_.deps == sched_type_t::DEPENDENCY_TIMESTAMPS &&
        this->record_type_is_timestamp(record, input->next_timestamp))
        need_new_input = true;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_fixed_tmpl_t<RecordType, ReaderType>::eof_or_idle_for_mode(
    output_ordinal_t output, input_ordinal_t prev_input)
{
    if (options_.mapping == sched_type_t::MAP_TO_CONSISTENT_OUTPUT ||
        this->live_input_count_.load(std::memory_order_acquire) == 0) {
        return sched_type_t::STATUS_EOF;
    }
    return sched_type_t::STATUS_IDLE;
}

template class scheduler_fixed_tmpl_t<memref_t, reader_t>;
template class scheduler_fixed_tmpl_t<trace_entry_t,
                                      dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
