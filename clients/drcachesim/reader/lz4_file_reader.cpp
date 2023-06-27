/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

#include "lz4_file_reader.h"

namespace dynamorio {
namespace drmemtrace {

trace_entry_t *
read_next_entry_common(lz4_reader_t *reader, bool *eof)
{
    if (reader->cur_buf >= reader->max_buf) {
        int len = reader->file
                      ->read(reinterpret_cast<char *>(&reader->buf), sizeof(reader->buf))
                      .gcount();
        if (len < static_cast<int>(sizeof(trace_entry_t)) ||
            len % static_cast<int>(sizeof(trace_entry_t)) != 0) {
            *eof = (len >= 0);
            return nullptr;
        }
        reader->cur_buf = reader->buf;
        reader->max_buf = reader->buf + (len / sizeof(trace_entry_t));
    }
    trace_entry_t *res = reader->cur_buf;
    ++reader->cur_buf;
    return res;
}

/**************************************************
 * lz4_reader_t specializations for file_reader_t.
 */

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<lz4_reader_t>::file_reader_t()
{
    input_file_.file = nullptr;
}

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<lz4_reader_t>::~file_reader_t<lz4_reader_t>()
{
    if (input_file_.file != nullptr) {
        delete input_file_.file;
        input_file_.file = nullptr;
    }
}

template <>
bool
file_reader_t<lz4_reader_t>::open_single_file(const std::string &path)
{
    auto file = new lz4_istream_t(path);
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    input_file_ = lz4_reader_t(file);
    return true;
}

template <>
trace_entry_t *
file_reader_t<lz4_reader_t>::read_next_entry()
{
    trace_entry_t *entry = read_queued_entry();
    if (entry != nullptr)
        return entry;
    entry = read_next_entry_common(&input_file_, &at_eof_);
    if (entry == nullptr)
        return entry;
    VPRINT(this, 4, "Read from file: type=%s (%d), size=%d, addr=%zu\n",
           trace_type_names[entry->type], entry->type, entry->size, entry->addr);
    entry_copy_ = *entry;
    return &entry_copy_;
}

} // namespace drmemtrace
} // namespace dynamorio
