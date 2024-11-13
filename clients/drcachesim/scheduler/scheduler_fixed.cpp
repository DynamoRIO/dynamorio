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

#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <mutex>
#include <thread>

#include "memref.h"
#include "mutex_dbg_owned.h"
#include "reader.h"
#include "record_file_reader.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_fixed_tmpl_t<RecordType, ReaderType>::pick_next_input_for_mode(
    output_ordinal_t output, uint64_t blocked_time, input_ordinal_t prev_index,
    input_ordinal_t &index)
{
    if (this->options_.deps == sched_type_t::DEPENDENCY_TIMESTAMPS) {
        uint64_t min_time = std::numeric_limits<uint64_t>::max();
        for (size_t i = 0; i < this->inputs_.size(); ++i) {
            std::lock_guard<mutex_dbg_owned> lock(*this->inputs_[i].lock);
            if (!this->inputs_[i].at_eof && this->inputs_[i].next_timestamp > 0 &&
                this->inputs_[i].next_timestamp < min_time) {
                min_time = this->inputs_[i].next_timestamp;
                index = static_cast<int>(i);
            }
        }
        if (index < 0) {
            stream_status_t status = this->eof_or_idle(output, prev_index);
            if (status != sched_type_t::STATUS_STOLE)
                return status;
            index = this->outputs_[output].cur_input;
            return sched_type_t::STATUS_OK;
        }
        VPRINT(this, 2,
               "next_record[%d]: advancing to timestamp %" PRIu64 " == input #%d\n",
               output, min_time, index);
    } else if (this->options_.mapping == sched_type_t::MAP_TO_CONSISTENT_OUTPUT) {
        // We're done with the prior thread; take the next one that was
        // pre-allocated to this output (pre-allocated to avoid locks). Invariant:
        // the same output will not be accessed by two different threads
        // simultaneously in this mode, allowing us to support a lock-free
        // parallel-friendly increment here.
        int indices_index = ++this->outputs_[output].input_indices_index;
        if (indices_index >=
            static_cast<int>(this->outputs_[output].input_indices.size())) {
            VPRINT(this, 2, "next_record[%d]: all at eof\n", output);
            return sched_type_t::STATUS_EOF;
        }
        index = this->outputs_[output].input_indices[indices_index];
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
    if (this->options_.deps == sched_type_t::DEPENDENCY_TIMESTAMPS &&
        this->record_type_is_timestamp(record, input->next_timestamp))
        need_new_input = true;
    return sched_type_t::STATUS_OK;
}

template class scheduler_fixed_tmpl_t<memref_t, reader_t>;
template class scheduler_fixed_tmpl_t<trace_entry_t,
                                      dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
