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

#include "scheduler.h"

#include <memory>

#include "scheduler_impl.h"

namespace dynamorio {
namespace drmemtrace {

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_status_t
scheduler_tmpl_t<RecordType, ReaderType>::init(
    std::vector<input_workload_t> &workload_inputs, int output_count,
    scheduler_options_t options)
{
    // TODO i#6831: Split up scheduler_impl_tmpl_t by mode and create the
    // mode-appropriate subclass here.
    impl_ = std::unique_ptr<scheduler_impl_tmpl_t<RecordType, ReaderType>,
                            scheduler_impl_deleter_t>(
        new scheduler_impl_tmpl_t<RecordType, ReaderType>);
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

template class scheduler_tmpl_t<memref_t, reader_t>;
template class scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
