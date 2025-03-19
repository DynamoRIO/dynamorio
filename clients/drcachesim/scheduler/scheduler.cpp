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

#include "scheduler.h"

#include <cinttypes>
#include <memory>
#include <mutex>

#include "scheduler_impl.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::init(
    std::vector<input_workload_t> &workload_inputs, int output_count,
    scheduler_options_t options)
{
    if (options.mapping == sched_type_t::MAP_TO_ANY_OUTPUT) {
        impl_ = std::unique_ptr<scheduler_impl_tmpl_t<RecordType, ReaderType>,
                                scheduler_impl_deleter_t>(
            new scheduler_dynamic_tmpl_t<RecordType, ReaderType>);
    } else if (options.mapping == sched_type_t::MAP_AS_PREVIOUSLY ||
               (options.mapping == sched_type_t::MAP_TO_RECORDED_OUTPUT &&
                options.replay_as_traced_istream != nullptr)) {
        impl_ = std::unique_ptr<scheduler_impl_tmpl_t<RecordType, ReaderType>,
                                scheduler_impl_deleter_t>(
            new scheduler_replay_tmpl_t<RecordType, ReaderType>);
    } else {
        // Non-dynamic and non-replay fixed modes such as analyzer
        // parallel mode with a static mapping of inputs to outputs and analyzer
        // serial mode with a simple time interleaving of all inputs onto one output.
        impl_ = std::unique_ptr<scheduler_impl_tmpl_t<RecordType, ReaderType>,
                                scheduler_impl_deleter_t>(
            new scheduler_fixed_tmpl_t<RecordType, ReaderType>);
    }
    return impl_->init(workload_inputs, output_count, std::move(options));
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_t *
scheduler_tmpl_t<RecordType, ReaderType>::get_stream(output_ordinal_t ordinal)
{
    return impl_->get_stream(ordinal);
}

template <typename RecordType, typename ReaderType>
int
scheduler_tmpl_t<RecordType, ReaderType>::get_input_stream_count() const
{
    return impl_->get_input_stream_count();
}

template <typename RecordType, typename ReaderType>
memtrace_stream_t *
scheduler_tmpl_t<RecordType, ReaderType>::get_input_stream_interface(
    input_ordinal_t input) const
{
    return impl_->get_input_stream_interface(input);
}

template <typename RecordType, typename ReaderType>
std::string
scheduler_tmpl_t<RecordType, ReaderType>::get_input_stream_name(
    input_ordinal_t input) const
{
    return impl_->get_input_stream_name(input);
}

template <typename RecordType, typename ReaderType>
int64_t
scheduler_tmpl_t<RecordType, ReaderType>::get_output_cpuid(output_ordinal_t output) const
{
    return impl_->get_output_cpuid(output);
}

template <typename RecordType, typename ReaderType>
std::string
scheduler_tmpl_t<RecordType, ReaderType>::get_error_string() const
{
    return impl_->get_error_string();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::write_recorded_schedule()
{
    return impl_->write_recorded_schedule();
}

template <typename RecordType, typename ReaderType>
void
scheduler_tmpl_t<RecordType, ReaderType>::scheduler_impl_deleter_t::operator()(
    scheduler_impl_tmpl_t<RecordType, ReaderType> *p)
{
    delete p;
}

/***************************************************************************
 * Scheduled stream.
 */

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::next_record(RecordType &record)
{
    return next_record(record, 0);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::next_record(RecordType &record,
                                                                uint64_t cur_time)
{
    if (max_ordinal_ > 0) {
        ++ordinal_;
        if (ordinal_ >= max_ordinal_)
            ordinal_ = 0;
    }
    typename scheduler_impl_tmpl_t<RecordType, ReaderType>::input_info_t *input = nullptr;
    sched_type_t::stream_status_t res =
        scheduler_->next_record(ordinal_, record, input, cur_time);
    if (res != sched_type_t::STATUS_OK)
        return res;

    // Update our memtrace_stream_t state.
    std::lock_guard<mutex_dbg_owned> guard(*input->lock);
    if (!input->reader->is_record_synthetic())
        ++cur_ref_count_;
    if (scheduler_->record_type_is_instr_boundary(record, prev_record_))
        ++cur_instr_count_;
    VPRINT(scheduler_, 5,
           "stream record#=%" PRId64 ", instr#=%" PRId64 " (cur input %" PRId64
           " record#=%" PRId64 ", instr#=%" PRId64 ")\n",
           cur_ref_count_, cur_instr_count_, input->tid,
           input->reader->get_record_ordinal(), input->reader->get_instruction_ordinal());

    // Update our header and other state.
    // If we skipped over these, advance_region_of_interest() sets them.
    // TODO i#5843: Check that all inputs have the same top-level headers here.
    // A possible exception is allowing warmup-phase-filtered traces to be mixed
    // with regular traces.
    trace_marker_type_t marker_type;
    uintptr_t marker_value;
    if (scheduler_->record_type_is_marker(record, marker_type, marker_value)) {
        switch (marker_type) {
        case TRACE_MARKER_TYPE_TIMESTAMP:
            last_timestamp_ = marker_value;
            if (first_timestamp_ == 0)
                first_timestamp_ = last_timestamp_;
            break;

        case TRACE_MARKER_TYPE_VERSION: version_ = marker_value; break;
        case TRACE_MARKER_TYPE_FILETYPE: filetype_ = marker_value; break;
        case TRACE_MARKER_TYPE_CACHE_LINE_SIZE: cache_line_size_ = marker_value; break;
        case TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT:
            chunk_instr_count_ = marker_value;
            break;
        case TRACE_MARKER_TYPE_PAGE_SIZE: page_size_ = marker_value; break;
        // While reader_t tracks kernel state, if we dynamically inject a sequence
        // the input readers will not see it: so we need our own state here.
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
        case TRACE_MARKER_TYPE_CONTEXT_SWITCH_START: in_kernel_trace_ = true; break;
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
        case TRACE_MARKER_TYPE_CONTEXT_SWITCH_END: in_kernel_trace_ = false; break;
        default: // No action needed.
            break;
        }
    }
    prev_record_ = record;
    return sched_type_t::STATUS_OK;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::unread_last_record()
{
    RecordType record;
    typename scheduler_impl_tmpl_t<RecordType, ReaderType>::input_info_t *input = nullptr;
    auto status = scheduler_->unread_last_record(ordinal_, record, input);
    if (status != sched_type_t::STATUS_OK)
        return status;
    // Restore state.  We document that get_last_timestamp() is not updated.
    std::lock_guard<mutex_dbg_owned> guard(*input->lock);
    if (!input->reader->is_record_synthetic())
        --cur_ref_count_;
    if (scheduler_->record_type_is_instr(record))
        --cur_instr_count_;
    return status;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::start_speculation(
    addr_t start_address, bool queue_current_record)
{
    return scheduler_->start_speculation(ordinal_, start_address, queue_current_record);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::stop_speculation()
{
    return scheduler_->stop_speculation(ordinal_);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::stream_status_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::set_active(bool active)
{
    return scheduler_->set_output_active(ordinal_, active);
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_record_ordinal() const
{
    if (TESTANY(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS, scheduler_->options_.flags))
        return scheduler_->get_input_record_ordinal(ordinal_);
    return cur_ref_count_;
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_instruction_ordinal() const

{
    if (TESTANY(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS, scheduler_->options_.flags))
        return scheduler_->get_input_stream(ordinal_)->get_instruction_ordinal();
    return cur_instr_count_;
}

template <typename RecordType, typename ReaderType>
std::string
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_stream_name() const
{
    return scheduler_->get_input_name(ordinal_);
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::input_ordinal_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_input_stream_ordinal() const
{
    return scheduler_->get_input_ordinal(ordinal_);
}

template <typename RecordType, typename ReaderType>
int
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_input_workload_ordinal() const
{
    return scheduler_->get_workload_ordinal(ordinal_);
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_last_timestamp() const
{
    if (TESTANY(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS, scheduler_->options_.flags))
        return scheduler_->get_input_last_timestamp(ordinal_);
    return last_timestamp_;
}

template <typename RecordType, typename ReaderType>
uint64_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_first_timestamp() const
{
    if (TESTANY(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS, scheduler_->options_.flags))
        return scheduler_->get_input_first_timestamp(ordinal_);
    return first_timestamp_;
}

template <typename RecordType, typename ReaderType>
bool
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::is_record_synthetic() const
{
    return scheduler_->is_record_synthetic(ordinal_);
}

template <typename RecordType, typename ReaderType>
int64_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_output_cpuid() const
{
    return scheduler_->get_output_cpuid(ordinal_);
}

template <typename RecordType, typename ReaderType>
int64_t
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_tid() const
{
    return scheduler_->get_tid(ordinal_);
}

template <typename RecordType, typename ReaderType>
memtrace_stream_t *
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_input_interface() const
{
    return scheduler_->get_input_stream_interface(get_input_stream_ordinal());
}

template <typename RecordType, typename ReaderType>
int
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_shard_index() const
{
    return scheduler_->get_shard_index(ordinal_);
}

template <typename RecordType, typename ReaderType>
bool
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::is_record_kernel() const
{
    return in_kernel_trace_;
}

template <typename RecordType, typename ReaderType>
double
scheduler_tmpl_t<RecordType, ReaderType>::stream_t::get_schedule_statistic(
    schedule_statistic_t stat) const
{
    return scheduler_->get_statistic(ordinal_, stat);
}

template class scheduler_tmpl_t<memref_t, reader_t>;
template class scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
