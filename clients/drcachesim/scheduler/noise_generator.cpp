/* **********************************************************
 * Copyright (c) 2025 Google, Inc.  All rights reserved.
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

#include <assert.h>
#include <climits>

#include "noise_generator.h"
#include "memref.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

noise_generator_t::noise_generator_t()
{
}

noise_generator_t::noise_generator_t(noise_generator_info_t &info)
    : num_records_to_generate_(info.num_records_to_generate)
    , pid_(info.pid)
    , tid_(info.tid)
{
}

noise_generator_t::~noise_generator_t()
{
}

bool
noise_generator_t::init()
{
    at_eof_ = false;
    ++*this;
    return true;
}

std::string
noise_generator_t::get_stream_name() const
{
    return "noise_generator";
}

trace_entry_t
noise_generator_t::generate_trace_entry()
{
    // TODO i#7216: this is a temporary trace record that we use as a placeholder until
    // the logic to generate noise records is in place.
    trace_entry_t generated_entry = { TRACE_TYPE_READ, 4, { 0xdeadbeef } };
    return generated_entry;
}

trace_entry_t *
noise_generator_t::read_next_entry()
{
    if (num_records_to_generate_ == 0) {
        at_eof_ = true;
        return nullptr;
    }

    // Do not change the order for generating TRACE_TYPE_THREAD and TRACE_TYPE_PID.
    // The scheduler expects a tid first and then a pid.
    if (!tid_generated_) {
        entry_ = { TRACE_TYPE_THREAD, sizeof(int), { tid_ } };
        tid_generated_ = true;
        return &entry_;
    }
    if (!pid_generated_) {
        entry_ = { TRACE_TYPE_PID, sizeof(int), { pid_ } };
        pid_generated_ = true;
        return &entry_;
    }
    if (!marker_timestamp_generated_) {
        entry_ = { TRACE_TYPE_MARKER,
                   TRACE_MARKER_TYPE_TIMESTAMP,
                   { static_cast<addr_t>(ULONG_MAX - 1) } };
        marker_timestamp_generated_ = true;
        return &entry_;
    }

    entry_ = generate_trace_entry();

    if (num_records_to_generate_ == 1) {
        entry_ = { TRACE_TYPE_THREAD_EXIT, sizeof(int), { tid_ } };
    }
    --num_records_to_generate_;

    return &entry_;
}

template <typename RecordType, typename ReaderType>
std::string
noise_generator_factory_t<RecordType, ReaderType>::get_error_string()
{
    return error_string_;
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::input_reader_t
noise_generator_factory_t<RecordType, ReaderType>::create_noise_generator(
    noise_generator_info_t &info)
{
    std::unique_ptr<ReaderType> noise_generator_begin =
        create_noise_generator_begin(info);
    std::unique_ptr<ReaderType> noise_generator_end = create_noise_generator_end();
    typename sched_type_t::input_reader_t reader(
        std::move(noise_generator_begin), std::move(noise_generator_end), info.tid);
    return reader;
}

template <>
std::unique_ptr<reader_t>
noise_generator_factory_t<memref_t, reader_t>::create_noise_generator_begin(
    noise_generator_info_t &info)
{
    return std::unique_ptr<noise_generator_t>(new noise_generator_t(info));
}

template <>
std::unique_ptr<reader_t>
noise_generator_factory_t<memref_t, reader_t>::create_noise_generator_end()
{
    return std::unique_ptr<noise_generator_t>(new noise_generator_t());
}

template <>
std::unique_ptr<record_reader_t>
noise_generator_factory_t<trace_entry_t, record_reader_t>::create_noise_generator_begin(
    noise_generator_info_t &info)
{
    // TODO i#7216: we'll need a record_reader_t noise generator to create core sharded
    // traces via record_filter_t.
    error_string_ = "Noise generator is not suppported for record_reader_t";
    return std::unique_ptr<dynamorio::drmemtrace::record_reader_t>();
}

template <>
std::unique_ptr<record_reader_t>
noise_generator_factory_t<trace_entry_t, record_reader_t>::create_noise_generator_end()
{
    // TODO i#7216: we'll need a record_reader_t noise generator to create core sharded
    // traces via record_filter_t.
    error_string_ = "Noise generator is not suppported for record_reader_t";
    return std::unique_ptr<dynamorio::drmemtrace::record_reader_t>();
}

template class noise_generator_factory_t<memref_t, reader_t>;
template class noise_generator_factory_t<trace_entry_t, record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
