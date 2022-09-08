/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
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

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<gzip_reader_t>::~file_reader_t<gzip_reader_t>()
{
    for (auto &gzip : input_files_)
        gzclose(gzip.file);
    delete[] thread_eof_;
}

template <>
bool
file_reader_t<gzip_reader_t>::open_single_file(const std::string &path)
{
    gzFile file = gzopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    input_files_.emplace_back(file);
    return true;
}

template <>
bool
file_reader_t<gzip_reader_t>::read_next_thread_entry(size_t thread_index,
                                                     OUT trace_entry_t *entry,
                                                     OUT bool *eof)
{
    gzip_reader_t *gzip = &input_files_[thread_index];
    if (gzip->cur_buf >= gzip->max_buf) {
        int len = gzread(gzip->file, gzip->buf, sizeof(gzip->buf));
        // Returns less than asked-for for end of file, or –1 for error.
        // We should always get a multiple of the record size.
        if (len < static_cast<int>(sizeof(*entry)) ||
            len % static_cast<int>(sizeof(*entry)) != 0) {
            *eof = (len >= 0);
            return false;
        }
        gzip->cur_buf = gzip->buf;
        gzip->max_buf = gzip->buf + (len / sizeof(*gzip->max_buf));
    }
    *entry = *gzip->cur_buf;
    ++gzip->cur_buf;
    VPRINT(this, 4, "Read from thread #%zd file: type=%d, size=%d, addr=%zu\n",
           thread_index, entry->type, entry->size, entry->addr);
    return true;
}

template <>
bool
file_reader_t<gzip_reader_t>::is_complete()
{
    // The gzip reading interface does not support seeking to SEEK_END so there
    // is no efficient way to read the footer.
    // We could have the trace file writer seek back and set a bit at the start.
    // Currently we are forced to not use this function.
    // XXX: Should we just remove this interface, then?
    return false;
}
