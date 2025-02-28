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
#include "noise_generator.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

noise_generator_t::noise_generator_t()
{
}

noise_generator_t::noise_generator_t(addr_t pid, addr_t tid,
                                     uint64_t num_records_to_generate)
    : num_records_to_generate_(num_records_to_generate)
    , pid_(pid)
    , tid_(tid)
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
    if (!marker_tid_generated_) {
        entry_ = { TRACE_TYPE_THREAD, sizeof(int), { tid_ } };
        marker_tid_generated_ = true;
        return &entry_;
    }
    if (!marker_pid_generated_) {
        entry_ = { TRACE_TYPE_PID, sizeof(int), { pid_ } };
        marker_pid_generated_ = true;
        return &entry_;
    }

    entry_ = generate_trace_entry();

    if (num_records_to_generate_ == 1) {
        entry_ = { TRACE_TYPE_THREAD_EXIT, sizeof(int), { tid_ } };
    }
    --num_records_to_generate_;

    return &entry_;
}

} // namespace drmemtrace
} // namespace dynamorio
