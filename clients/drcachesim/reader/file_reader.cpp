/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

#include "file_reader.h"

#include <fstream>
#include <string>

#include "reader.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<std::ifstream *>::file_reader_t()
{
    input_file_ = nullptr;
}

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<std::ifstream *>::~file_reader_t()
{
    if (input_file_ != nullptr) {
        delete input_file_;
        input_file_ = nullptr;
    }
}

template <>
bool
file_reader_t<std::ifstream *>::open_single_file(const std::string &path)
{
    input_file_ = new std::ifstream(path, std::ifstream::binary);
    if (!*input_file_)
        return false;
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    return true;
}

template <>
trace_entry_t *
file_reader_t<std::ifstream *>::read_next_entry()
{
    trace_entry_t *from_queue = read_queued_entry();
    if (from_queue != nullptr)
        return from_queue;
    if (!input_file_->read((char *)&entry_copy_, sizeof(entry_copy_))) {
        at_eof_ = input_file_->eof();
        return nullptr;
    }
    VPRINT(this, 4, "Read from file: type=%s (%d), size=%d, addr=%zu\n",
           trace_type_names[entry_copy_.type], entry_copy_.type, entry_copy_.size,
           entry_copy_.addr);
    return &entry_copy_;
}

} // namespace drmemtrace
} // namespace dynamorio
