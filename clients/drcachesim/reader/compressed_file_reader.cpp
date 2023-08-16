/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

#include "compressed_file_reader.h"

#include <zlib.h>

#include <memory>
#include <string>

#include "file_reader.h"
#include "record_file_reader.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

/**************************************************************************
 * Common logic used in the gzip_reader_t specializations for file_reader_t
 * and record_file_reader_t.
 */

bool
open_single_file_common(const std::string &path, gzFile &out)
{
    out = gzopen(path.c_str(), "rb");
    return out != nullptr;
}

trace_entry_t *
read_next_entry_common(gzip_reader_t *gzip, bool *eof)
{
    if (gzip->cur_buf >= gzip->max_buf) {
        int len = gzread(gzip->file, gzip->buf, sizeof(gzip->buf));
        // Returns less than asked-for if at end of file, or –1 for error.
        // We should always get a multiple of the record size.
        if (len < static_cast<int>(sizeof(trace_entry_t)) ||
            len % static_cast<int>(sizeof(trace_entry_t)) != 0) {
            *eof = (len >= 0);
            return nullptr;
        }
        gzip->cur_buf = gzip->buf;
        gzip->max_buf = gzip->buf + (len / sizeof(*gzip->max_buf));
    }
    trace_entry_t *res = gzip->cur_buf;
    ++gzip->cur_buf;
    return res;
}

/**************************************************
 * gzip_reader_t specializations for file_reader_t.
 */

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<gzip_reader_t>::file_reader_t()
{
    input_file_.file = nullptr;
}

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<gzip_reader_t>::~file_reader_t<gzip_reader_t>()
{
    if (input_file_.file != nullptr) {
        gzclose(input_file_.file);
        input_file_.file = nullptr;
    }
}

template <>
bool
file_reader_t<gzip_reader_t>::open_single_file(const std::string &path)
{
    gzFile file;
    if (!open_single_file_common(path, file))
        return false;
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    input_file_ = gzip_reader_t(file);
    return true;
}

template <>
trace_entry_t *
file_reader_t<gzip_reader_t>::read_next_entry()
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

/*********************************************************
 * gzip_reader_t specializations for record_file_reader_t.
 */

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
record_file_reader_t<gzip_reader_t>::~record_file_reader_t<gzip_reader_t>()
{
    if (input_file_ != nullptr) {
        gzclose(input_file_->file);
    }
}

template <>
bool
record_file_reader_t<gzip_reader_t>::open_single_file(const std::string &path)
{
    gzFile file;
    if (!open_single_file_common(path, file))
        return false;
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    input_file_ = std::unique_ptr<gzip_reader_t>(new gzip_reader_t(file));
    return true;
}

template <>
bool
record_file_reader_t<gzip_reader_t>::read_next_entry()
{
    trace_entry_t *entry = read_next_entry_common(input_file_.get(), &eof_);
    if (entry == nullptr)
        return false;
    cur_entry_ = *entry;
    VPRINT(this, 4, "Read from file: type=%s (%d), size=%d, addr=%zu\n",
           trace_type_names[cur_entry_.type], cur_entry_.type, cur_entry_.size,
           cur_entry_.addr);
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
