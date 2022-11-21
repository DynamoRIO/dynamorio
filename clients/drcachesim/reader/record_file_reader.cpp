/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

#include <fstream>
#include "record_file_reader.h"

namespace dynamorio {
namespace drmemtrace {

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
record_file_reader_t<std::ifstream>::~record_file_reader_t<std::ifstream>()
{
}

template <>
bool
record_file_reader_t<std::ifstream>::open_single_file(const std::string &path)
{
    auto fstream =
        std::unique_ptr<std::ifstream>(new std::ifstream(path, std::ifstream::binary));
    if (!*fstream)
        return false;
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    input_file_ = std::move(fstream);
    return true;
}

template <>
bool
record_file_reader_t<std::ifstream>::read_next_entry()
{
    if (!input_file_->read((char *)&cur_entry_, sizeof(cur_entry_)))
        return false;
    VPRINT(this, 4, "Read from file: type=%s (%d), size=%d, addr=%zu\n",
           trace_type_names[cur_entry_.type], cur_entry_.type, cur_entry_.size,
           cur_entry_.addr);
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
